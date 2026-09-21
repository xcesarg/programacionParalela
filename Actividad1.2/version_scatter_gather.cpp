#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <mpi.h>
#include <omp.h>

class OperacionesArreglos {
public:
    static void crearArregloMPI(double* A, double* B, int n_local, int rank, const char* hostname, bool detallado) {
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            #pragma omp for
            for (int i = 0; i < n_local; i++) {
                if (detallado) {
                    A[i] = (rank + 1) * 10 + i;
                    B[i] = (rank + 1) * 20 + i;
                    #pragma omp critical
                    {
                        std::cout << "[Equipo: " << hostname << "] [Proceso MPI: " << rank 
                                  << "] [Hilo OpenMP: " << thread_id << "] [Posicion: " << i 
                                  << "] [Operacion: Generacion] A=" << A[i] << ", B=" << B[i] << std::endl;
                    }
                } else {
                    A[i] = (rand() % 1000000) + 1;
                    B[i] = (rand() % 1000000) + 1;
                }
            }
        }
    }

    static void sumar(const double* A, const double* B, double* C, int n_local, int rank, const char* hostname, bool detallado) {
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            #pragma omp for
            for (int i = 0; i < n_local; i++) {
                C[i] = A[i] + B[i];
                if (detallado) {
                    #pragma omp critical
                    {
                        std::cout << "[Equipo: " << hostname << "] [Proceso MPI: " << rank 
                                  << "] [Hilo OpenMP: " << thread_id << "] [Posicion: " << i 
                                  << "] [Operacion: Suma] " << A[i] << " + " << B[i] << " = " << C[i] << std::endl;
                    }
                }
            }
        }
    }

    static void restar(const double* A, const double* B, double* C, int n_local, int rank, const char* hostname, bool detallado) {
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            #pragma omp for
            for (int i = 0; i < n_local; i++) {
                C[i] = A[i] - B[i];
                if (detallado) {
                    #pragma omp critical
                    {
                        std::cout << "[Equipo: " << hostname << "] [Proceso MPI: " << rank 
                                  << "] [Hilo OpenMP: " << thread_id << "] [Posicion: " << i 
                                  << "] [Operacion: Resta] " << A[i] << " - " << B[i] << " = " << C[i] << std::endl;
                    }
                }
            }
        }
    }

    static void multiplicar(const double* A, const double* B, double* C, int n_local, int rank, const char* hostname, bool detallado) {
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            #pragma omp for
            for (int i = 0; i < n_local; i++) {
                C[i] = A[i] * B[i];
                if (detallado) {
                    #pragma omp critical
                    {
                        std::cout << "[Equipo: " << hostname << "] [Proceso MPI: " << rank 
                                  << "] [Hilo OpenMP: " << thread_id << "] [Posicion: " << i 
                                  << "] [Operacion: Multiplicacion] " << A[i] << " * " << B[i] << " = " << C[i] << std::endl;
                    }
                }
            }
        }
    }

    static void cuadrado(const double* A, double* C, int n_local, int rank, const char* hostname, bool detallado) {
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            #pragma omp for
            for (int i = 0; i < n_local; i++) {
                C[i] = A[i] * A[i];
                if (detallado) {
                    #pragma omp critical
                    {
                        std::cout << "[Equipo: " << hostname << "] [Proceso MPI: " << rank 
                                  << "] [Hilo OpenMP: " << thread_id << "] [Posicion: " << i 
                                  << "] [Operacion: Cuadrado] (" << A[i] << ")^2 = " << C[i] << std::endl;
                    }
                }
            }
        }
    }
};

