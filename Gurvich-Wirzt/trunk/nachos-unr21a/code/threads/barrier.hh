#ifndef NACHOS_THREADS_BARRIER__HH
#define NACHOS_THREADS_BARRIER__HH

#include "condition.hh"
#include "lock.hh"

class Barrier
{
public:
    Barrier(const char *debugname, unsigned int bsize);
    ~Barrier();

    void Wait();
    const char *getName();

private:
    unsigned int size;
    unsigned int esperando;
    Lock *lockBarrera;
    Condition *pared;
    const char *name;
};

#endif