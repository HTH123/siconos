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

#include "Newton_methods.h"
#include <assert.h>            // for assert
#include <float.h>             // for DBL_MAX
#include <math.h>              // for fmax, pow, INFINITY, isfinite
#include <stdio.h>             // for fprintf, stderr
#include <stdlib.h>            // for free, calloc, malloc, getenv, atoi
#include "ArmijoSearch.h"      // for linesearch_Armijo2, armijo_extra_params
#include "GoldsteinSearch.h"   // for goldstein_extra_params, search_Goldste...
#include "NumericsMatrix.h"    // for NM_LU_solve, NM_tgemv, NM_duplicate, NM_clear
#include "SiconosBlas.h"       // for cblas_dcopy, cblas_dnrm2, cblas_dscal
#include "SolverOptions.h"     // for SolverOptions, SICONOS_DPARAM_RESIDU
#include "siconos_debug.h"             // for DEBUG_PRINT
#include "hdf5_logger.h"       // for SN_logh5_scalar_double, SN_logh5_vec_d...
#include "line_search.h"       // for search_data, fill_nm_data, free_ls_data
#include "numerics_verbose.h"  // for numerics_printf_verbose, numerics_printf
#include "sn_logger.h"         // for SN_LOG_SCALAR, SN_LOG_VEC, SN_LOG_MAT

typedef double (*linesearch_fptr)(int n, double theta, double preRHS, search_data*);

const char* const SICONOS_NEWTON_LSA_STR  = "Newton method LSA";



