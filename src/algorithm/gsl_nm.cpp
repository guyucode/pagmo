/*****************************************************************************
 *   Copyright (C) 2004-2009 The PaGMO development team,                     *
 *   Advanced Concepts Team (ACT), European Space Agency (ESA)               *
 *   http://apps.sourceforge.net/mediawiki/pagmo                             *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Developers  *
 *   http://apps.sourceforge.net/mediawiki/pagmo/index.php?title=Credits     *
 *   act@esa.int                                                             *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program; if not, write to the                           *
 *   Free Software Foundation, Inc.,                                         *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.               *
 *****************************************************************************/

#include <algorithm>
#include <boost/numeric/conversion/cast.hpp>
#include <cstddef>
#include <gsl/gsl_multimin.h>
#include <gsl/gsl_vector.h>
#include <new>
#include <sstream>
#include <string>

#include "../exceptions.h"
#include "../population.h"
#include "../problem/base.h"
#include "../types.h"
#include "base.h"
#include "gsl_nm.h"

namespace pagmo { namespace algorithm {

// Structure to feed parameters to the wrapper function.
struct gsl_nm_wrapper_params
{
	problem::base const		*p;
	decision_vector			x;
	fitness_vector			f;
	problem::base::size_type	cont_size;
};

// Main wrapper function.
inline static double gsl_nm_wrapper(const gsl_vector *v, void *params)
{
	gsl_nm_wrapper_params *par = (gsl_nm_wrapper_params *)params;
	// Fill up the continuous part of temporary storage with the contents of v.
	for (problem::base::size_type i = 0; i < par->cont_size; ++i) {
		par->x[i] = gsl_vector_get(v,i);
	}
	// Compute the objective function.
	par->p->objfun(par->f,par->x);
	return par->f[0];
}

/// Constructor.
/**
 * Constructor from maximum number of iterations, tolerance and initial step size.
 *
 * @param[in] max_iter maximum number of iterations the algorithm will be allowed to perform.
 * @param[in] tol desired tolerance in the localisation of the minimum.
 * @param[in] step_size initial step size for the simplex.
 */
gsl_nm::gsl_nm(int max_iter, const double &tol, const double &step_size):m_max_iter(boost::numeric_cast<std::size_t>(max_iter)),m_tol(tol),m_step_size(step_size)
{
	if (m_tol <= 0) {
		pagmo_throw(value_error,"tolerance must be a positive number");
	}
	if (step_size <= 0) {
		pagmo_throw(value_error,"initial step size must be a positive number");
	}
}

/// Clone method.
/**
 * @return algorithm::base_ptr to a copy of this.
 */
base_ptr gsl_nm::clone() const
{
	return base_ptr(new gsl_nm(*this));
}

/// Extra information in human readable format.
/**
 * @return a formatted string displaying the parameters of the algorithm.
 */
std::string gsl_nm::human_readable_extra() const
{
	std::ostringstream oss;
	oss << "\tmax_iter:\t" << m_max_iter << '\n';
	oss << "\ttolerance:\t" << m_tol << '\n';
	oss << "\tstep size:\t" << m_step_size << '\n';
	return oss.str();
}

/// Evolve method.
/**
 * The best individual of the population will be optimised.
 *
 * @param[in,out] pop population to be evolved.
 */
void gsl_nm::evolve(population &pop) const
{
	// Do nothing if the population is empty.
	if (!pop.size()) {
		return;
	}
	// Useful variables.
	const problem::base &problem = pop.problem();
	if (problem.get_f_dimension() != 1) {
		pagmo_throw(value_error,"this algorithm does not support multi-objective optimisation");
	}
	if (problem.get_c_dimension()) {
		pagmo_throw(value_error,"this algorithm does not support constrained optimisation");
	}
	const problem::base::size_type cont_size = problem.get_dimension() - problem.get_i_dimension();
	if (!cont_size) {
		pagmo_throw(value_error,"the problem has no continuous part");
	}
	// Extract the best individual.
	const population::size_type best_ind_idx = pop.get_best_idx();
	const population::individual_type &best_ind = pop.get_individual(best_ind_idx);
	// GSL wrapper parameters structure.
	gsl_nm_wrapper_params params;
	params.p = &problem;
	// Integer part of the temporay decision vector must be filled with the integer part of the best individual,
	// which will not be optimised.
	params.x.resize(problem.get_dimension());
	std::copy(best_ind.cur_x.begin() + cont_size, best_ind.cur_x.end(), params.x.begin());
	params.f.resize(1);
	params.cont_size = cont_size;
	// GSL function structure.
	gsl_multimin_function gsl_func;
	// Number of function components.
	gsl_func.n = boost::numeric_cast<std::size_t>(cont_size);
	gsl_func.f = &gsl_nm_wrapper;
	gsl_func.params = (void *)&params;
	// Mimimizer type and pointer.
	const gsl_multimin_fminimizer_type *type = gsl_multimin_fminimizer_nmsimplex2;
	gsl_multimin_fminimizer *s = 0;
	// Starting point and step sizes.
	gsl_vector *x = 0, *ss = 0;
	// Here we start the allocations.
	{
		// Recast as size_t here, in order to avoid potential overflows later.
		const std::size_t s_cont_size = boost::numeric_cast<std::size_t>(cont_size);
		// Allocate and check the allocation results.
		x = gsl_vector_alloc(s_cont_size);
		ss = gsl_vector_alloc(s_cont_size);
		s = gsl_multimin_fminimizer_alloc(type,s_cont_size);
		// Check the allocations.
		check_allocs(x,ss,s);
		// Starting point comes from the best individual.
		for (std::size_t i = 0; i < s_cont_size; ++i) {
			gsl_vector_set(x,i,best_ind.cur_x[i]);
		}
		// Set initial step sizes.
		gsl_vector_set_all(ss,m_step_size);
		// Init the solver.
		gsl_multimin_fminimizer_set(s,&gsl_func,x,ss);
		// Iterate.
		std::size_t iter = 0;
		int status;
		double size;
		do
		{
			++iter;
			status = gsl_multimin_fminimizer_iterate(s);
			if (status) {
				break;
			}
			size = gsl_multimin_fminimizer_size(s);
			status = gsl_multimin_test_size(size, m_tol);
		} while (status == GSL_CONTINUE && iter < m_max_iter);
	}
	// Free up resources.
	cleanup(x,ss,s);
	// Check the generated individual and change it to respect the bounds as necessary.
	for (problem::base::size_type i = 0; i < cont_size; ++i) {
		if (params.x[i] < problem.get_lb()[i]) {
			params.x[i] = problem.get_lb()[i];
		}
		if (params.x[i] > problem.get_ub()[i]) {
			params.x[i] = problem.get_ub()[i];
		}
	}
	// Replace the best individual.
	pop.set_x(best_ind_idx,params.x);
}

// Check GSL allocations done during evolve().
void gsl_nm::check_allocs(gsl_vector *x, gsl_vector *ss, gsl_multimin_fminimizer *s)
{
	// If at least one allocation failed, cleanup and throw a memory error.
	if (!x || !ss || !s) {
		cleanup(x,ss,s);
		throw std::bad_alloc();
	}
}

// Cleanup allocations done during evolve.
void gsl_nm::cleanup(gsl_vector *x, gsl_vector *ss, gsl_multimin_fminimizer *s)
{
	if (x) {
		gsl_vector_free(x);
	}
	if (ss) {
		gsl_vector_free(ss);
	}
	if (s) {
		gsl_multimin_fminimizer_free(s);
	}
}

}}
