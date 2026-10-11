#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <random>
#include <algorithm>
#include <limits>
#include <stdexcept>

const char* integrantes = "CARDENAS OLIVARES CESAR GIOVANNY; CERVANTES ARRIZON VICTOR HUGO; DIAZ TORRES JAZMIN MONTSERRAT";

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
        std::string directorio = std::string(carpeta) + "\\Actividad1_5";
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
    void escribir(const std::string& mensaje, int inicio = -1, int fin = -1,
                  const std::string& resultado = "No aplica", bool consola = true) {
        #pragma omp critical(registro)
        {
            SYSTEMTIME fecha;
            GetLocalTime(&fecha);
            std::ostringstream linea;
            linea << "[" << fecha.wYear << "-" << fecha.wMonth << "-" << fecha.wDay
                  << " " << fecha.wHour << ":" << fecha.wMinute << ":" << fecha.wSecond
                  << "." << fecha.wMilliseconds << "] [Equipo: " << equipo
                  << "] [Nodo MPI: " << rank << "] [Hilo OpenMP: " << omp_get_thread_num()
                  << "] [Bloque: ";
            if (inicio < 0) linea << "No aplica";
            else if (fin < inicio) linea << "vacio";
            else linea << inicio << " - " << fin;
            linea << "] [Resultado: " << resultado << "] " << mensaje;
            archivo << linea.str() << std::endl;
            if (consola) std::cout << linea.str() << std::endl;
        }
    }
};

std::string indice(int i) {
    if (i == std::numeric_limits<int>::max()) return "No encontrado";
    std::ostringstream texto;
    texto << "Encontrado en indice global " << i;
    return texto.str();
}

int leer(Registro& log, const std::string& pregunta, int minimo, int maximo) {
    for (;;) {
        log.escribir(pregunta);
        long long valor;
        if (std::cin >> valor && valor >= minimo && valor <= maximo) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::ostringstream respuesta;
            respuesta << "Entrada: " << valor;
            log.escribir(respuesta.str());
            return static_cast<int>(valor);
        }
        if (std::cin.eof()) throw std::runtime_error("Entrada finalizada antes de elegir Salir.");
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        log.escribir("Entrada invalida.");
    }
}

class Busqueda {
    static void mezclar(int* origen, int* destino, int izquierda, int medio, int derecha) {
        int i = izquierda, j = medio, k = izquierda;
        while (i < medio && j < derecha)
            destino[k++] = origen[i] <= origen[j] ? origen[i++] : origen[j++];
        while (i < medio) destino[k++] = origen[i++];
        while (j < derecha) destino[k++] = origen[j++];
    }
public:
    // Merge sort iterativo: cada par de bloques se mezcla independientemente.
    static void ordenar(int* datos, int* auxiliar, int n, bool paralelo) {
        int* origen = datos;
        int* destino = auxiliar;
        for (long long ancho = 1; ancho < n; ancho *= 2) {
            const int grupos = static_cast<int>((n + 2 * ancho - 1) / (2 * ancho));
            if (paralelo) {
                #pragma omp parallel for schedule(static)
                for (int g = 0; g < grupos; ++g) {
                    int izq = static_cast<int>(2 * ancho * g);
                    int medio = static_cast<int>(std::min<long long>(izq + ancho, n));
                    int der = static_cast<int>(std::min<long long>(izq + 2 * ancho, n));
                    mezclar(origen, destino, izq, medio, der);
                }
            } else {
                for (int g = 0; g < grupos; ++g) {
                    int izq = static_cast<int>(2 * ancho * g);
                    int medio = static_cast<int>(std::min<long long>(izq + ancho, n));
                    int der = static_cast<int>(std::min<long long>(izq + 2 * ancho, n));
                    mezclar(origen, destino, izq, medio, der);
                }
            }
            std::swap(origen, destino);
        }
        if (origen != datos) std::copy(origen, origen + n, datos);
    }

