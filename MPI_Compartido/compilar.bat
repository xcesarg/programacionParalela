@echo off
setlocal
cd /d "%~dp0"
set "GPP=C:\MinGW\bin\g++.exe"
set "MPI_INC=C:\Program Files (x86)\Microsoft SDKs\MPI\Include"
set "MPI_LIB=C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x86\msmpi.lib"
echo Compilando mpi_hola.cpp...
"%GPP%" -std=c++11 -DMSMPI_NO_SAL -I"%MPI_INC%" -I"%MPI_INC%\x86" "mpi_hola.cpp" "%MPI_LIB%" -o "mpi_hola.exe"
if errorlevel 1 exit /b 1
echo Compilando mpi_hibrido.cpp...
"%GPP%" -std=c++11 -fopenmp -DMSMPI_NO_SAL -I"%MPI_INC%" -I"%MPI_INC%\x86" "mpi_hibrido.cpp" "%MPI_LIB%" -L. -o "mpi_hibrido.exe"
if errorlevel 1 exit /b 1
echo Compilacion correcta: mpi_hola.exe y mpi_hibrido.exe