void newton_LSA(unsigned n, double *z, double *F, int *info, void* data, SolverOptions* options, functions_LSA* functions)
{
  /* size of the problem */
  assert(n>0);

  /* Checking the consistency of the functions_LSA struct */

  assert(functions->compute_F && "functions_LSA lacks compute_F");
  assert(functions->compute_F_merit && "functions_LSA lacks compute_F_merit");
  assert(functions->compute_error && "functions_LSA lacks compute_error");
  assert(((functions->compute_descent_direction) || \
          (functions->compute_RHS_desc && functions->compute_H_desc) || \
          functions->compute_H) && "functions_LSA lacks a way to compute a descente direction");
  assert(((!functions->compute_RHS_desc || !functions->compute_descent_direction) || \
          (functions->compute_JacTheta_merit || functions->compute_H)) && \
         "functions_LSA lacks a way to compute JacTheta_merit");


  int iter;
  /* if (verbose) */
  /*   solver_options_print(options); */
  int incx, incy;
  double theta, preRHS, tau, threshold;
  double theta_iter = 0.0;
  double norm_F_merit =0.0, norm_JacThetaF_merit=0.0;
  double err;

  double *workV1, *workV2;
  double *JacThetaF_merit, *F_merit;
  int itermax = options->iparam[SICONOS_IPARAM_MAX_ITER];
  double tol = options->dparam[SICONOS_DPARAM_TOL];

  incx = 1;
  incy = 1;

  /*output*/
  options->iparam[SICONOS_IPARAM_ITER_DONE] = 0;
  options->dparam[SICONOS_DPARAM_RESIDU] = 0.0;

  // Maybe there is a better way to initialize
//  for (unsigned int i = 0; i < n; i++) z[i] = 0.0;

  // if we keep the work space across calls
  if(options->iparam[SICONOS_IPARAM_PREALLOC] && (options->dWork == NULL))
  {
    options->dWork = (double *)calloc(4*n, sizeof(double));
    options->iWork = (int *)calloc(n, sizeof(int));
  }

  if(options->dWork)
  {
    F_merit = options->dWork;
    workV1 = F_merit + n;
    workV2 = workV1 + n;
    JacThetaF_merit = workV2 + n;
  }
  else
  {
    F_merit = (double *)calloc(n,  sizeof(double));
    JacThetaF_merit = (double *)calloc(n, sizeof(double));
    workV1 = (double *)calloc(n, sizeof(double));
    workV2 = (double *)calloc(n, sizeof(double));
  }

  newton_stats stats_iteration;
  if(options->callback)
  {
    stats_iteration.id = NEWTON_STATS_ITERATION;
  }

  assert(options->solverData);
  newton_LSA_param* params = (newton_LSA_param*)options->solverParameters;
  NumericsMatrix* H = ((newton_LSA_data*)options->solverData)->H;
  assert(params);
  assert(H);

  // Is this really still in use ???
  char* solver_opt = getenv("SICONOS_SPARSE_SOLVER");
  if(solver_opt)
  {
    NM_setSparseSolver(H, (NSM_linear_solver)atoi(solver_opt));
  }

  search_data ls_data;
  linesearch_fptr linesearch_algo;

  ls_data.compute_F = functions->compute_F;
  ls_data.compute_F_merit = functions->compute_F_merit;
  ls_data.z = z;
  ls_data.zc = workV2;
  ls_data.F = F;
  ls_data.F_merit = F_merit;
  ls_data.desc_dir = workV1;
  /** \todo this value should be settable by the user with a default value*/
  ls_data.alpha_min = options->dparam[SICONOS_DPARAM_LSA_ALPHA_MIN];
  ls_data.alpha0 = 2.0;
  ls_data.data = data;
  ls_data.set = NULL;
  ls_data.sigma = params->sigma;
  ls_data.searchtype = LINESEARCH;
  ls_data.extra_params = NULL;

  if(options->iparam[SICONOS_IPARAM_LSA_SEARCH_CRITERION] == SICONOS_LSA_GOLDSTEIN)
  {
    goldstein_extra_params* pG = (goldstein_extra_params*)malloc(sizeof(goldstein_extra_params));
    ls_data.extra_params = (void*) pG;
    search_Goldstein_params_init(pG);

    /*    if (options->iparam[SICONOS_IPARAM_GOLDSTEIN_ITERMAX])
        {
          pG->iter_max = options->iparam[SICONOS_IPARAM_GOLDSTEIN_ITERMAX];
        }
        if (options->dparam[SICONOS_DPARAM_GOLDSTEIN_C])
        {
          pG->c = options->dparam[SICONOS_DPARAM_GOLDSTEIN_C];
        }
        if (options->dparam[SICONOS_DPARAM_GOLDSTEIN_ALPHAMAX])
        {
          pG->alpha_max = options->dparam[SICONOS_DPARAM_GOLDSTEIN_ALPHAMAX];
        }*/
    linesearch_algo = &linesearch_Goldstein2;
  }
  else if(options->iparam[SICONOS_IPARAM_LSA_SEARCH_CRITERION] == SICONOS_LSA_ARMIJO)
  {
    armijo_extra_params* pG = (armijo_extra_params*)malloc(sizeof(armijo_extra_params));
    ls_data.extra_params = (void*) pG;
    search_Armijo_params_init(pG);
    linesearch_algo = &linesearch_Armijo2;
  }
  else
  {
    fprintf(stderr, "Newton_LSA :: unknown linesearch specified");
    linesearch_algo = &linesearch_Armijo2;
  }

  if(options->iparam[SICONOS_IPARAM_LSA_FORCE_ARCSEARCH])
  {
    assert(functions->get_set_from_problem_data && \
           "newton_LSA :: arc search selected but no get_set_from_problem_data provided!");
    ls_data.set = functions->get_set_from_problem_data(data);
    ls_data.searchtype = ARCSEARCH;
  }

  nm_ref_struct nm_ref_data;
  if(options->iparam[SICONOS_IPARAM_LSA_NONMONOTONE_LS] > 0)
  {
    fill_nm_data(&nm_ref_data, options->iparam);
    ls_data.nm_ref_data = &nm_ref_data;
  }
  else
  {
    ls_data.nm_ref_data = NULL;
  }

  // if error based on the norm of JacThetaF_merit, do something not too stupid
  // here
  JacThetaF_merit[0] = DBL_MAX;

  iter = 0;

  functions->compute_F(data, z, F);
  functions->compute_F_merit(data, z, F, F_merit);

  // Merit Evaluation
  norm_F_merit = cblas_dnrm2((int)n, F_merit, incx);
  theta = 0.5 * norm_F_merit * norm_F_merit;

  functions->compute_error(data, z, F, JacThetaF_merit, tol, &err);

  unsigned log_hdf5 = SN_logh5_loglevel(SN_LOGLEVEL_ALL);

  const char* hdf5_filename = getenv("SICONOS_HDF5_NAME");
  if(!hdf5_filename) hdf5_filename = "test.hdf5";
  SN_logh5* logger_s = NULL;
  if(log_hdf5)
  {
    logger_s = SN_logh5_init(hdf5_filename, itermax);
    SN_logh5_scalar_uinteger(0, "version", logger_s->file);
  }


  numerics_printf_verbose(1,"--- newton_LSA :: start iterations");
  // Newton Iteration
  while((iter < itermax) && (err > tol))
  {
    ++iter;
    int info_dir_search = 0;

    //functions->compute_F(data, z, F);

    if(log_hdf5)
    {
      SN_logh5_new_iter(iter, logger_s);
      SN_LOG_LIGHT(log_hdf5,SN_logh5_vec_double(n, z, "z", logger_s->group));
      SN_LOG_VEC(log_hdf5,SN_logh5_vec_double(n, F, "F", logger_s->group));
    }

    /**************************************************************************
     * START COMPUTATION DESCENT DIRECTION
     */
    if(functions->compute_descent_direction)
    {
      info_dir_search = functions->compute_descent_direction(data, z, F, workV1, options);
    }
    else
    {
      // Construction of H and F_desc
      if(functions->compute_RHS_desc)  // different merit function for the descent calc.(usually min)
      {
        functions->compute_H_desc(data, z, F, workV1, workV2, H);
        functions->compute_RHS_desc(data, z, F, F_merit);
        if(log_hdf5)
        {
          SN_LOG_MAT(log_hdf5, SN_logh5_NM(H, "H_desc", logger_s));
          SN_LOG_VEC(log_hdf5, SN_logh5_vec_double(n, F_merit, "F_merit_desc", logger_s->group));
        }

      } /* No computation of JacThetaFF_merit, this will come later */
      else
      {
        DEBUG_PRINT("Compute JacThetaF_merit. use merit function as descent computation. \n");
        functions->compute_H(data, z, F, workV1, workV2, H);
        //functions->compute_F_merit(data, z, F, F_merit);
        NM_tgemv(1., H, F_merit, 0., JacThetaF_merit);
        if(log_hdf5)
        {
          SN_LOG_MAT(log_hdf5,SN_logh5_NM(H, "H", logger_s));
          SN_LOG_VEC(log_hdf5,SN_logh5_vec_double(n, F_merit, "F_merit", logger_s->group));
        }

      }

      // Find direction by solving H * d = -F_desc
      cblas_dcopy((int)n, F_merit, incx, workV1, incy);
      cblas_dscal((int)n, -1.0, workV1, incx);
      // info_dir_search = NM_gesv(H, workV1, params->keep_H);
      NM_set_LU_factorized(H, false);
      info_dir_search = NM_LU_solve(params->keep_H ? NM_preserve(H) : H, workV1, 1);
    }
    /**************************************************************************
     * END COMPUTATION DESCENT DIRECTION
     */

    if(!info_dir_search && log_hdf5)  SN_LOG_VEC(log_hdf5,SN_logh5_vec_double(n, workV1, "desc_direction", logger_s->group));

    /**************************************************************************
     * START COMPUTATION JacTheta F_merit
     */
    // Computation of JacThetaF_merit
    // JacThetaF_merit = H^T * F_merit
    if(functions->compute_RHS_desc || functions->compute_descent_direction)  // need to compute H and F_merit for the merit
    {
      // /!\ maide! workV1 cannot be used since it contains the descent
      // direction ...

      if(functions->compute_JacTheta_merit)
      {
        functions->compute_JacTheta_merit(data, z, F, F_merit, workV2, JacThetaF_merit, options);
      }
      else
      {
        functions->compute_H(data, z, F, F_merit, workV2, H);
        functions->compute_F_merit(data, z, F, F_merit);
        NM_tgemv(1., H, F_merit, 0., JacThetaF_merit);
        if(log_hdf5)
        {
          SN_LOG_MAT(log_hdf5,SN_logh5_NM(H, "H", logger_s));
          SN_LOG_VEC(log_hdf5,SN_logh5_vec_double(n, F_merit, "F_merit", logger_s->group));
        }
      }
    }

    if(log_hdf5)
    {
      SN_LOG_VEC(log_hdf5,SN_logh5_vec_double(n, JacThetaF_merit, "JacThetaF_merit", logger_s->group));
      SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_integer(info_dir_search, "info_dir_search_solve", logger_s->group));
    }

    // xhub :: we should be able to continue even if DGESV fails!
    if(info_dir_search)
    {
      if(functions->compute_RHS_desc)  // we are safe here
      {
        numerics_printf("functions->compute_RHS_desc : no  descent direction found! searching for merit descent direction");
        cblas_dcopy((int)n, F_merit, incx, workV1, incy);
        cblas_dscal((int)n, -1.0, workV1, incx);
        // info_dir_search = NM_gesv(H, workV1, params->keep_H);
        NM_set_LU_factorized(H, false);
        info_dir_search = NM_LU_solve(params->keep_H ? NM_preserve(H) : H, workV1, 1);

        if(log_hdf5)
        {
          SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_integer(info_dir_search, "info_dir_search_solve_meritdesc", logger_s->group));
          if(!info_dir_search) SN_LOG_VEC(log_hdf5,SN_logh5_vec_double(n, workV1, "desc_merit_direction", logger_s->group));
        }
      }
      else
      {
        numerics_printf("Problem in DGESV, info = %d", info_dir_search);

        options->iparam[SICONOS_IPARAM_ITER_DONE] = iter;
        options->dparam[SICONOS_DPARAM_RESIDU] = theta;
        *info = 2;

        goto newton_LSA_free;
      }
    }

    if(info_dir_search == 0)  /* direction search succeeded */
    {
      numerics_printf_verbose(2,"direction search suceeded");
      // workV1 contains the direction d
      cblas_dcopy((int)n, z, incx, workV2, incy);
      cblas_daxpy((int)n, 1.0, workV1, incx, workV2, incy);     //  z + d --> z

      // compute new F_merit value and also the new merit
      functions->compute_F(data, workV2, F);
      functions->compute_F_merit(data, workV2, F, F_merit);


      theta_iter = cblas_dnrm2((int)n, F_merit, incx);
      theta_iter = 0.5 * theta_iter * theta_iter;
    }
    else /* direction search failed, backup to gradient step*/
    {
      numerics_printf("direction search failed, backup to gradient step");
      cblas_dcopy((int)n, JacThetaF_merit, incx, workV1, incy);
      cblas_dscal((int)n, -1.0, workV1, incx);
      theta_iter = INFINITY;
    }

    tau = 1.0;
    if((theta_iter > params->sigma * theta) || (info_dir_search > 0 && functions->compute_RHS_desc))  // Newton direction not good enough or min part failed
    {
      if(log_hdf5)
      {
        SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_double(theta_iter, "theta_iter", logger_s->group));
        SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_double(params->sigma * theta, "theta_iter_threshold", logger_s->group));
      }

      numerics_printf_verbose(2,"--- newton_LSA :: pure Newton direction not acceptable theta_iter = %g > %g = theta", theta_iter, theta);

      // Computations for the line search
      // preRHS = <JacThetaF_merit, d>
      // TODO: find a better name for this variable
      preRHS = cblas_ddot((int)n, JacThetaF_merit, incx, workV1, incy);

      // TODO we should not compute this if min descent search has failed
      threshold = -params->rho*pow(cblas_dnrm2((int)n, workV1, incx), params->p);
      //threshold = -rho*cblas_dnrm2(n, workV1, incx)*cblas_dnrm2(n, JacThetaF_merit, incx);

      if(log_hdf5)
      {
        SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_double(preRHS, "preRHS_newton", logger_s->group));
        SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_double(threshold, "preRHS_threshold", logger_s->group));
      }

      if(params->check_dir_quality && preRHS > threshold)
      {
        numerics_printf_verbose(2,"newton_LSA :: direction not acceptable %g > %g\n", preRHS, threshold);

        cblas_dcopy((int)n, JacThetaF_merit, incx, workV1, incy);
        cblas_dscal((int)n, -1.0, workV1, incx);
        preRHS = cblas_ddot((int)n, JacThetaF_merit, incx, workV1, incy);
      }

      if(log_hdf5)
      {
        SN_LOG_SCALAR(log_hdf5,SN_logh5_scalar_double(preRHS, "preRHS", logger_s->group));
      }

      // Line search
      tau = (*linesearch_algo)((int)n, theta, preRHS, &ls_data);
    }

    if(isfinite(tau))
      cblas_daxpy((int)n, tau, workV1, incx, z, incy);         //  z + tau*d --> z
    else
      cblas_daxpy((int)n, 1., workV1, incx, z, incy);           // hack (restart)

    // Construction of the RHS for the next iterate
    /* VA : What happens if we use  functions->compute_RHS_desc(data, z, F, F_merit); above */
    functions->compute_F(data, z, F);
    functions->compute_F_merit(data, z, F, F_merit);


    // Merit Evaluation
    norm_F_merit = cblas_dnrm2((int)n, F_merit, incx);
    theta = 0.5 * norm_F_merit * norm_F_merit;

    norm_JacThetaF_merit = cblas_dnrm2((int)n, JacThetaF_merit, 1);
    // Error Evaluation

    if(options->iparam[SICONOS_IPARAM_STOPPING_CRITERION] == SICONOS_STOPPING_CRITERION_RESIDU)
    {
      err = norm_F_merit;
    }
    else if(options->iparam[SICONOS_IPARAM_STOPPING_CRITERION] == SICONOS_STOPPING_CRITERION_STATIONARITY)
    {
      err = norm_JacThetaF_merit ;
    }
    else if(options->iparam[SICONOS_IPARAM_STOPPING_CRITERION] ==
            SICONOS_STOPPING_CRITERION_RESIDU_AND_STATIONARITY)
    {
      err = fmax(norm_F_merit, norm_JacThetaF_merit);
    }
    else if(options->iparam[SICONOS_IPARAM_STOPPING_CRITERION] ==
            SICONOS_STOPPING_CRITERION_USER_ROUTINE)
    {
      DEBUG_PRINT("user routine is used to compute the error\n");
      functions->compute_error(data, z, F, JacThetaF_merit, tol, &err);
    }







    if(log_hdf5)
    {
      SN_logh5_scalar_double(err, "error", logger_s->group);
      SN_logh5_scalar_double(tau, "tau", logger_s->group);
      SN_logh5_scalar_double(theta, "theta", logger_s->group);
      SN_logh5_end_iter(logger_s);
    }


    if(options->callback)
    {
      stats_iteration.merit_value = theta;
      stats_iteration.alpha = tau;
      stats_iteration.status = 0;
      options->callback->collectStatsIteration(options->callback->env, (int)n, z, F, err, &stats_iteration);
    }
    numerics_printf_verbose(1,"--- newton_LSA :: iter = %i,  norm merit function = %e, norm grad. merit function = %e, err = %e > tol = %e",iter, norm_F_merit, norm_JacThetaF_merit, err, tol);
  }

  options->iparam[SICONOS_IPARAM_ITER_DONE] = iter;
  options->dparam[SICONOS_DPARAM_RESIDU] = err;


  if(err > tol)
  {
    numerics_printf_verbose(1,"--- newton_LSA :: No convergence of the Newton algo after %d iterations and residue = %g ", iter, theta);
    *info = 1;
  }
  else
  {
    numerics_printf_verbose(1,"--- newton_LSA :: Convergence of the Newton algo after %d iterations and residue = %g ", iter, theta);
    *info = 0;
  }


