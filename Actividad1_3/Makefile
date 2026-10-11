CXX = C:/MinGW/bin/g++.exe
MPI_INC = C:/Program Files (x86)/Microsoft SDKs/MPI/Include
MPI_LIB = C:/Program Files (x86)/Microsoft SDKs/MPI/Lib/x86/msmpi.lib
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -fopenmp -DMSMPI_NO_SAL

all: actividad1_3_nueva.exe

actividad1_3_nueva.exe: actividad1_3_nueva.cpp
	$(CXX) $(CXXFLAGS) -I"$(MPI_INC)" -I"$(MPI_INC)/x86" actividad1_3_nueva.cpp "$(MPI_LIB)" -L. -lshell32 -o actividad1_3_nueva.exe
