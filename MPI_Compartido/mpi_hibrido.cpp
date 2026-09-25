/*
CARDENAS OLIVARES CESAR GIOVANNY
CERVANTES ARRIZON VICTOR HUGO
DIAZ TORRES JAZMIN MONTSERRAT
*/
#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "Integrantes: CARDENAS OLIVARES CESAR GIOVANNY; CERVANTES ARRIZON VICTOR HUGO; DIAZ TORRES JAZMIN MONTSERRAT" << std::endl;

    int mpi_rank, mpi_size;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int hostname_len;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Get_processor_name(hostname, &hostname_len);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int num_threads = omp_get_num_threads();
        #pragma omp critical
        {
            std::cout << "Hola desde el hilo " << thread_id << " de " << num_threads
                      << " del proceso MPI " << mpi_rank << " de " << mpi_size
                      << " en el host " << std::string(hostname, hostname_len) << std::endl;
        }
    }

    MPI_Finalize();

    std::cout << "Integrantes: CARDENAS OLIVARES CESAR GIOVANNY; CERVANTES ARRIZON VICTOR HUGO; DIAZ TORRES JAZMIN MONTSERRAT" << std::endl;
    return 0;
}
