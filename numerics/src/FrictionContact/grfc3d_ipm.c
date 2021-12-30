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

#include "CSparseMatrix_internal.h"
#include "grfc3d_Solvers.h"             // for GRFCProb, SolverOpt, Friction_cst, grfc3d_...
#include "grfc3d_compute_error.h"       // for grfc3d_compute_error
#include "SiconosLapack.h"

#include "SparseBlockMatrix.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <float.h>

#include "numerics_verbose.h"
#include "NumericsVector.h"
#include "float.h"
#include "JordanAlgebra.h"              // for JA functions

#include "NumericsSparseMatrix.h"
#include "NumericsMatrix.h"

#include <time.h>                       // for clock()
#include "gfc3d_ipm.h"                  // for primalResidual, dualResidual, ...
#include "cs.h"

/* #define DEBUG_MESSAGES */
/* #define DEBUG_STDOUT */
#include "siconos_debug.h"


typedef struct
{
  double * globalVelocity;  // v
  double * velocity;        // u
  double * reaction;        // r
}
  IPM_point;

typedef struct
{
  double * velocity_1; // velocity_1 = (t, u_bar)
  double * velocity_2; // velocity_2 = (t_prime, u_tilde)
  double * reaction_1; // reaction_1 = (r0, r_bar)
  double * reaction_2; // reaction_2 = (r0, r_tilde)
  double * t;
  double * t_prime;
}
  IPM_grfc3d_point;

typedef struct
{
  NumericsMatrix* mat;
  NumericsMatrix* inv_mat;
}
  IPM_change_of_variable;

typedef struct
{
  double alpha_primal; // primal step length
  double alpha_dual;   // dual step length
  double sigma;        // centering parameter
  double barr_param;   // barrier parameter
}
  IPM_internal_params;

typedef struct
{
  /* initial interior points */
  IPM_point* starting_point;        // initial point
  IPM_point* original_point;        // original point which is not changed by the matrix P_mu
  IPM_grfc3d_point* grfc3d_point;

  /* change of variable matrix */
  IPM_change_of_variable* P_mu;

  /* initial internal solver parameters */
  IPM_internal_params* internal_params;

  /* problem parameters related to their size
   * tmp_vault_m[0] = dualConstraint
   * tmp_vault_m[1] = gv_plus_dgv
   * tmp_vault_nd[0] = w
   * tmp_vault_nd[1] = primalConstraint
   * tmp_vault_nd[2] = vr_jprod
   * tmp_vault_nd[3] = v_plus_dv = u + alpha_p * du
   * tmp_vault_nd[4] = r_plus_dr = r + alpha_d * dr
   * tmp_vault_nd[0] = velocity_1t = F_1 * u_1
   * tmp_vault_nd[0] = velocity_2t = F_2 * u_2
   * tmp_vault_nd[0] = d_velocity_1t = F_1 * du_1
   * tmp_vault_nd[0] = d_velocity_2t = F_2 * du_2
   * tmp_vault_nd[0] = d_reaction_1t = Finv_1 * dr_1
   * tmp_vault_nd[0] = d_reaction_2t = Finv_2 * dr_2
   * tmp_vault_n_dminus2[0] = complemConstraint_1
   * tmp_vault_n_dminus2[1] = complemConstraint_2
   * tmp_vault_n_dminus2[2] = dvdr_jprod_1
   * tmp_vault_n_dminus2[3] = dvdr_jprod_2
   * tmp_vault_n_dminus2[4] = p_bar
   * tmp_vault_n_dminus2[5] = p_tilde


   * tmp_vault_n[0] = d_t
   * tmp_vault_n[1] = d_t_prime
   */
  double **tmp_vault_m;
  double **tmp_vault_nd;
  double **tmp_vault_n_dminus2;
  double **tmp_vault_n;
}
  Grfc3d_IPM_data;







/* ------------------------- Helper functions ------------------------------ */
/** Return a speacial sub-vector such that
 * the 1st element is always taken and
 * so do from i-th to j-th elements,
 * starting index is 1
 */
static void extract_vector(const double * const vec, const unsigned int vecSize, const int varsCount, const unsigned int i, const unsigned int j, double * out)
{
  assert(vec);
  assert(i >= 1);
  assert(i <= j);
  assert(out);

  unsigned int vec_dim = (int)(vecSize / varsCount);
  assert(j <= vec_dim);

  unsigned int out_dim = vec_dim - 2;
  assert(out_dim > 0);

  size_t posX = 0;
  size_t posY = 0;

  for(size_t k = 0; k < varsCount; k++)
  {
    out[posX++] = vec[posY];
    for(size_t l = i-1; l < j; l++)
    {
      out[posX++] = vec[posY + l];
    }
    posY += vec_dim;
  }
}


/** Compute a block matrix J of form
 *      |  1     0     0                 1     0     0                    ...  |
 *      |                                                                      |
 *      |  0     1     0                 0     0     0                    ...  |
 *      |                                                                      |
 *      |  0     0     1                 0     0     0                    ...  |
 *      |                                                                      |
 *      |  0     0     0                 0     1     0                    ...  |
 *      |                                                                      |
 *      |  0     0     0                 0     0     1                    ...  |
 *  J = |                                                                      |
 *      |                 .     .     .                  .     .     .    ...  |
 *      |                                                                      |
 *      |                 .     .     .                  .     .     .    ...  |
 *      |                                                                      |
 *      |                 .     .     .                  .     .     .    ...  |
 *      |                                                                      |
 *      |                 .     .     .                  .     .     .    ...  |
 *      |                                                                      |
 *      |                 .     .     .                  .     .     .    ...  |
 *      |                                                                      |
 *      | ...   ...   ...   ...   ...   ...   ...   ...   ...   ...   ... ...  |
 */
static NumericsMatrix* compute_J_matrix(const unsigned int varsCount)
{
  assert(varsCount > 0);

  NumericsMatrix * J = NM_create(NM_SPARSE, 5*varsCount, 3*varsCount*2);
  NumericsMatrix * J_1 = NM_create(NM_SPARSE, 5, 3);
  NumericsMatrix * J_2 = NM_create(NM_SPARSE, 5, 3);

  long J_nzmax = 3*2*varsCount;
  long J_1_nzmax = 3;
  long J_2_nzmax = 3;

  NM_triplet_alloc(J, J_nzmax);
  NM_triplet_alloc(J_1, J_1_nzmax);
  NM_triplet_alloc(J_2, J_1_nzmax);

  J->matrix2->origin = NSM_TRIPLET;
  J_1->matrix2->origin = NSM_TRIPLET;
  J_2->matrix2->origin = NSM_TRIPLET;

  NM_insert(J_1, NM_eye(3), 0, 0);
  NM_insert(J_2, NM_eye(1), 0, 0);
  NM_insert(J_2, NM_eye(2), 3, 1);

  for (unsigned i = 0; i < varsCount; i++)
  {
    NM_insert(J, J_1, i*5, i*3);
    NM_insert(J, J_2, i*5, varsCount*3 + i*3);
  }

  return J;
}





/* print iteres under a Matlab format in a file */
/* iteration = index of the iteration
   v = global velocity
   u = velocity
   r = reaction
   d = dimenion
   n = number of contact points
   m = number of degrees of freedom
*/
static void printInteresProbMatlabFile(int iteration, double * v, double * u, double * r, int d, int n, int m, double time, FILE * file)
{
  // fprintf(file,"v(%3i,:) = [",iteration+1);
  // for(int i = 0; i < m; i++)
  // {
  //   fprintf(file, "%20.16e, ", v[i]);
  // }
  // fprintf(file,"];\n");

  // fprintf(file,"u(%3i,:) = [",iteration+1);
  // for(int i = 0; i < n*d; i++)
  // {
  //   fprintf(file, "%20.16e, ", u[i]);
  // }
  // fprintf(file,"];\n");

  // fprintf(file,"r(%3i,:) = [",iteration+1);
  // for(int i = 0; i < n*d; i++)
  // {
  //   fprintf(file, "%20.16e, ", r[i]);
  // }
  // fprintf(file,"];\n");
  fprintf(file, "%d %lf", iteration, time);

  return;
}




















/* --------------------------- Interior-point method implementation ------------------------------ */
/* General rolling friction contact problem */
/* Convex case: */
/* problem: min .5 v'*M*v + f'*v, s.t. H*v + w \in F (rolling friction cone)
   d = dimenion
   n = number of contact points
   m = number of degrees of freedom
   M = m x m matrix
   f = m-vector
   H = n*d x m matrix
   w = n*d-vector */
