#include <immintrin.h>

void saxpyAVX2(int N, float scale, float X[], float Y[], float result[]) {
    __m256 s = _mm256_set1_ps(scale);
    for (int i = 0; i < N; i += 8) {
        __m256 x = _mm256_loadu_ps(X + i);
        __m256 y = _mm256_loadu_ps(Y + i);
        __m256 r = _mm256_add_ps(_mm256_mul_ps(s, x), y);
        _mm256_stream_ps(result + i, r);   // escritura sin pasar por cache
    }
    _mm_sfence();   // asegura que todas las streaming stores terminaron
}