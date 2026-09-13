#include <stdio.h>
#include <omp.h>

#include "OperacionesArreglos.h"

// suma

void OperacionesArreglos::sumarArreglosOpenMP(
    int *A,
    int *B,
    int *R,
    int tam)
{
#pragma omp parallel
    {
        int hilo = omp_get_thread_num();
        int total = omp_get_num_threads();

#pragma omp for
        for (int i = 0; i < tam; i++)
        {
            R[i] = A[i] + B[i];

            if (tam <= 100)
            {
                printf("[%d/%d] A[%d] + B[%d] = R[%d] -> %d\n",
                       hilo,
                       total,
                       i,
                       i,
                       i,
                       R[i]);
            }
        }
    }
}

void OperacionesArreglos::sumatoriaArregloOpenMP(
    const int *arreglo,
    int tam,
    long long *resultado)
{
    long long sumatoria = 0;

    // Cada hilo obtiene una suma parcial y OpenMP las combina al final.
#pragma omp parallel for reduction(+ : sumatoria)
    for (int i = 0; i < tam; i++)
    {
        sumatoria += arreglo[i];
    }

    // El resultado se entrega mediante el puntero recibido.
    *resultado = sumatoria;
}

void OperacionesArreglos::promedioArregloOpenMP(
    const int *arreglo,
    int tam,
    double *resultado)
{
    long long sumatoria = 0;

    // El promedio usa una reduccion para sumar sin condiciones de carrera.
#pragma omp parallel for reduction(+ : sumatoria)
    for (int i = 0; i < tam; i++)
    {
        sumatoria += arreglo[i];
    }

    *resultado = (tam > 0)
                     ? static_cast<double>(sumatoria) / tam
                     : 0.0;
}

void OperacionesArreglos::maximoArregloOpenMP(
    const int *arreglo,
    int tam,
    int *resultado)
{
    int maximo = 0;

    if (tam > 0)
    {
        maximo = arreglo[0];

        // La reduccion conserva el mayor valor encontrado por los hilos.
#pragma omp parallel for reduction(max : maximo)
        for (int i = 1; i < tam; i++)
        {
            if (arreglo[i] > maximo)
            {
                maximo = arreglo[i];
            }
        }
    }

    *resultado = maximo;
}

void OperacionesArreglos::minimoArregloOpenMP(
    const int *arreglo,
    int tam,
    int *resultado)
{
    int minimo = 0;

    if (tam > 0)
    {
        minimo = arreglo[0];

        // La reduccion conserva el menor valor encontrado por los hilos.
#pragma omp parallel for reduction(min : minimo)
        for (int i = 1; i < tam; i++)
        {
            if (arreglo[i] < minimo)
            {
                minimo = arreglo[i];
            }
        }
    }

    *resultado = minimo;
}
// resta
void OperacionesArreglos::restarArreglosOpenMP(
    int *A,
    int *B,
    int *R,
    int tam)
{
#pragma omp parallel
    {
        int hilo = omp_get_thread_num();
        int total = omp_get_num_threads();

#pragma omp for
        for (int i = 0; i < tam; i++)
        {
            R[i] = A[i] - B[i];

            if (tam <= 100)
            {
                printf("[%d/%d] A[%d] - B[%d] = R[%d] -> %d\n",
                       hilo,
                       total,
                       i,
                       i,
                       i,
                       R[i]);
            }
        }
    }
}

// multiplicacion
void OperacionesArreglos::multiplicarArreglosOpenMP(
    int *A,
    int *B,
    int *R,
    int tam)
{
#pragma omp parallel
    {
        int hilo = omp_get_thread_num();
        int total = omp_get_num_threads();

#pragma omp for
        for (int i = 0; i < tam; i++)
        {
            R[i] = A[i] * B[i];

            if (tam <= 100)
            {
                printf("[%d/%d] A[%d] * B[%d] = R[%d] -> %d\n",
                       hilo,
                       total,
                       i,
                       i,
                       i,
                       R[i]);
            }
        }
    }
}

// cuadrado

void OperacionesArreglos::cuadradoArregloOpenMP(
    int *A,
    int *R,
    int tam)
{
#pragma omp parallel
    {
        int hilo = omp_get_thread_num();
        int total = omp_get_num_threads();

#pragma omp for
        for (int i = 0; i < tam; i++)
        {
            R[i] = A[i] * A[i];

            if (tam <= 100)
            {
                printf("[%d/%d] A[%d] ^ 2 = R[%d] -> %d\n",
                       hilo,
                       total,
                       i,
                       i,
                       R[i]);
            }
        }
    }
}
