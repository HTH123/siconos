/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2018 INRIA.
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

#include <stdio.h>                         // for printf, NULL
#include <stdlib.h>                        // for malloc, calloc, free
#include "FrictionContactProblem.h"        // for FrictionContactProblem
#include "Friction_cst.h"                  // for SICONOS_FRICTION_2D_ENUM
#include "LCP_Solvers.h"                   // for lcp_compute_error, lcp_enu...
#include "LinearComplementarityProblem.h"  // for LinearComplementarityProblem
#include "NonSmoothDrivers.h"              // for linearComplementarity_driver
#include "NumericsFwd.h"                   // for SolverOptions, LinearCompl...
#include "SolverOptions.h"                 // for SolverOptions, SICONOS_DPA...
#include "fc2d_Solvers.h"                  // for fc2d_tolcp, fc2d_enum, fc2...
#include "fc2d_compute_error.h"            // for fc2d_compute_error
#include "numerics_verbose.h"              // for verbose
#include "SiconosBlas.h"                         // for cblas_dnrm2


void fc2d_enum(FrictionContactProblem* problem, double *reaction, double *velocity, int *info, SolverOptions* options)
{
  int i;
  // conversion into LCP
  LinearComplementarityProblem* lcp_problem = (LinearComplementarityProblem*)malloc(sizeof(LinearComplementarityProblem));

  fc2d_tolcp(problem, lcp_problem);
  /* frictionContact_display(problem); */
  /* linearComplementarity_display(lcp_problem); */



  double *zlcp = (double*)malloc(lcp_problem->size * sizeof(double));
  double *wlcp = (double*)malloc(lcp_problem->size * sizeof(double));

  for (i = 0; i < lcp_problem->size; i++)
  {
    zlcp[i] = 0.0;
    wlcp[i] = 0.0;
  }


  /*  FILE * fcheck = fopen("lcp_relay.dat","w"); */
  /*  info = linearComplementarity_printInFile(lcp_problem,fcheck); */

  // Call the lcp_solver

  SolverOptions * lcp_options = options->internalSolvers;

  lcp_enum_init(lcp_problem, lcp_options, 1);
  //}
  * info = linearComplementarity_driver(lcp_problem, zlcp , wlcp, lcp_options);
  if (options->filterOn > 0)
    lcp_compute_error(lcp_problem, zlcp, wlcp, lcp_options->dparam[0], &(lcp_options->dparam[1]));
  lcp_enum_reset(lcp_problem, lcp_options, 1);

  /*       printf("\n"); */
  int nc = problem->numberOfContacts;
  double norm_q = cblas_dnrm2(nc*2 , problem->q , 1);
  // Conversion of result
  for (i = 0; i < nc; i++)
  {

    /* printf("Contact number = %i\n",i); */
    reaction[2 * i] = zlcp[3 * i];
    reaction[2 * i + 1] = 1.0 / 2.0 * (zlcp[3 * i + 1] - wlcp[3 * i + 2]);
    /* printf("reaction[ %i]=%12.10e\n", 2*i, reaction[2*i]); */
    /* printf("reaction[ %i]=%12.10e\n", 2*i+1, reaction[2*i+1]); */

    velocity[2 * i] = wlcp[3 * i];
    velocity[2 * i + 1] = wlcp[3 * i + 1] - zlcp[3 * i + 2];
    /* printf("velocity[ %i]=%12.10e\n", 2*i, velocity[2*i]);   */
    /* printf("velocity[ %i]=%12.10e\n", 2*i+1, velocity[2*i+1]); */
  }

  /*        for (i=0; i< lcp_problem->size; i++){  */
  /*     printf("zlcp[ %i]=%12.10e,\t wlcp[ %i]=%12.10e \n", i, zlcp[i],i, wlcp[i]); */
  /*        } */
  /*        printf("\n"); */

  /*        for (i=0; i< problem->size; i++){  */
  /*     printf("z[ %i]=%12.10e,\t w[ %i]=%12.10e\n", i, z[i],i, w[i]); */
  /*        } */


  /*        printf("\n"); */
  options->iparam[SICONOS_IPARAM_ITER_DONE] = lcp_options->iparam[SICONOS_IPARAM_ITER_DONE];
  options->iparam[SICONOS_DPARAM_RESIDU] = lcp_options->iparam[SICONOS_DPARAM_RESIDU];

  if (options->dparam[SICONOS_DPARAM_RESIDU] > options->iparam[SICONOS_DPARAM_TOL])
  {

    if (verbose > 0)
      printf("--------------- FC2D - ENUM - No convergence after %i iterations"
             " residual = %14.7e < %7.3e\n", options->iparam[SICONOS_IPARAM_ITER_DONE],
             options->dparam[SICONOS_DPARAM_RESIDU],
             options->dparam[SICONOS_DPARAM_TOL] );

  }
  else
  {

    if (verbose > 0)
      printf("--------------- FC2D - ENUM - Convergence after %i iterations"
             " residual = %14.7e < %7.3e\n", options->iparam[SICONOS_IPARAM_ITER_DONE],
             options->dparam[SICONOS_DPARAM_RESIDU], options->dparam[SICONOS_DPARAM_TOL]);

    *info = 0;
  }
  double error;
  *info = fc2d_compute_error(problem, reaction , velocity, options->dparam[0], norm_q, &error);
  free(zlcp);
  free(wlcp);
  freeLinearComplementarityProblem(lcp_problem);
}


int fc2d_enum_setDefaultSolverOptions(SolverOptions* options)
{
  if (verbose > 0)
  {
    printf("Set the Default SolverOptions for the Enumerative Solver for fc2d\n");
  }
  /*  strcpy(options->solverName,"Lemke");*/
  options->solverId = SICONOS_FRICTION_2D_ENUM;
  options->numberOfInternalSolvers = 1;
  options->isSet = 1;
  options->filterOn = 1;
  options->iSize = 5;
  options->dSize = 5;
  options->iparam = (int *)calloc(options->iSize, sizeof(int));
  options->dparam = (double *)calloc(options->dSize, sizeof(double));
  options->dWork = NULL;
  solver_options_nullify(options);
  options->dparam[0] = 1e-6;
  options->internalSolvers = (SolverOptions *)malloc(sizeof(SolverOptions));
  linearComplementarity_enum_setDefaultSolverOptions(options->internalSolvers);

  return 0;
}


