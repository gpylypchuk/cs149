#include <stdio.h>
#include <algorithm>

#include "CycleTimer.h"
#include "saxpy_ispc.h"

extern void saxpySerial(int N, float a, float* X, float* Y, float* result);
extern void saxpyAVX2(int N, float scale, float X[], float Y[], float result[]);

// return GB/s
static float
toBW(int bytes, float sec) {
    return static_cast<float>(bytes) / (1024. * 1024. * 1024.) / sec;
}

static float
toGFLOPS(int ops, float sec) {
    return static_cast<float>(ops) / 1e9 / sec;
}

static void verifyResult(int N, float* result, float* gold) {
    for (int i=0; i<N; i++) {
        if (result[i] != gold[i]) {
            printf("Error: [%d] Got %f expected %f\n", i, result[i], gold[i]);
        }
    }
}

using namespace ispc;


int main() {

    const unsigned int N = 20 * 1000 * 1000; // 20 M element vectors (~80 MB)

    /*
        El factor 4 cuenta las transferencias de memoria por elemento: leer x[i], leer y[i], leer result[i] y escribir result[i].
        Lee result porque lo lee desde la cache y si hay miss lo trae de la RAM

        una linea de cache (64 bytes):
        [ r0 | r1 | r2 | r3 | r4 | r5 | r6 | r7 | r8 | r9 | r10 | r11 | r12 | r13 | r14 | r15 ]

        Asi ejecutaria el cpu
        result[0]  → miss: trae la línea (result[0..15]) desde la RAM
        result[1]  → hit: la línea ya está en caché
        result[2]  → hit
        ...
        result[15] → hit
        result[16] → miss: trae la línea siguiente (result[16..31])

        ---POSIBLE MEJORA--
        Juntar los 16 valores en un buffer y mandarlo directo a la ram con streaming store de AVX2
        MAXIMO TEORICO 16/12 BYTES = 1.33x
    */
    const unsigned int TOTAL_BYTES = 4 * N * sizeof(float);


    const unsigned int TOTAL_FLOPS = 2 * N;

    float scale = 2.f;

    float* arrayX = new float[N];
    float* arrayY = new float[N];
    float* resultSerial = new float[N];
    float* resultISPC = new float[N];
    float* resultTasks = new float[N];

    float* resultAVX2 = (float*) aligned_alloc(32, N * sizeof(float));

    // initialize array values
    for (unsigned int i=0; i<N; i++)
    {
        arrayX[i] = i;
        arrayY[i] = i;
        resultSerial[i] = 0.f;
        resultISPC[i] = 0.f;
        resultTasks[i] = 0.f;
    }

    //
    // Run the serial implementation. Repeat three times for robust
    // timing.
    //
    double minSerial = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime =CycleTimer::currentSeconds();
        saxpySerial(N, scale, arrayX, arrayY, resultSerial);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

    printf("[saxpy serial]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minSerial * 1000,
           toBW(TOTAL_BYTES, minSerial),
           toGFLOPS(TOTAL_FLOPS, minSerial));

    //
    // Run the _mm256_stream_ps implementation on ISPC with immintrin
    //
    double minAVX2 = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpyAVX2(N, scale, arrayX, arrayY, resultAVX2);
        double endTime = CycleTimer::currentSeconds();
        minAVX2 = std::min(minAVX2, endTime - startTime);
    }
    printf("[saxpy avx2 stream]:\t[%.3f] ms\n", minAVX2 * 1000);
    printf("\t\t\t\t(%.2fx speedup vs serial)\n", minSerial / minAVX2);

    //
    // Run the ISPC (single core) implementation
    //
    double minISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpy_ispc(N, scale, arrayX, arrayY, resultISPC);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
    }

    verifyResult(N, resultISPC, resultSerial);

    printf("[saxpy ispc]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minISPC * 1000,
           toBW(TOTAL_BYTES, minISPC),
           toGFLOPS(TOTAL_FLOPS, minISPC));

    //
    // Run the ISPC (multi-core) implementation
    //
    double minTaskISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpy_ispc_withtasks(N, scale, arrayX, arrayY, resultTasks);
        double endTime = CycleTimer::currentSeconds();
        minTaskISPC = std::min(minTaskISPC, endTime - startTime);
    }

    verifyResult(N, resultTasks, resultSerial);

    printf("[saxpy task ispc]:\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minTaskISPC * 1000,
           toBW(TOTAL_BYTES, minTaskISPC),
           toGFLOPS(TOTAL_FLOPS, minTaskISPC));

    printf("\t\t\t\t(%.2fx speedup from use of tasks)\n", minISPC/minTaskISPC);
    //printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    //printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);

    delete[] arrayX;
    delete[] arrayY;
    delete[] resultSerial;
    delete[] resultISPC;
    delete[] resultTasks;

    return 0;
}
