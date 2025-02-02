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

#include "GMPReduced.h"
#include <assert.h>                             // for assert
#ifndef __cplusplus
#include <stdbool.h>                            // for true
#endif
#include <stdio.h>                              // for printf, size_t, NULL
#include <stdlib.h>                             // for free, malloc, calloc
#include <string.h>                             // for memcpy
#include "SiconosBlas.h"                              // for cblas_dgemv, CblasNoT...
#include "FrictionContactProblem.h"             // for FrictionContactProblem
#include "GenericMechanicalProblem.h"           // for listNumericsProblem
#include "GenericMechanical_Solvers.h"          // for gmp_gauss_seidel, gmp...
#include "LCP_Solvers.h"                        // for lcp_enum_init, lcp_en...
#include "LinearComplementarityProblem.h"       // for LinearComplementarity...
#include "MLCP_Solvers.h"                       // for mixedLinearComplement...
#include "MixedLinearComplementarityProblem.h"  // for MixedLinearComplement...
#include "NonSmoothDrivers.h"                   // for linearComplementarity...
#include "NumericsMatrix.h"                     // for NumericsMatrix, NM_fill
#include "SolverOptions.h"                      // for SICONOS_NUMERICS_PROB...
#include "SparseBlockMatrix.h"                  // for SparseBlockStructured...
#include "lcp_cst.h"                            // for SICONOS_LCP_ENUM
#include "mlcp_cst.h"                           // for SICONOS_MLCP_ENUM
#include "pinv.h"                               // for pinv

void _GMPReducedEquality(GenericMechanicalProblem* pInProblem, double * reducedProb, double * Qreduced, int * Me_size, int* Mi_size);
void _GMPReducedGetSizes(GenericMechanicalProblem* pInProblem, int * Me_size, int* Mi_size);
void buildReducedGMP(GenericMechanicalProblem* pInProblem, double * Me, double * Mi, double * Qe, double * Qi, int * Me_Size, int* Mi_Size);

#ifdef GMP_DEBUG_REDUCED
static void printDenseMatrice(char* name, FILE * file, double * m, int N, int M)
{
  if(file)
  {
    fprintf(file, "%s=[ \n", name);
    for(int i = 0; i < N; i++)
    {
      fprintf(file, "[");
      for(int j = 0; j < M; j++)
      {
        fprintf(file, "%e\t  ", m[i + j * N]);
      }
      fprintf(file, "];\n");
    }
    fprintf(file, "];\n");
  }
  else
  {
    printf("%s=[ \n", name);
    for(int i = 0; i < N; i++)
    {
      printf("[");
      for(int j = 0; j < M; j++)
      {
        printf("%e\t  ", m[i + j * N]);
      }
      printf("];\n");
    }
    printf("];\n");
  }
}
#endif

void gmp_reduced_convert_solution(GenericMechanicalProblem* pInProblem, double * reaction, double * velocity,
                                  double * Re, double * Rreduced, double * Vreduced)
{
  listNumericsProblem * curProblem = 0;
  //int curRowE=0;
  //int curRowI=0;
  curProblem =  pInProblem->firstListElem;
  int curSize = 0;
  while(curProblem)
  {
    curSize = curProblem->size;
    switch(curProblem->type)
    {
    case SICONOS_NUMERICS_PROBLEM_EQUALITY:
    {
      memcpy(reaction, Re, curSize * sizeof(double));
      for(int i = 0; i < curSize; i++)
        velocity[i] = 0.0;
      Re += curSize;
      break;
    }
    case SICONOS_NUMERICS_PROBLEM_LCP:
    case SICONOS_NUMERICS_PROBLEM_FC3D:
    {
      memcpy(reaction, Rreduced, curSize * sizeof(double));
      memcpy(velocity, Vreduced, curSize * sizeof(double));
      Rreduced += curSize;
      Vreduced += curSize;
      break;
    }
    default:
      printf("gmp_reduced_convert_solution Numerics : gmp_gauss_seidel unknown problem type %d.\n", curProblem->type);

    }
    reaction += curSize;
    velocity += curSize;
    curProblem = curProblem->nextProblem;
  }
}
void _GMPReducedGetSizes(GenericMechanicalProblem* pInProblem, int * Me_size, int* Mi_size)
{
  listNumericsProblem * curProblem = 0;
  (* Me_size) = 0;
  (* Mi_size) = 0;
  curProblem =  pInProblem->firstListElem;
  while(curProblem)
  {
    if(curProblem->type == SICONOS_NUMERICS_PROBLEM_EQUALITY)
    {
      (*Me_size) += curProblem->size;;
    }
    else
    {
      (*Mi_size) += curProblem->size;
    }
    curProblem = curProblem->nextProblem;
  }
}

