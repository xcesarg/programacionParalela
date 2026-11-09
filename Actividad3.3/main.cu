// CARDENAS OLIVARES CESAR GIOVANNY
// CERVANTES ARRIZON VICTOR HUGO
// DIAZ TORRES JAZMIN MONTSERRAT

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <climits>
#include <cerrno>
#include <cctype>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cuda_runtime.h>

const int N = 40000000;
const int HILOS_POR_BLOQUE = 256;
const bool DETALLADO = false;

#define EQUIPO "E02"
#define FORMATO_DEVICE "[DEVICE] [Equipo: " EQUIPO "] [Bloque CUDA: %d] [Hilo CUDA: %d] [Indice Global: %d] [Arreglo: %s] [Operacion: %s] Valor: %lld\n"

FILE *archivoLog = nullptr;

// Los mensajes de la CPU se escriben en la consola y en el archivo.
void mensajeHost(const char *formato, ...)
{
    va_list argumentos;
    va_start(argumentos, formato);
    printf("[HOST] [Equipo: " EQUIPO "] ");
    vprintf(formato, argumentos);
    printf("\n");
    va_end(argumentos);

    if (archivoLog != nullptr)
    {
        va_start(argumentos, formato);
        fprintf(archivoLog, "[HOST] [Equipo: " EQUIPO "] ");
        vfprintf(archivoLog, formato, argumentos);
        fprintf(archivoLog, "\n");
        va_end(argumentos);
        fflush(archivoLog);
    }
    fflush(stdout);
}

void imprimirIntegrantes()
{
    mensajeHost("CARDENAS OLIVARES CESAR GIOVANNY | CERVANTES ARRIZON VICTOR HUGO | DIAZ TORRES JAZMIN MONTSERRAT");
}

void revisarCUDA(cudaError_t error, const char *operacion)
{
    if (error != cudaSuccess)
    {
        mensajeHost("Error en %s: %s", operacion, cudaGetErrorString(error));
        throw std::runtime_error("No se pudo completar la operacion CUDA.");
    }
}

__host__ __device__ const char *nombreOperacion(int operacion)
{
    switch (operacion)
    {
    case 2:
        return "Suma";
    case 3:
        return "Resta";
    case 4:
        return "Multiplicacion";
    case 5:
        return "Cuadrado";
    case 8:
        return "Sumatoria";
    case 10:
        return "Maximo";
    case 11:
        return "Minimo";
    default:
        return "Desconocida";
    }
}

__global__ void llenarArreglos(long long *a, long long *b, long long *c,
                               int n, bool detalle)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        a[i] = i + 1;
        b[i] = n - i;
        c[i] = 0;
        if (detalle)
        {
            printf(FORMATO_DEVICE, (int)blockIdx.x, (int)threadIdx.x, i,
                   "A", "Llenado secuencial", a[i]);
            printf(FORMATO_DEVICE, (int)blockIdx.x, (int)threadIdx.x, i,
                   "B", "Llenado secuencial", b[i]);
        }
    }
}

__global__ void operarArreglos(const long long *a, const long long *b,
                               long long *c, int n, int operacion, bool detalle)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        switch (operacion)
        {
        case 2:
            c[i] = a[i] + b[i];
            break;
        case 3:
            c[i] = a[i] - b[i];
            break;
        case 4:
            c[i] = a[i] * b[i];
            break;
        case 5:
            c[i] = a[i] * a[i];
            break;
        }
        if (detalle)
        {
            printf(FORMATO_DEVICE, (int)blockIdx.x, (int)threadIdx.x, i,
                   "C", nombreOperacion(operacion), c[i]);
        }
    }
}

