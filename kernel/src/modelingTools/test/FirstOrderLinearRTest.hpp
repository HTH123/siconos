/* Siconos-Kernel, Copyright INRIA 2005-2012.
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
#ifndef __FirstOrderLinearRTest__
#define __FirstOrderLinearRTest__

#include <cppunit/extensions/HelperMacros.h>
#include "NonSmoothDynamicalSystem.hpp"
#include "FirstOrderLinearR.hpp"

class FirstOrderLinearRTest : public CppUnit::TestFixture
{

private:
  /** serialization hooks
  */
  ACCEPT_SERIALIZATION(FirstOrderLinearRTest);


  // Name of the tests suite
  CPPUNIT_TEST_SUITE(FirstOrderLinearRTest);

  // tests to be done ...

  CPPUNIT_TEST(testBuildFirstOrderLinearR1);
  CPPUNIT_TEST(testBuildFirstOrderLinearR3);
  CPPUNIT_TEST(testBuildFirstOrderLinearR4);
  CPPUNIT_TEST(testBuildFirstOrderLinearR5);
  //  CPPUNIT_TEST(testSetC);
  CPPUNIT_TEST(testSetCPtr);
  CPPUNIT_TEST(testSetCPtr2);
  //  CPPUNIT_TEST(testSetD);
  CPPUNIT_TEST(testSetDPtr);
  //  CPPUNIT_TEST(testSetF);
  CPPUNIT_TEST(testSetFPtr);
  //  CPPUNIT_TEST(testSetE);
  CPPUNIT_TEST(testSetEPtr);
  //  CPPUNIT_TEST(testSetB);
  CPPUNIT_TEST(testSetBPtr);
  CPPUNIT_TEST(End);

  CPPUNIT_TEST_SUITE_END();

  // \todo exception test

  void testBuildFirstOrderLinearR1();
  void testBuildFirstOrderLinearR3();
  void testBuildFirstOrderLinearR4();
  void testBuildFirstOrderLinearR5();
  //  void testSetC();
  void testSetCPtr();
  void testSetCPtr2();
  //  void testSetD();
  void testSetDPtr();
  //  void testSetF();
  void testSetFPtr();
  //  void testSetE();
  void testSetEPtr();
  //  void testSetB();
  void testSetBPtr();
  void End();

  // Members

  SP::SimpleMatrix C, B, F, D;
  SP::SiconosVector e;
  SP::NonSmoothDynamicalSystem nsds;

public:
  void setUp();
  void tearDown();

};

#endif



