#ifndef OPERACIONESARREGLOS_H
#define OPERACIONESARREGLOS_H

// Cardenas Olivares Giovanny
// Cervantes Arrizon Victor
// Diaz Torres Montserrat
class OperacionesArreglos
{
public:

    void sumarArreglosOpenMP(int* A, int* B, int* R, int tam);

    void restarArreglosOpenMP(int* A, int* B, int* R, int tam);

    void multiplicarArreglosOpenMP(int* A, int* B, int* R, int tam);

    void cuadradoArregloOpenMP(int* A, int* R, int tam);

    void sumatoriaArregloOpenMP(const int* arreglo, int tam, long long* resultado);

    void promedioArregloOpenMP(const int* arreglo, int tam, double* resultado);

    void maximoArregloOpenMP(const int* arreglo, int tam, int* resultado);

    void minimoArregloOpenMP(const int* arreglo, int tam, int* resultado);
};

#endif