__global__ void reducirArreglo(const long long *arreglo, long long *parciales,
                               int n, int operacion, char nombre, bool detalle)
{
    // El espacio de esta memoria se indica al lanzar el kernel.
    extern __shared__ long long compartida[];
    int hilo = threadIdx.x;
    int i = blockIdx.x * blockDim.x + hilo;

    // Los hilos sin un elemento valido no deben cambiar el resultado.
    long long neutro = 0;
    if (operacion == 10)
        neutro = LLONG_MIN;
    if (operacion == 11)
        neutro = LLONG_MAX;
    compartida[hilo] = (i < n) ? arreglo[i] : neutro;
    __syncthreads();

    // En cada vuelta queda la mitad de los valores por combinar.
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1)
    {
        if (hilo < stride)
        {
            long long otro = compartida[hilo + stride];
            if (operacion == 8)
                compartida[hilo] += otro;
            if (operacion == 10 && otro > compartida[hilo])
                compartida[hilo] = otro;
            if (operacion == 11 && otro < compartida[hilo])
                compartida[hilo] = otro;
        }
        // Todos los hilos del bloque llegan aqui, incluso los que sobran.
        __syncthreads();
    }

    if (hilo == 0)
    {
        parciales[blockIdx.x] = compartida[0];
        if (detalle)
        {
            printf(FORMATO_DEVICE, (int)blockIdx.x, hilo, i,
                   nombre == 'A' ? "A" : "B", nombreOperacion(operacion), compartida[0]);
        }
    }
}

class OperacionesArreglos
{
private:
    long long *host_a, *host_b, *host_c, *host_parciales;
    long long *device_a, *device_b, *device_c, *device_parciales;
    int bloques;
    bool creados;
    cudaEvent_t inicio, fin;

    void iniciarTiempo()
    {
        revisarCUDA(cudaEventRecord(inicio), "iniciar medicion");
    }

    float terminarTiempo()
    {
        revisarCUDA(cudaGetLastError(), "lanzar kernel");
        revisarCUDA(cudaEventRecord(fin), "marcar fin del kernel");
        revisarCUDA(cudaEventSynchronize(fin), "esperar al kernel");
        float ms = 0;
        revisarCUDA(cudaEventElapsedTime(&ms, inicio, fin), "medir tiempo GPU");
        return ms;
    }

    void comprobarArreglos()
    {
        if (!creados)
            throw std::runtime_error("Primero utiliza la opcion 1 para crear los arreglos.");
    }

    // La GPU imprime en consola. La CPU guarda los mismos datos en el log
    // despues de copiarlos; el orden del archivo es por indice o por bloque.
    void guardarDatoGPU(int bloque, int hilo, int indice, const char *arreglo,
                        const char *operacion, long long valor)
    {
        if (DETALLADO)
        {
            fprintf(archivoLog, FORMATO_DEVICE, bloque, hilo, indice,
                    arreglo, operacion, valor);
        }
    }

    long long reducir(const long long *device, char nombre, int operacion)
    {
        comprobarArreglos();
        mensajeHost("Iniciando reduccion %s de %c en GPU. Cada mensaje DEVICE muestra un parcial por bloque.",
                    nombreOperacion(operacion), nombre);
        iniciarTiempo();
        reducirArreglo<<<bloques, HILOS_POR_BLOQUE, HILOS_POR_BLOQUE * sizeof(long long)>>>(
            device, device_parciales, N, operacion, nombre, DETALLADO);
        float ms = terminarTiempo();
        revisarCUDA(cudaMemcpy(host_parciales, device_parciales, bloques * sizeof(long long),
                               cudaMemcpyDeviceToHost),
                    "copiar parciales");

        // Segunda parte: juntar los resultados de los bloques en la CPU.
        long long resultado = host_parciales[0];
        for (int b = 1; b < bloques; b++)
        {
            if (operacion == 8)
                resultado += host_parciales[b];
            if (operacion == 10 && host_parciales[b] > resultado)
                resultado = host_parciales[b];
            if (operacion == 11 && host_parciales[b] < resultado)
                resultado = host_parciales[b];
        }

        for (int b = 0; DETALLADO && b < bloques; b++)
        {
            guardarDatoGPU(b, 0, b * HILOS_POR_BLOQUE, nombre == 'A' ? "A" : "B",
                           nombreOperacion(operacion), host_parciales[b]);
        }

        mensajeHost("%s de %c: %lld", nombreOperacion(operacion), nombre, resultado);
        mensajeHost("Tiempo kernel GPU: %.6f ms", ms);
        return resultado;
    }

public:
    OperacionesArreglos()
    {
        host_a = host_b = host_c = host_parciales = nullptr;
        device_a = device_b = device_c = device_parciales = nullptr;
        bloques = (N + HILOS_POR_BLOQUE - 1) / HILOS_POR_BLOQUE;
        creados = false;
        inicio = fin = nullptr;
    }

