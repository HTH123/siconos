# Copyright 2022 INRIA

import numpy as np

import siconos.numerics as sn


def vi_function_1D(n, x, F):
    F[0] = 1.0 + x[0]
    pass


def vi_nabla_function_1D(n, x, nabla_F):
    nabla_F[0] = 1.0
    pass


def vi_function_2D(n, z, F):
    M = np.array([[2.0, 1.0], [1.0, 2.0]])

    q = np.array([-5.0, -6.0])
    F[:] = np.dot(M, z) + q
    pass


def vi_nabla_function_2D(n, z, nabla_F):
    M = np.array([[2.0, 1.0], [1.0, 2.0]])
    nabla_F[:] = M
    pass


def vi_function_3D(n, z, F):
    M = np.array(((0.0, -1.0, 2.0), (2.0, 0.0, -2.0), (-1.0, 1.0, 0.0)))

    q = np.array((-3.0, 6.0, -1))
    F[:] = np.dot(M, z) + q
    pass


def vi_nabla_function_3D(n, z, nabla_F):
    M = np.array(((0.0, -1.0, 2.0), (2.0, 0.0, -2.0), (-1.0, 1.0, 0.0)))
    nabla_F[:] = M
    pass


# solution
xsol_1D = np.array([-1.0])
Fsol_1D = np.array([0.0])

xsol_2D = np.array([1.0, 1.0])
Fsol_2D = np.array([-2.0, -3.0])

xsol_3D = np.array((-1.0, -1.0, 1.0))
Fsol_3D = np.array((0.0, 2.0, -1.0))
# problem
# vi=N.MCP(1,1,vi_function,vi_Nablafunction)

xtol = 1e-8


def test_new():
    return sn.VI(1)


def test_vi_1D():
    vi = sn.VI(1, vi_function_1D)
    vi.set_compute_nabla_F(vi_nabla_function_1D)
    x = np.array([0.0])
    F = np.array([0.0])

    SO = sn.SolverOptions(sn.SICONOS_VI_BOX_QI)
    lb = np.array((-1.0,))
    ub = np.array((1.0,))
    vi.set_box_constraints(lb, ub)
    info = sn.variationalInequality_box_newton_QiLSA(vi, x, F, SO)
    print(info)
    print("x = ", x)
    print("F = ", F)
    assert np.linalg.norm(x - xsol_1D) <= xtol
    assert not info


def test_vi_2D():
    vi = sn.VI(2, vi_function_2D)
    vi.set_compute_nabla_F(vi_nabla_function_2D)
    x = np.array((0.0, 0.0))
    F = np.array((0.0, 0.0))

    SO = sn.SolverOptions(sn.SICONOS_VI_BOX_QI)
    lb = np.array((-1.0, -1.0))
    ub = np.array((1.0, 1.0))
    vi.set_box_constraints(lb, ub)
    info = sn.variationalInequality_box_newton_QiLSA(vi, x, F, SO)
    print(info)
    print(
        "number of iteration {:} ; precision {:}".format(
            SO.iparam[sn.SICONOS_IPARAM_ITER_DONE], SO.dparam[sn.SICONOS_DPARAM_RESIDU]
        )
    )
    print("x = ", x)
    print("F = ", F)
    assert np.linalg.norm(x - xsol_2D) <= xtol
    assert not info


def test_vi_3D():
    vi = sn.VI(3, vi_function_3D)
    x = np.zeros((3,))
    F = np.zeros((3,))

    SO = sn.SolverOptions(sn.SICONOS_VI_BOX_QI)
    vi.set_compute_nabla_F(vi_nabla_function_3D)
    lb = np.array((-1.0, -1.0, -1.0))
    ub = np.array((1.0, 1.0, 1.0))
    vi.set_box_constraints(lb, ub)
    info = sn.variationalInequality_box_newton_QiLSA(vi, x, F, SO)
    print(info)
    print(
        "number of iteration {:} ; precision {:}".format(
            SO.iparam[sn.SICONOS_IPARAM_ITER_DONE], SO.dparam[sn.SICONOS_DPARAM_RESIDU]
        )
    )
    print("x = ", x)
    print("F = ", F)
    assert np.linalg.norm(x - xsol_3D) <= xtol
    assert not info
    assert np.abs(SO.dparam[sn.SICONOS_DPARAM_RESIDU]) < 1e-10


