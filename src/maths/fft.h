/* Radix-2 FFT (iterative, in place, complex double) and the reference DFT. */
#ifndef HWD_FFT_H
#define HWD_FFT_H

#include <complex.h>

/* n must be a power of two >= 1. Forward transform, X_k = sum x_n e^{-2 pi i k n / N}.
 * Returns 0, or -1 if n is not a power of two. */
int fft_inplace(double complex *x, int n);
/* O(n^2) reference DFT, out != in; any n >= 1. */
void dft_naive(const double complex *in, double complex *out, int n);
/* Hann window coefficient for sample i of n. */
double hann(int i, int n);

#endif
