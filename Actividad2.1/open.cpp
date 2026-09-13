#include <iostream>
#include <stdlib.h>
#include <omp.h>

#define N 100

using namespace std;

int main()
{
#pragma omp parallel
    {
        int hilo = omp_get_thread_num();
        int total = omp_get_num_threads();

        int *arreglo = new int[N];

        double avance = 0;

        for (int i = 0; i < N; i++)
        {
            arreglo[i] = rand() % 1000;

            avance = ((i + 1) * 100.0) / N;

            cout << "[" << hilo + 1 << "/" << total << "] " << "Valor asignado: " << arreglo[i] << " en la posicion: " << i << " | Avance: " << avance << "%" << "\n";
        }

        delete[] arreglo;
    }

    return 0;
}