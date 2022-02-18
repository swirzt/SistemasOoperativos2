#include "channel.hh"
#include "barrier.hh"
#include "lock.hh"
#include "system.hh"

/*
Un canal permite transmitir datos de tamaño de un char entre 2 threads
Para hacerlo, el emisor y el receptor deben tomar el lock que le corresponde a cada uno
Luego por medio de una barrera se sincroniza el envío del mensaje
*/

Channel::Channel(const char *debugname)
{
    name = debugname;
    lockEmisor = new Lock("Lock Emisor canal");
    lockReceptor = new Lock("Lock Receptor canal");
    barrera = new Barrier("Barrera canal", 2);
    buzon = nullptr;
}

Channel::~Channel()
{
    delete lockEmisor;
    delete lockReceptor;
    delete barrera;
}

const char *Channel::getName()
{
    return name;
}

void Channel::Send(int message)
{
    DEBUG('t', "Thread: %s, entering to send\n", currentThread->GetName());
    lockEmisor->Acquire();
    DEBUG('t', "Adquired SendLock\n");
    barrera->Wait();
    DEBUG('t', "Thread: %s, going to send\n", currentThread->GetName());
    *buzon = message;
    barrera->Wait();
    lockEmisor->Release();
    DEBUG('t', "Thread: %s, sent\n", currentThread->GetName());
}

void Channel::Receive(int *message)
{
    DEBUG('t', "Thread: %s, receiving\n", currentThread->GetName());
    lockReceptor->Acquire();
    DEBUG('t', "Adquired ReceiveLock\n");
    buzon = message;
    barrera->Wait();
    DEBUG('t', "There is an emissor\n");
    barrera->Wait();
    buzon = nullptr;
    lockReceptor->Release();
    DEBUG('t', "Thread: %s, received\n", currentThread->GetName());
}