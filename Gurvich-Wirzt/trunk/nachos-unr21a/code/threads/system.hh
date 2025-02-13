/// All global variables used in Nachos are defined here.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_SYSTEM__HH
#define NACHOS_THREADS_SYSTEM__HH

#include "thread.hh"
#include "scheduler.hh"
#include "lib/utility.hh"
#include "machine/interrupt.hh"
#include "machine/statistics.hh"
#include "machine/timer.hh"

// Cantidad de veces que se intenta leer memoria
#define READPASS 5

/// Initialization and cleanup routines.

// Initialization, called before anything else.
extern void Initialize(int argc, char **argv);

// Cleanup, called when Nachos is done.
extern void Cleanup();

extern Thread *currentThread;       ///< The thread holding the CPU.
extern Thread *threadToBeDestroyed; ///< The thread that just finished.
extern Scheduler *scheduler;        ///< The ready list.
extern Interrupt *interrupt;        ///< Interrupt status.
extern Statistics *stats;           ///< Performance metrics.
extern Timer *timer;                ///< The hardware alarm clock.

#ifdef USER_PROGRAM
#include "machine/machine.hh"
extern Machine *machine; // User program memory and registers.
#include "userprog/synch_console.hh"
extern SynchConsole *synchconsole;

#ifndef SWAP
#include "lib/bitmap.hh"
extern Bitmap *pages;
#else
#include "vmem/coremap.hh"
extern Coremap *pages;
#endif

#include "lib/table.hh"
extern Table<Thread *> *activeThreads;
#endif

#ifdef FILESYS_NEEDED // *FILESYS* or *FILESYS_STUB*.
#include "filesys/file_system.hh"
extern FileSystem *fileSystem;
#endif

#ifdef FILESYS
#include "filesys/synch_disk.hh"
extern SynchDisk *synchDisk;
#include "threads/lock.hh"
#include "threads/condition.hh"
#include "lib/list.hh"
#include "filesys/open_file_data.hh"
typedef LinkedList<int, OpenFileData *> OpenFilesList;
extern OpenFilesList *openFilesData;
extern Lock *openFilesDataLock;
typedef LinkedList<int, Lock *> OpenSysList;
extern OpenSysList *openSysList;
extern Lock *openSysLock;
extern Lock *freeMapLock;

#endif

#ifdef NETWORK
#include "network/post.hh"
extern PostOffice *postOffice;
#endif

#endif
