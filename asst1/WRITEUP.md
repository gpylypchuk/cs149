# CS149 — Assignment 1: Writeup

## Program 1: Mandelbrot con threads

**Hardware:** Intel i5-8400 (6 núcleos, 6 threads, sin hyperthreading), WSL2 Ubuntu 24.04, g++ 13.
**Tiempo serial de referencia:** ~470 ms (vista 1).

### 1. Reparto por bloques de filas

La primera versión divide la imagen (1200 filas) en `numThreads` bloques de filas consecutivas. Cada thread llama a `mandelbrotSerial()` con su `startRow` y `numRows`, recorriendo todo el ancho de la imagen. El último thread se lleva las filas sobrantes cuando `height` no es divisible por `numThreads`.

| Threads | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|
| Speedup (bloques, vista 1) | 1.97x | **1.66x** | 2.45x | 2.47x | 3.22x | 3.45x | 4.01x |

El speedup está lejos de ser lineal, y con 3 threads es **peor que con 2**.

**Por qué:** el costo de cada píxel depende de cuántas iteraciones tarda en escaparse. Los píxeles dentro del conjunto (la zona blanca) llegan al máximo de 256 iteraciones, mientras que los del borde negro se escapan en 1 o 2. Con 3 bloques, el thread del medio recibe casi toda la zona blanca. Midiendo el tiempo de cada thread con `CycleTimer`:

![Tiempo por thread con 3 threads](images/prog1_tiempos_por_thread.png)

| | Thread 0 | Thread 1 | Thread 2 |
|---|---|---|---|
| Bloques | 95 ms | **291 ms** | 95 ms |

El tiempo total lo determina el thread más lento:

$$\text{speedup} \approx \frac{T_{\text{serial}}}{\max_k T_k} = \frac{470}{291} \approx 1.62$$

que coincide con el 1.66x medido. El trabajo total no cambió ($95 + 291 + 95 \approx 481$ ms); lo que falla es cómo está repartido. Los threads 0 y 2 tardan lo mismo porque el fractal es simétrico respecto del eje real, y por la misma razón 2 threads dan un speedup casi perfecto: cada mitad tiene exactamente el mismo trabajo.

### 2. Reparto intercalado (en rueda)

Para balancear la carga, cada thread toma una fila de cada `numThreads`, empezando por su `threadId`. Con 3 threads, el thread 0 hace las filas 0, 3, 6, …, el thread 1 las filas 1, 4, 7, …, y así. Como las filas vecinas cuestan parecido, cada thread recibe una mezcla equivalente de filas caras y baratas.

```cpp
for (int row = myId; row < height; row += numThreads) {
    mandelbrotSerial(args->x0, args->y0, args->x1, args->y1,
                     args->width, args->height,
                     row, 1,
                     args->maxIterations, args->output);
}
```

Con 3 threads, los tres tardan ~161 ms, justo el reparto perfecto ($481 / 3 \approx 160$ ms).

![Bloques vs. intercalado](images/prog1_bloques_vs_rueda.png)

| Threads | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|
| Bloques (vista 1) | 1.97x | 1.66x | 2.45x | 2.47x | 3.22x | 3.45x | 4.01x |
| Intercalado (vista 1) | 1.99x | 2.95x | 3.91x | 4.84x | 5.77x | 4.15x | 4.64x |
| Intercalado (vista 2) | 1.96x | 2.96x | 3.89x | 4.83x | 5.78x | 4.29x | 4.29x |

Hasta 6 threads el speedup es casi lineal (en la mejor corrida, 5.97x con 6 threads). Además, las dos vistas escalan prácticamente igual: con el reparto intercalado, el speedup no depende de dónde esté el trabajo caro dentro de la imagen.

![Vista 1 vs. vista 2](images/prog1_vistas.png)

Entre corridas hay una variación de ±0.2x (por ejemplo, 5.77x vs. 5.97x con 6 threads), atribuible a otros procesos de Windows y WSL2 compitiendo por los núcleos.

### 3. Más threads que núcleos

Con 7 y 8 threads el speedup **cae** respecto de 6, en las dos vistas. El i5-8400 tiene 6 núcleos sin hyperthreading, así que con 7 threads algún núcleo tiene que alternar entre dos threads y termina último. Si los threads quedaran fijos en sus núcleos, el núcleo más cargado tendría $\lceil n/6 \rceil$ threads de $W/n$ trabajo cada uno:

$$\text{speedup} \approx \frac{n}{\lceil n/6 \rceil}$$

| Threads | Modelo | Medido |
|---|---|---|
| 6 | 6.0 | 5.77–5.97 |
| 7 | 3.5 | 4.15–4.41 |
| 8 | 4.0 | 4.55–4.64 |
| 12 | 6.0 | 5.29 |

Lo medido queda por encima del modelo con 7 y 8 threads porque Linux migra threads entre núcleos a medida que se liberan, y eso recupera parte de la pérdida. Con 12 threads (múltiplo de 6) el speedup vuelve a subir, como predice el modelo, aunque no llega a 6 por el costo de los cambios de contexto y la competencia con los procesos de Windows.

En las máquinas myth (4 núcleos con 2 hyperthreads cada uno) el comportamiento es distinto: los threads 5 a 8 corren en hyperthreads, que comparten las unidades de ejecución del núcleo, y aportan menos que un núcleo completo.

Con el reparto por bloques, en cambio, 7 y 8 threads **mejoraban** el speedup: ahí el problema dominante era el desbalance, y más bloques más chicos lo reducían.

### Conclusión

- **El reparto importa más que la cantidad de threads.** Con bloques de filas, agregar threads no garantiza más speedup: con 3 threads cayó a 1.66x (peor que con 2), porque el bloque central concentra casi todo el trabajo y el tiempo total lo marca el thread más lento. Con el reparto intercalado, el speedup es casi lineal hasta 6 threads (≈5.8–6x) en ambas vistas.

- **El límite es la cantidad de núcleos.** Con más threads que núcleos el speedup cae: con 7 threads, algún núcleo tiene que alternar entre dos y termina último. El speedup se recupera parcialmente cuando la cantidad de threads es múltiplo de los núcleos (12 threads → 5.29x).

- **El reparto intercalado funciona porque filas vecinas cuestan parecido.** Al alternarlas, cada thread recibe una mezcla equivalente de filas caras y baratas. Si el costo variara de forma impredecible, un reparto estático podría quedar desbalanceado igual; en ese caso convendría un reparto dinámico (una cola de filas compartida de la que cada thread toma la siguiente al terminar).