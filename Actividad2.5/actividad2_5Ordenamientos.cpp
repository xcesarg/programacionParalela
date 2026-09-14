#include <iostream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <omp.h>
#include <iomanip>

using namespace std;



const int UMBRAL_TAREAS = 10000;

void copiarArreglo(const int* origen, int* destino, int n)
{
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
    {
        destino[i] = origen[i];
    }
}

void mostrarArreglo(const int* arreglo, int n)
{
    for (int i = 0; i < n; i++)
    {
        cout << arreglo[i] << " ";

        if ((i + 1) % 20 == 0)
        {
            cout << endl;
        }
    }

    if (n % 20 != 0)
    {
        cout << endl;
    }
}

bool estaOrdenado(const int* arreglo, int n)
{
    for (int i = 1; i < n; i++)
    {
        if (arreglo[i - 1] > arreglo[i])
        {
            return false;
        }
    }

    return true;
}



double llenarArregloParalelo(int* arreglo, int n, int maximo)
{
    double inicio = omp_get_wtime();

    #pragma omp parallel
    {
        const unsigned int semilla =
            static_cast<unsigned int>(time(nullptr))
            ^ (static_cast<unsigned int>(omp_get_thread_num() + 1) * 2654435761u);

        std::mt19937 generador(semilla);

        #pragma omp for
        for (int i = 0; i < n; i++)
        {
            arreglo[i] = static_cast<int>(generador() % static_cast<unsigned int>(maximo + 1));
        }
    }

    double fin = omp_get_wtime();

    return fin - inicio;
}


void shellSortSecuencial(int* arreglo, int n)
{
    for (int gap = n / 2; gap > 0; gap /= 2)
    {
        for (int i = gap; i < n; i++)
        {
            int temporal = arreglo[i];
            int j = i;

            while (j >= gap && arreglo[j - gap] > temporal)
            {
                arreglo[j] = arreglo[j - gap];
                j -= gap;
            }

            arreglo[j] = temporal;
        }
    }
}



void shellSortParalelo(int* arreglo, int n)
{
    for (int gap = n / 2; gap > 0; gap /= 2)
    {
       

        #pragma omp parallel for schedule(static)
        for (int r = 0; r < gap; r++)
        {
            for (int i = r + gap; i < n; i += gap)
            {
                int temporal = arreglo[i];
                int j = i;

                while (j >= r + gap && arreglo[j - gap] > temporal)
                {
                    arreglo[j] = arreglo[j - gap];
                    j -= gap;
                }

                arreglo[j] = temporal;
            }
        }

    }
}


void merge(int* arreglo, int inicio, int medio, int fin, int* auxiliar)
{
    int i = inicio;
    int j = medio + 1;
    int k = inicio;

    while (i <= medio && j <= fin)
    {
        if (arreglo[i] <= arreglo[j])
        {
            auxiliar[k] = arreglo[i];
            i++;
        }
        else
        {
            auxiliar[k] = arreglo[j];
            j++;
        }

        k++;
    }

    while (i <= medio)
    {
        auxiliar[k] = arreglo[i];
        i++;
        k++;
    }

    while (j <= fin)
    {
        auxiliar[k] = arreglo[j];
        j++;
        k++;
    }

    for (int x = inicio; x <= fin; x++)
    {
        arreglo[x] = auxiliar[x];
    }
}


void mergeSortSecuencial(int* arreglo, int inicio, int fin, int* auxiliar)
{
    if (inicio >= fin)
    {
        return;
    }

    int medio = inicio + (fin - inicio) / 2;

    mergeSortSecuencial(arreglo, inicio, medio, auxiliar);

    mergeSortSecuencial(arreglo, medio + 1, fin, auxiliar);

    merge(arreglo, inicio, medio, fin, auxiliar);
}


