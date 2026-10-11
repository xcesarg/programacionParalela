/*
 * Actividad 1.3: Reduce en MPI con OpenMP.
 * Operaciones distribuidas con MPI y OpenMP.
 */
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <mpi.h>
#include <omp.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <new>
#include <sstream>
#include <string>

static const char *INTEGRANTES =
    "CARDENAS OLIVARES CESAR GIOVANNY; CERVANTES ARRIZON VICTOR HUGO; DIAZ TORRES JAZMIN MONTSERRAT";

static std::string equipoFisico;

static const char *nombreDelNodo(int) { return equipoFisico.c_str(); }

class Registro {
    FILE *archivo_;
    int rank_;
    std::string equipo_;
public:
    Registro(int rank, const std::string &directorio)
        : archivo_(NULL), rank_(rank), equipo_(nombreDelNodo(rank)) {
        std::ostringstream ruta;
        ruta << directorio << "log_equipo_" << equipo_ << "_nodo_" << rank_ << ".txt";
        archivo_ = std::fopen(ruta.str().c_str(), "a");
    }
    bool valido() const { return archivo_ != NULL; }
    void escribir(const std::string &linea, bool consola = true) {
        #pragma omp critical(registro)
        {
            if (consola) {
                std::fputs((linea + "\n").c_str(), stdout);
                std::fflush(stdout);
            }
            if (archivo_) {
                std::fputs((linea + "\n").c_str(), archivo_);
                std::fflush(archivo_);
            }
        }
    }
    void evento(const std::string &operacion, int hilo = -1, int posicion = -1,
                const std::string &detalle = "", bool consola = true) {
        std::ostringstream s;
        s << "[Equipo: " << equipo_ << "] [Proceso MPI: " << rank_ << "]";
        if (hilo >= 0) s << " [Hilo OpenMP: " << hilo << "]";
        if (posicion >= 0) s << " [Posicion: " << posicion << "]";
        s << " [Operacion: " << operacion << "]";
        if (!detalle.empty()) s << " [" << detalle << "]";
        escribir(s.str(), consola);
    }
    void elemento(const char *equipo, int nodo, const std::string &operacion,
                  int hilo, int posicion, double valor) {
        std::ostringstream s;
        s << "[Equipo: " << equipo << "] [Proceso MPI: " << nodo
          << "] [Hilo OpenMP: " << hilo << "] [Posicion: " << posicion
          << "] [Operacion: " << operacion << "] [Valor: "
          << std::fixed << std::setprecision(0) << valor << "]";
        escribir(s.str());
    }
    void cerrar() {
        if (archivo_) { std::fclose(archivo_); archivo_ = NULL; }
    }
    ~Registro() { cerrar(); }
};

struct Seccion {
    int inicio;
    int cantidad;
};

class OperacionesArreglos {
    double *A_, *B_, *C_;
    Seccion parte_;
    Registro &log_;
    bool detalle_;
    int rank_;
public:
    OperacionesArreglos(Registro &log, int rank)
        : A_(NULL), B_(NULL), C_(NULL), parte_{0, 0}, log_(log),
          detalle_(false), rank_(rank) {}
    ~OperacionesArreglos() { liberar(); }
    void liberar() {
        delete[] A_; delete[] B_; delete[] C_;
        A_ = B_ = C_ = NULL;
    }
    bool crearArregloMPI(Seccion parte, bool detalle) {
        liberar();
        parte_ = parte;
        detalle_ = detalle;
        A_ = new(std::nothrow) double[parte.cantidad]();
        B_ = new(std::nothrow) double[parte.cantidad]();
        C_ = new(std::nothrow) double[parte.cantidad]();
        return A_ && B_ && C_;
    }
    double *A() { return A_; }
    double *B() { return B_; }
    double *C() { return C_; }
    int cantidad() const { return parte_.cantidad; }
    int inicio() const { return parte_.inicio; }

