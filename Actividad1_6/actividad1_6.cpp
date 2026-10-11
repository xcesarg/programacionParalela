#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <mpi.h>
#include <omp.h>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

typedef unsigned long long Indice;

const char* integrantes = "CARDENAS OLIVARES CESAR GIOVANNY; CERVANTES ARRIZON VICTOR HUGO; DIAZ TORRES JAZMIN MONTSERRAT";
const char* alfabeto = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const int etiquetaHallazgo = 41;
const int etiquetaHilo = 42;
const int etiquetaTerminado = 43;
const int etiquetaParada = 44;
const Indice tamanoLote = 4096ULL;

class Registro {
    std::ofstream archivo;
    std::string equipo;
    int rank;
public:
    Registro(int r) : rank(r) {
        char* nombre = new char[MPI_MAX_PROCESSOR_NAME + 1];
        int longitud = 0;
        MPI_Get_processor_name(nombre, &longitud);
        equipo.assign(nombre, longitud);
        delete[] nombre;

        char* carpeta = new char[MAX_PATH];
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, carpeta))) {
            delete[] carpeta;
            throw std::runtime_error("No se pudo localizar AppData local.");
        }
        std::string directorio = std::string(carpeta) + "\\Actividad1_6";
        delete[] carpeta;
        if (!CreateDirectoryA(directorio.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
            throw std::runtime_error("No se pudo crear la carpeta de logs.");

        std::ostringstream ruta;
        ruta << directorio << "\\log_equipo_" << equipo << "_nodo_" << rank << ".txt";
        archivo.open(ruta.str().c_str(), std::ios::app);
        if (!archivo) throw std::runtime_error("No se pudo abrir el log local.");
    }

    void nombres(bool consola) {
        archivo << integrantes << std::endl;
        if (consola) std::cout << integrantes << std::endl;
    }

    void escribir(const std::string& estado, const std::string& candidato = "No aplica",
                  Indice inicio = std::numeric_limits<Indice>::max(),
                  Indice fin = std::numeric_limits<Indice>::max(), int hilo = -1, bool consola = true) {
        #pragma omp critical(registro)
        {
            SYSTEMTIME fecha;
            GetLocalTime(&fecha);
            std::ostringstream linea;
            linea << "[" << fecha.wYear << "-" << fecha.wMonth << "-" << fecha.wDay
                  << " " << fecha.wHour << ":" << fecha.wMinute << ":" << fecha.wSecond
                  << "." << fecha.wMilliseconds << "] [Equipo: " << equipo
                  << "] [Nodo MPI: " << rank << "] [Hilo OpenMP: ";
            if (hilo < 0) linea << omp_get_thread_num(); else linea << hilo;
            linea << "] [Rango: ";
            if (inicio == std::numeric_limits<Indice>::max()) linea << "No aplica";
            else if (fin <= inicio) linea << "vacio";
            else linea << inicio << " - " << (fin - 1);
            linea << "] [Candidato: " << candidato << "] [Estado: " << estado << "]";
            archivo << linea.str() << std::endl;
            if (consola) std::cout << linea.str() << std::endl;
        }
    }
};

bool claveValida(const std::string& clave) {
    if (clave.empty() || clave.size() > 12) return false;
    for (std::size_t i = 0; i < clave.size(); ++i)
        if (std::strchr(alfabeto, clave[i]) == NULL) return false;
    return true;
}

Indice contarCombinaciones(int longitud) {
    Indice total = 1;
    for (int i = 0; i < longitud; ++i) total *= 36ULL;
    return total;
}

void convertirIndice(Indice indice, char* candidato, int longitud) {
    candidato[longitud] = '\0';
    for (int posicion = longitud - 1; posicion >= 0; --posicion) {
        candidato[posicion] = alfabeto[indice % 36ULL];
        indice /= 36ULL;
    }
}

