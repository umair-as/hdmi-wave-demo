/* FFT vs naive DFT: random complex inputs for n = 8..4096 and known signals. */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/fft.h"

#define TOL 1e-9

static unsigned long long rs = 0x9E3779B97F4A7C15ull;
static double urand(void)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return (double)(rs >> 11) / 9007199254740992.0 * 2.0 - 1.0;
}

/* max |a-b| relative to max |b| (or absolute when b is ~0) */
static double relerr(const double complex *a, const double complex *b, int n)
{
    double num = 0, den = 0;
    for (int i = 0; i < n; i++) {
        num = fmax(num, cabs(a[i] - b[i]));
        den = fmax(den, cabs(b[i]));
    }
    return den > 1e-300 ? num / den : num;
}

static int fails;
static void check(const char *what, int n, double err)
{
    int ok = err <= TOL;
    if (!ok) fails++;
    printf("%-14s n=%5d rel_err=%.3e %s\n", what, n, err, ok ? "ok" : "FAIL");
}

int main(void)
{
    for (int n = 8; n <= 4096; n *= 2) {
        double complex *x = malloc(sizeof *x * n), *f = malloc(sizeof *f * n), *d = malloc(sizeof *d * n);
        for (int i = 0; i < n; i++) x[i] = urand() + I * urand();
        memcpy(f, x, sizeof *x * n);
        if (fft_inplace(f, n)) { printf("fft rejected n=%d\n", n); return 1; }
        dft_naive(x, d, n);
        check("random", n, relerr(f, d, n));

        /* impulse at 0 -> all ones; delayed impulse -> pure phase ramp */
        memset(x, 0, sizeof *x * n); x[0] = 1;
        memcpy(f, x, sizeof *x * n); fft_inplace(f, n);
        for (int k = 0; k < n; k++) d[k] = 1;
        check("impulse", n, relerr(f, d, n));
        memset(x, 0, sizeof *x * n); x[3] = 1;
        memcpy(f, x, sizeof *x * n); fft_inplace(f, n);
        dft_naive(x, d, n);
        check("impulse@3", n, relerr(f, d, n));

        /* DC -> n at bin 0, zero elsewhere */
        for (int i = 0; i < n; i++) x[i] = 2.5;
        memcpy(f, x, sizeof *x * n); fft_inplace(f, n);
        for (int k = 0; k < n; k++) d[k] = k == 0 ? 2.5 * n : 0;
        check("dc", n, relerr(f, d, n));

        /* single-bin complex sinusoid e^{2 pi i m t / n} -> n at bin m */
        int m = n / 8 + 1;
        for (int i = 0; i < n; i++) x[i] = cexp(2 * M_PI * I * (double)((long long)m * i % n) / n);
        memcpy(f, x, sizeof *x * n); fft_inplace(f, n);
        for (int k = 0; k < n; k++) d[k] = k == m ? n : 0;
        check("sinusoid", n, relerr(f, d, n));
        /* real cosine at bin m -> n/2 at bins m and n-m */
        for (int i = 0; i < n; i++) x[i] = cos(2 * M_PI * (double)((long long)m * i % n) / n);
        memcpy(f, x, sizeof *x * n); fft_inplace(f, n);
        for (int k = 0; k < n; k++) d[k] = (k == m || k == n - m) ? n / 2.0 : 0;
        check("cosine", n, relerr(f, d, n));
        free(x); free(f); free(d);
    }
    double complex z[6];
    if (fft_inplace(z, 6) != -1) { puts("non power of two accepted"); fails++; }
    printf("%s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
