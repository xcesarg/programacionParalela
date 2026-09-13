#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#include "OperacionesArreglos.h"

#define TAM 100
#define TAM_GRANDE 10000000
#define MAX_ALEATORIO 2000000

void mostrarArreglo(int *arreglo, int tam, const char *nombre)
{
    // El arreglo grande no se imprime; solo se medira el tiempo de operacion.
    if (tam > TAM)
    {
        return;
    }

    printf("\n%s:\n", nombre);

    for (int i = 0; i < tam; i++)
    {
        printf("%d ", arreglo[i]);

        if ((i + 1) % 10 == 0)
        {
            printf("\n");
        }
    }

    printf("\n");
}

void llenarArreglos(int *A, int *B, int tam)
{
    for (int i = 0; i < tam; i++)
    {
        A[i] = i + 1;
        B[i] = tam - i;
    }
}

void llenarAleatorios(int *A, int *B, int tam, int limite = 1000)
{
    for (int i = 0; i < tam; i++)
    {
        A[i] = 1 + rand() % limite;
        B[i] = 1 + rand() % limite;
    }
}

void mostrarMenuTamano()
{
    printf("\n     SELECCION DEL TAMANO \n");
    printf("1. Arreglos de 100 elementos\n");
    printf("2. Arreglos de 10,000,000 elementos\n");
    printf("Seleccione una opcion: ");
}

void mostrarMenu()
{
    printf("\n");

    printf("     menu \n");

    printf("1. Llenar arreglos aleatoriamente\n");
    printf("2. Sumar arreglos\n");
    printf("3. Restar arreglos\n");
    printf("4. Multiplicar arreglos\n");
    printf("5. Cuadrado de un arreglo\n");
    printf("6. Sumatoria de un arreglo\n");
    printf("7. Promedio de un arreglo\n");
    printf("8. Maximo de un arreglo\n");
    printf("9. Minimo de un arreglo\n");
    printf("10. Salir\n");

    printf("Seleccione una opcion: ");
}

