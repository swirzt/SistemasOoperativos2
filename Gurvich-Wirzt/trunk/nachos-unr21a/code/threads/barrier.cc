#include "barrier.hh"
#include "condition.hh"
#include "lock.hh"

Barrier::Barrier(const char *debugname, unsigned int bsize)
{
    lockBarrera = new Lock("Barrier Lock");
    pared = new Condition("Barrier conditional", lockBarrera);
    name = debugname;
    this->size = size;
    esperando = 0;
}

Barrier::~Barrier()
{
    delete lockBarrera;
    delete pared;
}

void Barrier::Wait()
{
    lockBarrera->Acquire();
    if (size - 1 == esperando)
    {
        pared->Broadcast();
        esperando = 0;
    }
    else
    {
        esperando++;
        pared->Wait();
    }
    lockBarrera->Release();
}

const char *Barrier::getName()
{
    return name;
}