void grfc3d_IPM(GlobalRollingFrictionContactProblem* restrict problem, double* restrict reaction,
               double* restrict velocity, double* restrict globalVelocity,
               int* restrict info, SolverOptions* restrict options)
{
  clock_t t1 = clock();
  printf("\n\n#################### grfc3d_ipm.c 001 OK ####################\n\n");

 // verbose = 3;

  // the size of the problem detection
  unsigned int m = problem->M->size0;
  unsigned int nd = problem->H->size1;
  unsigned int d = problem->dimension;
  unsigned int n = problem->numberOfContacts;
  unsigned int m_plus_nd = m+nd;
  unsigned int d_minus_2 = d-2;
  unsigned int n_dminus2 = n*d_minus_2;
  unsigned int n_dplus1 = n*(d+1);




  size_t posX = 0; // Used as the index in the source array
  size_t posY = 0; // Used as the index in the destination array

  NumericsMatrix* M = NULL;
  NumericsMatrix* H_origin = NULL;

  // globalRollingFrictionContact_display(problem);

  /* symmetrization of the matrix M */
  if(!(NM_is_symmetric(problem->M)))
  {
    printf("#################### SYMMETRIZATION ####################\n");
    NumericsMatrix *MT = NM_transpose(problem->M);
    problem->M = NM_add(1/2., problem->M, 1/2., MT );
    //problem->M = Msym;
    NM_clear(MT);
  }

  //for(int i = 0; i < n ; i++) printf("mu[%d] = %g\n", i, problem->mu[i]);

  /* if SICONOS_FRICTION_3D_IPM_FORCED_SPARSE_STORAGE = SICONOS_FRICTION_3D_IPM_FORCED_SPARSE_STORAGE,
     we force the copy into a NM_SPARSE storageType */
  DEBUG_PRINTF("problem->M->storageType : %i\n",problem->M->storageType);
  if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_SPARSE_STORAGE] == SICONOS_FRICTION_3D_IPM_FORCED_SPARSE_STORAGE
     && problem->M->storageType == NM_SPARSE_BLOCK)
  {
    DEBUG_PRINT("Force a copy to sparse storage type\n");
    printf("\n\n\n######################### FORCE SPARSE STORAGE #########################\n\n\n");
    M = NM_create(NM_SPARSE,  problem->M->size0,  problem->M->size1);
    NM_copy_to_sparse(problem->M, M, DBL_EPSILON);
  }
  else
  {
    M = problem->M;
  }

  /* NM_display_storageType(problem->H); */
  /* NM_display(problem->H); */
  /* NV_display(problem->b, nd); */
  /* for(int  i = 0; i<nd*m; i++) */
  /*   problem->H->matrix0[i] *= 0.1; */


  DEBUG_PRINTF("problem->M->storageType : %i\n",problem->H->storageType);
  if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_SPARSE_STORAGE] == SICONOS_FRICTION_3D_IPM_FORCED_SPARSE_STORAGE
     && problem->H->storageType == NM_SPARSE_BLOCK)
  {
    DEBUG_PRINT("Force a copy to sparse storage type\n");
    H_origin = NM_create(NM_SPARSE,  problem->H->size1,  problem->H->size0);
    NM_copy_to_sparse(NM_transpose(problem->H), H_origin, DBL_EPSILON);
  }
  else
  {
    H_origin = NM_transpose(problem->H);  // H <== H' because some different storages in fclib and paper
  }

  // initialize solver if it is not set
  int internal_allocation=0;
  if(!options->dWork || (options->dWorkSize != (size_t)(m + nd + n_dplus1)))
  {
    grfc3d_IPM_init(problem, options);
    internal_allocation = 1;
  }

  Grfc3d_IPM_data * data = (Grfc3d_IPM_data *)options->solverData;
  NumericsMatrix *P_mu = data->P_mu->mat;
  NumericsMatrix *P_mu_inv = data->P_mu->inv_mat;

  double *w_origin = problem->b;
  double *w = data->tmp_vault_nd[0];

  // compute -f
  double *f = problem->q;
  // cblas_dscal(m, -1.0, f, 1); // f <== -f because some different storages in fclib and paper
  // double *minus_f = (double*)calloc(m, sizeof(double));
  // cblas_dcopy(m, f, 1, minus_f, 1);
  // cblas_dscal(m, -1.0, minus_f, 1);

  double *iden = JA_iden(n_dminus2, n);

  // change of variable
  // H_origin --> H
  NumericsMatrix *H = NM_multiply(P_mu, H_origin);

  // w_origin --> w
  NM_gemv(1.0, P_mu, w_origin, 0.0, w);
  //cs_print(NM_triplet(H),0);
  //printf("#################### NORM(w) = %g    NORM(f) = %g\n", NV_norm_2(w, nd), NV_norm_2(f,m));

  // compute -H
  NumericsMatrix *minus_H = NM_create(H->storageType, H->size0, H->size1);
  NM_copy(H, minus_H);
  NM_gemm(-1.0, H, NM_eye(H->size1), 0.0, minus_H);

  double alpha_primal_1 = 0;
  double alpha_primal_2 = 0;
  double alpha_dual_1 = 0;
  double alpha_dual_2 = 0;
  double alpha_primal = data->internal_params->alpha_primal;
  double alpha_dual = data->internal_params->alpha_dual;
  double barr_param = data->internal_params->barr_param;
  double sigma = data->internal_params->sigma;

  cblas_dcopy(nd, data->starting_point->reaction, 1, reaction, 1);
  cblas_dcopy(nd, data->starting_point->velocity, 1, velocity, 1);
  cblas_dcopy(m, data->starting_point->globalVelocity, 1, globalVelocity, 1);


  /* 1. t, t_prime */
  double * t = (double*)calloc(n, sizeof(double));
  double * t_prime = (double*)calloc(n, sizeof(double));
  for(unsigned int i = 0; i < n; ++ i)
  {
    t[i] = 2.0;
    t_prime[i] = 1.0;
  }

  double * velocity_1 = (double*)calloc(n_dminus2, sizeof(double));   // = (t, u_bar)
  double * velocity_2 = (double*)calloc(n_dminus2, sizeof(double));   // = (t', u_tilde)
  double * reaction_1 = (double*)calloc(n_dminus2, sizeof(double));   // = (r0, r_bar)
  double * reaction_2 = (double*)calloc(n_dminus2, sizeof(double));   // = (r0, r_tilde)







  double tol = options->dparam[SICONOS_DPARAM_TOL];
  unsigned int max_iter = options->iparam[SICONOS_IPARAM_MAX_ITER];
  double sgmp1 = options->dparam[SICONOS_FRICTION_3D_IPM_SIGMA_PARAMETER_1];
  double sgmp2 = options->dparam[SICONOS_FRICTION_3D_IPM_SIGMA_PARAMETER_2];
  double sgmp3 = options->dparam[SICONOS_FRICTION_3D_IPM_SIGMA_PARAMETER_3];
  double gmmp0 = 0.999;
  double gmmp1 = options->dparam[SICONOS_FRICTION_3D_IPM_GAMMA_PARAMETER_1];
  double gmmp2 = options->dparam[SICONOS_FRICTION_3D_IPM_GAMMA_PARAMETER_2];

  int hasNotConverged = 1;
  unsigned int iteration = 0;
  double pinfeas = 1e300;
  double dinfeas = 1e300;
  double complem_1 = 1e300;
  double complem_2 = 1e300;
  double gapVal = 1e300;
  double relgap = 1e300;
  double u1dotr1 = 1e300;     // u1 = velecity_1, r1 = reaction_1
  double u2dotr2 = 1e300;     // u2 = velecity_2, r2 = reaction_2
  double error_array[6];

  double gmm = gmmp0;
  double barr_param_a, e;
  double norm_f = cblas_dnrm2(m, f, 1);
  double norm_w = cblas_dnrm2(nd, w, 1);

  double *primalConstraint = data->tmp_vault_nd[1];
  double *dualConstraint = data->tmp_vault_m[0];
  double *complemConstraint_1 = data->tmp_vault_n_dminus2[0];
  double *complemConstraint_2 = data->tmp_vault_n_dminus2[1];

  double *d_globalVelocity = (double*)calloc(m,sizeof(double));
  double *d_velocity = (double*)calloc(nd,sizeof(double));
  double *d_velocity_1 = (double*)calloc(n_dminus2,sizeof(double));
  double *d_velocity_2 = (double*)calloc(n_dminus2,sizeof(double));
  double *d_reaction = (double*)calloc(nd,sizeof(double));
  double *d_reaction_1 = (double*)calloc(n_dminus2,sizeof(double));
  double *d_reaction_2 = (double*)calloc(n_dminus2,sizeof(double));
  double *d_t = (double*)calloc(n, sizeof(double));
  double *d_t_prime = (double*)calloc(n, sizeof(double));

  double *rhs = options->dWork;
  double *rhs_tmp = NULL;
  double *gv_plus_dgv = data->tmp_vault_m[1];
  //double *vr_jprod = data->tmp_vault_nd[2];
  double *v_plus_dv = data->tmp_vault_nd[3];
  double *r_plus_dr = data->tmp_vault_nd[4];
  double *dvdr_jprod_1 = data->tmp_vault_n_dminus2[2];
  double *dvdr_jprod_2 = data->tmp_vault_n_dminus2[3];
  double *vr_jprod_1 = data->tmp_vault_n_dminus2[22];
  double *vr_jprod_2 = data->tmp_vault_n_dminus2[23];

  NumericsMatrix *minus_M = NM_create(M->storageType, M->size0, M->size1);    // store the matrix -M to build the matrix of the Newton linear system

  NumericsMatrix *Jac; /* Jacobian matrix */
  long Jac_nzmax;
  size_t M_nzmax = NM_nnz(M);
  size_t H_nzmax = NM_nnz(H);

  NumericsMatrix *J = compute_J_matrix(n); /* use for Jac in the NT scaling case */

  double full_error = 1e300;
  char fws = ' '; /* finish without scaling */

  // if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] > 0)
  // {
  //   J = compute_J_matrix(n);
  // }



  /* -------------------------- Display problem info -------------------------- */
  if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_GET_PROBLEM_INFO] == SICONOS_FRICTION_3D_IPM_GET_PROBLEM_INFO_YES)
  {
    numerics_printf_verbose(1,"---- GRFC3D - IPM - Problem information");
    numerics_printf_verbose(1,"---- GRFC3D - IPM - 1-norm of M = %g norm of f = %g ", NM_norm_1(M), norm_f);
    numerics_printf_verbose(1,"---- GRFC3D - IPM - inf-norm of M = %g ", NM_norm_inf(M));

    numerics_printf_verbose(1,"---- GRFC3D - IPM - 1-norm of H = %g norm of w = %g ", NM_norm_1(problem->H), norm_w);
    numerics_printf_verbose(1,"---- GRFC3D - IPM - inf-norm of H = %g ", NM_norm_inf(problem->H));
    numerics_printf_verbose(1,"---- GRFC3D - IPM - M is symmetric = %i ", NM_is_symmetric(M));

    numerics_printf_verbose(1,"---- GRFC3D - IPM - M size = (%i, %i) ", M->size0, M->size1);
    numerics_printf_verbose(1,"---- GRFC3D - IPM - H size = (%i, %i) ", problem->H->size0, problem->H->size1);
  }


  /* -------------------------- Display IPM iterations -------------------------- */
  numerics_printf_verbose(-1, "problem dimensions d, n, m: %1i, %6i, %6i\n",d, n, m);
  numerics_printf_verbose(-1, "| it  |  rel gap  | pinfeas  | dinfeas  | <u1, r1> | <u2, r2> | complem1 | complem2 | full err | barparam | alpha_p  | alpha_d  |  sigma   | |dv|/|v| | |du|/|u| | |dr|/|r| |");
  numerics_printf_verbose(-1, "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------");

  double * p_bar = data->tmp_vault_n_dminus2[4];
  double * p_tilde = data->tmp_vault_n_dminus2[5];
  double * p2_bar = data->tmp_vault_n_dminus2[20];
  double * p2_tilde = data->tmp_vault_n_dminus2[21];
  NumericsMatrix* Qp_bar = NULL;
  NumericsMatrix* Qp_tilde = NULL;
  NumericsMatrix* Qpinv_bar = NULL;
  NumericsMatrix* Qpinv_tilde = NULL;
  NumericsMatrix* Qp2_bar = NULL;
  NumericsMatrix* Qp2_tilde = NULL;
  NumericsMatrix* F_1 = NULL;
  NumericsMatrix* Finv_1 = NULL;
  NumericsMatrix* Fsqr_1 = NULL;
  NumericsMatrix* F_2 = NULL;
  NumericsMatrix* Finv_2 = NULL;
  NumericsMatrix* Fsqr_2 = NULL;
  NumericsMatrix* tmpmat = NULL;
  NumericsMatrix* Qp_F = NULL;
  NumericsMatrix* F2 = NULL;
  double * velocity_1t = data->tmp_vault_n_dminus2[6];
  double * velocity_2t = data->tmp_vault_n_dminus2[7];
  double * d_velocity_1t = data->tmp_vault_n_dminus2[8];
  double * d_velocity_2t = data->tmp_vault_n_dminus2[9];
  double * d_reaction_1t = data->tmp_vault_n_dminus2[10];
  double * d_reaction_2t = data->tmp_vault_n_dminus2[11];
  double * velocity_1t_inv = data->tmp_vault_n_dminus2[12];
  double * velocity_2t_inv = data->tmp_vault_n_dminus2[13];
  double * Qp_velocity_1t_inv = data->tmp_vault_n_dminus2[14];
  double * Qp_velocity_2t_inv = data->tmp_vault_n_dminus2[15];
  double * F_velocity_1t_inv = data->tmp_vault_n_dminus2[16];
  double * F_velocity_2t_inv = data->tmp_vault_n_dminus2[17];
  double * tmp1 = data->tmp_vault_n_dminus2[18];
  double * tmp2 = data->tmp_vault_n_dminus2[19];

  FILE * matlab_file;
  FILE * matrixH;

  /* write matrix H in file */
  /* matrixH = fopen("matrixH.m", "w"); */
  /* CSparseMatrix_print_in_Matlab_file(NM_triplet(H), 0, matrixH); */
  /* /\* /\\* /\\\* NM_write_in_file(H, matrixH); *\\\/ *\\/ *\/ */
  /* fclose(matrixH); */

  //  FILE * dfile;

  //  dfile = fopen("dfile.m", "w");





  // /* writing data in a Matlab file */
  // if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_ITERATES_MATLAB_FILE])
  // {
  //   char matlab_file_name[256];
  //   sprintf(matlab_file_name,"nc-%d-.m", problem->numberOfContacts)
  //   matlab_file = fopen(matlab_file_name, "w");
  //   printInteresProbMatlabFile(iteration, globalVelocity, velocity, reaction, d, n, m, matlab_file);
  //   fclose(iterates);
  // }




  ComputeErrorGlobalRollingPtr computeError = NULL;
  computeError = (ComputeErrorGlobalRollingPtr)&grfc3d_compute_error;




  /* -------------------------- Check the full criterion -------------------------- */
  while(iteration < max_iter)
  {


    // /* writing data in a Matlab file */
    // if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_ITERATES_MATLAB_FILE])
    // {
    //   char matlab_file_name[256];
    //   sprintf(matlab_file_name,"n_nc-%d-.m", problem->numberOfContacts);
    //   matlab_file = fopen(matlab_file_name, "w");
    //   printInteresProbMatlabFile(iteration, globalVelocity, velocity, reaction, d, n, m, matlab_file);
    //   fclose(matlab_file);
    // }





    /* -------------------------- Extract vectors -------------------------- */
    /* 2. velocity_1 = (t, u_bar), velocity_2 = (t_prime, u_tilde) */
    extract_vector(velocity, nd, n, 2, 3, velocity_1);
    extract_vector(velocity, nd, n, 4, 5, velocity_2);
    for(unsigned int i = 0; i < n; i++)
    {
      velocity_1[i*d_minus_2] = t[i];
      velocity_2[i*d_minus_2] = t_prime[i];
    }

    /* 3. reaction_1 = (r0, r_bar), reaction_2 = (r0, r_tilde) */
    extract_vector(reaction, nd, n, 2, 3, reaction_1);
    extract_vector(reaction, nd, n, 4, 5, reaction_2);





















    /* -------------------------- ?????? -------------------------- */
    if ((options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_FINISH_WITHOUT_SCALING] == 1) && (full_error <= 1e-6) && (fws==' '))
    {
      // To solve the problem very accurately, the algorithm switches to a direct solution of the linear system without scaling//
      options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] = 0;
      fws = '*';
    }



    /* Primal residual = velocity - H * globalVelocity - w */
    primalResidual(velocity, H, globalVelocity, w, primalConstraint, &pinfeas);

    /* Dual residual = M*globalVelocity - H'*reaction + f */
    dualResidual(M, globalVelocity, H, reaction, f, dualConstraint, &dinfeas);








    // /* Dual gap = (primal value - dual value)/ (1 + abs(primal value) + abs(dual value)) */
    // // Note: primal objectif func = 1/2 * v' * M *v + f' * v
    // dualgap = dualGap(M, f, w, globalVelocity, reaction, nd, m);


    /* Gap value = u'.v */
    gapVal = cblas_ddot(nd, reaction, 1, velocity, 1);

    // Note: primal objectif func = 1/2 * v' * M *v + f' * v
    relgap = relGap(M, f, w, globalVelocity, reaction, nd, m, gapVal);

    // barr_param = (gapVal / nd)*sigma;
    // barr_param = gapVal / nd;
    barr_param = gapVal / n;
    //barr_param = complemResidualNorm(velocity, reaction, nd, n);
    //barr_param = (fws=='*' ? complemResidualNorm(velocity, reaction, nd, n)/n : complemResidualNorm_p(velocity, reaction, nd, n)/n) ;
    //barr_param = fabs(barr_param);


    complem_1 = complemResidualNorm(velocity_1, reaction_1, n_dminus2, n); // (t, u_bar) o (r0, r_bar)
    complem_2 = complemResidualNorm(velocity_2, reaction_2, n_dminus2, n); // (t', u_tilde) o (r0, r_tilde)


    /* ----- return to original variables ------ */
    NM_gemv(1.0, P_mu_inv, velocity, 0.0, data->original_point->velocity);
    NM_gemv(1.0, P_mu, reaction, 0.0, data->original_point->reaction);

    if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_UPDATE_S] == 1)         // non-smooth case
    {
      (*computeError)(problem,
                    data->original_point->reaction, data->original_point->velocity, globalVelocity,
                    tol, &full_error, 1);
    }
    else if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_UPDATE_S] == 0)    // convex case
    {
      (*computeError)(problem,
                    data->original_point->reaction, data->original_point->velocity, globalVelocity,
                    tol, &full_error, 0);
    }



    // error = fmax(barr_param, fmax(complem_1, fmax(complem_2, fmax(pinfeas, dinfeas))));



    u1dotr1 = cblas_ddot(n_dminus2, velocity_1, 1, reaction_1, 1);
    u2dotr2 = cblas_ddot(n_dminus2, velocity_2, 1, reaction_2, 1);



    error_array[0] = pinfeas;
    error_array[1] = dinfeas;
    error_array[2] = u1dotr1;
    error_array[3] = u2dotr2;
    error_array[4] = complem_1;
    error_array[5] = complem_2;






















    // check exit condition
    //if (error <= tol) //((NV_max(error, 4) <= tol) || (err <= tol))
    if (NV_max(error_array, 4) <= tol)
    {
      numerics_printf_verbose(-1, "| %3i%c| %9.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e |",
                              iteration, fws, relgap, pinfeas, dinfeas, u1dotr1, u2dotr2, complem_1, complem_2, full_error, barr_param);

      //if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_ITERATES_MATLAB_FILE])
      //  printInteresProbMatlabFile(iteration, globalVelocity, velocity, reaction, d, n, m, iterates);

      hasNotConverged = 0;
      break;
    }




    /* -------------------------- Compute NT directions -------------------------- */
    /* with NT scaling */
    if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING])
    {
      if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING_METHOD] == SICONOS_FRICTION_3D_IPM_NESTEROV_TODD_SCALING_WITH_QP)
      {
        Nesterov_Todd_vector(0, velocity_1, reaction_1, n_dminus2, n, p_bar);
        Qp_bar = QRmat(p_bar, n_dminus2, n);
        Nesterov_Todd_vector(2, velocity_1, reaction_1, n_dminus2, n, p2_bar);
        Qp2_bar = QRmat(p2_bar, n_dminus2, n);

        Nesterov_Todd_vector(0, velocity_2, reaction_2, n_dminus2, n, p_tilde);
        Qp_tilde = QRmat(p_tilde, n_dminus2, n);
        Nesterov_Todd_vector(2, velocity_2, reaction_2, n_dminus2, n, p2_tilde);
        Qp2_tilde = QRmat(p2_tilde, n_dminus2, n);
      }
      else if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING_METHOD] == SICONOS_FRICTION_3D_IPM_NESTEROV_TODD_SCALING_WITH_F)
      {
        Qp_bar = NTmat(velocity_1, reaction_1, n_dminus2, n);
        Qp2_bar = NTmatsqr(velocity_1, reaction_1, n_dminus2, n);

        Qp_tilde = NTmat(velocity_2, reaction_2, n_dminus2, n);
        Qp2_tilde = NTmatsqr(velocity_2, reaction_2, n_dminus2, n);
      }

      /*   1. Build the Jacobian matrix
       *
       *           m    nd   n(d+1)
       *        |  M    -H'    0   | m
       *        |                  |
       *  Jac = | -H     0     J   | nd
       *        |                  |
       *        |  0     J'    Q^2 | n(d+1)
       *
       *  where J is a block matrix
       *
       *  and
       *
       *          n(d-2)    n(d-2)
       *        | Qp_bar      0    | n(d-2)
       *   Q  = |                  |
       *        |  0      Qp_tilde | n(d-2)
       */

      Jac = NM_create(NM_SPARSE, m + nd + n_dplus1, m + nd + n_dplus1);
      Jac_nzmax = M_nzmax + 2*H_nzmax + 2*9*n*n + 6*n;
      NM_triplet_alloc(Jac, Jac_nzmax);
      Jac->matrix2->origin = NSM_TRIPLET;
      NM_insert(Jac, M, 0, 0);
      NM_insert(Jac, NM_transpose(minus_H), 0, m);
      NM_insert(Jac, minus_H, m, 0);
      NM_insert(Jac, J, m, m_plus_nd);
      NM_insert(Jac, NM_transpose(J), m_plus_nd, m);
      NM_insert(Jac, Qp2_bar, m_plus_nd, m_plus_nd);
      NM_insert(Jac, Qp2_tilde, m_plus_nd+n_dminus2, m_plus_nd+n_dminus2);
    }

    /* without NT scaling */
    else
    {
      /*   1. Build the Jacobian matrix
       *
       *               m     nd      n(d-2)    n(d-2)
       *            |  M     -H'        0         0    | m
       *            |                                  |
       *    Jac   = | -H      0      [...... J ......] | nd
       *            |                                  |
       *            |  0   block_1   arw(r1)      0    | n(d-2)
       *            |                                  |
       *            |  0   block_2      0      arw(r2) | n(d-2)
       *
       *  where
       *
       *                1       2        2
       *            |   t     u_bar'     0  | 1
       *  block_1 = |                       |         x n blocks
       *            | u_bar     tI       0  | 2
       *
       *  and
       *
       *                1       2        2
       *            |   t'      0     u_tilde' | 1
       *  block_2 = |                          |      x n blocks
       *            | u_tilde   0       t'I    | 2
       *
       */
      Jac = NM_create(NM_SPARSE, m + nd + n_dplus1, m + nd + n_dplus1);
      Jac_nzmax = M_nzmax + 2*H_nzmax + 2*9*n*n + 6*n;
      NM_triplet_alloc(Jac, Jac_nzmax);
      Jac->matrix2->origin = NSM_TRIPLET;
      NM_insert(Jac, M, 0, 0);
      NM_insert(Jac, NM_transpose(minus_H), 0, m);
      NM_insert(Jac, minus_H, m, 0);
      NM_insert(Jac, J, m, m_plus_nd);

      /* Create matrices block_1, block_2 */
      int n3, n5;
      n3 = 3*n;
      n5 = 5*n;
      NumericsMatrix * block_1 = NM_create(NM_SPARSE, n5, n3);
      NumericsMatrix * block_2 = NM_create(NM_SPARSE, n5, n3);
      long blocks_nzmax = 3*2*n;
      NM_triplet_alloc(block_1, blocks_nzmax);
      NM_triplet_alloc(block_2, blocks_nzmax);
      NM_fill(block_1, NM_SPARSE, n5, n3, block_1->matrix2);
      NM_fill(block_2, NM_SPARSE, n5, n3, block_2->matrix2);
      block_1->matrix2->origin = NSM_TRIPLET;
      block_2->matrix2->origin = NSM_TRIPLET;

      for(size_t i = 0; i < n; ++i)
      {
        posX = i * d_minus_2;   // row = 3*i
        posY = i * d;           // col = 5*i
        NM_entry(block_1, posX, posY, velocity_1[posX]);
        NM_entry(block_2, posX, posY, velocity_2[posX]);

        for(size_t j = 1; j < d_minus_2; ++j)
        {
          NM_entry(block_1, posX, posY + j, velocity_1[posX + j]);
          NM_entry(block_1, posX + j, posY, velocity_1[posX + j]);
          NM_entry(block_1, posX + j, posY + j, velocity_1[posX]);

          NM_entry(block_2, posX, posY + j + 2, velocity_2[posX + j]);
          NM_entry(block_2, posX + j, posY, velocity_2[posX + j]);
          NM_entry(block_2, posX + j, posY + j + 2, velocity_2[posX]);
        }
      }

      /* Continue adding into Jac matrix */
      NM_insert(Jac, block_1, m_plus_nd, m);
      NM_insert(Jac, Arrow_repr(reaction_1, n_dminus2, n), m_plus_nd, m_plus_nd);
      NM_insert(Jac, block_2, m_plus_nd+n_dminus2, m);
      NM_insert(Jac, Arrow_repr(reaction_2, n_dminus2, n), m_plus_nd+n_dminus2, m_plus_nd+n_dminus2);







    // if (iteration == 5)
    // {
    //   printf("\n\n========== PRINTING FOR DEBUG ==========\n");
    //   printf("\n\niteration: %d\n", iteration);
    //   // printf("Vector v:\n");
    //   // NM_vector_display(globalVelocity,m);
    //   // printf("\n\nVector u:\n");
    //   // NM_vector_display(velocity,nd);
    //   // printf("\n\nVector r:\n");
    //   // NM_vector_display(reaction,nd);
    //   printf("\n\nVector t:\n");
    //   NM_vector_display(t,n);
    //   printf("\n\nVector t_prime:\n");
    //   NM_vector_display(t_prime,n);
    //   printf("\n\nVector u_1:\n");
    //   NM_vector_display(velocity_1,n_dminus2);
    //   printf("\n\nVector u_2:\n");
    //   NM_vector_display(velocity_2,n_dminus2);
    //   printf("\n\nVector r_1:\n");
    //   NM_vector_display(reaction_1,n_dminus2);
    //   printf("\n\nVector r_2:\n");
    //   NM_vector_display(reaction_2,n_dminus2);

    //   // printf("Vector dv:\n");
    //   // NM_vector_display(d_globalVelocity,m);
    //   // printf("\n\nVector du:\n");
    //   // NM_vector_display(d_velocity,nd);
    //   // printf("\n\nVector dr:\n");
    //   // NM_vector_display(d_reaction,nd);
    //   // printf("\n\nVector dt:\n");
    //   // NM_vector_display(d_t,n);
    //   // printf("\n\nVector dt_prime:\n");
    //   // NM_vector_display(d_t_prime,n);
    //   // printf("\n\nVector du_1:\n");
    //   // NM_vector_display(d_velocity_1,n_dminus2);
    //   // printf("\n\nVector du_2:\n");
    //   // NM_vector_display(d_velocity_2,n_dminus2);
    //   // printf("\n\nVector dr_1:\n");
    //   // NM_vector_display(d_reaction_1,n_dminus2);
    //   // printf("\n\nVector dr_2:\n");
    //   // NM_vector_display(d_reaction_2,n_dminus2);

    //   printf("\n\nblock_1:\n");
    //   NM_display(block_1);
    //   printf("\n\nblock_2:\n");
    //   NM_display(block_2);
    //   printf("\n\nJac:\n");
    //   NM_display(Jac);
    //   printf("\n\nrhs:\n");
    //   NM_vector_display(rhs, m_plus_nd+n_dplus1);

    //   // printf("\n\nalpha_primal_1 = %f\n", alpha_primal_1);
    //   // printf("\n\nalpha_primal_2 = %f\n", alpha_primal_2);
    //   // printf("\n\nalpha_primal = %f\n", alpha_primal);
    //   // printf("\n\nalpha_dual_1 = %f\n", alpha_dual_1);
    //   // printf("\n\nalpha_dual_2 = %f\n", alpha_dual_2);
    //   // printf("\n\nalpha_dual = %f\n", alpha_dual);

    //   // printf("\n\ncomplem_1 = %9.16f\n", complem_1);
    //   // printf("\n\ncomplem_2 = %9.16f\n", complem_2);
    //   // printf("\n\nerror = %f\n", error);
    //   printf("========== END PRINTING FOR DEBUG ==========\n\n");
    //   break;
    // }



















      NM_clear(block_1);
      free(block_1);
      NM_clear(block_2);
      free(block_2);
    }






 //    /** Correction of w to take into account the dependence
 //        on the tangential velocity */
 //    if(options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_UPDATE_S] == 1)
 //    {
 //      for(unsigned int i = 0; i < nd; ++ i)
 //      {
 //        if(i % d == 0)
 //          /* w[i] = w_origin[i]/(problem->mu[(int)(i/d)]) */
 //    w[i] = w_origin[i] + sqrt(velocity[i+1]*velocity[i+1]+velocity[i+2]*velocity[i+2]);
 //      }

 //    }
 //






    /*  Build the right-hand side related to the first system (sigma = 0)
     *
     *  with NT scaling
     *  rhs = -
     *        [ M*v - H'*r + f ]  m         dualConstraint
     *        [  u - H*v - w   ]  nd        primalConstraint
     *        [      r_1       ]  n(d-2)    complemConstraint 1
     *        [      r_2       ]  n(d-2)    complemConstraint 2
     *
     *  without NT scaling
     *  rhs = -
     *        [ M*v - H'*r + f ]  m         dualConstraint
     *        [  u - H*v - w   ]  nd        primalConstraint
     *        [   u_1 o r_1    ]  n(d-2)    complemConstraint 1
     *        [   u_2 o r_2    ]  n(d-2)    complemConstraint 2
     */
    cblas_dcopy(m, dualConstraint, 1, rhs, 1);
    cblas_dcopy(nd, primalConstraint, 1, rhs+m, 1);
    if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] > 0)
    {
      cblas_dcopy(n_dminus2, reaction_1, 1, rhs+m_plus_nd, 1);
      assert((m_plus_nd+2*n_dminus2) == (m_plus_nd+n_dplus1)); // m + nd + n(d+1): size of rhs
      cblas_dcopy(n_dminus2, reaction_2, 1, rhs+m_plus_nd+n_dminus2, 1);
    }
    else
    {
      JA_prod(velocity_1, reaction_1, n_dminus2, n, complemConstraint_1);
      JA_prod(velocity_2, reaction_2, n_dminus2, n, complemConstraint_2);
      cblas_dcopy(n_dminus2, complemConstraint_1, 1, rhs+m_plus_nd, 1);
      assert((m_plus_nd+2*n_dminus2) == (m_plus_nd+n_dplus1)); // m + nd + n(d+1): size of rhs
      cblas_dcopy(n_dminus2, complemConstraint_2, 1, rhs+m_plus_nd+n_dminus2, 1);

    }
    cblas_dscal(m + nd + n_dplus1, -1.0, rhs, 1);






    if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] > 0)
    {
      /* Solving full symmetric Newton system with NT scaling via LDLT factorization */
      NSM_linearSolverParams(Jac)->solver = NSM_HSL;
      NM_LDLT_solve(Jac, rhs, 1);
    }
    else
    {
      /* Solving non-symmetric Newton system without NT scaling via LU factorization */
      NM_LU_solve(Jac, rhs, 1);
    }




    /* Retrieve the solutions
     * Note that the results are stored in rhs, then
     *
     *                 [ d_globalVelocity ]  m
     *                 [ d_reaction_0     ]  1 x n  |
     *                 [ d_reaction_bar   ]  2 x n  | = nd
     * Results rhs  =  [ d_reaction_tilde ]  2 x n  |
     *                 [ d_t              ]  1 x n  }
     *                 [ d_velocity_bar   ]  2 x n  } = n(d-2) = n_dminus2
     *                 [ d_t'             ]  1 x n  |
     *                 [ d_velocity_tilde ]  2 x n  | = n(d-2)
     *
     * and
     *
     * d_reaction   = (d_reaction_0, d_reaction_bar, d_reaction_tilde)
     * d_reaction_1 = (d_reaction_0, d_reaction_bar)
     * d_reaction_2 = (d_reaction_0, d_reaction_tilde)
     *
     * d_velocity   = (d_t + d_t', d_velocity_bar, d_velocity_tilde)
     * d_velocity_1 = (d_t , d_velocity_bar)
     * d_velocity_2 = (d_t', d_velocity_tilde)
     *
     */
    cblas_dcopy(m, rhs, 1, d_globalVelocity, 1);
    cblas_dcopy(nd, rhs+m, 1, d_reaction, 1);
    extract_vector(d_reaction, nd, n, 2, 3, d_reaction_1);
    extract_vector(d_reaction, nd, n, 4, 5, d_reaction_2);

    cblas_dcopy(n_dminus2, rhs+m_plus_nd, 1, d_velocity_1, 1);
    assert((m_plus_nd+2*n_dminus2) == (m_plus_nd+n_dplus1)); // m + nd + n(d+1): size of rhs
    cblas_dcopy(n_dminus2, rhs+m_plus_nd+n_dminus2, 1, d_velocity_2, 1);

    for(size_t i = 0; i < n; i++)
    {
      posX = i*d;
      posY = i*d_minus_2;
      d_velocity[posX] = d_velocity_1[posY] + d_velocity_2[posY];
      d_velocity[posX+1] = d_velocity_1[posY + 1];
      d_velocity[posX+2] = d_velocity_1[posY + 2];
      d_velocity[posX+3] = d_velocity_2[posY + 1];
      d_velocity[posX+4] = d_velocity_2[posY + 2];
    }







    /* computing the affine step-length */
    alpha_primal_1 = getStepLength(velocity_1, d_velocity_1, n_dminus2, n, gmm);
    alpha_primal_2 = getStepLength(velocity_2, d_velocity_2, n_dminus2, n, gmm);
    alpha_dual_1 = getStepLength(reaction_1, d_reaction_1, n_dminus2, n, gmm);
    alpha_dual_2 = getStepLength(reaction_2, d_reaction_2, n_dminus2, n, gmm);

    alpha_primal = fmin(alpha_primal_1, fmin(alpha_primal_2, fmin(alpha_dual_1, alpha_dual_2)));
    alpha_dual = alpha_primal;

    /* updating the gamma parameter used to compute the step-length */
    // gmm = gmmp1 + gmmp2 * fmin(alpha_primal, alpha_dual);
    gmm = gmmp1 + gmmp2 * alpha_primal;










    // if (iteration == 0)
    // {
    //   printf("\n\n========== PRINTING FOR DEBUG ==========\n");
    //   printf("\n\niteration: %d\n", iteration);
    //   // printf("Vector v:\n");
    //   // NM_vector_display(globalVelocity,m);
    //   // printf("\n\nVector u:\n");
    //   // NM_vector_display(velocity,nd);
    //   // printf("\n\nVector r:\n");
    //   // NM_vector_display(reaction,nd);
    //   printf("\n\nVector t:\n");
    //   NM_vector_display(t,n);
    //   printf("\n\nVector t_prime:\n");
    //   NM_vector_display(t_prime,n);
    //   printf("\n\nVector u_1:\n");
    //   NM_vector_display(velocity_1,n_dminus2);
    //   printf("\n\nVector u_2:\n");
    //   NM_vector_display(velocity_2,n_dminus2);
    //   printf("\n\nVector r_1:\n");
    //   NM_vector_display(reaction_1,n_dminus2);
    //   printf("\n\nVector r_2:\n");
    //   NM_vector_display(reaction_2,n_dminus2);

    //   // printf("Vector dv:\n");
    //   // NM_vector_display(d_globalVelocity,m);
    //   // printf("\n\nVector du:\n");
    //   // NM_vector_display(d_velocity,nd);
    //   // printf("\n\nVector dr:\n");
    //   // NM_vector_display(d_reaction,nd);
    //   // printf("\n\nVector dt:\n");
    //   // NM_vector_display(d_t,n);
    //   // printf("\n\nVector dt_prime:\n");
    //   // NM_vector_display(d_t_prime,n);
    //   // printf("\n\nVector du_1:\n");
    //   // NM_vector_display(d_velocity_1,n_dminus2);
    //   // printf("\n\nVector du_2:\n");
    //   // NM_vector_display(d_velocity_2,n_dminus2);
    //   // printf("\n\nVector dr_1:\n");
    //   // NM_vector_display(d_reaction_1,n_dminus2);
    //   // printf("\n\nVector dr_2:\n");
    //   // NM_vector_display(d_reaction_2,n_dminus2);

    //   printf("\n\nblock_1:\n");
    //   NM_display(block_1);
    //   printf("\n\nblock_2:\n");
    //   NM_display(block_2);
    //   printf("\n\nJac:\n");
    //   NM_display(Jac);
    //   printf("\n\nrhs:\n");
    //   NM_vector_display(rhs, m_plus_nd+n_dplus1);

    //   // printf("\n\nalpha_primal_1 = %f\n", alpha_primal_1);
    //   // printf("\n\nalpha_primal_2 = %f\n", alpha_primal_2);
    //   // printf("\n\nalpha_primal = %f\n", alpha_primal);
    //   // printf("\n\nalpha_dual_1 = %f\n", alpha_dual_1);
    //   // printf("\n\nalpha_dual_2 = %f\n", alpha_dual_2);
    //   // printf("\n\nalpha_dual = %f\n", alpha_dual);

    //   // printf("\n\ncomplem_1 = %9.16f\n", complem_1);
    //   // printf("\n\ncomplem_2 = %9.16f\n", complem_2);
    //   // printf("\n\nerror = %f\n", error);
    //   printf("========== END PRINTING FOR DEBUG ==========\n\n");
    //   break;
    // }



















    /* -------------------------- Predictor step of Mehrotra -------------------------- */
    cblas_dcopy(nd, velocity, 1, v_plus_dv, 1);
    cblas_dcopy(nd, reaction, 1, r_plus_dr, 1);
    cblas_daxpy(nd, alpha_primal, d_velocity, 1, v_plus_dv, 1);
    cblas_daxpy(nd, alpha_dual, d_reaction, 1, r_plus_dr, 1);

    /* affine barrier parameter */
    // barr_param_a = (cblas_ddot(nd, v_plus_dv, 1, r_plus_dr, 1) / nd)*sigma;
    // barr_param_a = cblas_ddot(nd, v_plus_dv, 1, r_plus_dr, 1) / nd;
    barr_param_a = cblas_ddot(nd, v_plus_dv, 1, r_plus_dr, 1) / n;
    //barr_param_a = complemResidualNorm(v_plus_dv, r_plus_dr, nd, n);
    // barr_param_a = (fws=='*' ? complemResidualNorm(v_plus_dv, r_plus_dr, nd, n)/n : complemResidualNorm_p(v_plus_dv, r_plus_dr, nd, n)/n);

    /* computing the centralization parameter */
    e = barr_param > sgmp1 ? fmax(1.0, sgmp2 * pow(fmin(alpha_primal, alpha_dual),2)) : sgmp3;
    sigma = fmin(1.0, pow(barr_param_a / barr_param, e))/5;

























    /* -------------------------- Corrector step of Mehrotra -------------------------- */
    cblas_dcopy(m, dualConstraint, 1, rhs, 1);
    cblas_dcopy(nd, primalConstraint, 1, rhs+m, 1);
    if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] > 0)
    {
      /* Right-hand side for symmetric Newton system with NT scaling */
      /* 2nd terms: 2 * barr_param * sigma * Qp_bar * (Qp_bar * u_1)^-1  */
      NM_gemv(1.0, Qp_bar, velocity_1, 0.0, velocity_1t);                         // velocity_1t        = Qp_bar * u_1
      NM_gemv(1.0, Qp_tilde, velocity_2, 0.0, velocity_2t);                       // velocity_2t        = Qp_tilde * u_2
      JA_inv(velocity_1t, n_dminus2, n, velocity_1t_inv);                         // velocity_1t_inv    = (Qp_bar * u_1)^-1
      JA_inv(velocity_2t, n_dminus2, n, velocity_2t_inv);                         // velocity_2t_inv    = (Qp_tilde * u_2)^-1
      NM_gemv(1.0, Qp_bar, velocity_1t_inv, 0.0, Qp_velocity_1t_inv);             // Qp_velocity_1t_inv = Qp_bar * (Qp_bar * u_1)^-1
      NM_gemv(1.0, Qp_tilde, velocity_2t_inv, 0.0, Qp_velocity_2t_inv);           // Qp_velocity_2t_inv = Qp_tilde * (Qp_tilde * u_2)^-1
      cblas_dscal(n_dminus2, 2 * barr_param * sigma, Qp_velocity_1t_inv, 1);      // Qp_velocity_1t_inv = 2nd term
      cblas_dscal(n_dminus2, 2 * barr_param * sigma, Qp_velocity_2t_inv, 1);      // Qp_velocity_2t_inv = 2nd term


      /* 3rd terms: (Qp_bar * du_1) o (Qpinv_1 * dr_1)  */
      NM_gemv(1.0, Qp_bar, d_velocity_1, 0.0, d_velocity_1t);                      // d_velocity_1t     = Qp_bar * du_1
      NM_gemv(1.0, Qp_tilde, d_velocity_2, 0.0, d_velocity_2t);                    // d_velocity_2t     = Qp_tilde * du_2
      QNTpinvz(velocity_1, reaction_1, d_reaction_1, n_dminus2, n, d_reaction_1t); // d_reaction_1t     = Qpinv_1 * dr_1
      QNTpinvz(velocity_2, reaction_2, d_reaction_2, n_dminus2, n, d_reaction_2t); // d_reaction_2t     = Qpinv_2 * dr_2
      JA_prod(d_velocity_1t, d_reaction_1t, n_dminus2, n, dvdr_jprod_1);           // dvdr_jprod_1      = 3rd term
      JA_prod(d_velocity_2t, d_reaction_2t, n_dminus2, n, dvdr_jprod_2);           // dvdr_jprod_1      = 3rd term


      /* updated rhs = r_1 - 2nd term + 3rd term */
      NV_sub(reaction_1, Qp_velocity_1t_inv, n_dminus2, tmp1);
      NV_sub(reaction_2, Qp_velocity_2t_inv, n_dminus2, tmp2);
      NV_add(tmp1, dvdr_jprod_1, n_dminus2, complemConstraint_1);
      NV_add(tmp2, dvdr_jprod_2, n_dminus2, complemConstraint_2);
    }
    else
    {
      /* Right-hand side for non-symmetric Newton system without NT scaling */
      /* 1st terms: u_1 o r_1  */
      JA_prod(velocity_1, reaction_1, n_dminus2, n, vr_jprod_1);
      JA_prod(velocity_2, reaction_2, n_dminus2, n, vr_jprod_2);

      /* 2nd terms: 2 * barr_param * sigma * e  */
      iden = JA_iden(n_dminus2, n);
      cblas_dscal(n_dminus2, 2 * barr_param * sigma, iden, 1);

      /* 3rd terms: du_1 o dr_1  */
      JA_prod(d_velocity_1, d_reaction_1, n_dminus2, n, dvdr_jprod_1);
      JA_prod(d_velocity_2, d_reaction_2, n_dminus2, n, dvdr_jprod_2);

      /* updated rhs = 1st term - 2nd term + 3rd term */
      NV_sub(vr_jprod_1, iden, n_dminus2, tmp1);
      NV_sub(vr_jprod_2, iden, n_dminus2, tmp2);
      NV_add(tmp1, dvdr_jprod_1, n_dminus2, complemConstraint_1);
      NV_add(tmp2, dvdr_jprod_2, n_dminus2, complemConstraint_2);
      free(iden);
    }


    /* Update only 2 complementarities of rhs  */
    // cblas_dcopy(m, dualConstraint, 1, rhs, 1);
    // cblas_dcopy(nd, primalConstraint, 1, rhs+m, 1);
    cblas_dcopy(n_dminus2, complemConstraint_1, 1, rhs+m_plus_nd, 1);
    cblas_dcopy(n_dminus2, complemConstraint_2, 1, rhs+m_plus_nd+n_dminus2, 1);
    cblas_dscal(m + nd + n_dplus1, -1.0, rhs, 1);











    if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] > 0)
    {
      /* Solving full symmetric Newton system with NT scaling via LDLT factorization */
      // NSM_linearSolverParams(Jac)->solver = NSM_HSL;
      NM_LDLT_solve(Jac, rhs, 1);
    }
    else
    {
      /* Solving non-symmetric Newton system without NT scaling via LU factorization */
      NM_LU_solve(Jac, rhs, 1);
    }




    NM_clear(Jac);
    free(Jac);



    /* Retrieve the solutions */
    cblas_dcopy(m, rhs, 1, d_globalVelocity, 1);
    cblas_dcopy(nd, rhs+m, 1, d_reaction, 1);
    extract_vector(d_reaction, nd, n, 2, 3, d_reaction_1);
    extract_vector(d_reaction, nd, n, 4, 5, d_reaction_2);

    cblas_dcopy(n_dminus2, rhs+m_plus_nd, 1, d_velocity_1, 1);
    assert((m_plus_nd+2*n_dminus2) == (m_plus_nd+n_dplus1)); // m + nd + n(d+1): size of rhs
    cblas_dcopy(n_dminus2, rhs+m_plus_nd+n_dminus2, 1, d_velocity_2, 1);

    for(size_t i = 0; i < n; i++)
    {
      posX = i*d;
      posY = i*d_minus_2;
      d_velocity[posX] = d_velocity_1[posY] + d_velocity_2[posY];
      d_velocity[posX+1] = d_velocity_1[posY + 1];
      d_velocity[posX+2] = d_velocity_1[posY + 2];
      d_velocity[posX+3] = d_velocity_2[posY + 1];
      d_velocity[posX+4] = d_velocity_2[posY + 2];
      d_t[i] = d_velocity_1[posY];
      d_t_prime[i] = d_velocity_2[posY];
    }











    // if (iteration == 0)
    // {
    //   printf("\n\n========== PRINTING FOR DEBUG ==========\n");
    //   printf("\n\niteration: %d\n", iteration);
    //   // printf("Vector v:\n");
    //   // NM_vector_display(globalVelocity,m);
    //   // printf("\n\nVector u:\n");
    //   // NM_vector_display(velocity,nd);
    //   // printf("\n\nVector r:\n");
    //   // NM_vector_display(reaction,nd);
    //   // printf("\n\nVector t:\n");
    //   // NM_vector_display(t,n);
    //   // printf("\n\nVector t_prime:\n");
    //   // NM_vector_display(t_prime,n);
    //   // printf("\n\nVector u_1:\n");
    //   // NM_vector_display(velocity_1,n_dminus2);
    //   // printf("\n\nVector u_2:\n");
    //   // NM_vector_display(velocity_2,n_dminus2);
    //   // printf("\n\nVector r_1:\n");
    //   // NM_vector_display(reaction_1,n_dminus2);
    //   // printf("\n\nVector r_2:\n");
    //   // NM_vector_display(reaction_2,n_dminus2);

    //   printf("Vector dv:\n");
    //   NM_vector_display(d_globalVelocity,m);
    //   printf("\n\nVector du:\n");
    //   NM_vector_display(d_velocity,nd);
    //   printf("\n\nVector dr:\n");
    //   NM_vector_display(d_reaction,nd);
    //   printf("\n\nVector dt:\n");
    //   NM_vector_display(d_t,n);
    //   printf("\n\nVector dt_prime:\n");
    //   NM_vector_display(d_t_prime,n);
    //   printf("\n\nVector du_1:\n");
    //   NM_vector_display(d_velocity_1,n_dminus2);
    //   printf("\n\nVector du_2:\n");
    //   NM_vector_display(d_velocity_2,n_dminus2);
    //   printf("\n\nVector dr_1:\n");
    //   NM_vector_display(d_reaction_1,n_dminus2);
    //   printf("\n\nVector dr_2:\n");
    //   NM_vector_display(d_reaction_2,n_dminus2);

    //   printf("\n\nJac:\n");
    //   NM_display(Jac);
    //   printf("\n\nrhs:\n");
    //   NM_vector_display(rhs, m_plus_nd+n_dplus1);

    //   // printf("\n\nalpha_primal_1 = %f\n", alpha_primal_1);
    //   // printf("\n\nalpha_primal_2 = %f\n", alpha_primal_2);
    //   // printf("\n\nalpha_primal = %f\n", alpha_primal);
    //   // printf("\n\nalpha_dual_1 = %f\n", alpha_dual_1);
    //   // printf("\n\nalpha_dual_2 = %f\n", alpha_dual_2);
    //   // printf("\n\nalpha_dual = %f\n", alpha_dual);

    //   printf("\n\ncomplem_1 = %9.16f\n", complem_1);
    //   printf("\n\ncomplem_2 = %9.16f\n", complem_2);
    //   // printf("\n\nerror = %f\n", error);
    //   printf("========== END PRINTING FOR DEBUG ==========\n\n");
    //   break;
    // }





















    /* computing the affine step-length */
    alpha_primal_1 = getStepLength(velocity_1, d_velocity_1, n_dminus2, n, gmm);
    alpha_primal_2 = getStepLength(velocity_2, d_velocity_2, n_dminus2, n, gmm);
    alpha_dual_1   = getStepLength(reaction_1, d_reaction_1, n_dminus2, n, gmm);
    alpha_dual_2   = getStepLength(reaction_2, d_reaction_2, n_dminus2, n, gmm);

    alpha_primal = fmin(alpha_primal_1, fmin(alpha_primal_2, fmin(alpha_dual_1, alpha_dual_2)));
    alpha_dual = alpha_primal;

    /* updating the gamma parameter used to compute the step-length */
    gmm = gmmp1 + gmmp2 * alpha_primal;


    /* print out all useful parameters */
    numerics_printf_verbose(-1, "| %3i%c| %9.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e | %.2e |",
                            iteration, fws, relgap, pinfeas, dinfeas, u1dotr1, u2dotr2, complem_1, complem_2, full_error, barr_param, alpha_primal, alpha_dual, sigma,
                            cblas_dnrm2(m, d_globalVelocity, 1)/cblas_dnrm2(m, globalVelocity, 1),
                            cblas_dnrm2(nd, d_velocity, 1)/cblas_dnrm2(nd, velocity, 1),
                            cblas_dnrm2(nd, d_reaction, 1)/cblas_dnrm2(nd, reaction, 1));









    /* ----- Update variables ----- */
    cblas_daxpy(m, alpha_primal, d_globalVelocity, 1, globalVelocity, 1);
    cblas_daxpy(nd, alpha_primal, d_velocity, 1, velocity, 1);
    cblas_daxpy(nd, alpha_dual, d_reaction, 1, reaction, 1);
    cblas_daxpy(n, alpha_dual, d_t, 1, t, 1);
    cblas_daxpy(n, alpha_dual, d_t_prime, 1, t_prime, 1);

    if (NV_isnan(globalVelocity, m) | NV_isnan(velocity, nd) | NV_isnan(reaction, nd))
    {
      hasNotConverged = 2;
      break;
    }





    iteration++;
  } // end of while loop


  /* -------------------------- Return to original variables -------------------------- */
  // TO DO: If we need this point, plz uncomment
  // NM_gemv(1.0, P_mu_inv, velocity, 0.0, data->original_point->velocity);
  // NM_gemv(1.0, P_mu, reaction, 0.0, data->original_point->reaction);
  // cblas_dcopy(m, globalVelocity, 1, data->original_point->globalVelocity, 1);

  options->dparam[SICONOS_DPARAM_RESIDU] = full_error; //NV_max(error, 4);
  options->iparam[SICONOS_IPARAM_ITER_DONE] = iteration;




  clock_t t2 = clock();
  long clk_tck = CLOCKS_PER_SEC;

  /* writing data in a Matlab file */
  if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_ITERATES_MATLAB_FILE])
  {
    char matlab_file_name[256];
    sprintf(matlab_file_name,"sigma_nc-%d-.m", problem->numberOfContacts);
    matlab_file = fopen(matlab_file_name, "w");
    printInteresProbMatlabFile(iteration, globalVelocity, velocity, reaction, d, n, m, (double)(t2-t1)/(double)clk_tck, matlab_file);
    fclose(matlab_file);
  }





  if(internal_allocation)
  {
    grfc3d_IPM_free(problem,options);
  }

  if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] > 0)
  {
    NM_clear(Qp_bar);
    free(Qp_bar);
    NM_clear(Qp2_bar);
    free(Qp2_bar);
    NM_clear(Qp_tilde);
    free(Qp_tilde);
    NM_clear(Qp2_tilde);
    free(Qp2_tilde);
  }







  NM_clear(H_origin);
  free(H_origin);
  NM_clear(minus_H);
  free(minus_H);
  NM_clear(H);
  free(H);
  NM_clear(minus_M);
  free(minus_M);
  NM_clear(J);
  free(J);

  // NM_clear(Jac); // already
  // free(Jac);

  free(t);
  free(t_prime);
  free(velocity_1);
  free(velocity_2);
  free(reaction_1);
  free(reaction_2);
  free(d_globalVelocity);
  free(d_velocity);
  free(d_velocity_1);
  free(d_velocity_2);
  free(d_reaction);
  free(d_reaction_1);
  free(d_reaction_2);
  free(d_t);
  free(d_t_prime);













  // if (options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_ITERATES_MATLAB_FILE])
  //   fclose(iterates);

  //  fclose(dfile);

  *info = hasNotConverged;








} // end of grfc3d_IPM











