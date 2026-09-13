/*
 * Alumno: Cardenas Olivares Cesar Giovanny
 * Actividad 2.0: Configuracion de ambiente de trabajo
 * Programa: Hola Mundo con MPI
 */

#include <mpi.h>
#include <iostream>

int main(int argc, char* argv[]) {

    std::cout << "Alumno: Cardenas Olivares Cesar Giovanny" << std::endl;

    MPI_Init(&argc, &argv);

    int rank;
    int size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::cout
        << "Hola Mundo desde el proceso "
        << rank
        << " de "
        << size
        << std::endl;

    MPI_Finalize();

    std::cout << "Alumno: Cardenas Olivares Cesar Giovanny" << std::endl;

    return 0;
}