newton_LSA_free:

  if(!options->dWork)
  {
    free(JacThetaF_merit);
    free(F_merit);
    free(workV1);
    free(workV2);
  }
  free_ls_data(&ls_data);

  if(log_hdf5)
  {
    SN_logh5_scalar_uinteger(iter, "nb_iter", logger_s->file);
    SN_logh5_scalar_double(err, "residual", logger_s->file);
    if(logger_s->group) SN_logh5_end_iter(logger_s);
    SN_logh5_end(logger_s);
  }

  newton_LSA_free_solverOptions(options);
}

void newton_lsa_set_default(SolverOptions* options)
{

  options->iparam[SICONOS_IPARAM_MAX_ITER] = 1000;
  options->dparam[SICONOS_DPARAM_TOL] = 1e-10;

  options->iparam[SICONOS_IPARAM_LSA_NONMONOTONE_LS] = 0;
  options->iparam[SICONOS_IPARAM_LSA_NONMONOTONE_LS_M] = 0;
  options->dparam[SICONOS_DPARAM_LSA_ALPHA_MIN] = 1e-16;

  options->dparam[SICONOS_IPARAM_STOPPING_CRITERION] = SICONOS_STOPPING_CRITERION_RESIDU;

}

void set_lsa_params_data(SolverOptions* options, NumericsMatrix* mat)
{
  assert(mat);
  if(!options->solverParameters)
  {
    options->solverParameters = malloc(sizeof(newton_LSA_param));
    newton_LSA_param* params = (newton_LSA_param*) options->solverParameters;

    // gamma in (0, 1) or (0, 0.5) ??
    // inconsistency between Facchinei--Pang and "A Theoretical and Numerical
    // Comparison of Some Semismooth Algorithm for Complementarity Problems"
    // The following values are taken from the latter.
    params->p = 2.1;
    params->sigma = .9;
    params->rho = 1e-8;

    params->keep_H = false;
    params->check_dir_quality = true;
  }

  if(!options->solverData)
  {
    options->solverData = malloc(sizeof(newton_LSA_data));
    newton_LSA_data* sd = (newton_LSA_data*) options->solverData;
    sd->H = NM_duplicate(mat);
  }
}

void newton_LSA_free_solverOptions(SolverOptions* options)
{
  if(options->solverParameters)
  {
    free(options->solverParameters);
    options->solverParameters = NULL;
  }

  if(options->solverData)
  {
    newton_LSA_data* sd = (newton_LSA_data*) options->solverData;
    assert(sd->H);
    NM_clear(sd->H);
    free(sd->H);
    free(sd);
    options->solverData = NULL;
  }

}
