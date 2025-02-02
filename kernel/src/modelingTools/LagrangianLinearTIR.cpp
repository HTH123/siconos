/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2022 INRIA.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include "LagrangianLinearTIR.hpp"
#include "SiconosAlgebraProd.hpp"  // for matrix-vector prod
#include "Interaction.hpp"
//
#include "LagrangianDS.hpp"
#include "BlockVector.hpp"
#include "SimulationGraphs.hpp"

#include <iostream>
// #define DEBUG_NOCOLOR
// #define DEBUG_STDOUT
// #define DEBUG_MESSAGES
#include "siconos_debug.h"

using namespace RELATION;

// Minimum data (C as pointer) constructor
LagrangianLinearTIR::LagrangianLinearTIR(SP::SimpleMatrix C):
  LagrangianR(LinearTIR)
{
  _jachq = C;
}

// Constructor from a complete set of data
LagrangianLinearTIR::LagrangianLinearTIR(SP::SimpleMatrix C,  SP::SimpleMatrix F, SP::SiconosVector e):
  LagrangianR(LinearTIR)
{
  _jachq = C;
  _F = F;
  _e = e;
}

// Minimum data (C, e as pointers) constructor
LagrangianLinearTIR::LagrangianLinearTIR(SP::SimpleMatrix C, SP::SiconosVector e):
  LagrangianR(LinearTIR)
{
  _jachq = C;
  _e = e;
}


void LagrangianLinearTIR::checkSize(Interaction& inter)
{
  unsigned int sizeY = inter.dimension();
  VectorOfBlockVectors& DSlink = inter.linkToDSVariables();
  if(!(_jachq) || _jachq->size(1) !=  inter.getSizeOfDS() ||  _jachq->size(0) != sizeY)
    THROW_EXCEPTION("LagrangianLinearTIR::checkSize inconsistent sizes between H matrix and the interaction.");

  if((_e) && _e->size() != sizeY)
    THROW_EXCEPTION("LagrangianLinearTIR::checkSize inconsistent sizes between e vector and the dimension of the interaction.");

  unsigned int sizeZ = DSlink[LagrangianR::z]->size();
  if((_F) && (
        _F->size(0) != sizeZ || _F->size(1) != sizeZ))
    THROW_EXCEPTION("LagrangianLinearTIR::checkSize inconsistent sizes between F matrix and the interaction.");


}
void LagrangianLinearTIR::computeOutput(double time, Interaction& inter, unsigned int derivativeNumber)
{
  DEBUG_BEGIN("LagrangianLinearTIR::computeOutput(double time, Interaction& inter, unsigned int derivativeNumber)\n");
  // get y and lambda of the interaction
  SiconosVector& y = *inter.y(derivativeNumber);
  VectorOfBlockVectors& DSlink = inter.linkToDSVariables();
  prod(*_jachq, *DSlink[LagrangianR::q0 + derivativeNumber], y);

  if(derivativeNumber == 0)
  {
    if(_e)
      y += *_e;
    if(_F)
      prod(*_F, *DSlink[LagrangianR::z], y, false);
  }

  if(_jachlambda)
  {
    SiconosVector& lambda = *inter.lambda(derivativeNumber);
    prod(*_jachlambda, lambda, y, false);
  }
  DEBUG_END("LagrangianLinearTIR::computeOutput(double time, Interaction& inter, unsigned int derivativeNumber)\n");
}
void LagrangianLinearTIR::computeInput(double time, Interaction& inter, unsigned int level)
{
  DEBUG_BEGIN("void LagrangianLinearTIR::computeInput(double time, Interaction& inter, unsigned int level)\n")
  // get lambda of the concerned interaction
  SiconosVector& lambda = *inter.lambda(level);
  VectorOfBlockVectors& DSlink = inter.linkToDSVariables();
  // computation of p = Ht lambda
  DEBUG_EXPR(lambda.display(););
  DEBUG_EXPR(_jachq->display(););
  DEBUG_EXPR(DSlink[LagrangianR::p0 + level]->display(););
  prod(lambda, *_jachq, *DSlink[LagrangianR::p0 + level], false);
  DEBUG_END("void LagrangianLinearTIR::computeInput(double time, Interaction& inter, unsigned int level)\n")
}

void LagrangianLinearTIR::display() const
{
  LagrangianR::display();
  std::cout << "===== Lagrangian Linear Relation display ===== " <<std::endl;
  std::cout << " C: " <<std::endl;
  if(_jachq)
    _jachq->display();
  else
    std::cout << " -> nullptr " <<std::endl;
  std::cout << " e: " <<std::endl;
  if(_e)
    _e->display();
  else
    std::cout << " -> nullptr " <<std::endl;
  std::cout << " F: " <<std::endl;
  if(_F)
    _F->display();
  else
    std::cout << " -> nullptr " <<std::endl;
  std::cout << "===================================== " <<std::endl;
}
