/*****************************************************************************
 *   Copyright (C) 2008, 2009 Advanced Concepts Team (European Space Agency) *
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

#include <iostream>

#include "src/GOclasses/algorithms/CS.h"
#include "src/GOclasses/basic/island.h"
#include "src/GOclasses/problems/TrajectoryProblems.h"

using namespace std;

int main(){
        CSalgorithm algo(0.001);
        messengerfullProb prob;
        island isl(prob,algo,20);
        cout << "Best: " << isl.best().getFitness() << endl;
        isl.evolve();
        isl.join();
        cout << "Best: " << isl.best().getFitness() << endl;
        return 0;
}