    void llenarSecuencial(double *a, double *b, double *c, const char *equipo, int nodo) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            int pos = parte_.inicio + i;
            a[i] = (double)(pos + 1);
            b[i] = (double)(2LL * (pos + 1));
            c[i] = 0.0;
            if (detalle_) {
                log_.elemento(equipo,nodo,"Llenar secuencial A",omp_get_thread_num(),pos,a[i]);
                log_.elemento(equipo,nodo,"Llenar secuencial B",omp_get_thread_num(),pos,b[i]);
            }
        }
    }
    void llenarAleatorio(double *a, double *b, double *c, const char *equipo, int nodo) {
        /* srand combina el tiempo con el rank; cada hilo deriva su propia semilla. */
        std::srand((unsigned)std::time(NULL) ^ ((unsigned)(rank_ + 1) * 2654435761u));
        #pragma omp parallel
        {
            unsigned semilla;
            #pragma omp critical(semilla_rand)
            semilla = (unsigned)std::rand() ^ ((unsigned)(omp_get_thread_num() + 1) * 2246822519u);
            if (!semilla) semilla = 1;
            #pragma omp for schedule(static)
            for (int i = 0; i < parte_.cantidad; ++i) {
                int pos = parte_.inicio + i;
                semilla ^= semilla << 13; semilla ^= semilla >> 17; semilla ^= semilla << 5;
                a[i] = (double)(semilla % 1000000u + 1u);
                semilla ^= semilla << 13; semilla ^= semilla >> 17; semilla ^= semilla << 5;
                b[i] = (double)(semilla % 1000000u + 1u);
                c[i] = 0.0;
                if (detalle_) {
                    log_.elemento(equipo,nodo,"Llenar aleatorio A",omp_get_thread_num(),pos,a[i]);
                    log_.elemento(equipo,nodo,"Llenar aleatorio B",omp_get_thread_num(),pos,b[i]);
                }
            }
        }
    }
    void sumar(const double *a, const double *b, double *c, const char *equipo, int nodo) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            c[i] = a[i] + b[i];
            if (detalle_) log_.elemento(equipo,nodo,"Suma A+B",omp_get_thread_num(),parte_.inicio+i,c[i]);
        }
    }
    void restar(const double *a, const double *b, double *c, const char *equipo, int nodo) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            c[i] = a[i] - b[i];
            if (detalle_) log_.elemento(equipo,nodo,"Resta A-B",omp_get_thread_num(),parte_.inicio+i,c[i]);
        }
    }
    void multiplicar(const double *a, const double *b, double *c, const char *equipo, int nodo) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            c[i] = a[i] * b[i];
            if (detalle_) log_.elemento(equipo,nodo,"Multiplicacion A*B",omp_get_thread_num(),parte_.inicio+i,c[i]);
        }
    }
    void cuadrado(const double *a, double *c, const char *equipo, int nodo) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            c[i] = a[i] * a[i];
            if (detalle_) log_.elemento(equipo,nodo,"Cuadrado A",omp_get_thread_num(),parte_.inicio+i,c[i]);
        }
    }
    double sumaLocal(const double *a, const std::string &nombre, const char *equipo, int nodo) {
        double suma = 0.0;
        #pragma omp parallel for reduction(+:suma) schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            suma += a[i];
            if (detalle_) log_.elemento(equipo,nodo,nombre,omp_get_thread_num(),parte_.inicio+i,a[i]);
        }
        return suma;
    }
    double maximoLocal(const double *a, const std::string &nombre, const char *equipo, int nodo) {
        double maximo = -1.0e300;
        #pragma omp parallel for reduction(max:maximo) schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            if (a[i] > maximo) maximo = a[i];
            if (detalle_) log_.elemento(equipo,nodo,nombre,omp_get_thread_num(),parte_.inicio+i,a[i]);
        }
        return maximo;
    }
    double minimoLocal(const double *a, const std::string &nombre, const char *equipo, int nodo) {
        double minimo = 1.0e300;
        #pragma omp parallel for reduction(min:minimo) schedule(static)
        for (int i = 0; i < parte_.cantidad; ++i) {
            if (a[i] < minimo) minimo = a[i];
            if (detalle_) log_.elemento(equipo,nodo,nombre,omp_get_thread_num(),parte_.inicio+i,a[i]);
        }
        return minimo;
    }
};

