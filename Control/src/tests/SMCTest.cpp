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
#include "SMCTest.hpp"

#include <ioMatrix.hpp>
#include <FirstOrderLinearTIDS.hpp>

#include "ControlZOHSimulation.hpp"
#include "ControlLsodarSimulation.hpp"
#include "LinearSensor.hpp"
#include "LinearSMC.hpp"
#include "ExplicitLinearSMC.hpp"
#include "Twisting.hpp"

#define CPPUNIT_ASSERT_NOT_EQUAL(message, alpha, omega)      \
            if ((alpha) == (omega)) CPPUNIT_FAIL(message);

// test suite registration
CPPUNIT_TEST_SUITE_REGISTRATION(SMCTest);


void SMCTest::setUp()
{
  _A.reset(new SimpleMatrix(_n, _n, 0));
  (*_A)(0, 1) = 1.0;
  (*_A)(1, 0) = 19.0;
  (*_A)(1, 1) = -2.0;

  _x0.reset(new SiconosVector(_n, 0));
  (*_x0)(0) = -15.0;
  (*_x0)(1) = 20.0;

  _C.reset(new SimpleMatrix(2, 2, 0));
  _C->eye();

  _B.reset(new SimpleMatrix(2, 1));
  (*_B)(1, 0) = 1.0;

  _Csurface.reset(new SimpleMatrix(1, 2, 0));
  (*_Csurface)(0, 0) = 1.0;
  (*_Csurface)(0, 1) = 1.0;

}

void SMCTest::init()
{
  _DS.reset(new FirstOrderLinearTIDS(_x0, _A));
  _sensor.reset(new LinearSensor(_DS, _C));
  _iSMC.reset(new LinearSMC(_sensor, _B));
  _iSMC->setCsurface(_Csurface);
}

void SMCTest::init2()
{
  _DS.reset(new FirstOrderLinearTIDS(_x0, _A));
  _sensor.reset(new LinearSensor(_DS, _C));
  _eSMC.reset(new ExplicitLinearSMC(_sensor, _B));
  _eSMC->setCsurface(_Csurface);
}

void SMCTest::initTwisting()
{
  _DS.reset(new FirstOrderLinearTIDS(_x0, _A));
  _sensor.reset(new LinearSensor(_DS, _C));
  _itw.reset(new Twisting(_sensor, 300., _beta, _h));
  SP::SimpleMatrix eye(new SimpleMatrix(2 , 2));
  eye->eye();
  _itw->setCsurface(eye);
}


void SMCTest::tearDown()
{}

void SMCTest::test_iSMC_ZOH()
{
  init();
  SP::ControlZOHSimulation simZOH(new ControlZOHSimulation(_t0, _T, _h));
  simZOH->setSaveOnlyMainSimulation(true);
  simZOH->addDynamicalSystem(_DS);
  simZOH->addSensor(_sensor, _h);
  simZOH->addActuator(_iSMC, _h);
  simZOH->initialize();
  simZOH->run();
  SimpleMatrix& data = *simZOH->data();
  ioMatrix::write("iSMC_ZOH.dat", "ascii", data, "noDim");
  // Reference Matrix
  SimpleMatrix dataRef(data);
  dataRef.zero();
  ioMatrix::read("iSMC.ref", "ascii", dataRef);
  std::cout << "------- Integration done, error = " << (data - dataRef).normInf() << " -------" <<std::endl;
  CPPUNIT_ASSERT_EQUAL_MESSAGE("test_iSMC_ZOH : ", (data - dataRef).normInf() < _tol, true);
}

void SMCTest::test_iSMC_Lsodar()
{
  init();
  SP::ControlLsodarSimulation simLsodar(new ControlLsodarSimulation(_t0, _T, _h));
  simLsodar->setSaveOnlyMainSimulation(true);
  simLsodar->addDynamicalSystem(_DS);
  simLsodar->addSensor(_sensor, _h);
  simLsodar->addActuator(_iSMC, _h);
  simLsodar->initialize();
  simLsodar->run();
  SimpleMatrix& data = *simLsodar->data();
  ioMatrix::write("iSMC_Lsodar.dat", "ascii", data, "noDim");
  // Reference Matrix
  SimpleMatrix dataRef(data);
  dataRef.zero();
  ioMatrix::read("iSMC.ref", "ascii", dataRef);
  std::cout << "------- Integration done, error = " << (data - dataRef).normInf() << " -------" <<std::endl;
  CPPUNIT_ASSERT_EQUAL_MESSAGE("test_iSMC_Lsodar : ", (data - dataRef).normInf() < _tol, true);
}

void SMCTest::test_eSMC_ZOH()
{
  init2();
  SP::ControlZOHSimulation simZOH(new ControlZOHSimulation(_t0, _T, _h));
  simZOH->setSaveOnlyMainSimulation(true);
  simZOH->addDynamicalSystem(_DS);
  simZOH->addSensor(_sensor, _h);
  simZOH->addActuator(_eSMC, _h);
  simZOH->initialize();
  simZOH->run();
  SimpleMatrix& data = *simZOH->data();
  ioMatrix::write("eSMC_ZOH.dat", "ascii", data, "noDim");
  // Reference Matrix
  SimpleMatrix dataRef(data);
  dataRef.zero();
  ioMatrix::read("eSMC.ref", "ascii", dataRef);
  std::cout << "------- Integration done, error = " << (data - dataRef).normInf() << " -------" <<std::endl;
  CPPUNIT_ASSERT_EQUAL_MESSAGE("test_iSMC_ZOH : ", (data - dataRef).normInf() < _tol, true);
}