/*mem loc done */
void buildReducedGMP(GenericMechanicalProblem* pInProblem, double * Me, double * Mi, double * Qe, double * Qi, int * Me_Size, int* Mi_Size)
{
  assert(pInProblem->M->storageType);
  //#ifdef TYTYFCRR
  SparseBlockStructuredMatrix* m = pInProblem->M->matrix1;
#ifdef GMP_DEBUG_REDUCED
  FILE * file  = fopen("buildReducedGMP_input.txt", "w");
  SBM_write_in_fileForScilab(m, file);
  fclose(file);
#endif
  int curSize = 0;
  //  int *newIndexOfBlockI;
  // int NbCol=pInProblem->size;
  int nbBlockCol = m->blocknumber1;
  unsigned int * newIndexOfCol = (unsigned int*) malloc(nbBlockCol * sizeof(unsigned int));

  /*Me building*/
  int MeRow = 0;
  int MiRow = 0;

  /**size of Me */
  listNumericsProblem * curProblem = 0;
  int nbBlockRowE = 0;
  int nbBlockRowI = 0;
  int numBlockRow = 0;
  curProblem =  pInProblem->firstListElem;
  while(curProblem)
  {
    if(numBlockRow)
      curSize = m->blocksize0[numBlockRow] - m->blocksize0[numBlockRow - 1];
    else
      curSize = m->blocksize0[numBlockRow];

    if(curProblem->type == SICONOS_NUMERICS_PROBLEM_EQUALITY)
    {
      nbBlockRowE++;
      MeRow += curSize;
    }
    else
    {
      nbBlockRowI++;
      MiRow += curSize;
    }
    curProblem = curProblem->nextProblem;
    numBlockRow++;
  }
  numBlockRow = 0;
  int numRowE = 0;
  int numRowI = 0;
  int numRow = 0;
  curProblem =  pInProblem->firstListElem;
  while(curProblem)
  {
    if(curProblem->type == SICONOS_NUMERICS_PROBLEM_EQUALITY)
    {
      newIndexOfCol[numRow] = numRowE;
      numRowE++;
    }
    else
    {
      newIndexOfCol[numRow] = numRowI + nbBlockRowE;
      numRowI++;
    }
    numRow++;
    curProblem = curProblem->nextProblem;
  }
#ifdef GMP_DEBUG_REDUCED
  printf("buildReducedGMP nb of block of eq=%i. nb of iq=%i\n", numRowE, numRowI);
#endif

  /*building of the permutation matrices*/
  SparseBlockStructuredMatrix * Maux = SBM_new();
  SBM_column_permutation(newIndexOfCol, m, Maux);
  SparseBlockStructuredMatrix * Morder = SBM_new();
  SBM_row_permutation(newIndexOfCol, Maux, Morder);
  SBMfree(Maux, 0);
  /*
    get the permutation indices of col (and row).

   */
  curProblem =  pInProblem->firstListElem;




  /**mem alloc for Me and Mi*/
  //int nbCol=MeRow+MiRow;
  *Me_Size = MeRow;
  *Mi_Size = MiRow;
  /*  Me=(double *) malloc(MeRow*nbCol*sizeof(double));
  Qe=(double *) malloc(MeRow*sizeof(double));
  Mi=(double *) malloc(MiRow*nbCol*sizeof(double));
  Qi=(double *) malloc(MiRow*sizeof(double));*/
  /** copi in Me*/
  int curPos = 0;
  for(int numBlockRow = 0; numBlockRow < nbBlockRowE; numBlockRow++)
  {
    SBM_row_to_dense(Morder, numBlockRow, Me, curPos, MeRow);
    curPos = Morder->blocksize1[numBlockRow];
  }
  curPos = 0;
  int firtMiLine = 0;
  if(nbBlockRowI > 0)
    firtMiLine = Morder->blocksize1[nbBlockRowE];

  for(int numBlockRow = nbBlockRowE; numBlockRow < nbBlockRowE + nbBlockRowI; numBlockRow++)
  {
    curPos = Morder->blocksize1[numBlockRow] - firtMiLine;
    SBM_row_to_dense(Morder, numBlockRow, Mi, curPos, MiRow);
  }
  SBMfree(Morder, 0);

  curProblem =  pInProblem->firstListElem;
  int curBlock = 0;
  int curPosIq = 0;
  int curPosEq = 0;
  double *curQ = pInProblem->q;
  double *curQe = Qe;
  double *curQi = Qi;
  curBlock = 0;
  while(curProblem)
  {
    if(curBlock)
    {
      curSize = m->blocksize0[curBlock] - m->blocksize0[curBlock - 1];
    }
    else
    {
      curSize = m->blocksize0[curBlock];
    }

    switch(curProblem->type)
    {
    case SICONOS_NUMERICS_PROBLEM_EQUALITY:
    {
      /** copy the current line block in Me*/
      memcpy(curQe, curQ, curSize * sizeof(double));
      curPosEq += curSize;
      curQe += curSize;
      break;
    }
    case SICONOS_NUMERICS_PROBLEM_LCP:
    case SICONOS_NUMERICS_PROBLEM_FC3D:
    {
      memcpy(curQi, curQ, curSize * sizeof(double));
      curPosIq += curSize;
      curQi += curSize;
      break;
    }
    default:
      printf("GMPReduced  buildReducedGMP: problemType unknown: %d . \n", curProblem->type);
    }
    curProblem = curProblem->nextProblem;
    curQ += curSize;
    curBlock++;
  }
#ifdef GMP_DEBUG_REDUCED
  int nbCol = MeRow + MiRow;
  printf("\\The Me matrix is:\n");
  printf("Me=[ \n");
  for(int i = 0; i < MeRow; i++)
  {
    printf("[");
    for(int j = 0; j < nbCol; j++)
    {
      printf("%e\t  ", Me[i + j * MeRow]);
    }
    printf("];\n");
  }
  printf("];\n");
  printf("Qe= [ \n");
  for(int i = 0; i < MeRow; i++)
    printf("%e\t  ", Qe[i]);
  printf("];\n");
  printf("\\The Mi matrix is:\n");
  printf("Mi=[ \n");
  for(int i = 0; i < MiRow; i++)
  {
    printf("[");
    for(int j = 0; j < nbCol; j++)
    {
      printf("%e\t  ", Mi[i + j * MiRow]);
    }
    printf("];\n");
  }
  printf("];\n");
  printf("Qi= [ \n");
  for(int i = 0; i < MiRow; i++)
    printf("%e\t  ", Qi[i]);
  printf("];\n");
#endif
  free(newIndexOfCol);
  //#endif
}
/*
 * The equalities are assamblate in one block.
 *
 *0=(Me_1 Me_2)(Re Ri)' + Qe
 *Vi=(Mi_1 Mi_2)(Re Ri)' + Qi
 *
 *and GS.
 */
