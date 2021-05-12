#include "../headers/structs.h"
#include "../headers/prototypes.h"


#include <assert.h>
#include <string.h>

#include <cuda_runtime.h>
#include <cuComplex.h>
//#include <cuComplex.h>
//#include <cufftw.h>


void DST(int Nx, int Ny, double *b_2D, double *bhat, fftw_plan p1,  double *in1, fftw_complex *out1, fftw_plan p2,  double *in2, fftw_complex *out2);

#ifdef USE_CUFFTW


void solver(System sys) {


    int i, num_streams = 4;
    cudaStream_t streams[num_streams];
    for ( i = 0; i < num_streams; i++ ) {
        cudaStreamCreateWithFlags( &streams[i], cudaStreamNonBlocking );
    }

    int Nx = sys.lat.Nx, Ny = sys.lat.Ny;
    
    
    int NR = 2*Nx + 2;
    int NC = NR/2 + 1;
    
    size_t size_in  = sizeof( double ) * NR * Ny;
    size_t size_out = sizeof( fftw_complex ) * NC * Ny;
        
    double *in, *in2;
    fftw_complex *out, *out2;
    cudaMallocManaged( ( void ** )&in, size_in, 1 );
    cudaMallocManaged( ( void ** )&out, size_out, 1 );

    cudaMemPrefetchAsync( in, size_in, 0, streams[0] );
    cudaMemPrefetchAsync( out, size_out, 0, streams[1] );

    cudaMemsetAsync( in, size_in, 0, streams[0] );
    cudaMemsetAsync( out, size_out, 0, streams[1] );
    
    
    NR = 2*Ny + 2;
    NC = NR/2 + 1;
    size_in  = sizeof( double ) * NR * Nx;
    size_out = sizeof( fftw_complex ) * NC * Nx;
    
    cudaMallocManaged( ( void ** )&in2, size_in, 1 );
    cudaMallocManaged( ( void ** )&out2, size_out, 1 );
    
    cudaMemPrefetchAsync( in2, size_in, 0, streams[0] );
    cudaMemPrefetchAsync( out2, size_out, 0, streams[1] );
    
    cudaMemsetAsync( in2, size_in, 0, streams[0] );
    cudaMemsetAsync( out2, size_out, 0, streams[1] );
    
    
    cuDoubleComplex *d_y;
    cudaMalloc( ( void ** )( &d_y ), sys.lat.Nxyz * sizeof( cuDoubleComplex ) );
    
    
    cudaMemPrefetchAsync( sys.rhs, sys.lat.Nxyz * sizeof( double _Complex ), 0, streams[0] ) ;

    cudaMemPrefetchAsync( sys.U, sys.lat.Nxyz * sizeof( double _Complex ), 0, streams[2] ) ;
    cudaMemPrefetchAsync( sys.L, sys.lat.Nxyz * sizeof( double _Complex ), 0, streams[2] ) ;
    cudaMemPrefetchAsync( sys.Up, sys.lat.Nxyz * sizeof( double _Complex ), 0, streams[2] ) ;

    cudaMemPrefetchAsync( sys.sol, sys.lat.Nxyz * sizeof( double _Complex ), 0, streams[3] ) ;

    /**********************BATCHED***************************/
    NR = 2*Nx + 2;
    NC = NR/2 + 1;
    const int rank = 1;
    int howmany = Ny;
    int n[] = {NR};
    const int istride = 1; const int ostride = 1;
    int idist = NR; int odist = NC;
    int *inembed = NULL, *onembed = NULL;
    /**********************BATCHED***************************/
    
    fftw_plan plan, plan2;
    
    plan = fftw_plan_many_dft_r2c( rank, n, howmany, in, inembed, istride, idist, out, onembed, ostride, odist, FFTW_ESTIMATE );
    
    
    
    /**********************BATCHED***************************/
    NR = 2*Ny + 2;
    NC = NR/2 + 1;
    const int rank2 = 1;
    int howmany2 = Nx;
    int n2[] = {NR};
    const int istride2 = 1; const int ostride2 = 1;
    int idist2 = NR; int odist2 = NC;
    int *inembed2 = NULL, *onembed2 = NULL;
    /**********************BATCHED***************************/
    
    
    plan2 = fftw_plan_many_dft_r2c( rank2, n2, howmany2, in2, inembed2, istride2, idist2, out2, onembed2, ostride2, odist2, FFTW_ESTIMATE );
    
    
    cudaMemPrefetchAsync( sys.sol, sys.lat.Nxyz * sizeof( double _Complex ), cudaCpuDeviceId, NULL );

    
    cudaFree( in );
    cudaFree( out );
    cudaFree( in2 );
    cudaFree( out2 );

    
    for (i = 0; i < num_streams; i++ ) {
        cudaStreamDestroy( streams[i] );
    }
    
    
    fftw_destroy_plan( plan );  /********************* FFTW *********************/
    fftw_destroy_plan( plan2 ); /********************* FFTW *********************/
}
#else
void solver(System sys) {
    int Nx = sys.lat.Nx, Ny = sys.lat.Ny, Nz = sys.lat.Nz;
    int Nxy,Nxz, Nxyz;
    double complex *rhat, *y_a, *xhat;
    /*
     Allocate the arrays for the 2D layer of the rhs for z = const
     and for the transform of that layer. And the 3D array of the
     transformed rhs.
     */
    Nxy = Nx*Ny;
    Nxz = Nx*Nz;
    Nxyz = Nx*Ny*Nz;

    rhat    = malloc(Nxyz * sizeof(double complex));
    y_a     = malloc(Nz * sizeof(double complex));
    xhat    = malloc(Nxyz * sizeof(double complex));
    /*
     FFT transformation of the rhs layer by layer (z = const).
     and store the transformed vectors in the rhat in the same order as rhs.
     */
    int i,j,l;
    int m, NR ,NC ;
    double *in1,*in2  ;
    fftw_complex *out1, *out2 ;
    fftw_plan p1, p2;
    
    m = Ny;
    NR = 2*Nx + 2;
    NC = NR/2 + 1;
    const int rank1 = 1;
    const int howmany1 = m;
    int nr1[] = {NR};
    const int istride1 = 1; const int ostride1 = 1;
    const int idist1 = NR; const int odist1 = NC;
    int *inembed1 = NULL, *onembed1 = NULL;
    
    in1 = (double*) fftw_malloc(sizeof(double) * NR*m);
    out1 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * NC*m);
    
    p1 = fftw_plan_many_dft_r2c(rank1, nr1, howmany1, in1, inembed1, istride1, idist1,  out1, onembed1, ostride1, odist1, FFTW_ESTIMATE);

    for( j = 0; j < NR*m; j++) {
        in1[j] = 0.0;
    }
    
    m = Nx;
    NR = 2*Ny + 2;
    NC = NR/2 + 1;
    const int rank2 = 1;
    const int howmany2 = m;
    int nr2[] = {NR};
    const int istride2 = 1; const int ostride2 = 1;
    const int idist2 = NR; const int odist2 = NC;
    int *inembed2 = NULL, *onembed2 = NULL;
    
    in2 = (double*) fftw_malloc(sizeof(double) * NR*m);
    out2 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * NC*m);
    
    p2 = fftw_plan_many_dft_r2c(rank2, nr2, howmany2, in2, inembed2, istride2, idist2,  out2, onembed2, ostride2, odist2, FFTW_ESTIMATE);
    
    for( j = 0; j < NR*m; j++) {
        in2[j] = 0.0;
    }

    double *b_2D_r= malloc(Nxy * sizeof(double));
    double *b_2D_i= malloc(Nxy * sizeof(double));
    double *bhat_r= malloc(Nxy * sizeof(double));
    double *bhat_i= malloc(Nxy * sizeof(double));


    for(l = 0; l < Nz; l++) {
        for(i = 0; i < Nxy; i++) {
            b_2D_r[i] = creal(sys.rhs[i + l*Nxy]) ;
            b_2D_i[i] = cimag(sys.rhs[i + l*Nxy]) ;
        }
        
        DST(Nx, Ny, b_2D_r, bhat_r, p1, in1, out1, p2, in2, out2);
        DST(Nx, Ny, b_2D_i, bhat_i, p1, in1, out1, p2, in2, out2);

        for(i = 0; i < Nxy; i++) {
            rhat[i + l*Nxy] = bhat_r[i] + I*bhat_i[i] ;
        }
    }
    
    /*
     Solution of the transformed system.
     */
    
    for( j = 0; j < Ny; j++) {
        for( i = 0; i < Nx; i++){
            y_a[0] = rhat[i + j*Nx] ;
            for( l = 1; l < Nz; l++) {
                y_a[l] = rhat[i + j*Nx + l*Nxy] - sys.L[l + i*Nz + j*Nxz]*y_a[l - 1] ;
            }
            xhat[Nz - 1 + i*Nz + j*Nxz] = y_a[Nz - 1]*sys.U[Nz - 1 + i*Nz + j*Nxz] ;
            for( l = Nz-2; l >= 0 ; l--) {
                xhat[l + i*Nz + j*Nxz] = ( y_a[l] - sys.Up[l + i*Nz + j*Nxz]*xhat[l + 1 + i*Nz + j*Nxz] )*sys.U[l + i*Nz + j*Nxz];
            }
        }
    }
    
    /*
     FFT transformation of the solution xhat layer by layer (z = const).
     and store the transformed vectors in the sol in the same order as xhat.
     */
    
    for(l = 0; l < Nz; l++) {
        for(i = 0; i < Nxy; i++) {
            b_2D_r[i] = creal(xhat[l + i*Nz]) ;
            b_2D_i[i] = cimag(xhat[l + i*Nz]) ;
        }
        
        DST(Nx, Ny, b_2D_r, bhat_r, p1, in1, out1, p2, in2, out2);
        DST(Nx, Ny, b_2D_i, bhat_i, p1, in1, out1, p2, in2, out2);

        for(i = 0; i < Nxy; i++) {
            sys.sol[i + l*Nxy] = bhat_r[i] + I*bhat_i[i] ;
        }
    }
    
    
    fftw_destroy_plan(p1); fftw_free(in1); fftw_free(out1);
    fftw_destroy_plan(p2); fftw_free(in2); fftw_free(out2);
    free(b_2D_r); b_2D_r = NULL;
    free(b_2D_i); b_2D_i = NULL;
    free(bhat_r); bhat_r = NULL;
    free(bhat_i); bhat_i = NULL;
    
    
    free(rhat); rhat = NULL;
    free(y_a); y_a = NULL;
    free(xhat); xhat = NULL;
    
    return;
}

#endif