static int leerEntero(Registro &log, const std::string &pregunta) {
    log.evento(pregunta);
    std::string linea;
    if (!std::getline(std::cin, linea)) return -1;
    std::istringstream s(linea);
    int valor; char sobrante;
    if (!(s >> valor) || (s >> sobrante)) {
        log.evento("Entrada invalida", -1, -1, "Texto: " + linea);
        return -1;
    }
    log.evento("Entrada seleccionada", -1, -1, "Valor: " + linea);
    return valor;
}

static void repartirDatos(int modo, int rank, int n, int size,
                          const int *cuentas, const int *despl,
                          OperacionesArreglos &op, double *globalA, double *globalB) {
    const int q = op.cantidad(), ini = op.inicio();
    if (modo == 1) {
        if (rank == 0) {
            std::copy(op.A(), op.A()+q, globalA+ini);
            std::copy(op.B(), op.B()+q, globalB+ini);
            for (int r=1; r<size; ++r) {
                MPI_Recv(globalA+despl[r], cuentas[r], MPI_DOUBLE, r, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(globalB+despl[r], cuentas[r], MPI_DOUBLE, r, 102, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            for (int r=1; r<size; ++r) {
                MPI_Send(globalA+despl[r], cuentas[r], MPI_DOUBLE, r, 103, MPI_COMM_WORLD);
                MPI_Send(globalB+despl[r], cuentas[r], MPI_DOUBLE, r, 104, MPI_COMM_WORLD);
            }
        } else {
            MPI_Send(op.A(), q, MPI_DOUBLE, 0, 101, MPI_COMM_WORLD);
            MPI_Send(op.B(), q, MPI_DOUBLE, 0, 102, MPI_COMM_WORLD);
            MPI_Recv(op.A(), q, MPI_DOUBLE, 0, 103, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(op.B(), q, MPI_DOUBLE, 0, 104, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        if (n % size == 0) {
            MPI_Gather(op.A(), q, MPI_DOUBLE, globalA, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Gather(op.B(), q, MPI_DOUBLE, globalB, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Scatter(globalA, q, MPI_DOUBLE, op.A(), q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Scatter(globalB, q, MPI_DOUBLE, op.B(), q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        } else {
            MPI_Gatherv(op.A(), q, MPI_DOUBLE, globalA, cuentas, despl, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Gatherv(op.B(), q, MPI_DOUBLE, globalB, cuentas, despl, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Scatterv(globalA, cuentas, despl, MPI_DOUBLE, op.A(), q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            MPI_Scatterv(globalB, cuentas, despl, MPI_DOUBLE, op.B(), q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        }
    }
}

static void reunirResultado(int modo, int rank, int n, int size,
                            const int *cuentas, const int *despl,
                            OperacionesArreglos &op, double *globalC) {
    const int q = op.cantidad();
    if (modo == 1) {
        if (rank == 0) {
            std::copy(op.C(), op.C()+q, globalC);
            for (int r=1; r<size; ++r)
                MPI_Recv(globalC+despl[r], cuentas[r], MPI_DOUBLE, r, 105, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else MPI_Send(op.C(), q, MPI_DOUBLE, 0, 105, MPI_COMM_WORLD);
    } else if (n % size == 0) {
        MPI_Gather(op.C(), q, MPI_DOUBLE, globalC, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    } else {
        MPI_Gatherv(op.C(), q, MPI_DOUBLE, globalC, cuentas, despl, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
}

static double combinar(double a, double b, int opcion) {
    if (opcion == 10) return std::max(a,b);
    if (opcion == 11) return std::min(a,b);
    return a+b;
}

static double reduccionGlobal(double local, int opcion, int modo, int variante,
                              int rank, int size) {
    double global = local;
    MPI_Op tipo = (opcion == 10) ? MPI_MAX : ((opcion == 11) ? MPI_MIN : MPI_SUM);
    if (modo == 1) {
        if (rank == 0) {
            for (int r=1; r<size; ++r) {
                double otro;
                MPI_Recv(&otro, 1, MPI_DOUBLE, r, 200+opcion, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                global = combinar(global, otro, opcion);
            }
        } else MPI_Send(&local, 1, MPI_DOUBLE, 0, 200+opcion, MPI_COMM_WORLD);
    } else if (modo == 2) {
        double *parciales = rank == 0 ? new double[size] : NULL;
        MPI_Gather(&local, 1, MPI_DOUBLE, parciales, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            global = parciales[0];
            for (int r=1; r<size; ++r) global = combinar(global, parciales[r], opcion);
            delete[] parciales;
        }
    } else if (variante == 2) {
        MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, tipo, MPI_COMM_WORLD);
    } else {
        MPI_Reduce(&local, &global, 1, MPI_DOUBLE, tipo, 0, MPI_COMM_WORLD);
    }
    return global;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    char nombreFisico[MPI_MAX_PROCESSOR_NAME];
    int longitud;
    MPI_Get_processor_name(nombreFisico, &longitud);
    equipoFisico.assign(nombreFisico, longitud);
    if (size < 3) {
        if (rank == 0) std::fprintf(stderr, "Se requieren al menos 3 procesos MPI.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    char *carpetaLocal = new char[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, carpetaLocal))) {
        delete[] carpetaLocal;
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    std::string directorio = std::string(carpetaLocal) + "\\Actividad1_3";
    delete[] carpetaLocal;
    if (!CreateDirectoryA(directorio.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    directorio += "\\";
    Registro log(rank, directorio);
    int logOk = log.valido(), todosLog = 0;
    MPI_Allreduce(&logOk, &todosLog, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!todosLog) {
        if (rank == 0) std::fprintf(stderr, "No se pudo crear un archivo log por rank.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    if (rank == 0) log.escribir(INTEGRANTES);
    MPI_Barrier(MPI_COMM_WORLD);
    int hilos = 1;
    #pragma omp parallel
    {
        #pragma omp single
        hilos = omp_get_num_threads();
    }
    std::time_t ahora = std::time(NULL);
    char fecha[40];
    std::strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M:%S", std::localtime(&ahora));
    log.evento("Nodo listo", -1, -1, std::string("Fecha: ")+fecha+"; Hilos: "+std::to_string(hilos));

    OperacionesArreglos op(log, rank);
    int *cuentas = new int[size], *despl = new int[size];
    double *globalA = NULL, *globalB = NULL, *globalC = NULL;
    int n=0, modo=3;
    bool llenos = false, resultado = false;
    for (;;) {
        int opcion=12;
        if (rank == 0) {
            log.evento("1 Crear arreglos | 2 Sumar | 3 Restar | 4 Multiplicar");
            log.evento("5 Cuadrado | 6 Llenar secuencial | 7 Llenar aleatorio | 8 Sumatoria");
            log.evento("9 Promedio | 10 Maximo | 11 Minimo | 12 Salir");
            opcion = leerEntero(log, "Seleccione una opcion (1-12)");
            if (std::cin.eof()) opcion = 12;
        }
        MPI_Bcast(&opcion, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (opcion == 12) break;
        if (opcion < 1 || opcion > 12) {
            if (rank == 0) log.evento("Opcion invalida");
            continue;
        }
        if (opcion == 1) {
            int nuevo=0, nuevoModo=0;
            if (rank == 0) {
                nuevo=leerEntero(log, "Tamano del arreglo: 40 o 4000000");
                nuevoModo=leerEntero(log, "Version MPI: 1 Send/Recv | 2 Scatter/Gather | 3 Reduce/Allreduce");
                if ((nuevo != 40 && nuevo != 4000000) || nuevoModo < 1 || nuevoModo > 3)
                    nuevo=0;
            }
            MPI_Bcast(&nuevo, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&nuevoModo, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (nuevo == 0) {
                if (rank == 0) log.evento("Tamano o version invalida");
                continue;
            }
            MPI_Barrier(MPI_COMM_WORLD);
            double inicioCreacion = MPI_Wtime();
            op.liberar();
            delete[] globalA; delete[] globalB; delete[] globalC;
            globalA=globalB=globalC=NULL;
            int base=nuevo/size, resto=nuevo%size;
            for (int r=0; r<size; ++r) {
                cuentas[r]=base+(r<resto);
                despl[r]=r*base+std::min(r,resto);
            }
            bool localOk=op.crearArregloMPI({despl[rank],cuentas[rank]}, nuevo==40);
            if (rank==0) {
                globalA=new(std::nothrow) double[nuevo];
                globalB=new(std::nothrow) double[nuevo];
                globalC=new(std::nothrow) double[nuevo];
                localOk=localOk && globalA && globalB && globalC;
            }
            int ok=localOk ? 1:0, todos=0;
            MPI_Allreduce(&ok, &todos, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
            if (!todos) {
                n=0; llenos=false; resultado=false;
                if (rank==0) log.evento("Memoria insuficiente");
                continue;
            }
            n=nuevo; modo=nuevoModo; llenos=false; resultado=false;
            std::ostringstream s;
            s << "Arreglos A, B y C creados; elementos locales: " << cuentas[rank]
              << "; inicio: " << despl[rank] << "; version MPI: " << modo;
            log.evento(s.str(),-1,-1,"",n==40);
            double tiempoCreacion = MPI_Wtime()-inicioCreacion, tiempoGlobalCreacion=0;
            MPI_Reduce(&tiempoCreacion,&tiempoGlobalCreacion,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
            std::ostringstream tc;
            tc << std::fixed << std::setprecision(6) << tiempoCreacion;
            log.evento("Tiempo local opcion 1",-1,-1,"Segundos: "+tc.str(),n==40);
            MPI_Bcast(&tiempoGlobalCreacion,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
            tc.str(""); tc.clear();
            tc << std::fixed << std::setprecision(6) << tiempoGlobalCreacion;
            log.evento("Tiempo global opcion 1",-1,-1,"Segundos: "+tc.str(),rank==0);
            continue;
        }
        if (n==0) {
            if (rank==0) log.evento("Primero cree los arreglos con opcion 1");
            continue;
        }
        if (opcion>=2 && opcion<=5 && !llenos) {
            if (rank==0) log.evento("Primero llene A y B con opcion 6 o 7");
            continue;
        }
        if (opcion>=8 && opcion<=11 && !llenos) {
            if (rank==0) log.evento("Primero llene los arreglos");
            continue;
        }
        int arreglo=1, variante=1;
        if (opcion>=8 && opcion<=11) {
            if (rank==0) {
                arreglo=leerEntero(log,"Arreglo para reducir: 1 A | 2 B | 3 C");
                if (modo==3) variante=leerEntero(log,"Colectiva: 1 MPI_Reduce | 2 MPI_Allreduce");
                if (arreglo==3 && !resultado) {
                    log.evento("Arreglo C aun no tiene resultado; se usara A");
                    arreglo=1;
                }
            }
            MPI_Bcast(&arreglo,1,MPI_INT,0,MPI_COMM_WORLD);
            MPI_Bcast(&variante,1,MPI_INT,0,MPI_COMM_WORLD);
            if (arreglo<1 || arreglo>3 || variante<1 || variante>2) {
                if (rank==0) log.evento("Seleccion invalida");
                continue;
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if (n==40 && opcion>=8)
            log.evento(modo==1 ? "Reducir con Send/Recv" : modo==2 ? "Reducir con Gather" :
                       variante==1 ? "Reducir con MPI_Reduce" : "Reducir con MPI_Allreduce");
        double inicio=MPI_Wtime();
        if (opcion==6 || opcion==7) {
            if (opcion==6) op.llenarSecuencial(op.A(),op.B(),op.C(),nombreDelNodo(rank),rank);
            else op.llenarAleatorio(op.A(),op.B(),op.C(),nombreDelNodo(rank),rank);
            if (n==40) log.evento(rank==0 ? "Reuniendo y distribuyendo bloques A y B" : "Intercambiando bloques A y B con el maestro");
            repartirDatos(modo,rank,n,size,cuentas,despl,op,globalA,globalB);
            llenos=true; resultado=false;
        } else if (opcion>=2 && opcion<=5) {
            if (opcion==2) op.sumar(op.A(),op.B(),op.C(),nombreDelNodo(rank),rank);
            if (opcion==3) op.restar(op.A(),op.B(),op.C(),nombreDelNodo(rank),rank);
            if (opcion==4) op.multiplicar(op.A(),op.B(),op.C(),nombreDelNodo(rank),rank);
            if (opcion==5) op.cuadrado(op.A(),op.C(),nombreDelNodo(rank),rank);
            if (n==40) log.evento(rank==0 ? "Reuniendo resultado C" : "Enviando bloque C al maestro");
            reunirResultado(modo,rank,n,size,cuentas,despl,op,globalC);
            resultado=true;
        } else {
            double *datos = arreglo==1 ? op.A() : (arreglo==2 ? op.B() : op.C());
            std::string nombre=opcion==8?"Sumatoria Local":opcion==9?"Promedio Local":opcion==10?"Maximo Local":"Minimo Local";
            double local = opcion==10 ? op.maximoLocal(datos,nombre,nombreDelNodo(rank),rank)
                          : opcion==11 ? op.minimoLocal(datos,nombre,nombreDelNodo(rank),rank)
                          : op.sumaLocal(datos,nombre,nombreDelNodo(rank),rank);
            if (n==40) {
                std::ostringstream parcial;
                parcial << std::fixed << std::setprecision(opcion==9?4:0)
                        << (opcion==9 ? local/op.cantidad() : local);
                log.evento("Parcial "+nombre,-1,-1,"Valor: "+parcial.str());
            }
            double global=reduccionGlobal(local,opcion,modo,variante,rank,size);
            if (opcion==9 && (rank==0 || (modo==3 && variante==2))) global/=n;
            if (n==40 && (rank==0 || (modo==3 && variante==2))) {
                std::ostringstream s;
                s << std::fixed << std::setprecision(opcion==9?4:0) << global;
                std::string metodo = modo==1 ? "Send/Recv" : modo==2 ? "Gather" :
                    variante==1 ? "MPI_Reduce" : "MPI_Allreduce";
                log.evento("Resultado global "+nombre,-1,-1,"Valor: "+s.str()+"; Metodo: "+metodo);
            }
        }
        double localSeg=MPI_Wtime()-inicio, globalSeg=0;
        MPI_Reduce(&localSeg,&globalSeg,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
        std::ostringstream tiempo;
        tiempo << std::fixed << std::setprecision(6) << localSeg;
        log.evento("Tiempo local opcion "+std::to_string(opcion),-1,-1,"Segundos: "+tiempo.str(),n==40);
        MPI_Bcast(&globalSeg,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
        {
            tiempo.str(""); tiempo.clear();
            tiempo << std::fixed << std::setprecision(6) << globalSeg;
            std::string metodo = modo==1 ? "Send/Recv" : modo==2 ? "Scatter/Gather" :
                (opcion>=8 ? (variante==1 ? "MPI_Reduce" : "MPI_Allreduce") : "Scatter/Gather");
            log.evento("Tiempo global opcion "+std::to_string(opcion),-1,-1,
                "Segundos: "+tiempo.str()+"; Metodo: "+metodo+"; N: "+std::to_string(n),rank==0);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank==0) log.escribir(INTEGRANTES);
    delete[] globalA; delete[] globalB; delete[] globalC;
    delete[] cuentas; delete[] despl;
    log.cerrar();
    MPI_Finalize();
    return 0;
}