void gmp_reduced_equality_solve(GenericMechanicalProblem* pInProblem, double *reaction, double *velocity, int * info, SolverOptions* options)
{

  SparseBlockStructuredMatrix* m = pInProblem->M->matrix1;
  int nbRow = m->blocksize0[m->blocknumber0 - 1];
  int nbCol = m->blocksize1[m->blocknumber1 - 1];

  int Me_size;
  int Mi_size;
  double * reducedProb = (double *)malloc(nbRow * nbCol * sizeof(double));
  double * Qreduced = (double *)malloc(nbRow * sizeof(double));
  double *Rreduced = (double *) calloc(nbCol, sizeof(double));
  double *Vreduced = (double *) calloc(nbRow, sizeof(double));

  _GMPReducedEquality(pInProblem, reducedProb, Qreduced, &Me_size, &Mi_size);

  if(Me_size == 0)
  {
    gmp_gauss_seidel(pInProblem, reaction, velocity, info, options);
    free(reducedProb);
    free(Qreduced);
    free(Rreduced);
    free(Vreduced);
    return;
  }
  listNumericsProblem * curProblem = 0;
  GenericMechanicalProblem * _pnumerics_GMP = genericMechanicalProblem_new();
  curProblem =  pInProblem->firstListElem;
  if(Me_size)
    gmp_add(_pnumerics_GMP, SICONOS_NUMERICS_PROBLEM_EQUALITY, Me_size);
  unsigned int curPos = 0;
  unsigned int curPosEq = 0;
  unsigned int curPosInq = Me_size;
  while(curProblem)
  {
    unsigned int curSize = curProblem->size;
    switch(curProblem->type)
    {
    case SICONOS_NUMERICS_PROBLEM_EQUALITY:
    {
      memcpy(Vreduced + curPosEq, velocity + curPos, curSize * sizeof(double));
      memcpy(Rreduced + curPosEq, reaction + curPos, curSize * sizeof(double));
      curPosEq += curSize;
      curPos += curSize;
      break;
    }
    case SICONOS_NUMERICS_PROBLEM_LCP:
    {
      memcpy(Vreduced + curPosInq, velocity + curPos, curSize * sizeof(double));
      memcpy(Rreduced + curPosInq, reaction + curPos, curSize * sizeof(double));
      curPosInq += curSize;
      curPos += curSize;
      gmp_add(_pnumerics_GMP, curProblem->type, curProblem->size);
      break;
    }
    case SICONOS_NUMERICS_PROBLEM_FC3D:
    {
      memcpy(Vreduced + curPosInq, velocity + curPos, curSize * sizeof(double));
      memcpy(Rreduced + curPosInq, reaction + curPos, curSize * sizeof(double));
      curPosInq += curSize;
      curPos += curSize;
      FrictionContactProblem* pFC3D = (FrictionContactProblem*)gmp_add(_pnumerics_GMP, curProblem->type, curProblem->size);
      *(pFC3D->mu) = *(((FrictionContactProblem*)curProblem->problem)->mu);
      break;
    }
    default:
      printf("GMPReduced  buildReducedGMP: problemType unknown: %d . \n", curProblem->type);
    }
    curProblem = curProblem->nextProblem;
  }

#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  //  printDenseMatrice("newPrb",file,reducedProb,nbRow,nbCol);
  //  printDenseMatrice("newQ",file,Qreduced,nbRow,1);
#endif
  NumericsMatrix numM;
  NM_null(&numM);
  numM.storageType = 0;
  numM.matrix0 = reducedProb;
  numM.matrix1 = 0;
  numM.size0 = nbRow;
  numM.size1 = nbCol;
  _pnumerics_GMP->M = &numM;
  _pnumerics_GMP->q = Qreduced;
  gmp_gauss_seidel(_pnumerics_GMP, Rreduced, Vreduced, info, options);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  if(*info)
  {
    printf("\nGMPREduced2 failed!\n");
  }
  else
  {
    printf("\nGMPREduced2 succed!\n");
    //    printDenseMatrice("R",file,Rreduced,nbRow,1);
    //    printDenseMatrice("V",file,Vreduced,nbRow,1);
  }
#endif
  if(!*info)
  {
    gmp_reduced_convert_solution(pInProblem, reaction, velocity, Rreduced, Rreduced + Me_size, Vreduced + Me_size);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
    //    printDenseMatrice("R2",file,reaction,nbRow,1);
    //    printDenseMatrice("V2",file,velocity,nbRow,1);
#endif
    double err;
    int tolViolate = gmp_compute_error(pInProblem, reaction, velocity, options->dparam[SICONOS_DPARAM_TOL], options, &err);
    if(tolViolate)
    {
      printf("GMPReduced2, warnning, reduced problem solved, but error of initial probleme violated tol = %e, err= %e\n", options->dparam[SICONOS_DPARAM_TOL], err);
    }
  }


#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  //    fclose(file);
#endif
  free(Rreduced);
  free(Vreduced);
  genericMechanicalProblem_free(_pnumerics_GMP, NUMERICS_GMP_FREE_GMP);
  free(Qreduced);
  free(reducedProb);
}

