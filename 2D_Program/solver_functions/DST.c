#include "../headers/structs.h"

#include "cuda_helper.h"
#include "cuda_kernels.h"
#include <cuComplex.h>
#include <cufftw.h>

#define USE_BATCHED 1
#define USE_CUFFTW 1

void forwardDST( System           sys,
                 DSTN             dst,
                 cuDoubleComplex *d_rhs,
                 cuDoubleComplex *d_rhat,
                 fftw_plan        plan,
                 double *         in,
                 fftw_complex *   out,
                 fftw_plan        plan2,
                 double *         in2,
                 fftw_complex *   out2 ) {

#if USE_BATCHED
#pragma omp critical( fftw_execute )
    {
        load_1st_DST_wrapper( sys, dst, d_rhs, in, in2 );

        fftw_execute( plan );  /********************* FFTW *********************/
        fftw_execute( plan2 ); /********************* FFTW *********************/

        store_1st_DST_wrapper( sys, dst, out, out2, d_rhat );
    }
#else

#pragma omp for
    for ( j = 0; j < Ny; j++ ) {
        my = j * Nx;

        for ( i = 0; i < dst.Nx; i++ ) {
            in[i + 1] = creal( rhs[i + my] );
        }
        for ( i = 0; i < dst.Nx; i++ ) {
            in2[i + 1] = cimag( rhs[i + my] );
        }

        fftw_execute( plan );  /********************* FFTW *********************/
        fftw_execute( plan2 ); /********************* FFTW *********************/

        for ( i = 0; i < dst.Nx; i++ ) {
            rhat[i + my] = dst.coef * ( -cimag( out[i + 1] ) - I * cimag( out2[i + 1] ) );
        }
    }

#endif
}

void reverseDST( System           sys,
                 DSTN             dst,
                 cuDoubleComplex *d_xhat,
                 cuDoubleComplex *d_sol,
                 fftw_plan        plan,
                 double *         in,
                 fftw_complex *   out,
                 fftw_plan        plan2,
                 double *         in2,
                 fftw_complex *   out2 ) {

#if USE_BATCHED
#pragma omp critical( fftw_execute )
    {
        load_2st_DST_wrapper( sys, dst, d_xhat, in, in2 );

        fftw_execute( plan );  /********************* FFTW *********************/
        fftw_execute( plan2 ); /********************* FFTW *********************/

        store_2st_DST_wrapper( sys, dst, out, out2, d_sol );
    }
#else

#pragma omp for
    for ( j = 0; j < Ny; j++ ) {
        my = j * Nx;

        for ( i = 0; i < dst.Nx; i++ ) {
            in[i + 1] = creal( xhat[j + i * Ny] );
        }
        for ( i = 0; i < dst.Nx; i++ ) {
            in2[i + 1] = cimag( xhat[j + i * Ny] );
        }

        fftw_execute( plan );  /********************* FFTW *********************/
        fftw_execute( plan2 ); /********************* FFTW *********************/

        for ( i = 0; i < dst.Nx; i++ ) {
            sol[i + my] = dst.coef * ( -cimag( out[i + 1] ) - I * cimag( out2[i + 1] ) );
        }
    }

#endif
}
