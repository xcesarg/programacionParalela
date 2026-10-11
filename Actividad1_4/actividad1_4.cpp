#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <mpi.h>
#include <omp.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <new>
#include <sstream>
#include <string>

static const char *INTEGRANTES =
    "CARDENAS OLIVARES CESAR GIOVANNY; CERVANTES ARRIZON VICTOR HUGO; DIAZ TORRES JAZMIN MONTSERRAT";

static const int TAG_TRANSACCION = 10;
static const int TAG_RESPUESTA = 11;
static const int TAG_FIN = 12;

#pragma pack(push, 1)
struct Transaccion {
    int id_cliente;
    int id_hilo_openmp;
    int tipo_operacion;
    double monto;
};

struct RespuestaServidor {
    bool aprobada;
    double saldo_resultante;
    char mensaje[100];
};
#pragma pack(pop)

class Registro {
    FILE *archivo_;
    std::string equipo_;
    int rank_;

public:
    Registro(const std::string &equipo, int rank) : archivo_(NULL), equipo_(equipo), rank_(rank) {
        std::ostringstream ruta;
        char *carpetaLocal = new char[MAX_PATH];
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, carpetaLocal))) {
            delete[] carpetaLocal;
            return;
        }
        std::string directorio = std::string(carpetaLocal) + "\\Actividad1_4";
        delete[] carpetaLocal;
        if (!CreateDirectoryA(directorio.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
            return;
        ruta << directorio << "\\log_equipo_" << equipo_ << "_nodo_" << rank_ << ".txt";
        archivo_ = std::fopen(ruta.str().c_str(), "a");
    }

    bool valido() const { return archivo_ != NULL; }

    void escribir(const std::string &linea, bool consola = true) {
        if (consola) {
            std::cout << linea << std::endl;
        }
        if (archivo_) {
            std::fwrite(linea.data(), 1, linea.size(), archivo_);
            std::fwrite("\n", 1, 1, archivo_);
            std::fflush(archivo_);
        }
    }

    void cerrar() {
        if (archivo_) {
            std::fclose(archivo_);
            archivo_ = NULL;
        }
    }

    ~Registro() { cerrar(); }
};

static const char *nombreOperacion(int tipo) {
    if (tipo == 1) return "Deposito";
    if (tipo == 2) return "Retiro";
    return "Consulta";
}

static std::string dinero(double valor) {
    std::ostringstream salida;
    salida << "$" << std::fixed << std::setprecision(2) << valor;
    return salida.str();
}

static std::string mensajeTransaccion(const std::string &equipo, int proceso,
                                      const Transaccion &t, const RespuestaServidor &r) {
    std::ostringstream salida;
    salida << "[Equipo: " << equipo << "] [Proceso MPI: " << proceso << "]"
           << " [Cliente MPI: " << t.id_cliente << "]"
           << " [Hilo OpenMP: " << t.id_hilo_openmp << "]"
           << " [Operacion: " << nombreOperacion(t.tipo_operacion) << "]"
           << " [Monto: " << dinero(t.monto) << "]"
           << " [Estado: " << (r.aprobada ? "Aprobado" : "Rechazado") << "]"
           << " [Saldo: " << dinero(r.saldo_resultante) << "]"
           << " [Mensaje: " << r.mensaje << "]";
    return salida.str();
}

static unsigned int siguienteAleatorio(unsigned int &estado) {
    estado ^= estado << 13;
    estado ^= estado >> 17;
    estado ^= estado << 5;
    return estado;
}

static bool generarTransacciones(Transaccion *historial, int cantidad, int rank,
                                int numeroHilos) {
    if (!historial) return false;

    std::srand(static_cast<unsigned int>(std::time(NULL)) +
               static_cast<unsigned int>(rank) * 2654435761u);
    unsigned int *semillas = new (std::nothrow) unsigned int[numeroHilos];
    if (!semillas) return false;

    #pragma omp parallel
    {
        const int hilo = omp_get_thread_num();
        #pragma omp critical(semilla_aleatoria)
        {
            semillas[hilo] = static_cast<unsigned int>(std::rand()) ^
                             (static_cast<unsigned int>(rank + 1) * 2246822519u) ^
                             (static_cast<unsigned int>(hilo + 1) * 3266489917u);
            if (semillas[hilo] == 0) semillas[hilo] = 1;
        }
        #pragma omp barrier
        unsigned int estado = semillas[hilo];
        #pragma omp for schedule(static)
        for (int i = 0; i < cantidad; ++i) {
            const unsigned int operacion = siguienteAleatorio(estado) % 3u + 1u;
            const unsigned int centavos = siguienteAleatorio(estado) % 99901u + 100u;
            historial[i].id_cliente = rank;
            historial[i].id_hilo_openmp = hilo;
            historial[i].tipo_operacion = static_cast<int>(operacion);
            historial[i].monto = operacion == 3 ? 0.0 : centavos / 100.0;
        }
    }

    delete[] semillas;
    return true;
}

