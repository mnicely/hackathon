#include "../headers/structs.h"

#include <assert.h>
#include <string.h>

#include <cuComplex.h>
#include <cuda_runtime.h>
#include <cufft.h>

#include "cuda_helper.h"
#include "cuda_kernels.h"

// #define USE_CUFFTW 1

#ifdef USE_CUFFTW

void DST1( const cudaStream_t *streams,
           const int           l,
           const int           Nx,
           const int           Ny,
           double complex * rhs,
           cuDoubleComplex *rhat,
           cufftHandle      p1,
           double *         in1,
           cuDoubleComplex *out1,
           cufftHandle      p2,
           double *         in2,
           cuDoubleComplex *out2 );

void DST2( const cudaStream_t *streams,
           const int           l,
           const int           Nx,
           const int           Ny,
           const int           Nz,
           double complex * xhat,
           cuDoubleComplex *sol,
           cufftHandle      p1,
           double *         in1,
           cuDoubleComplex *out1,
           cufftHandle      p2,
           double *         in2,
           cuDoubleComplex *out2 );

void DST( int              Nx,
          int              Ny,
          double *         b_2D,
          double *         bhat,
          cufftHandle      p1,
          double *         in1,
          cuDoubleComplex *out1,
          cufftHandle      p2,
          double *         in2,
          cuDoubleComplex *out2 );