int main()
{

    printf(" Cardenas Olivares Giovanny \n  ");
    printf(" Cervantes Arrizon Victor \n  ");
    printf(" Diaz Torres Montserrat \n  ");

    srand(12345);

    omp_set_num_threads(4);

    OperacionesArreglos operaciones;

    int opcionTamano;
    int tam = 0;

    // Este menu evita tener que cambiar TAM manualmente en el codigo.
    do
    {
        mostrarMenuTamano();
        scanf("%d", &opcionTamano);

        switch (opcionTamano)
        {
        case 1:
            tam = TAM;
            break;

        case 2:
            tam = TAM_GRANDE;
            break;

        default:
            printf("\nERROR: Seleccione 1 o 2.\n");
            break;
        }
    } while (tam == 0);

    int *A = new int[tam];

    int *B = new int[tam];

    int *R = new int[tam];

    switch (opcionTamano)
    {
    case 1:
        llenarArreglos(A, B, tam);

        printf("\nCantidad de elementos: %d\n", tam);
        printf("Arreglo A: valores ascendentes.\n");
        printf("Arreglo B: valores descendentes.\n");

        mostrarArreglo(A, tam, "Arreglo A");
        mostrarArreglo(B, tam, "Arreglo B");
        break;

    case 2:
        printf("\nCreando arreglos dinamicos de %d elementos...\n", tam);
        llenarAleatorios(A, B, tam, MAX_ALEATORIO);
        printf("Arreglos llenados con valores aleatorios de 1 a %d.\n",
               MAX_ALEATORIO);
        printf("No se imprimiran los arreglos; solo los tiempos.\n");
        break;
    }

    // menu

    int opcion;

    do
    {
        mostrarMenu();

        scanf("%d", &opcion);

        switch (opcion)
        {
        case 1:
        {

            printf("       LLENADO \n");

            int limiteAleatorio = (tam == TAM) ? 1000 : MAX_ALEATORIO;
            llenarAleatorios(A, B, tam, limiteAleatorio);

            mostrarArreglo(A, tam, "Arreglo A");

            mostrarArreglo(B, tam, "Arreglo B");
            break;
        }

        // suma

        case 2:
        {

            printf("           suma del arreglo \n");

            mostrarArreglo(A, tam, "Arreglo A");

            mostrarArreglo(B, tam, "Arreglo B");

            double inicio = omp_get_wtime();

            operaciones.sumarArreglosOpenMP(
                A,
                B,
                R,
                tam);

            double fin = omp_get_wtime();

            mostrarArreglo(R, tam, "Resultado A + B");

            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }

        // resta

        case 3:
        {

            printf("resta del arreglo  \n");

            mostrarArreglo(A, tam, "Arreglo A");

            mostrarArreglo(B, tam, "Arreglo B");

            double inicio = omp_get_wtime();

            operaciones.restarArreglosOpenMP(
                A,
                B,
                R,
                tam);

            double fin = omp_get_wtime();

            mostrarArreglo(R, tam, "Resultado A - B");

            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }

        // mu;tiplicacion

        case 4:
        {

            printf("         multiplicacion del arreglo \n");

            mostrarArreglo(A, tam, "Arreglo A");

            mostrarArreglo(B, tam, "Arreglo B");

            double inicio = omp_get_wtime();

            operaciones.multiplicarArreglosOpenMP(
                A,
                B,
                R,
                tam);

            double fin = omp_get_wtime();

            mostrarArreglo(R, tam, "Resultado A * B");

            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }

        // cuadrado

        case 5:
        {

            printf("          cuadrado del arreglo \n");

            mostrarArreglo(A, tam, "Arreglo A");

            double inicio = omp_get_wtime();

            operaciones.cuadradoArregloOpenMP(
                A,
                R,
                tam);

            double fin = omp_get_wtime();

            mostrarArreglo(R, tam, "Resultado A ^ 2");

            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }

        case 6:
        {
            printf("          sumatoria del arreglo A \n");

            mostrarArreglo(A, tam, "Arreglo A");

            double inicio = omp_get_wtime();

            long long sumatoria;
            operaciones.sumatoriaArregloOpenMP(A, tam, &sumatoria);

            double fin = omp_get_wtime();

            printf("Resultado de la sumatoria: %lld\n", sumatoria);
            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }

        case 7:
        {
            printf("          promedio del arreglo A \n");

            mostrarArreglo(A, tam, "Arreglo A");

            double inicio = omp_get_wtime();

            double promedio;
            operaciones.promedioArregloOpenMP(A, tam, &promedio);

            double fin = omp_get_wtime();

            printf("Resultado del promedio: %.2f\n", promedio);
            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }
        case 8:
        {
            printf("          maximo del arreglo A \n");

            mostrarArreglo(A, tam, "Arreglo A");

            double inicio = omp_get_wtime();

            int maximo;
            operaciones.maximoArregloOpenMP(A, tam, &maximo);

            double fin = omp_get_wtime();

            printf("Resultado maximo: %d\n", maximo);
            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }
        case 9:
        {
            printf("          minimo del arreglo A \n");

            mostrarArreglo(A, tam, "Arreglo A");

            double inicio = omp_get_wtime();

            int minimo;
            operaciones.minimoArregloOpenMP(A, tam, &minimo);

            double fin = omp_get_wtime();

            printf("Resultado minimo: %d\n", minimo);
            printf("Tiempo de ejecucion paralelo: %.10f segundos\n",
                   fin - inicio);
            break;
        }
        case 10:
        {
            printf("\nSaliendo del programa...\n");
            break;
        }

        // error

        default:
        {
            printf("\nERROR: Opcion no valida.\n");
            printf("Seleccione una opcion del 1 al 10.\n");
            break;
        }
        }

    } while (opcion != 10);

    delete[] A;
    delete[] B;
    delete[] R;

    printf("\n       PROGRAMA FINALIZADO CORRECTAMENTE\n");

    printf(" Cardenas Olivares Giovanny \n  ");
    printf(" Cervantes Arrizon Victor \n  ");
    printf(" Diaz Torres Montserrat \n  ");

    return 0;
}