static void prepararRespuesta(const Transaccion &t, double &saldo,
                              RespuestaServidor &respuesta) {
    respuesta.aprobada = true;
    if (t.tipo_operacion == 1) {
        saldo += t.monto;
        std::snprintf(respuesta.mensaje, sizeof(respuesta.mensaje), "Deposito aplicado");
    } else if (t.tipo_operacion == 2) {
        if (saldo >= t.monto) {
            saldo -= t.monto;
            std::snprintf(respuesta.mensaje, sizeof(respuesta.mensaje), "Retiro aplicado");
        } else {
            respuesta.aprobada = false;
            std::snprintf(respuesta.mensaje, sizeof(respuesta.mensaje), "Fondos insuficientes");
        }
    } else {
        std::snprintf(respuesta.mensaje, sizeof(respuesta.mensaje), "Consulta de saldo");
    }
    respuesta.saldo_resultante = saldo;
}

static void atenderServidor(int rank, int size, const std::string &equipo, Registro &log,
                            double &saldo, bool detalle, long long &procesadas) {
        procesadas = 0;
        int clientesFinalizados = 0;
        while (clientesFinalizados < size - 1) {
            Transaccion t;
            MPI_Status estado;
            MPI_Recv(&t, static_cast<int>(sizeof(t)), MPI_BYTE, MPI_ANY_SOURCE,
                     MPI_ANY_TAG, MPI_COMM_WORLD, &estado);
            if (estado.MPI_TAG == TAG_FIN) {
                ++clientesFinalizados;
                log.escribir("[Equipo: " + equipo + "] [Nodo MPI: 0] [TAG_FIN recibido del cliente: " +
                             std::to_string(estado.MPI_SOURCE) + "]", detalle);
                continue;
            }

            RespuestaServidor respuesta;
            prepararRespuesta(t, saldo, respuesta);
            log.escribir(mensajeTransaccion(equipo, rank, t, respuesta), detalle);
            MPI_Send(&respuesta, static_cast<int>(sizeof(respuesta)), MPI_BYTE,
                     estado.MPI_SOURCE, TAG_RESPUESTA, MPI_COMM_WORLD);
            ++procesadas;
        }
}

