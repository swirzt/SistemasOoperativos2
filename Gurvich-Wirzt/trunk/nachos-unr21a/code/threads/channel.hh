#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

#include "lock.hh"
#include "barrier.hh"

class Channel
{
public:
    Channel(const char *debugname);

    ~Channel();

    void Send(int message);
    void Receive(int *message);
    const char *getName();

private:
    int *buzon;
    Lock *lockEmisor;
    Lock *lockReceptor;
    Barrier *barrera;

    const char *name;
};

#endif