void imprimirIntegrantes() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "INTEGRANTES DEL EQUIPO (Orden Alfabetico por Primer Apellido):" << std::endl;
    std::cout << "1. Cardenas Olivares Giovanny" << std::endl;
    std::cout << "2. Cervantes Arrizon Victor " << std::endl;
    std::cout << "3. Diaz Torres Jazmin Montserrat " << std::endl;
    std::cout << "==========================================================" << std::endl;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char hostname[MPI_MAX_PROCESSOR_NAME];
    int len;
    MPI_Get_processor_name(hostname, &len);

    if (rank == 0) {
        imprimirIntegrantes();
    }

    int opcion = 0;
    int N = 0;
    bool detallado = false;

    if (rank == 0) {
        std::cout << "\n--- CONFIGURACION DE EJECUCION ---" << std::endl;
        std::cout << "1. Primera ejecucion (40 elementos - Mensajes detallados)" << std::endl;
        std::cout << "2. Segunda ejecucion (4,000,000 elementos - Solo tiempo)" << std::endl;
        std::cout << "Seleccione el tipo de prueba: ";
        int tipo;
        std::cin >> tipo;
        if (tipo == 1) {
            N = 40;
            detallado = true;
        } else {
            N = 4000000;
            detallado = false;
        }
    }

    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int flag_detallado = detallado ? 1 : 0;
    MPI_Bcast(&flag_detallado, 1, MPI_INT, 0, MPI_COMM_WORLD);
    detallado = (flag_detallado == 1);

    int n_local = N / size;

    double* A_global = nullptr;
    double* B_global = nullptr;
    double* C_global = nullptr;

    if (rank == 0) {
        A_global = new double[N];
        B_global = new double[N];
        C_global = new double[N];
    }

    double* A_local = new double[n_local];
    double* B_local = new double[n_local];
    double* C_local = new double[n_local];

    srand(time(NULL) + rank);

    do {
        if (rank == 0) {
            std::cout << "\n================ MENU DE OPERACIONES ================" << std::endl;
            std::cout << "1. Crear / Inicializar arreglos" << std::endl;
            std::cout << "2. Sumar arreglos (A + B)" << std::endl;
            std::cout << "3. Restar arreglos (A - B)" << std::endl;
            std::cout << "4. Multiplicar arreglos (A * B)" << std::endl;
            std::cout << "5. Cuadrado de un arreglo (A^2)" << std::endl;
            std::cout << "6. Salir" << std::endl;
            std::cout << "Seleccione una opcion: ";
            std::cin >> opcion;
        }

        MPI_Bcast(&opcion, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (opcion >= 1 && opcion <= 5) {
            double inicio = MPI_Wtime();

            if (opcion == 1) {
                OperacionesArreglos::crearArregloMPI(A_local, B_local, n_local, rank, hostname, detallado);
                MPI_Gather(A_local, n_local, MPI_DOUBLE, A_global, n_local, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                MPI_Gather(B_local, n_local, MPI_DOUBLE, B_global, n_local, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            } else {
                MPI_Scatter(A_global, n_local, MPI_DOUBLE, A_local, n_local, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                MPI_Scatter(B_global, n_local, MPI_DOUBLE, B_local, n_local, MPI_DOUBLE, 0, MPI_COMM_WORLD);

                if (opcion == 2) OperacionesArreglos::sumar(A_local, B_local, C_local, n_local, rank, hostname, detallado);
                else if (opcion == 3) OperacionesArreglos::restar(A_local, B_local, C_local, n_local, rank, hostname, detallado);
                else if (opcion == 4) OperacionesArreglos::multiplicar(A_local, B_local, C_local, n_local, rank, hostname, detallado);
                else if (opcion == 5) OperacionesArreglos::cuadrado(A_local, C_local, n_local, rank, hostname, detallado);

                MPI_Gather(C_local, n_local, MPI_DOUBLE, C_global, n_local, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            }

            double fin = MPI_Wtime();

            if (rank == 0) {
                std::cout << "\n[RESULTADO] Tiempo de ejecucion: " << (fin - inicio) << " segundos." << std::endl;
                if (detallado && opcion > 1) {
                    std::cout << "Arreglo C global (Resultado): ";
                    for (int i = 0; i < N; i++) std::cout << C_global[i] << " ";
                    std::cout << std::endl;
                }
            }
        }

    } while (opcion != 6);

    delete[] A_local;
    delete[] B_local;
    delete[] C_local;

    if (rank == 0) {
        delete[] A_global;
        delete[] B_global;
        delete[] C_global;
        imprimirIntegrantes();
    }

    MPI_Finalize();
    return 0;
}