/*
 * The equalities are eliminated.
 *
 *0=(Me_1 Me_2)(Re Ri)' + Qe
 *Vi=(Mi_1 Mi_2)(Re Ri)' + Qi
 *
 *Re=-Me_1^{-1}(Me_2Ri+Qe)
 *
 *Vi=(Mi_2-Mi_1 Me_1^{-1} Me_2)Ri+Qi-Mi1 Me_1^{-1} Qe
 *
 */
void gmp_reduced_solve(GenericMechanicalProblem* pInProblem, double *reaction, double *velocity, int * info, SolverOptions* options)
{

  SparseBlockStructuredMatrix* m = pInProblem->M->matrix1;
  int nbRow = m->blocksize0[m->blocknumber0 - 1];
  int nbCol = m->blocksize1[m->blocknumber1 - 1];
  double *Me = (double *) malloc(nbRow * nbCol * sizeof(double));
  double *Qe = (double *) malloc(nbRow * sizeof(double));
  double *Mi = (double *) malloc(nbRow * nbCol * sizeof(double));
  double *Qi = (double *) malloc(nbRow * sizeof(double));
  int Me_size;
  int Mi_size;
  buildReducedGMP(pInProblem, Me, Mi, Qe, Qi, &Me_size, &Mi_size);

  if((Me_size == 0 || Mi_size == 0))
  {
    gmp_gauss_seidel(pInProblem, reaction, velocity, info, options);
    free(Me);
    free(Qe);
    free(Mi);
    free(Qi);
    return;
  }

  double * pseduInvMe1 = (double *)malloc(Me_size * Me_size * sizeof(double));
  memcpy(pseduInvMe1, Me, Me_size * Me_size * sizeof(double));
  pinv(pseduInvMe1, Me_size, Me_size, 1e-16);
  double *Mi2 = Mi + Mi_size * Me_size;
  double *Mi1 = Mi;
  double *Me2 = Me + Me_size * Me_size;
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  double *Me1 = Me;
  FILE * file  = fopen("buildReducedGMP_output.txt", "w");
  printf("GMPReducedsolve\n");
  printDenseMatrice("Me1", file, Me1, Me_size, Me_size);
  printDenseMatrice("Me2", file, Me2, Me_size, Mi_size);
  printDenseMatrice("Mi1", file, Mi1, Mi_size, Me_size);
  printDenseMatrice("Mi2", file, Mi2, Mi_size, Mi_size);
  printDenseMatrice("Qe", file, Qe, Me_size, 1);
  printDenseMatrice("Qi", file, Qi, Mi_size, 1);
  printDenseMatrice("Me1inv", file, pseduInvMe1, Me_size, Me_size);
#endif


  double * reducedProb = (double *)malloc(Mi_size * Mi_size * sizeof(double));
  memcpy(reducedProb, Mi2, Mi_size * Mi_size * sizeof(double));

  double * Mi1pseduInvMe1 = (double *)malloc(Mi_size * Me_size * sizeof(double));
  cblas_dgemm(CblasColMajor,CblasNoTrans, CblasNoTrans, Mi_size, Me_size, Me_size, -1.0, Mi1, Mi_size, pseduInvMe1, Me_size, 0.0, Mi1pseduInvMe1, Mi_size);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  printDenseMatrice("minusMi1pseduInvMe1", file, Mi1pseduInvMe1, Mi_size, Me_size);
  fprintf(file, "_minusMi1pseduInvMe1=-Mi1*Me1inv;\n");
#endif
  cblas_dgemv(CblasColMajor,CblasNoTrans, Mi_size, Me_size, 1.0, Mi1pseduInvMe1, Mi_size, Qe, 1, 1.0, Qi, 1);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  printDenseMatrice("newQi", file, Qi, Mi_size, 1);
  fprintf(file, "_newQi=Qi+_minusMi1pseduInvMe1*Qe;\n");
#endif
  cblas_dgemm(CblasColMajor,CblasNoTrans, CblasNoTrans, Mi_size, Mi_size, Me_size, 1.0, Mi1pseduInvMe1, Mi_size, Me2, Me_size, 1.0, reducedProb, Mi_size);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  printDenseMatrice("W", file, reducedProb, Mi_size, Mi_size);
  fprintf(file, "_W=Mi2+_minusMi1pseduInvMe1*Me2;\n");

#endif
  listNumericsProblem * curProblem = 0;
  GenericMechanicalProblem * _pnumerics_GMP = genericMechanicalProblem_new();
  curProblem =  pInProblem->firstListElem;
  while(curProblem)
  {
    switch(curProblem->type)
    {
    case SICONOS_NUMERICS_PROBLEM_EQUALITY:
    {
      break;
    }
    case SICONOS_NUMERICS_PROBLEM_LCP:
    {
      gmp_add(_pnumerics_GMP, curProblem->type, curProblem->size);
      break;
    }
    case SICONOS_NUMERICS_PROBLEM_FC3D:
    {
      FrictionContactProblem* pFC3D = (FrictionContactProblem*)gmp_add(_pnumerics_GMP, curProblem->type, curProblem->size);
      *(pFC3D->mu) = *(((FrictionContactProblem*)curProblem->problem)->mu);
      break;
    }
    default:
      printf("GMPReduced  buildReducedGMP: problemType unknown: %d . \n", curProblem->type);
    }
    curProblem = curProblem->nextProblem;
  }
  NumericsMatrix numM;
  NM_null(&numM);
  numM.storageType = 0;
  numM.matrix0 = reducedProb;
  numM.matrix1 = 0;
  numM.size0 = Mi_size;
  numM.size1 = Mi_size;
  _pnumerics_GMP->M = &numM;
  _pnumerics_GMP->q = Qi;
  double *Rreduced = (double *) malloc(Mi_size * sizeof(double));
  double *Vreduced = (double *) malloc(Mi_size * sizeof(double));
  gmp_gauss_seidel(_pnumerics_GMP, Rreduced, Vreduced, info, options);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  if(*info)
  {
    printf("\nGMPREduced failed!\n");
  }
  else
  {
    printf("\nGMPREduced succed!\n");
    printDenseMatrice("Ri", file, Rreduced, Mi_size, 1);
    printDenseMatrice("Vi", file, Vreduced, Mi_size, 1);
  }
#endif
  if(!*info)
  {
    /*Re computation*/
    double * Re = (double*)malloc(Me_size * sizeof(double));
    double * Rbuf = (double*)malloc(Me_size * sizeof(double));
    memcpy(Rbuf, Qe, Me_size * sizeof(double));
    cblas_dgemv(CblasColMajor,CblasNoTrans, Me_size, Mi_size, 1.0, Me2, Me_size, Rreduced, 1, 1.0, Rbuf, 1);
    cblas_dgemv(CblasColMajor,CblasNoTrans, Me_size, Me_size, -1.0, pseduInvMe1, Me_size, Rbuf, 1, 0.0, Re, 1);
#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
    fprintf(file, "_Re=-Me1inv*(Me2*Ri+Qe);\n");
    printDenseMatrice("Re", file, Re, Me_size, 1);
#endif
    gmp_reduced_convert_solution(pInProblem, reaction, velocity, Re, Rreduced, Vreduced);
    double err;
    int tolViolate = gmp_compute_error(pInProblem, reaction, velocity, options->dparam[SICONOS_DPARAM_TOL], options, &err);
    if(tolViolate)
    {
      printf("GMPReduced, warnning, reduced problem solved, but error of initial probleme violated tol = %e, err= %e\n", options->dparam[SICONOS_DPARAM_TOL], err);
    }
    free(Re);
    free(Rbuf);
  }

#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  fclose(file);
#endif
  free(Rreduced);
  free(Vreduced);
  genericMechanicalProblem_free(_pnumerics_GMP, NUMERICS_GMP_FREE_GMP);
  free(Me);
  free(Mi);
  free(Qe);
  free(Qi);
  free(pseduInvMe1);
  free(reducedProb);
  free(Mi1pseduInvMe1);
  //  GenericMechanicalProblem GMPOutProblem;
  //  SparseBlockStructuredMatrix mOut;

}

