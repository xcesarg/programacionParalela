#include <iostream>
#include <string>
#include <omp.h>

const std::string CARACTERES = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

unsigned long long calcularEspacio(int longitud) {
    unsigned long long total = 1;
    for (int i = 0; i < longitud; i++) {
        total *= CARACTERES.size();
    }
    return total;
}

std::string posicionAClave(unsigned long long posicion, int longitud) {
    std::string combinacion(longitud, 'A');
    // Conversión a base 36: el último carácter cambia más rápido.
    for (int i = longitud - 1; i >= 0; i--) {
        combinacion[i] = CARACTERES[posicion % CARACTERES.size()];
        posicion /= CARACTERES.size();
    }
    return combinacion;
}

bool claveValida(const std::string& clave, int longitud) {
    if (clave.empty() || clave.size() != static_cast<unsigned int>(longitud)) {
        return false;
    }
    for (int i = 0; i < longitud; i++) {
        if (CARACTERES.find(clave[i]) == std::string::npos) {
            return false;
        }
    }
    return true;
}

void buscarSecuencial(const std::string& clave, int longitud,
                     unsigned long long total, std::string& resultado,
                     unsigned long long& revisadas, double& tiempo) {
    resultado.clear();
    revisadas = 0;
    std::cout << "\nInicio de la búsqueda secuencial.\n";
    double inicio = omp_get_wtime();
    for (unsigned long long posicion = 0; posicion < total; posicion++) {
        std::string combinacion = posicionAClave(posicion, longitud);
        revisadas++;
        if (combinacion == clave) {
            resultado = combinacion;
            break;
        }
    }
    tiempo = omp_get_wtime() - inicio;
    std::cout << "Fin de la búsqueda secuencial.\n";
}

void buscarParalela(const std::string& clave, int longitud,
                    unsigned long long total, std::string& resultado,
                    int& ganador, int& hilosUtilizados, double& tiempo) {
    bool encontrada = false;
    resultado.clear();
    ganador = -1;
    unsigned long long* inicios = nullptr;
    unsigned long long* finales = nullptr;
    unsigned long long* cantidades = nullptr;

    std::cout << "\nInicio de la búsqueda paralela.\n";
    double inicio = omp_get_wtime();
    // Cada hilo ejecuta este bloque y recorre solamente su propio rango.
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int numeroHilos = omp_get_num_threads();

        // Un hilo reserva los arreglos; la barrera implícita espera su creación.
        #pragma omp single
        {
            hilosUtilizados = numeroHilos;
            inicios = new unsigned long long[numeroHilos];
            finales = new unsigned long long[numeroHilos];
            cantidades = new unsigned long long[numeroHilos];
        }

        // Los primeros 'sobrante' hilos reciben una combinación adicional.
        unsigned long long base = total / numeroHilos;
        unsigned long long sobrante = total % numeroHilos;
        unsigned long long extrasAnteriores = id;
        if (extrasAnteriores > sobrante) {
            extrasAnteriores = sobrante;
        }
        cantidades[id] = base;
        if (static_cast<unsigned long long>(id) < sobrante) {
            cantidades[id]++;
        }
        inicios[id] = id * base + extrasAnteriores;
        finales[id] = inicios[id];
        if (cantidades[id] > 0) {
            finales[id] = inicios[id] + cantidades[id] - 1;
        }

        // Evita que los mensajes de distintos hilos se mezclen.
        #pragma omp critical(salida)
        {
            std::cout << "Hilo " << id << " -> Inicio: ";
            if (cantidades[id] > 0) {
                std::cout << posicionAClave(inicios[id], longitud)
                          << " -> Fin: " << posicionAClave(finales[id], longitud);
            } else {
                std::cout << "sin rango -> Fin: sin rango";
            }
            std::cout << " -> Cantidad: " << cantidades[id]
                      << " -> Inició su búsqueda.\n";
        }

        bool encontroEsteHilo = false;
        bool detenido = false;
        for (unsigned long long revisadas = 0; revisadas < cantidades[id]; revisadas++) {
            bool detener;
            // Todas las lecturas y escrituras concurrentes de la bandera son atómicas.
            #pragma omp atomic read
            detener = encontrada;
            if (detener) {
                detenido = true;
                break;
            }

            std::string combinacion = posicionAClave(inicios[id] + revisadas, longitud);
            if (combinacion == clave) {
                // Solo un hilo registra el resultado y después publica la bandera.
                #pragma omp critical(resultado)
                {
                    bool yaEncontrada;
                    #pragma omp atomic read
                    yaEncontrada = encontrada;
                    if (!yaEncontrada) {
                        resultado = combinacion;
                        ganador = id;
                        encontroEsteHilo = true;
                        #pragma omp atomic write
                        encontrada = true;
                    }
                }
                detenido = !encontroEsteHilo;
                break;
            }
        }

        #pragma omp critical(salida)
        {
            if (encontroEsteHilo) {
                std::cout << "Clave encontrada por el hilo: " << id << '\n';
            }
            std::cout << "Hilo " << id << " -> Terminó su búsqueda: ";
            if (encontroEsteHilo) {
                std::cout << "encontró la clave.\n";
            } else if (detenido) {
                std::cout << "detenido porque otro hilo encontró la clave.\n";
            } else {
                std::cout << "rango agotado sin encontrar la clave.\n";
            }
        }
    }
    tiempo = omp_get_wtime() - inicio;
    std::cout << "Fin de la búsqueda paralela.\n";

    delete[] inicios;
    delete[] finales;
    delete[] cantidades;
}

