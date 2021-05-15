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

#include "condition.hh"
#include "system.hh"
/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    lock = conditionLock;
    waitQueue = new List<Semaphore *>;
}

Condition::~Condition()
{
    delete waitQueue;
}

const char *
Condition::GetName() const
{
    return name;
}

void Condition::Wait()
{
    ASSERT(lock->IsHeldByCurrentThread());
    lock->Release();
    Semaphore *newSem = new Semaphore(currentThread->GetName(), 0);
    waitQueue->Append(newSem);
    DEBUG('t', "%s going to wait CONDITION\n", currentThread->GetName());
    // Entra a esperar
    newSem->P();
    DEBUG('t', "%s im awake CONDITION\n", currentThread->GetName());
    // LLegó un signal
    lock->Acquire();
}

void Condition::Signal()
{
    ASSERT(lock->IsHeldByCurrentThread());
    if (!waitQueue->IsEmpty())
    {
        Semaphore *toSignal = waitQueue->Pop();
        toSignal->V();
        delete toSignal;
    }
}

void Condition::Broadcast()
{
    ASSERT(lock->IsHeldByCurrentThread());
    while (!waitQueue->IsEmpty())
    {
        Semaphore *toSignal = waitQueue->Pop();
        toSignal->V();
        delete toSignal;
    }
}