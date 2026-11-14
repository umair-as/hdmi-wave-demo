#include "fft.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int fft_inplace(double complex *x, int n)
{
    if (n < 1 || (n & (n - 1))) return -1;
    /* bit-reversal permutation */
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double complex t = x[i]; x[i] = x[j]; x[j] = t; }
    }
    /* butterflies; twiddles evaluated directly (no recurrence drift) */
    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        double step = -2.0 * M_PI / len;
        for (int k = 0; k < half; k++) {
            double complex w = cos(step * k) + I * sin(step * k);
            for (int s = 0; s < n; s += len) {
                double complex u = x[s + k], v = x[s + k + half] * w;
                x[s + k] = u + v;
                x[s + k + half] = u - v;
            }
        }
    }
    return 0;
}

void dft_naive(const double complex *in, double complex *out, int n)
{
    for (int k = 0; k < n; k++) {
        double complex acc = 0;
        for (int j = 0; j < n; j++) {
            /* reduce k*j mod n first so the angle stays in [0, 2 pi) */
            long long m = ((long long)k * j) % n;
            double a = -2.0 * M_PI * (double)m / n;
            acc += in[j] * (cos(a) + I * sin(a));
        }
        out[k] = acc;
    }
}

double hann(int i, int n)
{
    return n > 1 ? 0.5 - 0.5 * cos(2.0 * M_PI * i / (n - 1)) : 1.0;
}