    ~OperacionesArreglos()
    {
        liberarMemoria();
    }

    void liberarMemoria()
    {
        delete[] host_a;
        delete[] host_b;
        delete[] host_c;
        delete[] host_parciales;
        host_a = host_b = host_c = host_parciales = nullptr;
        if (device_a)
            revisarLiberacion(cudaFree(device_a));
        if (device_b)
            revisarLiberacion(cudaFree(device_b));
        if (device_c)
            revisarLiberacion(cudaFree(device_c));
        if (device_parciales)
            revisarLiberacion(cudaFree(device_parciales));
        device_a = device_b = device_c = device_parciales = nullptr;
        if (inicio)
            revisarLiberacion(cudaEventDestroy(inicio));
        if (fin)
            revisarLiberacion(cudaEventDestroy(fin));
        inicio = fin = nullptr;
        creados = false;
    }

    void revisarLiberacion(cudaError_t error)
    {
        if (error != cudaSuccess)
            mensajeHost("Error al liberar un recurso CUDA: %s", cudaGetErrorString(error));
    }

    bool estanCreados() const { return creados; }

    void crearArregloCUDA()
    {
        liberarMemoria();
        mensajeHost("Creando A, B y C: N=%d, bloques=%d, hilos por bloque=%d.", N, bloques, HILOS_POR_BLOQUE);
        host_a = new long long[N];
        host_b = new long long[N];
        host_c = new long long[N];
        host_parciales = new long long[bloques];
        revisarCUDA(cudaMalloc((void **)&device_a, N * sizeof(long long)), "reservar A");
        revisarCUDA(cudaMalloc((void **)&device_b, N * sizeof(long long)), "reservar B");
        revisarCUDA(cudaMalloc((void **)&device_c, N * sizeof(long long)), "reservar C");
        revisarCUDA(cudaMalloc((void **)&device_parciales, bloques * sizeof(long long)), "reservar parciales");
        revisarCUDA(cudaEventCreate(&inicio), "crear evento inicial");
        revisarCUDA(cudaEventCreate(&fin), "crear evento final");
        creados = true;
        llenarSecuencial();
    }

    void llenarSecuencial()
    {
        comprobarArreglos();
        mensajeHost("Llenado secuencial: A va de 1 a N, B de N a 1 y C queda en cero.");
        iniciarTiempo();
        llenarArreglos<<<bloques, HILOS_POR_BLOQUE>>>(device_a, device_b, device_c, N, DETALLADO);
        float ms = terminarTiempo();
        revisarCUDA(cudaMemcpy(host_a, device_a, N * sizeof(long long), cudaMemcpyDeviceToHost), "copiar A");
        revisarCUDA(cudaMemcpy(host_b, device_b, N * sizeof(long long), cudaMemcpyDeviceToHost), "copiar B");
        for (int i = 0; i < N; i++)
        {
            host_c[i] = 0;
            guardarDatoGPU(i / HILOS_POR_BLOQUE, i % HILOS_POR_BLOQUE, i, "A", "Llenado secuencial", host_a[i]);
            guardarDatoGPU(i / HILOS_POR_BLOQUE, i % HILOS_POR_BLOQUE, i, "B", "Llenado secuencial", host_b[i]);
        }
        mensajeHost("Llenado terminado. Tiempo kernel GPU: %.6f ms", ms);
    }