void _GMPReducedEquality(GenericMechanicalProblem* pInProblem, double * reducedProb, double * Qreduced, int * Me_size, int* Mi_size)
{

  SparseBlockStructuredMatrix* m = pInProblem->M->matrix1;
  int nbRow = m->blocksize0[m->blocknumber0 - 1];
  int nbCol = m->blocksize1[m->blocknumber1 - 1];

  _GMPReducedGetSizes(pInProblem, Me_size, Mi_size);
  if(*Me_size == 0)
  {
    memcpy(Qreduced, pInProblem->q, (*Mi_size)*sizeof(double));
    SBM_to_dense(m, reducedProb);
    return;
  }

  double *Me = (*Me_size) ? (double *) malloc((*Me_size) * nbCol * sizeof(double)) : 0;
  double *Mi = (*Mi_size) ? (double *) malloc((*Mi_size) * nbCol * sizeof(double)) : 0;
  double *Qi = (double *) malloc(nbRow * sizeof(double));
  buildReducedGMP(pInProblem, Me, Mi, Qreduced, Qi, Me_size, Mi_size);

#ifdef GMP_DEBUG_GMPREDUCED_SOLVE
  double *Me1 = Me;
  double *Me2 = Me + (*Me_size) * (*Me_size);
  double *Mi1 = Mi;
  double *Mi2 = Mi + (*Mi_size) * (*Me_size);
  FILE * file  = fopen("buildReduced2GMP_output.txt", "w");
  printf("GMP2Reducedsolve\n");
  printDenseMatrice("Me1", file, Me1, *Me_size, *Me_size);
  printDenseMatrice("Me2", file, Me2, *Me_size, *Mi_size);
  printDenseMatrice("Mi1", file, Mi1, *Mi_size, *Me_size);
  printDenseMatrice("Mi2", file, Mi2, *Mi_size, *Mi_size);
  printDenseMatrice("Qe", file, Qreduced, *Me_size, 1);
  printDenseMatrice("Qi", file, Qi, *Mi_size, 1);
  fclose(file);
#endif
  for(int numCol = 0; numCol < nbCol; numCol++)
  {
    if(*Me_size)
      memcpy(reducedProb + numCol * nbRow, Me + numCol * (*Me_size), (*Me_size)*sizeof(double));
    if(*Mi_size)
      memcpy(reducedProb + numCol * nbRow + (*Me_size), Mi + numCol * (*Mi_size), (*Mi_size)*sizeof(double));
  }
  if(*Mi_size)
    memcpy(Qreduced + (*Me_size), Qi, (*Mi_size)*sizeof(double));
  free(Me);
  free(Mi);
  free(Qi);
}

