/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "thread_test_simple.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>
#include "semaphore.hh"

#ifdef SEMAPHORE_TEST
Semaphore *sem = new Semaphore("Semaforo test simple", 3);
#endif

/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.
void SimpleThread(void *name_)
{
    // Reinterpret arg `name` as a string.
    char *name = (char *)name_;

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    for (unsigned num = 0; num < 10; num++)
    {
#ifdef SEMAPHORE_TEST
        DEBUG('s', "Esperando semaforo\n");
        sem->P();
        DEBUG('s', "Entro al semaforo\n");
#endif
        printf("*** Thread `%s` is running: iteration %u\n", name, num);
#ifdef SEMAPHORE_TEST
        DEBUG('s', "Saliendo del semaforo\n");
        sem->V();
        DEBUG('s', "Salio semaforo\n");
#endif
        currentThread->Yield();
    }
    printf("!!! Thread `%s` has finished\n", name);
}

/// Set up a ping-pong between several threads.
///
/// Do it by launching one thread which calls `SimpleThread`, and finally
/// calling `SimpleThread` on the current thread.

#define NUM_THREAD_FORK 4 // No usar 9 o mas

void ThreadTestSimple()
{
    for (int i = 0; i < NUM_THREAD_FORK; i++)
    {
        char *name = new char[64];
        strncpy(name, "Hilo numero: ", 64);
        char n = i + 50;
        name[13] = n;
        Thread *newThread = new Thread(name);
        newThread->Fork(SimpleThread, (void *)name);
    }
    SimpleThread((void *)"Hilo numero: 1");
}
