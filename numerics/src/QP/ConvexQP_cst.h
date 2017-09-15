#ifndef CONVEXQP_CST_H
#define CONVEXQP_CST_H
/** \file ConvexQP_cst.h */


/** \enum CONVEXQP_SOLVER encode the list of solvers as integers, to avoid mispelling
 * with const char* const  variables
 */
enum CONVEXQP_SOLVER
{
  SICONOS_CONVEXQP_PG = 1200
};

extern const char* const   SICONOS_CONVEXQP_PGoC_STR;

enum SICONOS_CONVEXQP_PGOC_IPARAM
{
  /** index in iparam to store the maximum number of iterations */
  SICONOS_CONVEXQP_PGOC_LINESEARCH_MAXITER = 10
};
enum SICONOS_CONVEXQP_PGOC_DPARAM
{
  /** index in dparam to store the rho value for projection formulation */
  SICONOS_CONVEXQP_PGOC_RHO = 3,
  /** index in dparam to store the minrho value for projection formulation */
  SICONOS_CONVEXQP_PGOC_RHOMIN = 4,
  /** index in dparam to store the mu value for line search algo */
  SICONOS_CONVEXQP_PGOC_LINESEARCH_MU = 5,
  /** index in dparam to store the tau value for line search algo */
  SICONOS_CONVEXQP_PGOC_LINESEARCH_TAU  = 6 
};




#endif