char* convertirIndice(Indice indice, int longitud) {
    char* candidato = new char[static_cast<std::size_t>(longitud) + 1];
    convertirIndice(indice, candidato, longitud);
    return candidato;
}

void imprimirExito(Registro& log, const char* clave, Indice indice,
                   int hilo, double parcial) {
    std::ostringstream texto;
    texto << "[EXITO] Clave encontrada en indice " << indice
          << "; tiempo parcial=" << std::fixed << std::setprecision(6) << parcial << " s";
    log.escribir(texto.str(), clave, indice, indice + 1, hilo);
}

void busquedaSecuencial(const char* clave, int longitud, Indice total, bool detalle,
                        Registro& log, Indice& resultado, double& segundos) {
    resultado = std::numeric_limits<Indice>::max();
    const double inicio = MPI_Wtime();
    char* candidato = new char[static_cast<std::size_t>(longitud) + 1];
    for (Indice i = 0; i < total; ++i) {
        convertirIndice(i, candidato, longitud);
        if (detalle) log.escribir("Evaluando", candidato, i, i + 1, 0);
        const bool coincide = std::strcmp(candidato, clave) == 0;
        if (coincide) {
            resultado = i;
            break;
        }
    }
    delete[] candidato;
    segundos = MPI_Wtime() - inicio;
    if (resultado == std::numeric_limits<Indice>::max())
        log.escribir("Rango secuencial agotado; clave no encontrada.");
    else imprimirExito(log, clave, resultado, 0, segundos);
}