/* initialize solver (allocate memory) */
void grfc3d_IPM_init(GlobalRollingFrictionContactProblem* problem, SolverOptions* options)
{
  unsigned int m = problem->M->size0;
  unsigned int nd = problem->H->size1;
  unsigned int d = problem->dimension;

  unsigned int n = (int)(nd / d);
  unsigned int n_dminus2 = n*(d-2);


  // TO DO: need to review this. Ex. if options->dWork existed and options->dWorkSize is unexpected, then what will we do ?
  if(!options->dWork || options->dWorkSize != (size_t)(m + nd + n*(d+1)))
  {
    options->dWork = (double*)calloc(m + nd + n*(d+1), sizeof(double));
    options->dWorkSize = m + nd + n*(d+1);
  }


  /* ------------- initialize starting point ------------- */
  options->solverData=(Grfc3d_IPM_data *)malloc(sizeof(Grfc3d_IPM_data));
  Grfc3d_IPM_data * data = (Grfc3d_IPM_data *)options->solverData;


  /* --------- allocate memory for IPM point ----------- */
  data->starting_point = (IPM_point*)malloc(sizeof(IPM_point));

  /* 1. v */
  data->starting_point->globalVelocity = (double*)calloc(m, sizeof(double));
  for(unsigned int i = 0; i < m; ++ i)
    data->starting_point->globalVelocity[i] = 0.01;

  /* 2. u */
  data->starting_point->velocity = (double*)calloc(nd, sizeof(double));
  for(unsigned int i = 0; i < nd; ++ i)
  {
    data->starting_point->velocity[i] = 0.001;
    if(i % d == 0)
      data->starting_point->velocity[i] = 3.0;
  }

  /* 3. r */
  data->starting_point->reaction = (double*)calloc(nd, sizeof(double));
  for(unsigned int i = 0; i < nd; ++ i)
  {
    data->starting_point->reaction[i] = 0.04;
    if(i % d == 0)
      data->starting_point->reaction[i] = 0.5;
  }


  /* original point which is not changed by the matrix P_mu */
  data->original_point = (IPM_point*)malloc(sizeof(IPM_point));
  data->original_point->globalVelocity = (double*)calloc(m, sizeof(double));
  data->original_point->velocity = (double*)calloc(nd, sizeof(double));
  data->original_point->reaction = (double*)calloc(nd, sizeof(double));



  /* --------- allocate memory for IPM grfc3d point ----------- */
  // data->grfc3d_point = (IPM_grfc3d_point*)malloc(sizeof(IPM_grfc3d_point));

  /* 1. t, t_prime */
  // data->grfc3d_point->t = (double*)calloc(n, sizeof(double));
  // data->grfc3d_point->t_prime = (double*)calloc(n, sizeof(double));
  // for(unsigned int i = 0; i < n; ++ i)
  // {
  //   data->grfc3d_point->t[i] = 1.0;
  //   data->grfc3d_point->t_prime[i] = 1.0;
  // }

  /*
   * 2. velocity_1 = (t, u_bar), velocity_2 = (t_prime, u_tilde)
   * We will assign these variables later, in the while loop
   */
  // data->grfc3d_point->velocity_1 = (double*)calloc(n_dminus2, sizeof(double));
  // data->grfc3d_point->velocity_2 = (double*)calloc(n_dminus2, sizeof(double));

  /*
   * 3. reaction_1 = (r0, r_bar), reaction_2 = (r0, r_tilde)
   * We will assign these variables later, in the while loop
   */
  // data->grfc3d_point->reaction_1 = (double*)calloc(n_dminus2, sizeof(double));
  // data->grfc3d_point->reaction_2 = (double*)calloc(n_dminus2, sizeof(double));


  /* ------ initialize the change of variable matrix P_mu ------- */
  data->P_mu = (IPM_change_of_variable*)malloc(sizeof(IPM_change_of_variable));
  data->P_mu->mat = NM_create(NM_SPARSE, nd, nd);
  NM_triplet_alloc(data->P_mu->mat, nd);
  data->P_mu->mat->matrix2->origin = NSM_TRIPLET;
  for(unsigned int i = 0; i < nd; ++i)
  {
    if(i % d == 0)
      /* NM_entry(data->P_mu->mat, i, i, 1. / problem->mu[(int)(i/d)]); */
      NM_entry(data->P_mu->mat, i, i, 1.);
    else if(i % d == 1 || i % d == 2)
      /* NM_entry(data->P_mu->mat, i, i, 1.); */
      NM_entry(data->P_mu->mat, i, i, problem->mu[(int)(i/d)]);
    else
      /* NM_entry(data->P_mu->mat, i, i, 1.); */
      NM_entry(data->P_mu->mat, i, i, problem->mu_r[(int)(i/d)]);
  }


  /* ------ initialize the inverse P_mu_inv of the change of variable matrix P_mu ------- */
  data->P_mu->inv_mat = NM_create(NM_SPARSE, nd, nd);
  NM_triplet_alloc(data->P_mu->inv_mat, nd);
  data->P_mu->inv_mat->matrix2->origin = NSM_TRIPLET;
  for(unsigned int i = 0; i < nd; ++i)
  {
    if(i % d == 0)
      /* NM_entry(data->P_mu->inv_mat, i, i, problem->mu[(int)(i/d)]); */
      NM_entry(data->P_mu->inv_mat, i, i, 1.);
    else if(i % d == 1 || i % d == 2)
      /* NM_entry(data->P_mu->inv_mat, i, i, 1.); */
      NM_entry(data->P_mu->inv_mat, i, i, 1.0/problem->mu[(int)(i/d)]);
    else
      /* NM_entry(data->P_mu->inv_mat, i, i, 1.); */
      NM_entry(data->P_mu->inv_mat, i, i, 1.0/problem->mu_r[(int)(i/d)]);
  }


  /* ------ initial parameters initialization ---------- */
  data->internal_params = (IPM_internal_params*)malloc(sizeof(IPM_internal_params));
  data->internal_params->alpha_primal = 1.0;
  data->internal_params->alpha_dual = 1.0;
  data->internal_params->sigma = 0.1;
  data->internal_params->barr_param = 1.0;


  /* ----- temporary vaults initialization ------- */
  data->tmp_vault_m = (double**)malloc(2 * sizeof(double*));
  for(unsigned int i = 0; i < 2; ++i)
    data->tmp_vault_m[i] = (double*)calloc(m, sizeof(double));

  data->tmp_vault_nd = (double**)malloc(10 * sizeof(double*));
  for(unsigned int i = 0; i < 10; ++i)
    data->tmp_vault_nd[i] = (double*)calloc(nd, sizeof(double));

  data->tmp_vault_n_dminus2 = (double**)malloc(25 * sizeof(double*));
  for(unsigned int i = 0; i < 25; ++i)
    data->tmp_vault_n_dminus2[i] = (double*)calloc(n_dminus2, sizeof(double));

  data->tmp_vault_n = (double**)malloc(2 * sizeof(double*));
  for(unsigned int i = 0; i < 2; ++i)
    data->tmp_vault_n[i] = (double*)calloc(n, sizeof(double));
} // end of grfc3d_IPM_init