void mergeSortParaleloRecursivo(
    int* arreglo,
    int inicio,
    int fin,
    int* auxiliar)
{
    if (inicio >= fin)
    {
        return;
    }

    int medio = inicio + (fin - inicio) / 2;

    if (fin - inicio + 1 <= UMBRAL_TAREAS)
    {
        mergeSortSecuencial(arreglo, inicio, fin, auxiliar);
        return;
    }

    #pragma omp task shared(arreglo, auxiliar) firstprivate(inicio, medio)
    {
        mergeSortParaleloRecursivo(
            arreglo,
            inicio,
            medio,
            auxiliar
        );
    }

    #pragma omp task shared(arreglo, auxiliar) firstprivate(medio, fin)
    {
        mergeSortParaleloRecursivo(
            arreglo,
            medio + 1,
            fin,
            auxiliar
        );
    }

   
    #pragma omp taskwait

    merge(arreglo, inicio, medio, fin, auxiliar);
}

void mergeSortParalelo(
    int* arreglo,
    int n,
    int* auxiliar)
{
    #pragma omp parallel
    {
        #pragma omp single
        {
            mergeSortParaleloRecursivo(
                arreglo,
                0,
                n - 1,
                auxiliar
            );
        }
    }
}



double ejecutarShellSecuencial(
    const int* original,
    int* arreglo,
    int n)
{
    copiarArreglo(original, arreglo, n);

    double inicio = omp_get_wtime();

    shellSortSecuencial(arreglo, n);

    double fin = omp_get_wtime();

    return fin - inicio;
}


double ejecutarShellParalelo(
    const int* original,
    int* arreglo,
    int n)
{
    copiarArreglo(original, arreglo, n);

    double inicio = omp_get_wtime();

    shellSortParalelo(arreglo, n);

    double fin = omp_get_wtime();

    return fin - inicio;
}



double ejecutarMergeSecuencial(
    const int* original,
    int* arreglo,
    int n,
    int* auxiliar)
{
    copiarArreglo(original, arreglo, n);

    double inicio = omp_get_wtime();

    mergeSortSecuencial(
        arreglo,
        0,
        n - 1,
        auxiliar
    );

    double fin = omp_get_wtime();

    return fin - inicio;
}



double ejecutarMergeParalelo(
    const int* original,
    int* arreglo,
    int n,
    int* auxiliar)
{
    copiarArreglo(original, arreglo, n);

    double inicio = omp_get_wtime();

    mergeSortParalelo(
        arreglo,
        n,
        auxiliar
    );

    double fin = omp_get_wtime();

    return fin - inicio;
}


void mostrarResultado(
    const char* nombre,
    const char* version,
    int n,
    double tiempo,
    bool correcto)
{
    cout << endl;
    cout << "---------------------------------------------" << endl;
    cout << nombre << " - " << version << endl;
    cout << "Elementos: " << n << endl;
    cout << fixed << setprecision(6);
    cout << "Tiempo: " << tiempo << " segundos" << endl;
    cout << "Ordenamiento correcto: "
         << (correcto ? "SI" : "NO") << endl;
    cout << "---------------------------------------------" << endl;
}


void ejecutarTodos(
    const int* original,
    int* arreglo,
    int* auxiliar,
    int n,
    bool mostrar)
{
    double tiempoShellSec;
    double tiempoShellPar;
    double tiempoMergeSec;
    double tiempoMergePar;



    tiempoShellSec =
        ejecutarShellSecuencial(
            original,
            arreglo,
            n
        );

    bool shellSecCorrecto =
        estaOrdenado(arreglo, n);

    if (mostrar)
    {
        mostrarResultado(
            "Shell Sort",
            "Secuencial",
            n,
            tiempoShellSec,
            shellSecCorrecto
        );

        mostrarArreglo(arreglo, n);
    }


    tiempoShellPar =
        ejecutarShellParalelo(
            original,
            arreglo,
            n
        );

    bool shellParCorrecto =
        estaOrdenado(arreglo, n);

    if (mostrar)
    {
        mostrarResultado(
            "Shell Sort",
            "Paralelo",
            n,
            tiempoShellPar,
            shellParCorrecto
        );

        mostrarArreglo(arreglo, n);
    }


    tiempoMergeSec =
        ejecutarMergeSecuencial(
            original,
            arreglo,
            n,
            auxiliar
        );

    bool mergeSecCorrecto =
        estaOrdenado(arreglo, n);

    if (mostrar)
    {
        mostrarResultado(
            "Merge Sort",
            "Secuencial",
            n,
            tiempoMergeSec,
            mergeSecCorrecto
        );

        mostrarArreglo(arreglo, n);
    }


    tiempoMergePar =
        ejecutarMergeParalelo(
            original,
            arreglo,
            n,
            auxiliar
        );

    bool mergeParCorrecto =
        estaOrdenado(arreglo, n);

    if (mostrar)
    {
        mostrarResultado(
            "Merge Sort",
            "Paralelo con tareas",
            n,
            tiempoMergePar,
            mergeParCorrecto
        );

        mostrarArreglo(arreglo, n);
    }



    cout << endl;
   
    cout << "COMPARACION DE RESULTADOS" << endl;
  

    cout << fixed << setprecision(6);

    cout << "Shell secuencial : "
         << tiempoShellSec << " s" << endl;

    cout << "Shell paralelo   : "
         << tiempoShellPar << " s" << endl;

    cout << "Merge secuencial : "
         << tiempoMergeSec << " s" << endl;

    cout << "Merge paralelo   : "
         << tiempoMergePar << " s" << endl;

    cout << endl;

    if (tiempoShellPar > 0)
    {
        cout << "Speedup Shell: "
             << tiempoShellSec / tiempoShellPar
             << endl;
    }

    if (tiempoMergePar > 0)
    {
        cout << "Speedup Merge: "
             << tiempoMergeSec / tiempoMergePar
             << endl;
    }

    cout << "-----------" << endl;
}