    static int buscar(const int* datos, int n, int offset, int target,
                      bool binaria, bool detalle, Registro& log) {
        int respuesta = std::numeric_limits<int>::max();
        const int partes = omp_get_max_threads();
        const bool candidato = !binaria || (n > 0 && target >= datos[0] && target <= datos[n-1]);
        // Un bloque contiguo por iteracion; parallel for asigna los bloques a los hilos.
        #pragma omp parallel for schedule(static,1) reduction(min:respuesta)
        for (int h = 0; h < partes; ++h) {
            int inicio = static_cast<int>(static_cast<long long>(n) * h / partes);
            int fin = static_cast<int>(static_cast<long long>(n) * (h + 1) / partes);
            int encontrado = std::numeric_limits<int>::max();
            if (binaria) {
                if (candidato && inicio < fin && target >= datos[inicio] && target <= datos[fin-1]) {
                    int a = inicio, b = fin;
                    while (a < b) {
                        int medio = a + (b - a) / 2;
                        if (datos[medio] < target) a = medio + 1;
                        else b = medio;
                    }
                    if (a < fin && datos[a] == target) encontrado = offset + a;
                }
            } else {
                for (int i = inicio; i < fin; ++i) {
                    if (datos[i] == target) {
                        encontrado = offset + i;
                        break;
                    }
                }
            }
            respuesta = std::min(respuesta, encontrado);
            if (detalle)
                log.escribir(binaria ? "Busqueda binaria" : "Busqueda secuencial",
                             offset + inicio, offset + fin - 1, indice(encontrado));
        }
        return respuesta;
    }
};

struct Medicion {
    bool disponible;
    double local, distribuido, comunicacion;
    Medicion() : disponible(false), local(0), distribuido(0), comunicacion(0) {}
};

void comparativa(Registro& log, const char* nombre, const Medicion& m,
                 int procesos, int hilosLocal, int hilosTotales) {
    if (!m.disponible) { log.escribir(std::string(nombre) + ": sin medicion."); return; }
    double speedup = m.distribuido > 0 ? m.local / m.distribuido : 0;
    double recursos = static_cast<double>(hilosTotales) / hilosLocal;
    std::ostringstream texto;
    texto << std::fixed << std::setprecision(9) << nombre
          << ": local OpenMP=" << m.local << " s; distribuido MPI+OpenMP="
          << m.distribuido << " s; comunicacion (maximo por proceso)="
          << m.comunicacion << " s; speedup local/distribuido=" << speedup
          << "; eficiencia relativa a hilos=" << 100 * speedup / recursos
          << "%; procesos=" << procesos << "; hilos local=" << hilosLocal
          << "; hilos distribuidos totales=" << hilosTotales;
    log.escribir(texto.str());
}

