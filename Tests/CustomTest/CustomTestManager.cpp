/*    
    This file is a part of Stonefish.

    Stonefish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Stonefish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

//
//  JointsTestManager.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 04/03/2014.
//  Copyright (c) 2014-2021 Patryk Cieslak. All rights reserved.
//

#include "CustomTestManager.h"

#include <core/ScenarioParser.h>
#include <entities/statics/Plane.h>
#include <entities/solids/Box.h>
#include <entities/solids/Sphere.h>
#include <entities/solids/Cylinder.h>
#include <graphics/OpenGLPointLight.h>
#include <graphics/OpenGLTrackball.h>
#include <joints/FixedJoint.h>
#include <joints/SphericalJoint.h>
#include <joints/RevoluteJoint.h>
#include <joints/PrismaticJoint.h>
#include <joints/CylindricalJoint.h>
#include <utils/UnitSystem.h>
#include <utils/SystemUtil.hpp>

CustomTestManager::CustomTestManager(sf::Scalar stepsPerSecond) 
  : SimulationManager(stepsPerSecond, sf::SolverType::SOLVER_SI, sf::CollisionFilteringType::COLLISION_EXCLUSIVE)
{
}

void CustomTestManager::BuildScenario()
{
    sf::ScenarioParser parser(this);
    parser.Parse(std::string(DATA_DIR_PATH) + "sparus2_haifa_seafloor.scn");
}