int main()
{
    int* arregloOriginal = nullptr;
    int* arregloTrabajo = nullptr;
    int* arregloAuxiliar = nullptr;

    int n = 0;
    int opcion;

    bool arregloCreado = false;

    do
    {
        cout << endl;
       
        cout << "       ACTIVIDAD 2.5 - ORDENAMIENTOS" << endl;
        

        cout << "1. Crear y llenar arreglo" << endl;
        cout << "2. Shell Sort secuencial" << endl;
        cout << "3. Shell Sort paralelo" << endl;
        cout << "4. Merge Sort secuencial" << endl;
        cout << "5. Merge Sort paralelo con tareas" << endl;
        cout << "6. Ejecutar todas las versiones" << endl;
        cout << "7. Mostrar arreglo original" << endl;
        cout << "8. Salir" << endl;

        cout << endl;
        cout << "Seleccione una opcion: ";
        cin >> opcion;

        switch (opcion)
        {
           

            case 1:
            {
                if (arregloCreado)
                {
                    delete[] arregloOriginal;
                    delete[] arregloTrabajo;
                    delete[] arregloAuxiliar;

                    arregloOriginal = nullptr;
                    arregloTrabajo = nullptr;
                    arregloAuxiliar = nullptr;

                    arregloCreado = false;
                }

                cout << endl;
                cout << "Ingrese el numero de elementos: ";
                cin >> n;

                if (n <= 0)
                {
                    cout << "El tamano debe ser mayor que cero." << endl;
                    break;
                }

                arregloOriginal = new int[n];
                arregloTrabajo = new int[n];
                arregloAuxiliar = new int[n];

                int maximo;

                if (n <= 100)
                {
                    maximo = 200;
                }
                else
                {
                    maximo = 2000000;
                }

                double tiempoLlenado =
                    llenarArregloParalelo(
                        arregloOriginal,
                        n,
                        maximo
                    );

                arregloCreado = true;

                cout << endl;
             
                cout << "ARREGLO CREADO" << endl;
                

                cout << "Elementos: "
                     << n << endl;

                cout << "Rango: 0 - "
                     << maximo << endl;

                cout << fixed << setprecision(6);

                cout << "Tiempo de llenado paralelo: "
                     << tiempoLlenado
                     << " segundos"
                     << endl;

                if (n <= 100)
                {
                    cout << endl;
                    cout << "Arreglo desordenado:" << endl;

                    mostrarArreglo(
                        arregloOriginal,
                        n
                    );
                }
                else
                {
                    cout << endl;
                    cout << "El arreglo no se muestra debido a su tamano." << endl;
                }

                break;
            }


            case 2:
            {
                if (!arregloCreado)
                {
                    cout << "Primero debe crear el arreglo." << endl;
                    break;
                }

                double tiempo =
                    ejecutarShellSecuencial(
                        arregloOriginal,
                        arregloTrabajo,
                        n
                    );

                bool correcto =
                    estaOrdenado(
                        arregloTrabajo,
                        n
                    );

                mostrarResultado(
                    "Shell Sort",
                    "Secuencial",
                    n,
                    tiempo,
                    correcto
                );

                if (n <= 100)
                {
                    cout << endl;
                    cout << "Arreglo ordenado:" << endl;

                    mostrarArreglo(
                        arregloTrabajo,
                        n
                    );
                }

                break;
            }


            case 3:
            {
                if (!arregloCreado)
                {
                    cout << "Primero debe crear el arreglo." << endl;
                    break;
                }

                double tiempo =
                    ejecutarShellParalelo(
                        arregloOriginal,
                        arregloTrabajo,
                        n
                    );

                bool correcto =
                    estaOrdenado(
                        arregloTrabajo,
                        n
                    );

                mostrarResultado(
                    "Shell Sort",
                    "Paralelo",
                    n,
                    tiempo,
                    correcto
                );

                if (n <= 100)
                {
                    cout << endl;
                    cout << "Arreglo ordenado:" << endl;

                    mostrarArreglo(
                        arregloTrabajo,
                        n
                    );
                }

                break;
            }

    
            case 4:
            {
                if (!arregloCreado)
                {
                    cout << "Primero debe crear el arreglo." << endl;
                    break;
                }

                double tiempo =
                    ejecutarMergeSecuencial(
                        arregloOriginal,
                        arregloTrabajo,
                        n,
                        arregloAuxiliar
                    );

                bool correcto =
                    estaOrdenado(
                        arregloTrabajo,
                        n
                    );

                mostrarResultado(
                    "Merge Sort",
                    "Secuencial",
                    n,
                    tiempo,
                    correcto
                );

                if (n <= 100)
                {
                    cout << endl;
                    cout << "Arreglo ordenado:" << endl;

                    mostrarArreglo(
                        arregloTrabajo,
                        n
                    );
                }

                break;
            }

    

            case 5:
            {
                if (!arregloCreado)
                {
                    cout << "Primero debe crear el arreglo." << endl;
                    break;
                }

                double tiempo =
                    ejecutarMergeParalelo(
                        arregloOriginal,
                        arregloTrabajo,
                        n,
                        arregloAuxiliar
                    );

                bool correcto =
                    estaOrdenado(
                        arregloTrabajo,
                        n
                    );

                mostrarResultado(
                    "Merge Sort",
                    "Paralelo con tareas",
                    n,
                    tiempo,
                    correcto
                );

                if (n <= 100)
                {
                    cout << endl;
                    cout << "Arreglo ordenado:" << endl;

                    mostrarArreglo(
                        arregloTrabajo,
                        n
                    );
                }

                break;
            }

    

            case 6:
            {
                if (!arregloCreado)
                {
                    cout << "Primero debe crear el arreglo." << endl;
                    break;
                }

                bool mostrar = (n <= 100);

                if (n > 100)
                {
                    cout << endl;
                    cout << "Ejecutando las cuatro versiones..." << endl;
                    cout << "No se mostrara el contenido del arreglo." << endl;
                }

                ejecutarTodos(
                    arregloOriginal,
                    arregloTrabajo,
                    arregloAuxiliar,
                    n,
                    mostrar
                );

                break;
            }

     
            case 7:
            {
                if (!arregloCreado)
                {
                    cout << "Primero debe crear el arreglo." << endl;
                    break;
                }

                if (n <= 100)
                {
                    cout << endl;
                    cout << "Arreglo original:" << endl;

                    mostrarArreglo(
                        arregloOriginal,
                        n
                    );
                }
                else
                {
                    cout << "El arreglo tiene demasiados elementos para mostrarlo."
                         << endl;
                }

                break;
            }

    

            case 8:
            {
                cout << endl;
                cout << "Saliendo del programa..." << endl;
                break;
            }

            default:
            {
                cout << "Opcion no valida." << endl;
                break;
            }
        }

    } while (opcion != 8);

    delete[] arregloOriginal;
    delete[] arregloTrabajo;
    delete[] arregloAuxiliar;

    return 0;
}