void solver( System sys ) {

    PUSH_RANGE( "solver", 0 )

    PUSH_RANGE( "stream creation", 0 )
    int          num_streams = 5;
    cudaStream_t streams[num_streams];
    for ( int i = 0; i < num_streams; i++ ) {
        CUDA_RT_CALL( cudaStreamCreateWithFlags( &streams[i], cudaStreamNonBlocking ) );
    }
    POP_RANGE

    PUSH_RANGE( "stream creation", 0 )
    int         num_events = 5;
    cudaEvent_t events[num_events];
    for ( int i = 0; i < num_events; i++ ) {
        CUDA_RT_CALL( cudaEventCreateWithFlags( &events[i], cudaEventDisableTiming ) );
    }
    POP_RANGE

    PUSH_RANGE( "setup", 1 )

    int    Nx = sys.lat.Nx, Ny = sys.lat.Ny, Nz = sys.lat.Nz;
    int    Nxy, Nxz, Nxyz;
    double complex *rhat, *y_a, *xhat;
    /*
     Allocate the arrays for the 2D layer of the rhs for z = const
     and for the transform of that layer. And the 3D array of the
     transformed rhs.
     */
    Nxy  = Nx * Ny;
    Nxz  = Nx * Nz;
    Nxyz = Nx * Ny * Nz;

    CUDA_RT_CALL( cudaMalloc( ( void ** )( &rhat ), sizeof( double complex ) * Nxyz ) );
    CUDA_RT_CALL( cudaMalloc( ( void ** )( &y_a ), sizeof( double complex ) * Nxyz ) );
    CUDA_RT_CALL( cudaMalloc( ( void ** )( &xhat ), sizeof( double complex ) * Nxyz ) );

    /*
     FFT transformation of the rhs layer by layer (z = const).
     and store the transformed vectors in the rhat in the same order as rhs.
     */
    int              i, j, l;
    int              m, NR, NC;
    double *         in1, *in2;
    cuDoubleComplex *out1, *out2;

    cufftHandle p1, p2;
    size_t      workspace;

    m                  = Ny;
    NR                 = 2 * Nx + 2;
    NC                 = NR / 2 + 1;
    const int rank1    = 1;
    const int howmany1 = m;
    int       nr1[]    = { NR };
    const int istride1 = 1;
    const int ostride1 = 1;
    const int idist1   = NR;
    const int odist1   = NC;
    int *     inembed1 = NULL, *onembed1 = NULL;

    size_t size_in1  = ( sizeof( double ) * NR * m ) * 2;
    size_t size_out1 = ( sizeof( cuDoubleComplex ) * NC * m ) * 2;

    PUSH_RANGE( "Create p1", 6 )
    CUDA_RT_CALL( cudaMalloc( ( void ** )&in1, size_in1 ) );
    CUDA_RT_CALL( cudaMalloc( ( void ** )&out1, size_out1 ) );

    CUDA_RT_CALL( cufftCreate( &p1 ) );
    CUDA_RT_CALL( cufftSetStream( p1, streams[0] ) );
    CUDA_RT_CALL( cufftMakePlanMany( p1,
                                     rank1,
                                     nr1,
                                     inembed1,
                                     istride1,
                                     idist1,
                                     onembed1,
                                     ostride1,
                                     odist1,
                                     CUFFT_D2Z,
                                     ( howmany1 * 2 ),
                                     &workspace ) );

    CUDA_RT_CALL( cudaMemsetAsync( in1, size_in1, 0, streams[0] ) );
    CUDA_RT_CALL( cudaMemsetAsync( out1, size_out1, 0, streams[0] ) );
    POP_RANGE

    m                  = Nx;
    NR                 = 2 * Ny + 2;
    NC                 = NR / 2 + 1;
    const int rank2    = 1;
    const int howmany2 = m;
    int       nr2[]    = { NR };
    const int istride2 = 1;
    const int ostride2 = 1;
    const int idist2   = NR;
    const int odist2   = NC;
    int *     inembed2 = NULL, *onembed2 = NULL;

    size_t size_in2  = ( sizeof( double ) * NR * m ) * 2;
    size_t size_out2 = ( sizeof( cuDoubleComplex ) * NC * m ) * 2;

    PUSH_RANGE( "Create p2", 7 )
    CUDA_RT_CALL( cudaMalloc( ( void ** )( &in2 ), size_in2 ) );
    CUDA_RT_CALL( cudaMalloc( ( void ** )( &out2 ), size_out2 ) );

    CUDA_RT_CALL( cufftCreate( &p2 ) );
    CUDA_RT_CALL( cufftSetStream( p2, streams[0] ) );
    CUDA_RT_CALL( cufftMakePlanMany( p2,
                                     rank2,
                                     nr2,
                                     inembed2,
                                     istride2,
                                     idist2,
                                     onembed2,
                                     ostride2,
                                     odist2,
                                     CUFFT_D2Z,
                                     ( howmany2 * 2 ),
                                     &workspace ) );

    CUDA_RT_CALL( cudaMemsetAsync( in2, size_in2, 0, streams[0] ) );
    CUDA_RT_CALL( cudaMemsetAsync( out2, size_out2, 0, streams[0] ) );
    POP_RANGE

    CUDA_RT_CALL( cudaMemPrefetchAsync( sys.rhs, Nxyz * sizeof( double _Complex ), 0, streams[1] ) );
    CUDA_RT_CALL( cudaEventRecord( events[0], streams[1] ) );

    CUDA_RT_CALL( cudaMemPrefetchAsync( sys.U, Nxyz * sizeof( double _Complex ), 0, streams[2] ) );
    CUDA_RT_CALL( cudaMemPrefetchAsync( sys.L, Nxyz * sizeof( double _Complex ), 0, streams[2] ) );
    CUDA_RT_CALL( cudaMemPrefetchAsync( sys.Up, Nxyz * sizeof( double _Complex ), 0, streams[2] ) );
    CUDA_RT_CALL( cudaEventRecord( events[1], streams[2] ) );

    CUDA_RT_CALL( cudaMemPrefetchAsync( sys.sol, Nxyz * sizeof( double _Complex ), 0, streams[3] ) );
    CUDA_RT_CALL( cudaEventRecord( events[2], streams[3] ) );

    POP_RANGE

    PUSH_RANGE( "DST1", 2 )
    CUDA_RT_CALL( cudaStreamWaitEvent( streams[0], events[0], cudaEventWaitDefault ) );  // Wait for sys.rhs
    for ( l = 0; l < Nz; l++ ) {
        DST1( streams, l, Nx, Ny, sys.rhs, rhat, p1, in1, out1, p2, in2, out2 );
    }
    CUDA_RT_CALL( cudaStreamSynchronize( streams[0] ) );
    POP_RANGE

    /*
     Solution of the transformed system.
     */

    PUSH_RANGE( "trig", 3 )
    CUDA_RT_CALL( cudaStreamWaitEvent( streams[0], events[1], cudaEventWaitDefault ) );  // Wait forsys.U, sys.L, sys.Up
    triangular_solver_wrapper( streams[0], sys, Nx, Ny, Nz, rhat, xhat, y_a );
    CUDA_RT_CALL( cudaStreamSynchronize( streams[0] ) );
    POP_RANGE

    /*
     FFT transformation of the solution xhat layer by layer (z = const).
     and store the transformed vectors in the sol in the same order as xhat.
     */

    PUSH_RANGE( "DST2", 4 )
    CUDA_RT_CALL( cudaStreamWaitEvent( streams[0], events[2], cudaEventWaitDefault ) );  // Wait for sys.sol
    for ( l = 0; l < Nz; l++ ) {
        DST2( streams, l, Nx, Ny, Nz, xhat, sys.sol, p1, in1, out1, p2, in2, out2 );
    }
    CUDA_RT_CALL( cudaStreamSynchronize( streams[0] ) );
    POP_RANGE

    CUDA_RT_CALL( cudaStreamSynchronize( streams[0] ) );  // Wait for DST2

    CUDA_RT_CALL( cudaMemPrefetchAsync( sys.sol, Nxyz * sizeof( double _Complex ), cudaCpuDeviceId, NULL ) );

    PUSH_RANGE( "Cleanup", 5 )
    for ( int i = 0; i < num_streams; i++ ) {
        CUDA_RT_CALL( cudaStreamDestroy( streams[i] ) );
    }

    for ( int i = 0; i < num_events; i++ ) {
        CUDA_RT_CALL( cudaEventDestroy( events[i] ) );
    }
    cufftDestroy( p1 );
    cudaFree( in1 );
    cudaFree( out1 );
    cufftDestroy( p2 );
    cudaFree( in2 );
    cudaFree( out2 );

    cudaFree( rhat );
    rhat = NULL;
    cudaFree( y_a );
    y_a = NULL;
    cudaFree( xhat );
    xhat = NULL;
    POP_RANGE

    POP_RANGE

    return;
}
#else