static void ejecutarCliente(int rank, int operacionesPorCliente, const std::string &equipo,
                            Registro &log, Transaccion *historial, bool detalle) {
        for (int i = 0; i < operacionesPorCliente; ++i) {
            MPI_Send(&historial[i], static_cast<int>(sizeof(Transaccion)), MPI_BYTE,
                     0, TAG_TRANSACCION, MPI_COMM_WORLD);
            RespuestaServidor respuesta;
            MPI_Recv(&respuesta, static_cast<int>(sizeof(respuesta)), MPI_BYTE,
                     0, TAG_RESPUESTA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            log.escribir(mensajeTransaccion(equipo, rank, historial[i], respuesta), detalle);
        }
        Transaccion fin = {rank, -1, 0, 0.0};
        MPI_Send(&fin, static_cast<int>(sizeof(fin)), MPI_BYTE, 0, TAG_FIN, MPI_COMM_WORLD);
        log.escribir("[Equipo: " + equipo + "] [Nodo MPI: " + std::to_string(rank) +
                     "] [TAG_FIN enviado al servidor]", detalle);
        delete[] historial;
}

static void atenderSimulacion(int rank, int size, int operacionesPorCliente,
                              const std::string &equipo, Registro &log,
                              double &saldo, bool detalle, long long &procesadas) {
    Transaccion *historial = NULL;
    if (rank != 0) historial = new (std::nothrow) Transaccion[operacionesPorCliente];
    int memoriaDisponible = (rank == 0 || historial != NULL) ? 1 : 0;
    int memoriaTodos = 0;
    MPI_Allreduce(&memoriaDisponible, &memoriaTodos, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);
    if (!memoriaTodos) {
        delete[] historial;
        if (rank == 0) log.escribir("No se pudo reservar el historial de transacciones.");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return;
    }

    int historialGenerado = 1;
    if (rank != 0) {
        historialGenerado = generarTransacciones(historial, operacionesPorCliente, rank,
                                                 omp_get_max_threads()) ? 1 : 0;
    }
    int historialesGenerados = 0;
    MPI_Allreduce(&historialGenerado, &historialesGenerados, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);
    if (!historialesGenerados) {
        delete[] historial;
        if (rank == 0) log.escribir("No se pudo generar el historial de transacciones.");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return;
    }

    if (rank == 0)
        atenderServidor(rank, size, equipo, log, saldo, detalle, procesadas);
    else
        ejecutarCliente(rank, operacionesPorCliente, equipo, log, historial, detalle);
}
static int leerOpcion(Registro &log) {
    log.escribir("Menu: 1 Simulacion basica | 2 Simulacion masiva | 3 Consultar saldo final | 4 Salir");
    log.escribir("Seleccione una opcion (1-4):");
    std::string linea;
    if (!std::getline(std::cin, linea)) return 4;
    std::istringstream entrada(linea);
    int opcion = 0;
    char sobrante;
    if (!(entrada >> opcion) || (entrada >> sobrante) || opcion < 1 || opcion > 4) {
        log.escribir("Opcion invalida.");
        return 0;
    }
    log.escribir("Opcion seleccionada: " + linea);
    return opcion;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank = 0, size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 5) {
        if (rank == 0) std::fprintf(stderr, "La practica requiere 5 procesos MPI: 1 servidor y 4 clientes.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    char nombreEquipo[MPI_MAX_PROCESSOR_NAME];
    int longitudNombre = 0;
    MPI_Get_processor_name(nombreEquipo, &longitudNombre);
    const std::string equipo(nombreEquipo, longitudNombre);
    Registro log(equipo, rank);

    int logDisponible = log.valido() ? 1 : 0;
    int logsDisponibles = 0;
    MPI_Allreduce(&logDisponible, &logsDisponibles, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!logsDisponibles) {
        if (rank == 0) std::fprintf(stderr, "No se pudo crear el archivo de log local.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    if (rank == 0) log.escribir(INTEGRANTES);
    MPI_Barrier(MPI_COMM_WORLD);

    std::ostringstream encabezado;
    encabezado << "[Equipo: " << equipo << "] [Nodo MPI: " << rank
               << "] [Hilos OpenMP disponibles: " << omp_get_max_threads() << "] Inicio";
    log.escribir(encabezado.str());

    omp_set_dynamic(0);
    double saldo = 10000.0;
    long long procesadas = 0;
    bool salir = false;
    while (!salir) {
        int opcion = 0;
        if (rank == 0) opcion = leerOpcion(log);
        MPI_Bcast(&opcion, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (opcion == 0) continue;
        if (opcion == 4) {
            salir = true;
            continue;
        }

        if (opcion == 3) {
            if (rank == 0) {
                std::ostringstream consulta;
                consulta << "[Equipo: " << equipo << "] [Proceso MPI: 0] [Operacion: Consulta de saldo final]"
                         << " [Saldo: " << dinero(saldo) << "]";
                log.escribir(consulta.str());
            }
            continue;
        }

        int operaciones = opcion == 1 ? 5 : 100000;
        if (rank == 0 && opcion == 2) {
            const char *limite = std::getenv("ACTIVIDAD1_4_OPERACIONES_CLIENTE");
            if (limite && *limite) {
                char *fin = NULL;
                const long cantidad = std::strtol(limite, &fin, 10);
                if (fin != limite && *fin == '\0' && cantidad > 0 && cantidad <= 100000)
                    operaciones = static_cast<int>(cantidad);
                else
                    log.escribir("Limite de prueba invalido; se usaran 100000 operaciones por cliente.");
            }
        }
        MPI_Bcast(&operaciones, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (opcion == 2 && operaciones != 100000) {
            log.escribir("[Equipo: " + equipo + "] [Nodo MPI: " + std::to_string(rank) +
                         "] [Prueba abreviada: " + std::to_string(operaciones) +
                         " operaciones por cliente; total esperado: " +
                         std::to_string(static_cast<long long>(operaciones) * (size - 1)) + "]",
                         rank == 0);
        }
        const bool detalle = opcion == 1;
        MPI_Barrier(MPI_COMM_WORLD);
        const double inicio = MPI_Wtime();
        atenderSimulacion(rank, size, operaciones, equipo, log, saldo, detalle, procesadas);
        MPI_Barrier(MPI_COMM_WORLD);
        const double tiempoLocal = MPI_Wtime() - inicio;
        double tiempoTotal = 0.0;
        MPI_Reduce(&tiempoLocal, &tiempoTotal, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        std::ostringstream tiempoNodo;
        tiempoNodo << std::fixed << std::setprecision(6) << tiempoLocal;
        log.escribir("[Equipo: " + equipo + "] [Nodo MPI: " + std::to_string(rank) +
                     "] [Tiempo MPI_Wtime local: " + tiempoNodo.str() + " s]", false);
        if (rank == 0) {
            std::ostringstream resultado;
            resultado << std::fixed << std::setprecision(6) << tiempoTotal;
            if (detalle) {
                log.escribir("[Equipo: " + equipo + "] [Nodo MPI: 0] [Tiempo total: " +
                             resultado.str() + " s] [Transacciones: " +
                             std::to_string(procesadas) + "] [Saldo final: " + dinero(saldo) + "]");
            } else {
                log.escribir("Tiempo total MPI_Wtime: " + resultado.str() + " s");
                log.escribir("Transacciones procesadas por el servidor: " + std::to_string(procesadas));
                log.escribir("Saldo final consolidado: " + dinero(saldo));
            }
        }
    }

    log.escribir("[Equipo: " + equipo + "] [Nodo MPI: " + std::to_string(rank) + "] Fin de sesion", false);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) log.escribir(INTEGRANTES);
    MPI_Barrier(MPI_COMM_WORLD);
    log.cerrar();
    MPI_Finalize();
    return 0;
}