/* deallocate memory */
void grfc3d_IPM_free(GlobalRollingFrictionContactProblem* problem, SolverOptions* options)
{
  if(options->dWork)
  {
    free(options->dWork);
    options->dWork = NULL;
    options->dWorkSize = 0;
  }

  if(options->solverData)
  {
    Grfc3d_IPM_data * data = (Grfc3d_IPM_data *)options->solverData;

    free(data->starting_point->globalVelocity);
    data->starting_point->globalVelocity = NULL;

    free(data->starting_point->velocity);
    data->starting_point->velocity = NULL;

    free(data->starting_point->reaction);
    data->starting_point->reaction = NULL;

    free(data->starting_point);
    data->starting_point = NULL;

    free(data->original_point->globalVelocity);
    data->original_point->globalVelocity = NULL;

    free(data->original_point->velocity);
    data->original_point->velocity = NULL;

    free(data->original_point->reaction);
    data->original_point->reaction = NULL;

    free(data->original_point);
    data->original_point = NULL;

    NM_clear(data->P_mu->mat);
    free(data->P_mu->mat);
    data->P_mu->mat = NULL;

    NM_clear(data->P_mu->inv_mat);
    free(data->P_mu->inv_mat);
    data->P_mu->inv_mat = NULL;

    free(data->P_mu);
    data->P_mu = NULL;

    for(unsigned int i = 0; i < 2; ++i)
      free(data->tmp_vault_m[i]);
    free(data->tmp_vault_m);
    data->tmp_vault_m = NULL;

    for(unsigned int i = 0; i < 10; ++i)
      free(data->tmp_vault_nd[i]);
    free(data->tmp_vault_nd);
    data->tmp_vault_nd = NULL;

    for(unsigned int i = 0; i < 25; ++i)
      free(data->tmp_vault_n_dminus2[i]);
    free(data->tmp_vault_n_dminus2);
    data->tmp_vault_n_dminus2 = NULL;

    for(unsigned int i = 0; i < 2; ++i)
      free(data->tmp_vault_n[i]);
    free(data->tmp_vault_n);
    data->tmp_vault_n = NULL;

    // free(data->grfc3d_point->velocity_1);
    // data->grfc3d_point->velocity_1 = NULL;

    // free(data->grfc3d_point->velocity_2);
    // data->grfc3d_point->velocity_2 = NULL;

    // free(data->grfc3d_point->reaction_1);
    // data->grfc3d_point->reaction_1 = NULL;

    // free(data->grfc3d_point->reaction_2);
    // data->grfc3d_point->reaction_2 = NULL;

    // free(data->grfc3d_point->t);
    // data->grfc3d_point->t = NULL;

    // free(data->grfc3d_point->t_prime);
    // data->grfc3d_point->t_prime = NULL;

    // free(data->grfc3d_point);

    free(data->internal_params);
    data->internal_params = NULL;
  }

  free(options->solverData);
  options->solverData = NULL;

} // end of grfc3d_IPM_free