    void llenarAleatorio()
    {
        comprobarArreglos();
        mensajeHost("Llenando A y B en HOST con valores aleatorios entre 1 y 1000000.");
        for (int i = 0; i < N; i++)
        {
            host_a[i] = 1 + rand() % 1000000;
            host_b[i] = 1 + rand() % 1000000;
            host_c[i] = 0;
            if (DETALLADO)
                mensajeHost("[Indice Global: %d] [Operacion: Llenado aleatorio] A=%lld B=%lld", i, host_a[i], host_b[i]);
        }
        revisarCUDA(cudaMemcpy(device_a, host_a, N * sizeof(long long), cudaMemcpyHostToDevice), "enviar A");
        revisarCUDA(cudaMemcpy(device_b, host_b, N * sizeof(long long), cudaMemcpyHostToDevice), "enviar B");
        revisarCUDA(cudaMemset(device_c, 0, N * sizeof(long long)), "limpiar C");
        revisarCUDA(cudaDeviceSynchronize(), "terminar llenado aleatorio");
        mensajeHost("Llenado aleatorio terminado y copiado a GPU.");
    }

    void operacionAritmetica(int operacion)
    {
        comprobarArreglos();
        mensajeHost("Iniciando %s en GPU; el resultado se guarda en C.", nombreOperacion(operacion));
        iniciarTiempo();
        operarArreglos<<<bloques, HILOS_POR_BLOQUE>>>(device_a, device_b, device_c, N, operacion, DETALLADO);
        float ms = terminarTiempo();
        revisarCUDA(cudaMemcpy(host_c, device_c, N * sizeof(long long), cudaMemcpyDeviceToHost), "copiar C");
        for (int i = 0; i < N; i++)
        {
            guardarDatoGPU(i / HILOS_POR_BLOQUE, i % HILOS_POR_BLOQUE, i, "C", nombreOperacion(operacion), host_c[i]);
        }
        mensajeHost("%s terminada. Tiempo kernel GPU: %.6f ms", nombreOperacion(operacion), ms);
    }

    void reduccionSumatoria()
    {
        reducir(device_a, 'A', 8);
        reducir(device_b, 'B', 8);
    }

    void reduccionPromedio()
    {
        long long sumaA = reducir(device_a, 'A', 8);
        long long sumaB = reducir(device_b, 'B', 8);
        mensajeHost("Promedio de A: %.6f | Promedio de B: %.6f", (double)sumaA / N, (double)sumaB / N);
    }

    void reduccionMaximo()
    {
        reducir(device_a, 'A', 10);
        reducir(device_b, 'B', 10);
    }

    void reduccionMinimo()
    {
        reducir(device_a, 'A', 11);
        reducir(device_b, 'B', 11);
    }
};

void mostrarMenu()
{
    mensajeHost("----------- MENU -----------");
    mensajeHost("1. Crear arreglos (%d elementos)", N);
    mensajeHost("2. Sumar arreglos");
    mensajeHost("3. Restar arreglos");
    mensajeHost("4. Multiplicar arreglos");
    mensajeHost("5. Calcular el cuadrado de A");
    mensajeHost("6. Llenar secuencial");
    mensajeHost("7. Llenar aleatorio");
    mensajeHost("8. Sumatoria de A y B");
    mensajeHost("9. Promedio de A y B");
    mensajeHost("10. Maximo de A y B");
    mensajeHost("11. Minimo de A y B");
    mensajeHost("12. Salir");
    mensajeHost("Escribe una opcion:");
}

