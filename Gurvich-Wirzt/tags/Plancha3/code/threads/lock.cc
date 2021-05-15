/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "lock.hh"
#include "semaphore.hh"
#include "system.hh"
#include <string.h>

/// Dummy functions -- so we can compile our later assignments.

Lock::Lock(const char *debugName)
{
    name = debugName;
    sem = new Semaphore(debugName, 1);
    holder = nullptr;
}

Lock::~Lock()
{
    delete sem;
}

const char *
Lock::GetName() const
{
    return name;
}

// 0 menos importante, COLAS-1 mas importante

void Lock::Acquire()
{
    ASSERT(!IsHeldByCurrentThread());
    holder = currentThread->GetName();
    holderPriority = currentThread->GetPrio();
    currentThread->SetPrio(COLAS - 1);
    sem->P();
}

void Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    holder = nullptr;
    currentThread->SetPrio(holderPriority);
    sem->V();
}

bool Lock::IsHeldByCurrentThread() const
{
    return holder == currentThread->GetName();
}
