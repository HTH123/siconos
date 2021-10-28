/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2021 INRIA.
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

/*! \file FirstOrderType2R.hpp
  \brief non linear relations: \f$y=h(x,\lambda,z), r=g(\lambda,z)\f$
 */

#ifndef FirstOrderType2R_H
#define FirstOrderType2R_H

#include "FirstOrderR.hpp"

typedef void (*Type2PtrH)(unsigned int, double*, unsigned int, double*, unsigned int, double*);
typedef void (*Type2PtrG)(unsigned int, double*,  unsigned int, double*);

/** First order non linear Relation.
 *
 *  Relation for First Order Dynamical Systems, with:
 *
 * \rststar
 *
 * .. math::
 *
 *   y &= h(x,\lambda,z)\\
 *   r &= g(\lambda,z)
 *
 * \endrststar
 *
 * Operators (and their corresponding plug-in):
 * - h: saved in Interaction as y (plug-in: output[0])
 * - \f$ \nabla_x h \f$: jacobianH[0] ( output[1] )
 * - g: saved in DS as r ( input[0])
 * - \f$ \nabla_\lambda g \f$: jacobianG[0] ( input[1] )
 *
 */
class FirstOrderType2R : public FirstOrderR
{
protected:
  /** serialization hooks
  */
  ACCEPT_SERIALIZATION(FirstOrderType2R);

public:

  /** Basic contructor */
  FirstOrderType2R();

  /** data constructor
   *  \param pluginh name of the plugin to compute h
   *  \param pluging name of the plugin to compute g
   */
  FirstOrderType2R(const std::string& pluginh, const std::string& pluging);

  /** data constructor
  *  \param pluginh name of the plugin to compute h
  *  \param pluging name of the plugin to compute g
  *  \param pluginJacobianhx name of the plugin to compute the Jacobian of h according to x \f$\nabla_x h\f$
  *  \param pluginJacobianglambda name of the plugin to compute the jacobian of g according to lambda
  */
  FirstOrderType2R(const std::string& pluginh, const std::string& pluging, const std::string& pluginJacobianhx, const std::string& pluginJacobianglambda);

  /** destructor
  */
  ~FirstOrderType2R() {};

  /** initialize the relation (check sizes, memory allocation ...)
   * \param inter the interaction that owns this relation
   */
  virtual void initialize(Interaction& inter);

  /** check sizes of the relation specific operators.
   * \param inter an Interaction using this relation
   */
  virtual void checkSize(Interaction& inter);

  /** to compute the output y = h(t,x,...) of the Relation
      \param time current time value
      \param x coordinates of the dynamical systems involved in the relation
      \param lambda interaction \f$\lambda\f$ vector
      \param y the resulting vector
  */
  virtual void computeh(double time, const BlockVector& x, const SiconosVector& lambda, SiconosVector& y);

  /** to compute the nonsmooth input r = g(t,x,...) of the Relation
      \param time current time value
      \param lambda interaction \f$\lambda\f$ vector
      \param r the resulting vector
  */
  virtual void computeg(double time, const SiconosVector& lambda, BlockVector& r);

  /** to compute \f$ C = \nabla_x h \f$
      \param time current time value
      \param x coordinates of the dynamical systems involved in the relation
      \param lambda interaction \f$\lambda\f$ vector
      \param[out] C the resulting matrix
  */
  virtual void computeJachx(double time, const BlockVector& x, const SiconosVector& lambda, SimpleMatrix& C);

  /** to compute \f$ B = \nabla_{\lambda}g \f$
      \param time current time value
      \param lambda interaction \f$\lambda\f$ vector
      \param[out] B the resulting matrix
  */
  virtual void computeJacglambda(double time, const SiconosVector& lambda, SimpleMatrix& B);

  /** to compute \f$ D = \nabla_{\lambda}h \f$
      \param time current time value
      \param x coordinates of the dynamical systems involved in the relation
      \param lambda interaction \f$\lambda\f$ vector
      \param[out] D the resulting matrix
  */
  virtual void computeJachlambda(double time, const BlockVector& x, const SiconosVector& lambda, SimpleMatrix& D);

  /** default function to compute y, using the data from the Interaction and DS
  *  \param time current time (not used)
  *  \param inter Interaction using this Relation
  *  \param level not used
  */
  virtual void computeOutput(double time, Interaction& inter,
                             unsigned int level = 0);

  /** default function to compute r, using the data from the Interaction and DS
  *  \param time current time (not used)
  *  \param inter Interaction using this Relation
  *  \param level not used
  */
  virtual void computeInput(double time, Interaction& inter,
                            unsigned int level = 0);

  /** return true if the relation requires the computation of residu
      \return true if residu are required, false otherwise
   */
  virtual bool requireResidu()
  {
    return true;
  }

  virtual void computeJach(double time, Interaction& inter);

  virtual void computeJacg(double time, Interaction& inter);

  ACCEPT_STD_VISITORS();

};

TYPEDEF_SPTR(FirstOrderType2R)

#endif