int main()
{
    archivoLog = fopen("log_ejecucion_cuda.txt", "a");
    imprimirIntegrantes();
    int estado = 0;
    try
    {
        if (!archivoLog)
            throw std::runtime_error("No se pudo abrir log_ejecucion_cuda.txt.");
        time_t ahora = time(nullptr);
        // ctime ya incluye un salto de linea; lo quitamos para mantener el formato.
        std::string fecha = ctime(&ahora);
        if (!fecha.empty() && fecha.back() == '\n')
            fecha.pop_back();
        mensajeHost("Inicio de ejecucion: %s", fecha.c_str());
        mensajeHost("Actividad 3.3: reducciones en CUDA. Log en modo acumulativo.");

        int dispositivos = 0;
        revisarCUDA(cudaGetDeviceCount(&dispositivos), "detectar GPU");
        if (dispositivos == 0)
            throw std::runtime_error("No hay una GPU CUDA disponible.");
        revisarCUDA(cudaSetDevice(0), "seleccionar GPU");
        cudaDeviceProp propiedades;
        revisarCUDA(cudaGetDeviceProperties(&propiedades, 0), "consultar GPU");
        mensajeHost("GPU: %s | Compute capability: %d.%d", propiedades.name, propiedades.major, propiedades.minor);
        mensajeHost("Multiprocesadores: %d | Maximo de hilos por bloque: %d", propiedades.multiProcessorCount, propiedades.maxThreadsPerBlock);
        mensajeHost("Memoria global: %.2f MiB | Memoria compartida por bloque: %zu bytes",
                    propiedades.totalGlobalMem / (1024.0 * 1024.0), propiedades.sharedMemPerBlock);
        if (N <= 0 || HILOS_POR_BLOQUE <= 0 || HILOS_POR_BLOQUE > propiedades.maxThreadsPerBlock || (HILOS_POR_BLOQUE & (HILOS_POR_BLOQUE - 1)) != 0 || HILOS_POR_BLOQUE * sizeof(long long) > propiedades.sharedMemPerBlock)
            throw std::runtime_error("Revisa N y los hilos por bloque: usa una potencia de dos permitida por la GPU.");

        mensajeHost("N=%d | Hilos por bloque=%d | Detalle=%s", N, HILOS_POR_BLOQUE, DETALLADO ? "activado" : "desactivado");
        if (DETALLADO)
            mensajeHost("Los tiempos de esta prueba incluyen los printf del kernel. Desactiva DETALLADO para medir sin esas impresiones.");
        srand((unsigned int)time(nullptr));
        OperacionesArreglos arreglos;

        while (true)
        {
            mostrarMenu();
            std::string entrada;
            if (!std::getline(std::cin, entrada))
            {
                mensajeHost("Fin de la entrada. Cerrando el programa.");
                break;
            }
            char *finEntrada;
            errno = 0;
            long opcion = strtol(entrada.c_str(), &finEntrada, 10);
            bool huboNumero = finEntrada != entrada.c_str();
            while (isspace((unsigned char)*finEntrada))
                finEntrada++;
            if (!huboNumero || *finEntrada != '\0' || errno == ERANGE || opcion < 1 || opcion > 12)
            {
                mensajeHost("Opcion invalida. Escribe un numero entero del 1 al 12.");
                continue;
            }
            mensajeHost("Opcion seleccionada: %ld", opcion);
            if (opcion == 12)
                break;
            if (opcion != 1 && !arreglos.estanCreados())
            {
                mensajeHost("Primero utiliza la opcion 1 para crear los arreglos.");
                continue;
            }
            switch (opcion)
            {
            case 1:
                arreglos.crearArregloCUDA();
                break;
            case 2:
            case 3:
            case 4:
            case 5:
                arreglos.operacionAritmetica((int)opcion);
                break;
            case 6:
                arreglos.llenarSecuencial();
                break;
            case 7:
                arreglos.llenarAleatorio();
                break;
            case 8:
                arreglos.reduccionSumatoria();
                break;
            case 9:
                arreglos.reduccionPromedio();
                break;
            case 10:
                arreglos.reduccionMaximo();
                break;
            case 11:
                arreglos.reduccionMinimo();
                break;
            }
        }
        mensajeHost("Liberando memoria del HOST y del DEVICE.");
    }
    catch (const std::exception &error)
    {
        mensajeHost("Ejecucion detenida: %s", error.what());
        estado = 1;
    }

    if (archivoLog && ferror(archivoLog))
    {
        mensajeHost("Error de escritura en el log. Revisa el espacio y los permisos.");
        estado = 1;
    }
    mensajeHost("Fin de ejecucion. Estado: %s", estado == 0 ? "correcto" : "con errores");
    imprimirIntegrantes();
    if (archivoLog)
        fclose(archivoLog);
    return estado;
}
