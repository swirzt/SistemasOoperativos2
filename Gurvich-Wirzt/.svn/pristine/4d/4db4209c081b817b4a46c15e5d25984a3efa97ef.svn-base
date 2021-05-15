/// Data structures to export a synchronous interface to the raw disk device.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_SYNCHCONSOLE__HH
#define NACHOS_FILESYS_SYNCHCONSOLE__HH

#include "machine/console.hh"
#include "threads/lock.hh"
#include "threads/semaphore.hh"

class SynchConsole
{
public:
    SynchConsole();

    ~SynchConsole();

    // Si queremos leer/escribir un solo byte llamarla con size = 1
    void ReadBuffer(char *buffer, int size);        //Lectura desde un buffer
    void WriteBuffer(const char *buffer, int size); //Escritura a un buffer

    // Funciones que incrementan los semaforos
    void WriteDone();
    void ReadAvail();

private:
    void ReadChar(char *buffer);        //Lee un caracter
    void WriteChar(const char *buffer); //Escribe un caracter
    Console *consola;
    Semaphore *readAvail;
    Semaphore *writeDone;
    Lock *lockWrite; // Los bloqueamos por separado
    Lock *lockRead;
};

#endif
