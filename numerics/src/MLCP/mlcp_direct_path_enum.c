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

/*
|A C| |u| |a| |0|
|   |*| |+| |=| |
|D B| |v| |b| |w|
0<z*v>0
dim(u)=mm
dim(v)=nn

**************************************************************************/
#include "mlcp_direct_path_enum.h"
#include <stdio.h>                              // for printf
#include "MLCP_Solvers.h"                       // for mixedLinearComplement...
#include "MixedLinearComplementarityProblem.h"  // for MixedLinearComplement...
#include "SolverOptions.h"                      // for SolverOptions
#include "mlcp_direct.h"                        // for mlcp_direct_getNbDWork
#include "mlcp_path_enum.h"                     // for mlcp_path_enum, mlcp_...

static int sN;
static int sM;

static int * siWorkPathEnum = 0;
static int * siWorkDirect = 0;
static double * sdWorkPathEnum = 0;
static double * sdWorkDirect = 0;

int mixedLinearComplementarity_directPathEnum_setDefaultSolverOptions(MixedLinearComplementarityProblem* problem, SolverOptions* pSolver)
{
  mixedLinearComplementarity_default_setDefaultSolverOptions(problem, pSolver);
  return 0;
}


int mlcp_direct_path_enum_getNbIWork(MixedLinearComplementarityProblem* problem, SolverOptions* options)
{
  return mlcp_direct_getNbIWork(problem, options) + mlcp_path_enum_getNbIWork(problem, options);
}
int mlcp_direct_path_enum_getNbDWork(MixedLinearComplementarityProblem* problem, SolverOptions* options)
{
  return mlcp_direct_getNbDWork(problem, options) + mlcp_path_enum_getNbDWork(problem, options);
}



/*
 *options->iparam[5] : n0 number of possible configuration.
 * dparam[5] : (in) a positive value, tolerane about the sign.
 *options->iWork : double work memory of  mlcp_direct_enum_getNbIWork() integers  (2(nn+mm))+((n + m)*(n0+1) + nO*m)
 *options->dWork : double work memory of mlcp_direct_enum_getNbDWork() doubles  ((nn+mm)*(nn+mm) + 3*(nn+mm))+(n + m + n0*(n+m)*(n+m))
 *
 *
 */

void mlcp_direct_path_enum_init(MixedLinearComplementarityProblem* problem, SolverOptions* options)
{
  sN = problem->n;
  sM = problem->m;
  int iOffset = mlcp_direct_getNbIWork(problem, options);
  int dOffset = mlcp_direct_getNbDWork(problem, options);
  siWorkPathEnum = options->iWork + iOffset;
  siWorkDirect = options->iWork;
  sdWorkPathEnum = options->dWork + dOffset;
  sdWorkDirect = options->dWork;
  mlcp_direct_init(problem, options);
  options->dWork = sdWorkPathEnum;
  options->iWork = siWorkPathEnum;
  mlcp_path_enum_init(problem, options);

}
void mlcp_direct_path_enum_reset()
{
  mlcp_direct_reset();
  mlcp_path_enum_reset();
  siWorkPathEnum = 0;
  siWorkDirect = 0;
  sdWorkPathEnum = 0;
  sdWorkDirect = 0;
}

/*
 * The are no memory allocation in mlcp_direct, all necessary memory must be allocated by the user.
 *
 *options:
 * iparam[0] : (in) verbose.
 * dparam[0] : (in) a positive value, tolerane about the sign used by the enum algo.
 * iparam[5] : (in)  n0 number of possible configuration.
 * dparam[5] : (in) a positive value, tolerane about the sign.
 * dWork : working float zone size : n + m + n0*(n+m)*(n+m)  . MUST BE ALLOCATED BY THE USER.
 * iWork : working int zone size : (n + m)*(n0+1) + nO*m. MUST BE ALLOCATED BY THE USER.
 * double *z : size n+m
 * double *w : size n+m
 * info : output. info == 0 if success
 */
void mlcp_direct_path_enum(MixedLinearComplementarityProblem* problem, double *z, double *w, int *info, SolverOptions* options)
{
  if (!siWorkPathEnum)
  {
    *info = 1;
    printf("MLCP_DIRECT_PATH_ENUM error, call a non initialised method!!!!!!!!!!!!!!!!!!!!!\n");
    return;
  }
  /*First, try direct solver*/
  options->dWork = sdWorkDirect;
  options->iWork = siWorkDirect;
  mlcp_direct(problem, z, w, info, options);
  if (*info)
  {
    options->dWork = sdWorkPathEnum;
    options->iWork = siWorkPathEnum;
    /*solver direct failed, so run the enum solver.*/
    mlcp_path_enum(problem, z, w, info, options);
    if (!(*info))
    {
      mlcp_direct_addConfigFromWSolution(problem, w + sN);
    }
  }
}