def test_vi_C_interface():
    try:
        from cffi import FFI

        cffi_is_present = True
    except ImportError:
        cffi_is_present = False
        return

    if cffi_is_present:
        h = 1e-5
        T = 1.0
        t = 0.0

        theta = 1.0
        gamma = 1.0
        g = 9.81
        kappa = 0.4

        xk = np.array((1.0, 10.0))
        ffi = FFI()
        ffi.cdef("void set_cstruct(uintptr_t p_env, void* p_struct);")
        ffi.cdef(
            """typedef struct
                 {
                 int id;
                 double* xk;
                 double h;
                 double theta;
                 double gamma;
                 double g;
                 double kappa;
                 unsigned int f_eval;
                 unsigned int nabla_eval;
                  } data;
                 """
        )

        data_struct = ffi.new("data*")
        data_struct.id = -1  # to avoid freeing the data in the destructor
        data_struct.xk = ffi.cast("double *", xk.ctypes.data)
        data_struct.h = h
        data_struct.theta = theta
        data_struct.gamma = gamma
        data_struct.g = g
        data_struct.kappa = kappa

        vi = sn.VI(2)
        import siconos

        D = ffi.dlopen(siconos.__path__[0] + "/_pynumerics.so")
        D.set_cstruct(vi.get_env_as_long(), ffi.cast("void*", data_struct))
        vi.set_compute_F_and_nabla_F_as_C_functions(
            "ZhuravlevIvanov.so", "compute_F", "compute_nabla_F"
        )

        lambda_ = np.zeros((2,))
        xkp1 = np.zeros((2,))

        SO = sn.SolverOptions(sn.SICONOS_VI_BOX_QI)
        lb = np.array((-1.0, -1.0))
        ub = np.array((1.0, 1.0))
        vi.set_box_constraints(lb, ub)

        N = int(T / h + 10)
        print(N)
        SO.dparam[sn.SICONOS_DPARAM_TOL] = 1e-24
        SO.iparam[sn.SICONOS_IPARAM_MAX_ITER] = 100
        SO.iparam[sn.SICONOS_VI_IPARAM_ACTIVATE_UPDATE] = 1
        SO.iparam[sn.SICONOS_VI_IPARAM_DECREASE_RHO] = 0
        SO.iparam[4] = 5  # ???

        signs = np.empty((N, 2))
        sol = np.empty((N, 2))
        sol[0, :] = xk

        k = 0
        # sn.numerics_set_verbose(3)

        while t <= T:
            k += 1
            info = sn.variationalInequality_box_newton_QiLSA(vi, lambda_, xkp1, SO)
            if info > 0:
                print(lambda_)
                #            vi_function(2, signs[k-1, :], xkp1)
                lambda_[0] = -np.sign(xkp1[0])
                lambda_[1] = -np.sign(xkp1[1])
                if np.abs(xk[0]) < 1e-10:
                    lambda_[0] = 0.01
                if np.abs(xk[1]) < 1e-10:
                    lambda_[1] = 0.01
                    print("ok lambda")
                    print(lambda_)
                info = sn.variationalInequality_box_newton_QiLSA(vi, lambda_, xkp1, SO)
                print(
                    "iter {:} ; solver iter = {:} ; prec = {:}".format(
                        k,
                        SO.iparam[sn.SICONOS_IPARAM_ITER_DONE],
                        SO.dparam[sn.SICONOS_DPARAM_RESIDU],
                    )
                )
                if info > 0:
                    print("VI solver failed ! info = {:}".format(info))
                    print(xk)
                    print(lambda_)
                    print(xkp1)
                    kaboom()
            sol[k, 0:2] = xkp1
            np.copyto(xk, xkp1, casting="no")
            signs[k, 0:2] = lambda_
            t = k * h
            # z[:] = 0.0