/* setup default solver parameters */
void grfc3d_IPM_set_default(SolverOptions* options)
{

  options->iparam[SICONOS_IPARAM_MAX_ITER] = 200;

  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_GET_PROBLEM_INFO] = SICONOS_FRICTION_3D_IPM_GET_PROBLEM_INFO_NO;

  /* 0: convex case;  1: non-smooth case */
  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_UPDATE_S] = 0;

  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING] = 0;
  //options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING_METHOD] = SICONOS_FRICTION_3D_IPM_NESTEROV_TODD_SCALING_WITH_QP;
  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_NESTEROV_TODD_SCALING_METHOD] = SICONOS_FRICTION_3D_IPM_NESTEROV_TODD_SCALING_WITH_F;

  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_ITERATES_MATLAB_FILE] = 0;

  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_REDUCED_SYSTEM] = 0;

  options->iparam[SICONOS_FRICTION_3D_IPM_IPARAM_FINISH_WITHOUT_SCALING] = 1;

  options->dparam[SICONOS_DPARAM_TOL] = 1e-10;
  options->dparam[SICONOS_FRICTION_3D_IPM_SIGMA_PARAMETER_1] = 1e-5;
  options->dparam[SICONOS_FRICTION_3D_IPM_SIGMA_PARAMETER_2] = 3.;
  options->dparam[SICONOS_FRICTION_3D_IPM_SIGMA_PARAMETER_3] = 1.;
  options->dparam[SICONOS_FRICTION_3D_IPM_GAMMA_PARAMETER_1] = 0.9;
  options->dparam[SICONOS_FRICTION_3D_IPM_GAMMA_PARAMETER_2] = 0.09; //0.09

} // end of grfc3d_IPM_set_default