int main() {
    std::cout.setf(std::ios::fixed);
    std::cout.precision(9);

    while (true) {
        std::string opcion;
        std::cout << "\n1. Ejecutar búsqueda\n2. Salir\nOpción: ";
        if (!std::getline(std::cin, opcion) || opcion == "2") {
            break;
        }
        if (opcion != "1") {
            std::cout << "Opción inválida.\n";
            continue;
        }

        int longitud = 0;
        while (longitud == 0) {
            std::string entrada;
            std::cout << "Longitud de la clave (3, 5, 6, 7 o 10): ";
            if (!std::getline(std::cin, entrada)) {
                return 0;
            }
            if (entrada == "3" || entrada == "5" || entrada == "6" || entrada == "7") {
                longitud = entrada[0] - '0';
            } else if (entrada == "10") {
                longitud = 10;
            } else {
                std::cout << "Longitud inválida. Usa 3, 5, 6, 7 o 10.\n";
            }
        }

        std::string clave;
        bool valida;
        do {
            std::cout << "Clave de prueba (A-Z y 0-9): ";
            if (!std::getline(std::cin, clave)) {
                return 0;
            }
            valida = claveValida(clave, longitud);
            if (!valida) {
                std::cout << "Clave inválida. Debe tener " << longitud
                          << " caracteres A-Z o 0-9, sin espacios.\n";
            }
        } while (!valida);

        unsigned long long total = calcularEspacio(longitud);
        std::cout << "\nClave: " << clave << "\nLongitud: " << longitud
                  << "\nEspacio de búsqueda: " << total << '\n';

        std::string resultadoSecuencial;
        std::string resultadoParalelo;
        unsigned long long revisadas;
        double tiempoSecuencial;
        double tiempoParalelo;
        int ganador;
        int hilosUtilizados;

        buscarSecuencial(clave, longitud, total, resultadoSecuencial,
                         revisadas, tiempoSecuencial);
        buscarParalela(clave, longitud, total, resultadoParalelo,
                        ganador, hilosUtilizados, tiempoParalelo);

        std::cout << "\nRESULTADOS\n"
                  << "Clave: " << clave << "\nLongitud: " << longitud
                  << "\nEspacio de búsqueda: " << total
                  << "\n\nSecuencial\nClave encontrada: " << resultadoSecuencial
                  << "\nCombinaciones revisadas: " << revisadas
                  << "\nTiempo: " << tiempoSecuencial << " segundos"
                  << "\n\nParalelo\nHilos utilizados: " << hilosUtilizados
                  << "\nClave encontrada: " << resultadoParalelo
                  << "\nHilo ganador: " << ganador
                  << "\nTiempo: " << tiempoParalelo << " segundos\n";
        if (tiempoParalelo > 0) {
            std::cout << "\nSpeedup: " << tiempoSecuencial / tiempoParalelo << "x\n";
        } else {
            std::cout << "\nSpeedup: no calculable (tiempo paralelo igual a cero).\n";
        }
    }
    return 0;
}