int main(int argc, char** argv) {
    int soporte = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &soporte);
    int rank = 0, procesos = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procesos);
    try {
        Registro log(rank);
        log.nombres(rank == 0);
        MPI_Barrier(MPI_COMM_WORLD);
        if (soporte < MPI_THREAD_FUNNELED) throw std::runtime_error("MPI no admite MPI_THREAD_FUNNELED.");
        omp_set_dynamic(0);
        int hilos = omp_get_max_threads(), totalHilos = 0;
        MPI_Reduce(&hilos, &totalHilos, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        SYSTEM_INFO sistema;
        GetSystemInfo(&sistema);
        const char* procesador = std::getenv("PROCESSOR_IDENTIFIER");
        std::ostringstream encabezado;
        encabezado << "Inicio; procesador=" << (procesador ? procesador : "No disponible")
                   << "; procesadores logicos=" << sistema.dwNumberOfProcessors
                   << "; hilos OpenMP=" << hilos << "; procesos MPI=" << procesos;
        log.escribir(encabezado.str());
        int* datos = NULL;
        int n = 0, detalle = 1, ordenado = 0;
        double tSec = 0, tOmp = 0;
        bool hayOrden = false;
        Medicion secuencial, binaria;
        for (;;) {
            int opcion = 0;
            if (rank == 0)
                opcion = leer(log,
                    "MENU: 1 Generar arreglo | 2 Ordenar con OpenMP | "
                    "3 Busqueda secuencial local/distribuida | "
                    "4 Busqueda binaria local/distribuida | 5 Tiempos y comparativa | 6 Salir",
                    1, 6);
            MPI_Bcast(&opcion, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (opcion == 6) break;
            if (opcion == 1) {
                int maximo = 0, semilla = 0;
                if (rank == 0) {
                    n = leer(log, "Tamano del arreglo (ej. 100 o 10000000):", 1, 200000000);
                    maximo = leer(log, "Valor aleatorio maximo (minimo=0):", 0, 2147483647);
                    semilla = leer(log, "Semilla de aleatoriedad:", 0, 2147483647);
                    detalle = leer(log, "Impresiones detalladas: 1=si (control), 0=no (rendimiento):", 0, 1);
                    delete[] datos;
                    datos = new int[n];
                    std::minstd_rand generador(static_cast<unsigned int>(semilla));
                    std::uniform_int_distribution<int> valores(0, maximo);
                    for (int i = 0; i < n; ++i) datos[i] = valores(generador);
                }
                MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
                MPI_Bcast(&detalle, 1, MPI_INT, 0, MPI_COMM_WORLD);
                ordenado = 0;
                hayOrden = false;
                secuencial = Medicion();
                binaria = Medicion();
                log.escribir("Arreglo generado en el maestro; mediciones reiniciadas.", 0, n-1, "No aplica", detalle);
                if (rank == 0 && detalle)
                    for (int i = 0; i < n; ++i) {
                        std::ostringstream texto; texto << "Dato generado: " << datos[i];
                        log.escribir(texto.str(), i, i);
                    }
            } else if (opcion == 2) {
                if (n == 0) { if (rank == 0) log.escribir("Primero genere el arreglo."); continue; }
                if (ordenado) { if (rank == 0) log.escribir("Arreglo ya ordenado; genere otro para repetir la comparacion."); continue; }
                if (rank == 0) {
                    int* copia = new int[n];
                    int* auxiliar = new int[n];
                    std::copy(datos, datos+n, copia);
                    double inicio = MPI_Wtime();
                    Busqueda::ordenar(copia, auxiliar, n, false);
                    tSec = MPI_Wtime() - inicio;
                    inicio = MPI_Wtime();
                    Busqueda::ordenar(datos, auxiliar, n, true);
                    tOmp = MPI_Wtime() - inicio;
                    if (!std::equal(datos, datos+n, copia) || !std::is_sorted(datos, datos+n))
                        throw std::runtime_error("La validacion del ordenamiento fallo.");
                    delete[] copia;
                    delete[] auxiliar;
                    if (detalle)
                        for (int i = 0; i < n; ++i) {
                            std::ostringstream texto; texto << "Dato ordenado: " << datos[i];
                            log.escribir(texto.str(), i, i);
                        }
                }
                MPI_Bcast(&tSec, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                MPI_Bcast(&tOmp, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
                ordenado = 1;
                hayOrden = true;
                secuencial = Medicion();
                binaria = Medicion();
                std::ostringstream texto;
                texto << std::fixed << std::setprecision(9)
                      << "Ordenamiento validado; secuencial=" << tSec << " s; OpenMP=" << tOmp << " s.";
                log.escribir(texto.str(), 0, n-1);
            } else if (opcion == 3 || opcion == 4) {
                if (n == 0 || (opcion == 4 && !ordenado)) {
                    if (rank == 0) log.escribir(n == 0 ? "Primero genere el arreglo." : "Primero ordene con OpenMP.");
                    continue;
                }
                int target = 0, resultadoLocal = 0;
                double tiempoLocal = 0;
                if (rank == 0) {
                    target = leer(log, "Valor a buscar:", -2147483647-1, 2147483647);
                    double inicio = MPI_Wtime();
                    resultadoLocal = Busqueda::buscar(datos, n, 0, target, opcion == 4, detalle, log);
                    tiempoLocal = MPI_Wtime() - inicio;
                    log.escribir("Resultado local OpenMP", 0, n-1, indice(resultadoLocal), detalle);
                }
                int* cantidades = new int[procesos];
                int* desplazamientos = new int[procesos];
                for (int p = 0; p < procesos; ++p) {
                    cantidades[p] = n / procesos + (p < n % procesos ? 1 : 0);
                    desplazamientos[p] = p * (n / procesos) + std::min(p, n % procesos);
                }
                int cantidad = cantidades[rank], offset = desplazamientos[rank];
                int* bloque = new int[std::max(1, cantidad)];
                MPI_Barrier(MPI_COMM_WORLD);
                double inicioTotal = MPI_Wtime(), inicioCom = MPI_Wtime();
                MPI_Bcast(&target, 1, MPI_INT, 0, MPI_COMM_WORLD);
                MPI_Scatterv(datos, cantidades, desplazamientos, MPI_INT,
                             bloque, cantidad, MPI_INT, 0, MPI_COMM_WORLD);
                double comunicacion = MPI_Wtime() - inicioCom;
                if (detalle) {
                    std::ostringstream texto; texto << "Bloque recibido; target=" << target;
                    log.escribir(texto.str(), offset, offset+cantidad-1);
                }
                int encontrado = Busqueda::buscar(bloque, cantidad, offset, target, opcion == 4, detalle, log);
                int global = 0;
                inicioCom = MPI_Wtime();
                MPI_Reduce(&encontrado, &global, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
                comunicacion += MPI_Wtime() - inicioCom;
                double transcurrido = MPI_Wtime() - inicioTotal, tiempoGlobal = 0, comGlobal = 0;
                MPI_Reduce(&transcurrido, &tiempoGlobal, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
                MPI_Reduce(&comunicacion, &comGlobal, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
                std::ostringstream tiemposNodo;
                tiemposNodo << std::fixed << std::setprecision(9)
                            << "Tiempo del nodo: comunicacion=" << comunicacion
                            << " s; total=" << transcurrido << " s.";
                log.escribir(tiemposNodo.str(), offset, offset+cantidad-1, indice(encontrado));
                if (rank == 0) {
                    if (global != resultadoLocal) throw std::runtime_error("Resultados local y distribuido diferentes.");
                    log.escribir("Resultado global validado con busqueda local", 0, n-1, indice(global), detalle);
                    Medicion& m = opcion == 3 ? secuencial : binaria;
                    m.disponible = true;
                    m.local = tiempoLocal;
                    m.distribuido = tiempoGlobal;
                    m.comunicacion = comGlobal;
                    comparativa(log, opcion == 3 ? "Secuencial" : "Binaria", m, procesos, hilos, totalHilos);
                }
                delete[] bloque;
                delete[] cantidades;
                delete[] desplazamientos;
            } else if (opcion == 5 && rank == 0) {
                if (hayOrden) {
                    std::ostringstream texto;
                    double aceleracion = tOmp > 0 ? tSec / tOmp : 0;
                    texto << std::fixed << std::setprecision(9)
                          << "Ordenamiento: secuencial=" << tSec << " s; OpenMP=" << tOmp
                          << " s; speedup=" << aceleracion
                          << "; eficiencia=" << 100 * aceleracion / hilos << "%.";
                    log.escribir(texto.str());
                } else log.escribir("Ordenamiento: sin medicion.");
                comparativa(log, "Secuencial", secuencial, procesos, hilos, totalHilos);
                comparativa(log, "Binaria", binaria, procesos, hilos, totalHilos);
            }
        }
        delete[] datos;
        log.escribir("Fin de la sesion.");
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
