/* Siconos-Kernel, Copyright INRIA 2005-2011.
 * Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 * Siconos is a free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * Siconos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Siconos; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Contact: Vincent ACARY, siconos-team@lists.gforge.inria.fr
*/


#ifndef BulletSiconos_hpp
#define BulletSiconos_hpp

#include <bullet/BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>
#include <bullet/BulletCollision/CollisionShapes/btTriangleIndexVertexMaterialArray.h>

#include "SiconosPointers.hpp"

DEFINE_SPTR(btCollisionShape);
DEFINE_SPTR(btBoxShape);
DEFINE_SPTR(btCylinderShape);
DEFINE_SPTR(btManifoldPoint);

DEFINE_SPTR(btTriangleIndexVertexMaterialArray);

static SP::btTriangleIndexVertexMaterialArray x;

DEFINE_SPTR(btCollisionObject);
DEFINE_SAPTR(btCollisionObject);

DEFINE_SPTR(btCollisionWorld);
DEFINE_SPTR(btDefaultCollisionConfiguration);
DEFINE_SPTR(btCollisionDispatcher);
DEFINE_SPTR(btVector3);

DEFINE_SPTR(btPersistentManifold);

#endif
