/*
 * Alumno: Cardenas Olivares Cesar Giovanny
 * Actividad 2.0: Configuracion de ambiente de trabajo
 * Programa: Hola Mundo con OpenMP
 */

#include <omp.h>
#include <iostream>

int main() {

    std::cout << "Alumno: Cardenas Olivares Cesar Giovanny" << std::endl;

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int total = omp_get_num_threads();

        #pragma omp critical
        {
            std::cout
                << "Hola desde el hilo "
                << id
                << " de "
                << total
                << std::endl;
        }
    }

    std::cout << "Alumno: Cardenas Olivares Cesar Giovanny" << std::endl;

    return 0;
}