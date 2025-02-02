.. _siconos_getting_started:

Quickstart
==========

This manual aims at giving you a short overview of the software functionnalities and to provide some guidelines for your first steps with nonsmooth systems
simulation with Siconos. There you will learn basics things to know to describe a nonsmooth problem or to run a simulation using siconos, for both APIs, C++ or Python.
For a more detailed description of Siconos and its functionnalities please check the :ref:`full_documentation` table of content.


.. contents::
   :local:


What is Siconos?
----------------

Siconos is an open-source scientific software primarily targeted at modeling and simulating nonsmooth dynamical systems :cite:`Acary.Brogliato2008`: 

* **Mechanical systems** (rigid or solid) with unilateral contact and Coulomb friction and impact
  (Nonsmooth mechanics, contact dynamics, multibody systems dynamics or granular materials).
* **Switched Electrical Circuits** such as electrical circuits with ideal and piecewise linear components: power converter, rectifier, Phase-Locked Loop (PLL) or Analog-to-Digital converter.
* Sliding mode control systems.
* Biology Gene regulatory networks.

Other applications are found in Systems and Control (hybrid systems, differential inclusions,
optimal control with state constraints), Optimization (Complementarity systems and Variational inequalities), 
Fluid Mechanics, Computer Graphics, ...

Check :ref:`siconos_examples` manual for an overview of the various problems handled with Siconos.
  

Try it
------

You can use one of the end-user docker images available in `siconos-tutorials registry <https://gricad-gitlab.univ-grenoble-alpes.fr/nonsmooth/siconos-tutorials/container_registry>`_ .

For example, start a jupyter container including siconos, all running examples and some tutorials notebooks,

.. code-block:: bash


docker run -p 8888:8888 -ti gricad-registry.univ-grenoble-alpes.fr/nonsmooth/siconos-tutorials/siconoslab-release-4.4
# Then, access in your browser at http://localhost:8888
# and start siconos-notebooks/index.ipynb

Another possible (but unstable) way to start with Siconos, try the tutorial notebooks available here :

.. image:: https://mybinder.org/badge.svg
   :target: https://mybinder.org/v2/git/https%3A%2F%2Fgricad-gitlab.univ-grenoble-alpes.fr%2Fnonsmooth%2Fsiconos-tutorial.git/b08a0514b22b3927b58bddce3c4018f27ac0fc7d?filepath=siconos-notebooks%2Findex.ipynb

This page proposes a Python interactive interface (notebooks) to Siconos, that you can use through your web browser, online.


Siconos usage in a few steps
----------------------------

#.  :ref:`download` the software
#.  :ref:`siconos_install_guide`
#.  Run your first simulation.

There are two ways to use Siconos

* As a library (C++) : in that case you need to write a c++ file (driver) to describe and solve your problem,
  and then run siconos to build and execute your program.
* As a Python package.

Python API is generated (swig) from C++ and thus both API are quite equivalent although C++ might be more complete. 
 
Anyway, for new users we recommend the Python API which is easier to understand.

Below are two examples (Python and C++) of a Siconos process. We just build and print a first-order dynamical
system (See :class:`FirstOrderLinearDS`).

**Example of Siconos Python API usage**

.. code-block:: python

   # import siconos package
   import siconos.kernel as sk
   # import numpy package
   import numpy as np

   # Create a dynamical system
   size = 10
   x0 = np.random.random(size)
   ds = sk.FirstOrderLinearDS(x0)
   print(ds)

**Example of Siconos C++ API usage**

Write a c++ file, e.g. run.cpp

.. code-block:: c++

   #include "SiconosKernel.hpp"
   int main()
   {
    unsigned int size = 10;
    SP::SiconosVector x0(new SiconosVector(size));
    SP::SimpleMatrix A(new SimpleMatrix(size, size));
    A->randomize();

    SP::FirstOrderLinearDS ds(new FirstOrderLinearDS(x0, A));
    ds->display();
   }

And, compile, link and execute (in one shot, thanks to siconos script)::

  siconos run.cpp


For new simulation, start with the :ref:`template for c++ driver file <template_siconos_driver>`
or try to mimic one of the examples available at (https://github.com/siconos/siconos-tutorials).
  
More
----
     
We should sort, clean, update the followings ...

.. toctree::
   :maxdepth: 2

   nsds_basics
   tutorial_python/index
   tutorial_cpp/siconos_tutorial
   tutorial_cpp/RLCD
   running_siconos
   cpp_reminder