void DST( int           Nx,
          int           Ny,
          double *      b_2D,
          double *      bhat,
          fftw_plan     p1,
          double *      in1,
          fftw_complex *out1,
          fftw_plan     p2,
          double *      in2,
          fftw_complex *out2 );

void solver( System sys ) {
    int    Nx = sys.lat.Nx, Ny = sys.lat.Ny, Nz = sys.lat.Nz;
    int    Nxy, Nxz, Nxyz;
    double complex *rhat, *y_a, *xhat;
    /*
     Allocate the arrays for the 2D layer of the rhs for z = const
     and for the transform of that layer. And the 3D array of the
     transformed rhs.
     */
    Nxy  = Nx * Ny;
    Nxz  = Nx * Nz;
    Nxyz = Nx * Ny * Nz;

    rhat = malloc( Nxyz * sizeof( double complex ) );
    y_a  = malloc( Nz * sizeof( double complex ) );
    xhat = malloc( Nxyz * sizeof( double complex ) );
    /*
     FFT transformation of the rhs layer by layer (z = const).
     and store the transformed vectors in the rhat in the same order as rhs.
     */
    int           i, j, l;
    int           m, NR, NC;
    double *      in1, *in2;
    fftw_complex *out1, *out2;
    fftw_plan     p1, p2;

    m                  = Ny;
    NR                 = 2 * Nx + 2;
    NC                 = NR / 2 + 1;
    const int rank1    = 1;
    const int howmany1 = m;
    int       nr1[]    = { NR };
    const int istride1 = 1;
    const int ostride1 = 1;
    const int idist1   = NR;
    const int odist1   = NC;
    int *     inembed1 = NULL, *onembed1 = NULL;

    in1  = ( double * )fftw_malloc( sizeof( double ) * NR * m );
    out1 = ( fftw_complex * )fftw_malloc( sizeof( fftw_complex ) * NC * m );

    p1 = fftw_plan_many_dft_r2c(
        rank1, nr1, howmany1, in1, inembed1, istride1, idist1, out1, onembed1, ostride1, odist1, FFTW_ESTIMATE );

    for ( j = 0; j < NR * m; j++ ) {
        in1[j] = 0.0;
    }

    m                  = Nx;
    NR                 = 2 * Ny + 2;
    NC                 = NR / 2 + 1;
    const int rank2    = 1;
    const int howmany2 = m;
    int       nr2[]    = { NR };
    const int istride2 = 1;
    const int ostride2 = 1;
    const int idist2   = NR;
    const int odist2   = NC;
    int *     inembed2 = NULL, *onembed2 = NULL;

    in2  = ( double * )fftw_malloc( sizeof( double ) * NR * m );
    out2 = ( fftw_complex * )fftw_malloc( sizeof( fftw_complex ) * NC * m );

    p2 = fftw_plan_many_dft_r2c(
        rank2, nr2, howmany2, in2, inembed2, istride2, idist2, out2, onembed2, ostride2, odist2, FFTW_ESTIMATE );

    for ( j = 0; j < NR * m; j++ ) {
        in2[j] = 0.0;
    }

    double *b_2D_r = malloc( Nxy * sizeof( double ) );
    double *b_2D_i = malloc( Nxy * sizeof( double ) );
    double *bhat_r = malloc( Nxy * sizeof( double ) );
    double *bhat_i = malloc( Nxy * sizeof( double ) );

    for ( l = 0; l < Nz; l++ ) {
        for ( i = 0; i < Nxy; i++ ) {
            b_2D_r[i] = creal( sys.rhs[i + l * Nxy] );
            b_2D_i[i] = cimag( sys.rhs[i + l * Nxy] );
        }

        DST( Nx, Ny, b_2D_r, bhat_r, p1, in1, out1, p2, in2, out2 );
        DST( Nx, Ny, b_2D_i, bhat_i, p1, in1, out1, p2, in2, out2 );

        for ( i = 0; i < Nxy; i++ ) {
            rhat[i + l * Nxy] = bhat_r[i] + I * bhat_i[i];
        }
    }

    /*
     Solution of the transformed system.
     */

    for ( j = 0; j < Ny; j++ ) {
        for ( i = 0; i < Nx; i++ ) {
            y_a[0] = rhat[i + j * Nx];
            for ( l = 1; l < Nz; l++ ) {
                y_a[l] = rhat[i + j * Nx + l * Nxy] - sys.L[l + i * Nz + j * Nxz] * y_a[l - 1];
            }
            xhat[Nz - 1 + i * Nz + j * Nxz] = y_a[Nz - 1] * sys.U[Nz - 1 + i * Nz + j * Nxz];
            for ( l = Nz - 2; l >= 0; l-- ) {
                xhat[l + i * Nz + j * Nxz] =
                    ( y_a[l] - sys.Up[l + i * Nz + j * Nxz] * xhat[l + 1 + i * Nz + j * Nxz] ) *
                    sys.U[l + i * Nz + j * Nxz];
            }
        }
    }

    /*
     FFT transformation of the solution xhat layer by layer (z = const).
     and store the transformed vectors in the sol in the same order as xhat.
     */

    for ( l = 0; l < Nz; l++ ) {
        for ( i = 0; i < Nxy; i++ ) {
            b_2D_r[i] = creal( xhat[l + i * Nz] );
            b_2D_i[i] = cimag( xhat[l + i * Nz] );
        }

        DST( Nx, Ny, b_2D_r, bhat_r, p1, in1, out1, p2, in2, out2 );
        DST( Nx, Ny, b_2D_i, bhat_i, p1, in1, out1, p2, in2, out2 );

        for ( i = 0; i < Nxy; i++ ) {
            sys.sol[i + l * Nxy] = bhat_r[i] + I * bhat_i[i];
        }
    }

    fftw_destroy_plan( p1 );
    fftw_free( in1 );
    fftw_free( out1 );
    fftw_destroy_plan( p2 );
    fftw_free( in2 );
    fftw_free( out2 );
    free( b_2D_r );
    b_2D_r = NULL;
    free( b_2D_i );
    b_2D_i = NULL;
    free( bhat_r );
    bhat_r = NULL;
    free( bhat_i );
    bhat_i = NULL;

    free( rhat );
    rhat = NULL;
    free( y_a );
    y_a = NULL;
    free( xhat );
    xhat = NULL;

    return;
}

#endif
