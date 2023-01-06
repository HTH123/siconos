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

/*! \file StaticBody.hpp
  \brief Definition of an abstract 3D rigid body above NewtonEulerDS
*/


#ifndef StaticBody_h
#define StaticBody_h

// #include <MechanicsFwd.hpp>
// #include <NewtonEulerDS.hpp>
// #include <SiconosVisitor.hpp>
// #include <SiconosContactor.hpp>

class StaticBody : public std::enable_shared_from_this<StaticBody>
{
public:

  StaticBody() {};
  SP::SiconosContactorSet contactorSet;
  SP::SiconosVector base;
  int  number;
  virtual ~StaticBody() {};
};

#endif /* StaticBody_h */