#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>

void sqrtAVX2(int N, float initialGuess, float values[], float output[]) {
    // AVX2 PS(Packed Single 32 bits por instancia, lane)
    const __m256 one = _mm256_set1_ps(1.f);
    const __m256 three = _mm256_set1_ps(3.f);
    const __m256 half = _mm256_set1_ps(0.5f);
    const __m256 threshold = _mm256_set1_ps(0.00001f);
    const __m256 signMask = _mm256_set1_ps(-0.f); // solo el bit de signo

    for (int i = 0; i < N; i += 8) {
        __m256 x = _mm256_loadu_ps(values + i);
        __m256 guess = _mm256_set1_ps(initialGuess);

        // error = |guess*guess*x - 1|
        __m256 g2 = _mm256_mul_ps(guess, guess);
        __m256 diff = _mm256_sub_ps(_mm256_mul_ps(g2, x), one);
        // Para floats, se borra el bit de signo
        __m256 error = _mm256_andnot_ps(signMask, diff);

        // mask  = error > threshold  | _CMP_GT_OQ: a > b, en true todos los bits en 1.
        __m256 mask = _mm256_cmp_ps(error, threshold, _CMP_GT_OQ);

        while (_mm256_movemask_ps(mask) != 0) {
            // newGuess = (3*guess - x*guess^3) * 0.5
            __m256 g3 = _mm256_mul_ps(_mm256_mul_ps(guess, guess), guess);
            __m256 newGuess = _mm256_mul_ps(_mm256_sub_ps(_mm256_mul_ps(three, guess),_mm256_mul_ps(x, g3)),half);
            
            // _mm256_blendv_ps(guess, newGuess, mask)
            
            //a (guess):     [ 1.20 | 0.57 | 0.90 | 0.58 | ... ]
            //b (newGuess):  [ 1.10 | 0.58 | 0.85 | 0.58 | ... ]
            //mask:          [ FFFF | 0000 | FFFF | 0000 | ... ]
            //                  ↓      ↓      ↓      ↓
            //resultado:     [ 1.10 | 0.57 | 0.85 | 0.58 | ... ]
            //                  b      a      b      a

            // es resultado[k] = mask[k] ? b[k] : a[k], para los 8 lanes a la vez

            guess = _mm256_blendv_ps(guess, newGuess, mask);

            // recalcular error y mask
            g2 = _mm256_mul_ps(guess, guess);
            diff = _mm256_sub_ps(_mm256_mul_ps(g2, x), one);
            error = _mm256_andnot_ps(signMask, diff);
            mask = _mm256_cmp_ps(error, threshold, _CMP_GT_OQ);
        }

        _mm256_storeu_ps(output + i, _mm256_mul_ps(x, guess));
    }

}
