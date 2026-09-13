# Actividad 2.6: Problema práctico en OpenMP

Programación Paralela. Búsqueda exhaustiva de una clave de prueba académica
introducida por el usuario, en una sola computadora.

## Integrantes

- CESAR GIOVANNY CARDENAS OLIVARES
- VICTOR HUGO CERVANTES ARRIZON
- JAZMIN MONTSERRAT DIAZ TORRES

## Compilación y ejecución

Se necesita un compilador g++ con soporte de OpenMP.

```bash
g++ main.cpp -o programa -fopenmp
./programa
```

El menú permite ejecutar una búsqueda o salir. Introduce la longitud y después
la clave. Las longitudes disponibles son 3, 5, 6, 7 y 10. Se rechazan claves
vacías, de longitud incorrecta o con caracteres distintos de `A-Z` y `0-9`,
incluidos los espacios y las minúsculas.

Para la primera ejecución usa 3 caracteres, por ejemplo `ABC`. Para la segunda
escoge una clave de prueba académica, preferentemente de 10 caracteres. Si esa
longitud resulta impráctica por el tiempo de ejecución, puedes reducirla a 7,
6 o 5, como permite la actividad. Utiliza la misma clave para la versión
secuencial y la paralela. La longitud utilizada debe quedar claramente indicada
en la salida y en el reporte.

OpenMP determina la cantidad de hilos. Para ejecutar, por ejemplo, con 4:

```bash
OMP_NUM_THREADS=4 ./programa
```

## Solución

`main.cpp` contiene funciones para validar la clave, calcular el espacio,
convertir posiciones y ejecutar ambas búsquedas con la misma clave.

El alfabeto es exactamente `ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`.
El espacio se calcula multiplicando por 36 tantas veces como indique la longitud.
Se usa `unsigned long long` para representar también las posiciones de longitud 10.
La conversión interpreta una posición en base 36, completando desde la derecha:
la posición 0 corresponde a `AAA` y la última a `999` para longitud 3.
Solo se genera la combinación actual de cada búsqueda; no se guardan todas.

- **Secuencial:** recorre desde la posición 0, cuenta las combinaciones revisadas
  y termina al encontrar la clave.
- **Paralela:** divide el total entre la cantidad real de hilos. Los primeros
  hilos reciben una combinación extra hasta repartir el sobrante. Cada hilo
  recorre exclusivamente su rango y comunica su inicio, fin, cantidad y estado.

Tres arreglos dinámicos guardan inicios, finales inclusivos y cantidades por hilo.
Se reservan mediante `new[]` y se liberan con `delete[]` al terminar la región paralela.

## Directivas OpenMP y sincronización

- `parallel`: crea el equipo de hilos para la búsqueda.
- `single`: un hilo reserva los arreglos según el tamaño real del equipo.
  Su barrera implícita garantiza que estén disponibles antes de usarlos.
- `atomic read` y `atomic write`: permiten consultar y actualizar la bandera
  compartida sin condiciones de carrera.
- `critical(resultado)`: cuando un hilo encuentra la clave, verifica la bandera,
  guarda la clave y su identificador, y finalmente marca la bandera.
- `critical(salida)`: mantiene completos los mensajes de cada hilo.

Los demás hilos consultan la bandera antes de cada combinación y salen de su
ciclo cuando está marcada. Un hilo que ya estaba evaluando una combinación
puede terminar esa evaluación antes de detenerse. Al finalizar la región
paralela, su barrera implícita garantiza que el resultado esté disponible
para mostrarlo en el programa principal. Un hilo también puede agotar su
rango antes de que otro encuentre la clave; se informa ese estado por separado.

Los tiempos se miden con `omp_get_wtime()`. El tiempo paralelo incluye crear
el equipo, reservar los arreglos y mostrar los mensajes de los hilos.
El tiempo secuencial corresponde al recorrido y registro del resultado.
Se muestra una sola comparación por búsqueda:
`speedup = tiempo_secuencial / tiempo_paralelo`.
