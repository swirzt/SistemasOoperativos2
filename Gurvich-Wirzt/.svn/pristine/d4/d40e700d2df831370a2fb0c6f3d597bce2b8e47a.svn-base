#include "channel.hh"
#include "condition.hh"
#include "lock.hh"
#include "system.hh"

Channel::Channel(const char *debugname)
{
    name = debugname;
    lockER = new Lock(name);
    mensajes = new List<int *>;
    emisores = new List<Condition *>;
    receptores = new List<Condition *>;
}

Channel::~Channel()
{
    delete lockER;
    delete mensajes;
    delete emisores;
    delete receptores;
}

const char *Channel::getName()
{
    return name;
}

void Channel::Send(int message)
{
    lockER->Acquire();
    Condition *emisor = new Condition(currentThread->GetName(), lockER);
    while (receptores->IsEmpty())
    {
        emisores->Append(emisor);
        emisor->Wait();
    }
    delete emisor;
    Condition *receptor = receptores->Pop();
    int *buffer = mensajes->Pop();
    *buffer = message;
    receptor->Signal();
    lockER->Release();
}

void Channel::Receive(int *message)
{
    lockER->Acquire();
    Condition *receptor = new Condition(currentThread->GetName(), lockER);
    receptores->Append(receptor);
    mensajes->Append(message);
    if (!emisores->IsEmpty())
    {
        Condition *emisor = emisores->Pop();
        emisor->Signal();
    }
    receptor->Wait();
    delete receptor;
    lockER->Release();
}