void SMCTest::test_eSMC_Lsodar()
{
  init2();
  SP::ControlLsodarSimulation simLsodar(new ControlLsodarSimulation(_t0, _T, _h));
  simLsodar->setSaveOnlyMainSimulation(true);
  simLsodar->addDynamicalSystem(_DS);
  simLsodar->addSensor(_sensor, _h);
  simLsodar->addActuator(_eSMC, _h);
  simLsodar->initialize();
  simLsodar->run();
  SimpleMatrix& data = *simLsodar->data();
  ioMatrix::write("eSMC_Lsodar.dat", "ascii", data, "noDim");
  // Reference Matrix
  SimpleMatrix dataRef(data);
  dataRef.zero();
  ioMatrix::read("eSMC.ref", "ascii", dataRef);
  std::cout << "------- Integration done, error = " << (data - dataRef).normInf() << " -------" <<std::endl;
  CPPUNIT_ASSERT_EQUAL_MESSAGE("test_iSMC_Lsodar : ", (data - dataRef).normInf() < _tol, true);
}

void SMCTest::test_itw_ZOH()
{
  initTwisting();
  SP::ControlZOHSimulation simZOH(new ControlZOHSimulation(_t0, _T, _h));
  simZOH->setSaveOnlyMainSimulation(true);
  simZOH->addDynamicalSystem(_DS);
  simZOH->addSensor(_sensor, _h);
  simZOH->addActuator(_itw, _h);
  simZOH->initialize();
  simZOH->run();
  SimpleMatrix& data = *simZOH->data();
  ioMatrix::write("itw_ZOH.dat", "ascii", data, "noDim");
  // Reference Matrix
  SimpleMatrix dataRef(data);
  dataRef.zero();
  ioMatrix::read("itw.ref", "ascii", dataRef);
  // it is a bad idea to compare solutions to an AVI that does not admit a unique solution
  SiconosVector lambda1 = SiconosVector(data.size(0));
  SiconosVector lambda2 = SiconosVector(data.size(0));
  data.getCol(3, lambda1);
  data.getCol(4, lambda2);
  axpy(_beta, lambda2, lambda1);
  SiconosVector lambda1Ref = SiconosVector(data.size(0));
  SiconosVector lambda2Ref = SiconosVector(data.size(0));
  dataRef.getCol(3, lambda1Ref);
  dataRef.getCol(4, lambda2Ref);
  axpy(_beta, lambda2Ref, lambda1Ref);
  data.setCol(3, lambda1);
  dataRef.setCol(3, lambda1Ref);
  data.resize(data.size(0), 4);
  dataRef.resize(data.size(0), 4);
  std::cout << "------- Integration done, error = " << (data - dataRef).normInf() << " -------" <<std::endl;
  CPPUNIT_ASSERT_EQUAL_MESSAGE("test_itw_ZOH : ", (data - dataRef).normInf() < _tol, true);
}

void SMCTest::test_itw_Lsodar()
{
  initTwisting();
  SP::ControlLsodarSimulation simLsodar(new ControlLsodarSimulation(_t0, _T, _h));
  simLsodar->setSaveOnlyMainSimulation(true);
  simLsodar->addDynamicalSystem(_DS);
  simLsodar->addSensor(_sensor, _h);
  simLsodar->addActuator(_itw, _h);
  simLsodar->initialize();
  simLsodar->run();
  SimpleMatrix& data = *simLsodar->data();
  ioMatrix::write("itw_Lsodar.dat", "ascii", data, "noDim");
  // Reference Matrix
  SimpleMatrix dataRef(data);
  dataRef.zero();
  ioMatrix::read("itw.ref", "ascii", dataRef);
  // it is a bad idea to compare solutions to an AVI that does not admit a unique solution
  SiconosVector lambda1 = SiconosVector(data.size(0));
  SiconosVector lambda2 = SiconosVector(data.size(0));
  data.getCol(3, lambda1);
  data.getCol(4, lambda2);
  axpy(_beta, lambda2, lambda1);
  SiconosVector lambda1Ref = SiconosVector(data.size(0));
  SiconosVector lambda2Ref = SiconosVector(data.size(0));
  dataRef.getCol(3, lambda1Ref);
  dataRef.getCol(4, lambda2Ref);
  axpy(_beta, lambda2Ref, lambda1Ref);
  data.setCol(3, lambda1);
  dataRef.setCol(3, lambda1Ref);
  data.resize(data.size(0), 4);
  dataRef.resize(data.size(0), 4);
  std::cout << "------- Integration done, error = " << (data - dataRef).normInf() << " -------" <<std::endl;
  CPPUNIT_ASSERT_EQUAL_MESSAGE("test_itw_Lsodar : ", (data - dataRef).normInf() < _tol, true);
}