void gmp_as_mlcp(GenericMechanicalProblem* pInProblem, double *reaction, double *velocity, int* info, SolverOptions* options)
{

  /*First, we don't manage FC3D.*/
  listNumericsProblem * curProblem = 0;
  curProblem =  pInProblem->firstListElem;
  while(curProblem)
  {
    switch(curProblem->type)
    {
    case SICONOS_NUMERICS_PROBLEM_EQUALITY:
    case SICONOS_NUMERICS_PROBLEM_LCP:
      break;
    case SICONOS_NUMERICS_PROBLEM_FC3D:
    {
      printf("gmp_as_mlcp Numerics ERROR: gmp_as_mlcp doesn't deal with FC3D.\n");
      *info = 1;
      return;
    }
    default:
      printf("gmp_as_mlcp Numerics : gmp_gauss_seidel unknown problem type %d.\n", curProblem->type);
    }
    curProblem = curProblem->nextProblem;
  }
  int Me_size;
  int Mi_size;

  SparseBlockStructuredMatrix* m = pInProblem->M->matrix1;
  int nbRow = m->blocksize0[m->blocknumber0 - 1];
  int nbCol = m->blocksize1[m->blocknumber1 - 1];

  double * reducedProb = (double *)malloc(nbRow * nbCol * sizeof(double));
  double * Qreduced = (double *)malloc(nbRow * sizeof(double));
  _GMPReducedEquality(pInProblem, reducedProb, Qreduced, &Me_size, &Mi_size);

  if(!Me_size)
  {
    /*it is a lcp.*/
    LinearComplementarityProblem aLCP;
    SolverOptions *aLcpOptions = solver_options_create(SICONOS_LCP_ENUM);
    NumericsMatrix M;
    NM_null(&M);
    M.storageType = 0;
    M.size0 = Mi_size;
    M.size1 = Mi_size;
    M.matrix0 = reducedProb;
    M.matrix1 = 0;
    aLCP.size = Mi_size;
    aLCP.q = Qreduced;
    aLCP.M = &M;
    lcp_enum_init(&aLCP, aLcpOptions, 1);
    *info = linearComplementarity_driver(&aLCP, reaction, velocity, aLcpOptions);
    lcp_enum_reset(&aLCP, aLcpOptions, 1);
    solver_options_delete(aLcpOptions);
    aLcpOptions = NULL;

    goto END_GMP3;
  }
  if(!Mi_size)
  {
    /*it is a linear system.*/
    assert(Me_size >= 0);
    for(size_t i = 0; i < (size_t)Me_size; ++i) reaction[i] = -Qreduced[i];
    NumericsMatrix M;
    NM_null(&M);
    NM_fill(&M, NM_DENSE, Me_size, Me_size, reducedProb);
    // *info = NM_gesv(&M, reaction, true);
    *info = NM_LU_solve(&M, reaction, 1);
    M.matrix0 = NULL;
    NM_clear(&M);
    goto END_GMP3;
  }
  /*it is a MLCP*/
  MixedLinearComplementarityProblem aMLCP;
  SolverOptions * aMlcpOptions = solver_options_create(SICONOS_MLCP_ENUM);
  aMLCP.n = Me_size;
  aMLCP.m = Mi_size;
  aMLCP.blocksRows = 0;
  aMLCP.blocksIsComp = 0;
  aMLCP.isStorageType1 = 1;
  aMLCP.isStorageType2 = 0;

  aMLCP.A = 0;
  aMLCP.B = 0;
  aMLCP.C = 0;
  aMLCP.D = 0;
  aMLCP.a = 0;
  aMLCP.b = 0;
  aMLCP.q = Qreduced;
  NumericsMatrix M;
  NM_null(&M);
  M.storageType = 0;
  M.size0 = Mi_size + Me_size;
  M.size1 = Mi_size + Me_size;
  M.matrix0 = reducedProb;
  M.matrix1 = 0;
  aMLCP.M = &M;
  mlcp_driver_init(&aMLCP, aMlcpOptions);
  aMlcpOptions->dparam[SICONOS_DPARAM_TOL] = options->dparam[SICONOS_DPARAM_TOL];
  *info = mlcp_driver(&aMLCP, reaction, velocity, aMlcpOptions);

  mlcp_driver_reset(&aMLCP, aMlcpOptions);
  solver_options_delete(aMlcpOptions);
  aMlcpOptions = NULL;
END_GMP3:
  ;

  free(reducedProb);
  free(Qreduced);

}
