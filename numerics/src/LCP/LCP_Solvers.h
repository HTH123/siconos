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
#ifndef LCP_SOLVERS_H
#define LCP_SOLVERS_H

/*!\file LCP_Solvers.h
  Subroutines for the resolution of Linear Complementarity Problems.
  \rst
  
  See detailed documentation in :ref:`lcp_solvers`
  \endrst

*/

#include "NumericsFwd.h"
#include "SiconosConfig.h"

#if defined(__cplusplus) && !defined(BUILD_AS_CPP)
extern "C"
{
#endif

  /** lcp_qp uses a quadratic programm formulation for solving a LCP
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   *                0 : convergence  / minimization sucessfull
   *                1 : Too Many iterations
   *              2 : Accuracy insuficient to satisfy convergence criterion
   *                5 : Length of working array insufficient
   *                Other : The constraints are inconstent
   *  \param[in,out] options structure used to define the solver and its parameters.
   *
   */
  void lcp_qp(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_cpg is a CPG (Conjugated Projected Gradient) solver for LCP based on quadratic minimization.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0: convergence
   1: iter = itermax
   2: negative diagonal term
   3: pWp nul
   *  \param[in,out] options structure used to define the solver and its parameters.
   *
   */
  void lcp_cpg(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_pgs (Projected Gauss-Seidel) is a basic Projected Gauss-Seidel solver for LCP.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : convergence
   1 : iter = itermax
   2 : negative diagonal term
   \param[in,out] options structure used to define the solver and its parameters.
  */
  void lcp_pgs(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_rpgs (Regularized Projected Gauss-Seidel ) is a solver for LCP, able to handle matrices with null diagonal terms.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : convergence
   1 : iter = itermax
   2 : negative diagonal term
   *  \param[in,out] options structure used to define the solver and its parameters.

   \todo Sizing the regularization paramter and apply it only on null diagnal term

  */
  void lcp_rpgs(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_psor Projected Succesive over relaxation solver for LCP. See cottle, Pang Stone Chap 5
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : convergence
   1 : iter = itermax
   2 : negative diagonal term
   *  \param[in,out] options structure used to define the solver and its parameters.
   \todo use the relax parameter
   \todo add test
   \todo add a vector of relaxation parameter wtith an auto sizing (see SOR algorithm for linear solver.)

  */
  void lcp_psor(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_nsqp use a quadratic programm formulation for solving an non symmetric LCP
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   *                0 : convergence  / minimization sucessfull
   *                1 : Too Many iterations
   *            2 : Accuracy insuficient to satisfy convergence criterion
   *                5 : Length of working array insufficient
   *                Other : The constraints are inconstent
   *  \param[in,out] options structure used to define the solver and its parameters.
   *
   */
  void lcp_nsqp(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_latin (LArge Time INcrements) is a basic latin solver for LCP.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : convergence
   1 : iter = itermax
   2 : Cholesky Factorization failed 
   3 : nul diagonal term
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_latin(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_latin_w (LArge Time INcrements) is a basic latin solver with relaxation for LCP.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : convergence
   1 : iter = itermax
   2 : Cholesky Factorization failed 
   3 : nul diagonal term
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_latin_w(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_lexicolemke is a direct solver for LCP based on pivoting method principle for degenerate problem 
   *  Choice of pivot variable is performed via lexicographic ordering 
   *  Ref: "The Linear Complementarity Problem" Cottle, Pang, Stone (1992)
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   * 0 : convergence
   * 1 : iter = itermax
   * 2 : negative diagonal term
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_lexicolemke(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_newton_min uses a nonsmooth Newton method based on the min formulation  (or max formulation) of the LCP
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   *  0 : convergence  / minimization sucessfull
   *  1 : Too Many iterations
   *  2 : Accuracy insuficient to satisfy convergence criterion
   *  5 : Length of working array insufficient
   *  Other : The constraints are inconstent
   *  \param[in,out] options structure used to define the solver and its parameters.
   *
   *  \todo Optimizing the memory allocation (Try to avoid the copy of JacH into A)
   *  \todo Add rules for the computation of the penalization rho
   *  \todo Add a globalization strategy based on a decrease of a merit function. (Nonmonotone LCP) Reference in Ferris Kanzow 2002
   */
  void lcp_newton_min(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_newton_FB use a nonsmooth newton method based on the Fischer-Bursmeister convex function
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   *  0 - convergence
   *  1 - iter = itermax
   *  2 - failure in the descent direction search (in LAPACK) 
   *
   *  \param[in,out] options structure used to define the solver and its parameters.
   *  \todo Optimizing the memory allocation (Try to avoid the copy of JacH into A)
   *  \todo Add rules for the computation of the penalization rho
   *  \todo Add a globalization strategy based on a decrease of a merit function. (Nonmonotone LCP) Reference in Ferris Kanzow 2002
   */

  void lcp_newton_FB(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_newton_minFB use a nonsmooth newton method based on both a min and Fischer-Bursmeister reformulation
   *  References: Facchinei--Pang (2003)
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   *  0 - convergence
   *  1 - iter = itermax
   *  2 - failure in the descent direction search (in LAPACK) 
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_newton_minFB(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** path solver
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : convergence
   1 : iter = itermax
   2 : negative diagonal term
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_path(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** enumerative solver
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   0 : success
   1 : failed
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_enum(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** Proceed with initialisation required before any call of the enum solver.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] options structure used to define the solver and its parameters.
   *  \param[in] withMemAlloc If it is not 0, then the necessary work memory is allocated.
   */
  void lcp_enum_init(LinearComplementarityProblem* problem, SolverOptions* options, int withMemAlloc);
  
  /** Reset state for enum solver parameters.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] options structure used to define the solver and its parameters.
   *  \param[in]  withMemAlloc If it  is not 0, then the work memory is free.
   */
  void lcp_enum_reset(LinearComplementarityProblem* problem, SolverOptions* options, int withMemAlloc);

  /** lcp_avi_caoferris is a direct solver for LCP based on an Affine Variational Inequalities (AVI) reformulation
   *  The AVI solver is here the one from Cao and Ferris 
   *  Ref: "A Pivotal Method for Affine Variational Inequalities" Menglin Cao et Michael Ferris (1996)
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   * 0 : convergence
   * 1 : iter = itermax
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_avi_caoferris(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_pivot is a direct solver for LCP based on a pivoting method
   *  It can currently use Bard, Murty's least-index or Lemke rule for choosing
   *  the pivot. The default one is Lemke and it can be changed by setting
   *  iparam[SICONOS_LCP_IPARAM_PIVOTING_METHOD_TYPE]. The list of choices are in the enum LCP_PIVOT (see lcp_cst.h).
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   * 0 : convergence
   * 1 : iter = itermax
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_pivot(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);
  void lcp_pivot_covering_vector(LinearComplementarityProblem* problem, double* u , double* s, int *info , SolverOptions* options, double* cov_vec);
  void lcp_pivot_lumod(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);
  void lcp_pivot_lumod_covering_vector(LinearComplementarityProblem* problem, double* u , double* s, int *info , SolverOptions* options, double* cov_vec);

  /** lcp_pathsearch is a direct solver for LCP based on the pathsearch algorithm
   *  \warning this solver is available for testing purposes only! consider
   *  using lcp_pivot() if you are looking for simular solvers
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   * 0 : convergence
   * 1 : iter = itermax
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_pathsearch(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** lcp_gams uses the solver provided by GAMS 
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] info an integer which returns the termination value:
   * 0 : convergence
   * 1 : iter = itermax
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_gams(LinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options);

  /** generic interface used to call any LCP solver applied on a Sparse-Block structured matrix M, with a Gauss-Seidel process
   *  to solve the global problem (formulation/solving of local problems for each row of blocks)
   *
   *  \param[in] problem structure that represents the LCP (M, q...). M must be a SparseBlockStructuredMatrix
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param info an integer which returns the termination value:
   0 : convergence
   >0 : failed, depends on local solver
   *  \param[in,out] options structure used to define the solver and its parameters.
   */
  void lcp_nsgs_SBM(LinearComplementarityProblem* problem, double *z, double *w, int* info, SolverOptions* options);
  /** Construct local problem from a "global" one
   *
   *  \param rowNumber index of the local problem
   *  \param blmat matrix containing the problem
   *  \param local_problem problem to fill
   *  \param q big q
   *  \param z big z
   */
  void lcp_nsgs_SBM_buildLocalProblem(int rowNumber, SparseBlockStructuredMatrix* const blmat, LinearComplementarityProblem* local_problem, double* q, double* z);

  /** Computes error criterion and update \f$ w = Mz + q \f$.
   *
   *  \param[in] problem structure that represents the LCP (M, q...)
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[in] tolerance threshold used to validate the solution: if the error is less than this value, the solution is accepted
   *  \param[out] error the actual error of the solution with respect to the problem
   *  \return status: 0 : convergence, 1: error > tolerance
   */
  int lcp_compute_error(LinearComplementarityProblem* problem, double *z , double *w, double tolerance, double* error);

  /** Computes error criterion.
      \rst 
      see :ref:`lcp_error`
      \endrst 
   *
   *  \param[in] n size of the LCP
   *  \param[in,out] z a n-vector of doubles which contains the initial solution and returns the solution of the problem.
   *  \param[in,out] w a n-vector of doubles which returns the solution of the problem.
   *  \param[out] error the result of the computation
   */
  void lcp_compute_error_only(unsigned int n,  double *z , double *w, double * error);

  /*   /\** Function used to extract from LCP matrix the part which corresponds to non null z */
  /*    *\/ */
  /*   int extractLCP( NumericsMatrix* MGlobal, double *z , int *indic, int *indicop, double *submatlcp , double *submatlcpop, */
  /*     int *ipiv , int *sizesublcp , int *sizesublcpop); */

  /*   /\** Function used to solve the problem with sub-matrices from extractLCP */
  /*    *\/ */
  /*   int predictLCP(int sizeLCP, double* q, double *z , double *w , double tol, */
  /*   int *indic , int *indicop , double *submatlcp , double *submatlcpop , */
  /*    int *ipiv , int *sizesublcp , int *sizesublcpop , double *subq , double *bufz , double *newz); */

  /**
     Interface to solvers for Linear Complementarity Problems, dedicated to dense matrix storage
     
     \param[in] problem the LinearComplementarityProblem structure which handles the problem (M,q)
     \param[in,out] z a n-vector of doubles which contains the solution of the problem.
     \param[in,out] w a n-vector of doubles which contains the solution of the problem.
     \param[in,out] options structure used to define the solver(s) and their parameters
     \return info termination value
     - 0 : successful
     - >0 : otherwise see each solver for more information about the log info
  */
  int lcp_driver_DenseMatrix(LinearComplementarityProblem* problem, double *z , double *w, SolverOptions* options);

  void lcp_ConvexQP_ProjectedGradient(LinearComplementarityProblem* problem, double *reaction, double *velocity, int* info, SolverOptions* options);

  /**
     \defgroup SetSolverOptions Functions used to set values for the solver parameters of each driver. 
     
     Each function is of the form : 
     
     void  <formulation>_<solver_name>_set_default(SolverOptions*)
     
     e.g. void lcp_lexicolemke_set_default(SolverOptions * options)
     
     No need to create a function if the solvers does not need specific parameters. Use 
     solver_options_initialize instead.
     
     \param options SolverOptions structure to be initialized (must not be the NULL pointer !)
     
     @{
  */
  void lcp_lexicolemke_set_default(SolverOptions* options);
  void lcp_nsgs_sbm_set_default(SolverOptions* options);
  void lcp_latin_set_default(SolverOptions* options);
  void lcp_latin_w_set_default(SolverOptions* options);
  void lcp_newton_FB_set_default(SolverOptions* options);
  void lcp_psor_set_default(SolverOptions* options);
  void lcp_rpgs_set_default(SolverOptions* options);
  void lcp_enum_set_default(SolverOptions* options);
  void lcp_pivot_set_default(SolverOptions* options);
  void lcp_pathsearch_set_default(SolverOptions* options);
  void lcp_pivot_lumod_set_default(SolverOptions* options);
  /** @} */
  
  
  #if defined(__cplusplus) && !defined(BUILD_AS_CPP)
}
#endif

#endif