void esperarParada(Registro& log, Indice inicio, Indice fin) {
    int disponible = 0;
    MPI_Status estado;
    while (!disponible) {
        MPI_Iprobe(0, etiquetaParada, MPI_COMM_WORLD, &disponible, &estado);
        if (!disponible) Sleep(1);
    }
    int parada = 0;
    MPI_Recv(&parada, 1, MPI_INT, 0, etiquetaParada, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    log.escribir("Orden de parada general recibida; proceso detenido limpiamente.",
                 "No aplica", inicio, fin);
}

void buscarRangoParalelo(const char* clave, int longitud, Indice inicio, Indice cantidad,
                         bool detalle, Registro& log) {
    const Indice fin = inicio + cantidad;
    log.escribir("Inicio de busqueda; rango asignado.", "No aplica", inicio, fin, 0, detalle);
    std::atomic<bool> encontrado(false), paradaGeneral(false);
    const double inicioBusqueda = MPI_Wtime();

    #pragma omp parallel
    {
        log.escribir("Inicio de hilo OpenMP.", "No aplica", inicio, fin,
                     omp_get_thread_num(), detalle);
    }
    // Los lotes permiten abandonar el rango sin recorrer iteraciones pendientes.
    for (Indice lote = inicio; lote < fin && !encontrado.load() && !paradaGeneral.load();
         lote += tamanoLote) {
        const Indice limite = (fin - lote < tamanoLote) ? fin : lote + tamanoLote;
        #pragma omp parallel
        {
            const int hilo = omp_get_thread_num();
            unsigned revisadas = 0;
            char* candidato = new char[static_cast<std::size_t>(longitud) + 1];
            #pragma omp for schedule(dynamic, 64)
            for (long long valor = static_cast<long long>(lote);
                 valor < static_cast<long long>(limite); ++valor) {
                if (encontrado.load() || paradaGeneral.load()) continue;
                if (revisadas++ % 64 == 0) {
                    // MPI_THREAD_SERIALIZED: nunca hay dos llamadas MPI simultaneas.
                    #pragma omp critical(mpi_local)
                    {
                        int disponible = 0;
                        MPI_Iprobe(0, etiquetaParada, MPI_COMM_WORLD, &disponible, MPI_STATUS_IGNORE);
                        if (disponible) {
                            int parada = 0;
                            MPI_Recv(&parada, 1, MPI_INT, 0, etiquetaParada,
                                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                            paradaGeneral.store(true);
                        }
                    }
                }
                if (paradaGeneral.load()) continue;
                const Indice indice = static_cast<Indice>(valor);
                convertirIndice(indice, candidato, longitud);
                if (detalle) log.escribir("Evaluando", candidato, indice, indice + 1, hilo);
                if (std::strcmp(candidato, clave) == 0) {
                    bool esperado = false;
                    if (encontrado.compare_exchange_strong(esperado, true)) {
                        #pragma omp critical(mpi_local)
                        {
                            // El hilo avisa al maestro antes de esperar a los demas hilos.
                            MPI_Send(&indice, 1, MPI_UNSIGNED_LONG_LONG, 0,
                                     etiquetaHallazgo, MPI_COMM_WORLD);
                            MPI_Send(&hilo, 1, MPI_INT, 0, etiquetaHilo, MPI_COMM_WORLD);
                            imprimirExito(log, clave, indice, hilo, MPI_Wtime() - inicioBusqueda);
                        }
                    }
                }
            }
            delete[] candidato;
        }
        if (detalle && !encontrado.load() && !paradaGeneral.load())
            log.escribir("Lote procesado.", "No aplica", lote, limite);
    }
    if (paradaGeneral.load())
        log.escribir("Parada temprana recibida; se cancela el resto del rango.", "No aplica", inicio, fin);
    else if (!encontrado.load())
        log.escribir("Rango asignado agotado.", "No aplica", inicio, fin, 0, detalle);
    #pragma omp parallel
    {
        log.escribir("Fin de hilo OpenMP.", "No aplica", inicio, fin,
                     omp_get_thread_num(), detalle);
    }
    MPI_Send(NULL, 0, MPI_CHAR, 0, etiquetaTerminado, MPI_COMM_WORLD);
    if (!paradaGeneral.load()) esperarParada(log, inicio, fin);
    log.escribir("Fin de proceso esclavo.");
}
void busquedaDistribuida(const char* clave, int longitud, Indice total,
                         bool detalle, int rank, int procesos, Registro& log,
                         Indice& resultado, double& segundos) {
    const int esclavos = procesos - 1;
    if (esclavos < 1) {
        if (rank == 0) log.escribir("Se requiere al menos un proceso esclavo; ejecute con 2 o mas procesos MPI.");
        resultado = std::numeric_limits<Indice>::max();
        segundos = 0;
        return;
    }

    Indice* limites = new Indice[static_cast<std::size_t>(procesos) * 2];
    Indice* locales = new Indice[2];
    if (rank == 0) {
        const Indice base = total / static_cast<Indice>(esclavos);
        const Indice residuo = total % static_cast<Indice>(esclavos);
        limites[0] = 0;
        limites[1] = 0;
        for (int proceso = 1; proceso < procesos; ++proceso) {
            const Indice posicion = static_cast<Indice>(proceso - 1);
            const Indice cantidad = base + (posicion < residuo ? 1ULL : 0ULL);
            limites[static_cast<std::size_t>(proceso) * 2] = posicion * base + (posicion < residuo ? posicion : residuo);
            limites[static_cast<std::size_t>(proceso) * 2 + 1] = cantidad;
        }
        std::ostringstream msg;
        msg << "Espacio total=" << total << "; esclavos=" << esclavos
            << "; longitud=" << longitud << "; modo detallado=" << (detalle ? "si" : "no");
        log.escribir(msg.str(), "No aplica", 0, total, 0, detalle);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double inicioTotal = MPI_Wtime();
    MPI_Scatter(limites, 2, MPI_UNSIGNED_LONG_LONG, locales, 2,
                MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
    delete[] limites;

    const Indice inicio = locales[0];
    const Indice cantidad = locales[1];
    if (rank != 0) {
        buscarRangoParalelo(clave, longitud, inicio, cantidad, detalle, log);
    } else {
        bool* finalizado = new bool[procesos];
        for (int p = 0; p < procesos; ++p) finalizado[p] = false;
        int faltan = esclavos;
        bool hallazgo = false;
        bool paradaEnviada = false;
        double tiempoHallazgo = 0;
        Indice indiceGlobal = std::numeric_limits<Indice>::max();
        int rankGanador = -1, hiloGanador = -1;
        MPI_Request* solicitudes = new MPI_Request[procesos];
        int* mensajesParada = new int[procesos];
        for (int p = 0; p < procesos; ++p) { solicitudes[p] = MPI_REQUEST_NULL; mensajesParada[p] = 1; }

        while (faltan > 0) {
            int disponible = 0;
            MPI_Status estado;
            MPI_Iprobe(MPI_ANY_SOURCE, etiquetaHallazgo, MPI_COMM_WORLD, &disponible, &estado);
            if (disponible) {
                Indice indice = 0;
                int hilo = -1;
                const int fuente = estado.MPI_SOURCE;
                MPI_Recv(&indice, 1, MPI_UNSIGNED_LONG_LONG, fuente,
                         etiquetaHallazgo, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&hilo, 1, MPI_INT, fuente, etiquetaHilo,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (!hallazgo) {
                    hallazgo = true;
                    indiceGlobal = indice;
                    rankGanador = fuente;
                    hiloGanador = hilo;
                    tiempoHallazgo = MPI_Wtime() - inicioTotal;
                    log.escribir("[EXITO] Maestro recibe coincidencia; enviando parada temprana a los esclavos.",
                                 clave, indice, indice + 1, 0);
                    for (int p = 1; p < procesos; ++p)
                        MPI_Isend(&mensajesParada[p], 1, MPI_INT, p, etiquetaParada,
                                  MPI_COMM_WORLD, &solicitudes[p]);
                    paradaEnviada = true;
                }
            }

            disponible = 0;
            MPI_Iprobe(MPI_ANY_SOURCE, etiquetaTerminado, MPI_COMM_WORLD, &disponible, &estado);
            if (disponible) {
                const int fuente = estado.MPI_SOURCE;
                MPI_Recv(NULL, 0, MPI_CHAR, fuente, etiquetaTerminado,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (!finalizado[fuente]) {
                    finalizado[fuente] = true;
                    --faltan;
                    log.escribir("Busqueda finalizada en nodo MPI " + std::to_string(fuente) + ".",
                                 "No aplica", std::numeric_limits<Indice>::max(),
                                 std::numeric_limits<Indice>::max(), 0, detalle);
                }
            }
            if (!disponible) Sleep(1);
        }

        int hallazgoPendiente = 0;
        MPI_Status estadoPendiente;
        MPI_Iprobe(MPI_ANY_SOURCE, etiquetaHallazgo, MPI_COMM_WORLD,
                   &hallazgoPendiente, &estadoPendiente);
        while (hallazgoPendiente) {
            Indice indice = 0;
            int hilo = -1;
            const int fuente = estadoPendiente.MPI_SOURCE;
            MPI_Recv(&indice, 1, MPI_UNSIGNED_LONG_LONG, fuente,
                     etiquetaHallazgo, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&hilo, 1, MPI_INT, fuente, etiquetaHilo,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (!hallazgo) {
                hallazgo = true;
                indiceGlobal = indice;
                rankGanador = fuente;
                hiloGanador = hilo;
                tiempoHallazgo = MPI_Wtime() - inicioTotal;
                log.escribir("[EXITO] Maestro recibe coincidencia; enviando parada temprana a los esclavos.",
                             clave, indice, indice + 1, 0);
            }
            MPI_Iprobe(MPI_ANY_SOURCE, etiquetaHallazgo, MPI_COMM_WORLD,
                       &hallazgoPendiente, &estadoPendiente);
        }

        if (!paradaEnviada) {
            for (int p = 1; p < procesos; ++p)
                MPI_Isend(&mensajesParada[p], 1, MPI_INT, p, etiquetaParada,
                          MPI_COMM_WORLD, &solicitudes[p]);
        }
        MPI_Waitall(procesos, solicitudes, MPI_STATUSES_IGNORE);
        segundos = hallazgo ? tiempoHallazgo : MPI_Wtime() - inicioTotal;
        resultado = indiceGlobal;
        if (hallazgo) {
            char* encontrada = convertirIndice(indiceGlobal, longitud);
            if (std::strcmp(encontrada, clave) != 0)
                throw std::runtime_error("El resultado distribuido no coincide con la clave.");
            std::ostringstream resumen;
            resumen << "Clave global validada; nodo ganador=" << rankGanador
                    << "; hilo ganador=" << hiloGanador << "; indice=" << indiceGlobal
                    << "; tiempo total=" << std::fixed << std::setprecision(6) << segundos
                    << " s";
            log.escribir(resumen.str(), encontrada, indiceGlobal, indiceGlobal + 1, 0);
            delete[] encontrada;
        } else {
            log.escribir("Espacio completo agotado; clave no encontrada.", "No aplica", 0, total, 0);
        }
        delete[] finalizado;
        delete[] solicitudes;
        delete[] mensajesParada;
    }

    delete[] locales;
    MPI_Barrier(MPI_COMM_WORLD);
}

void comparar(Registro& log, bool haySecuencial, double secuencial,
              bool hayDistribuida, double distribuida,
              int procesos, int hilos) {
    if (!haySecuencial || !hayDistribuida) {
        log.escribir("Ejecute primero los modos secuencial y distribuido para comparar.");
        return;
    }
    const double speedup = distribuida > 0 ? secuencial / distribuida : 0;
    const double eficiencia = procesos > 1
        ? 100.0 * speedup / procesos : 0;
    log.escribir("Modo | Tiempo (s)");
    std::ostringstream fila;
    fila << std::fixed << std::setprecision(6) << "Secuencial (1 nodo, 1 hilo) | " << secuencial;
    log.escribir(fila.str());
    fila.str(""); fila.clear();
    fila << std::fixed << std::setprecision(6) << "Paralelo distribuido (MPI + OpenMP) | " << distribuida;
    log.escribir(fila.str());
    fila.str(""); fila.clear();
    fila << std::fixed << std::setprecision(6)
         << "Speedup=" << speedup << "; eficiencia=" << eficiencia
         << "%; p (procesos MPI)=" << procesos << "; hilos por esclavo=" << hilos << ".";
    log.escribir(fila.str());
}

int leerOpcion(Registro& log) {
    for (;;) {
        log.escribir("MENU: 1 Ingresar clave | 2 Busqueda secuencial | 3 Busqueda paralela distribuida | 4 Comparativa | 5 Salir");
        int opcion = 0;
        if (std::cin >> opcion && opcion >= 1 && opcion <= 5) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::ostringstream entrada; entrada << "Opcion elegida: " << opcion;
            log.escribir(entrada.str());
            return opcion;
        }
        if (std::cin.eof()) throw std::runtime_error("Entrada finalizada antes de elegir Salir.");
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        log.escribir("Opcion invalida.");
    }
}

int main(int argc, char** argv) {
    int soporte = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &soporte);
    int rank = 0, procesos = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procesos);
    try {
        Registro log(rank);
        log.nombres(rank == 0);
        MPI_Barrier(MPI_COMM_WORLD);
        if (soporte < MPI_THREAD_SERIALIZED)
            throw std::runtime_error("MPI no admite MPI_THREAD_SERIALIZED.");
        omp_set_dynamic(0);

        SYSTEM_INFO sistema;
        GetSystemInfo(&sistema);
        const char* procesador = std::getenv("PROCESSOR_IDENTIFIER");
        std::ostringstream inicio;
        inicio << "Inicio de proceso; procesador=" << (procesador ? procesador : "No disponible")
               << "; procesadores logicos=" << sistema.dwNumberOfProcessors
               << "; rango MPI=" << rank << "; hilos OpenMP=" << omp_get_max_threads()
               << "; procesos MPI=" << procesos;
        log.escribir(inicio.str());

        char* clave = NULL;
        int longitud = 0;
        Indice total = 0;
        bool haySecuencial = false, hayDistribuida = false;
        double tiempoSecuencial = 0, tiempoDistribuido = 0;
        bool salir = false;

        while (!salir) {
            int opcion = 0;
            if (rank == 0) opcion = leerOpcion(log);
            MPI_Bcast(&opcion, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (opcion == 5) { salir = true; continue; }

            if (opcion == 1) {
                if (rank == 0) {
                    for (;;) {
                        log.escribir("Ingrese clave en mayusculas (A-Z, 0-9; longitud de 1 a 12):");
                        std::string nueva;
                        std::getline(std::cin, nueva);
                        if (claveValida(nueva)) {
                            delete[] clave;
                            longitud = static_cast<int>(nueva.size());
                            clave = new char[static_cast<std::size_t>(longitud) + 1];
                            std::memcpy(clave, nueva.c_str(), static_cast<std::size_t>(longitud) + 1);
                            total = contarCombinaciones(longitud);
                            std::ostringstream respuesta;
                            respuesta << "Clave capturada; longitud=" << longitud
                                      << "; espacio de busqueda=" << total << " combinaciones.";
                            log.escribir(respuesta.str(), clave);
                            break;
                        }
                        log.escribir("Clave invalida: use solo A-Z y 0-9, con longitud de 1 a 12.", nueva);
                    }
                }
                MPI_Bcast(&longitud, 1, MPI_INT, 0, MPI_COMM_WORLD);
                if (rank != 0) { delete[] clave; clave = new char[static_cast<std::size_t>(longitud) + 1]; }
                MPI_Bcast(clave, longitud + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
                total = contarCombinaciones(longitud);
                haySecuencial = hayDistribuida = false;
                log.escribir("Clave y longitud recibidas del maestro.", clave, 0, total, 0, longitud == 3);
            } else if (opcion == 2 || opcion == 3) {
                if (longitud == 0) {
                    if (rank == 0) log.escribir("Primero ingrese una clave.");
                    continue;
                }
                const bool detalle = longitud == 3;
                if (opcion == 2) {
                    if (rank == 0) {
                        Indice resultado = std::numeric_limits<Indice>::max();
                        busquedaSecuencial(clave, longitud, total, detalle, log,
                                           resultado, tiempoSecuencial);
                        haySecuencial = true;
                        if (resultado != std::numeric_limits<Indice>::max()) {
                            char* encontrada = convertirIndice(resultado, longitud);
                            std::ostringstream resumen;
                            resumen << "Modo secuencial completado; indice=" << resultado
                                    << "; tiempo=" << std::fixed << std::setprecision(6)
                                    << tiempoSecuencial << " s.";
                            log.escribir(resumen.str(), encontrada, resultado, resultado + 1, 0);
                            delete[] encontrada;
                        }
                    }
                } else {
                    Indice resultado = std::numeric_limits<Indice>::max();
                    double tiempo = 0;
                    busquedaDistribuida(clave, longitud, total, detalle, rank, procesos,
                                        log, resultado, tiempo);
                    if (rank == 0) {
                        tiempoDistribuido = tiempo;

                        hayDistribuida = procesos > 1;
                    }
                }
            } else if (opcion == 4 && rank == 0) {
                comparar(log, haySecuencial, tiempoSecuencial,
                         hayDistribuida, tiempoDistribuido, procesos,
                         omp_get_max_threads());
            }
        }

        delete[] clave;
        log.escribir("Fin de proceso MPI; cierre limpio.");
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
        log.nombres(rank == 0);
    } catch (const std::exception& error) {
        std::cerr << "[Nodo MPI: " << rank << "] Error: " << error.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    return 0;
}
