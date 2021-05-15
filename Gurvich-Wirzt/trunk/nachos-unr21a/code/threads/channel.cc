#include "channel.hh"
#include "condition.hh"
#include "lock.hh"
#include "system.hh"

Channel::Channel(const char *debugname)
{
    name = debugname;
    lockER = new Lock(name);
    buzon = new List<int *>;
    emisores = new List<Condition *>;
    receptores = new List<Condition *>;
}

Channel::~Channel()
{
    delete lockER;
    delete buzon;
    delete emisores;
    delete receptores;
}

const char *Channel::getName()
{
    return name;
}

void Channel::Send(int message)
{
    DEBUG('t', "Thread: %s, entering to send\n", currentThread->GetName());
    lockER->Acquire();
    Condition *emisor = new Condition(currentThread->GetName(), lockER);
    while (receptores->IsEmpty())
    {
        emisores->Append(emisor);
        DEBUG('t', "Thread: %s, waiting for receptor\n", currentThread->GetName());
        emisor->Wait();
        DEBUG('t', "Thread: %s, there is a receptor\n", currentThread->GetName());
    }
    delete emisor;
    DEBUG('t', "Thread: %s, going to send\n", currentThread->GetName());
    Condition *receptor = receptores->Pop();
    int *buffer = buzon->Pop();
    *buffer = message;
    receptor->Signal();
    lockER->Release();
    DEBUG('t', "Thread: %s, sent\n", currentThread->GetName());
}

void Channel::Receive(int *message)
{
    DEBUG('t', "Thread: %s, receiving\n", currentThread->GetName());
    lockER->Acquire();
    Condition *receptor = new Condition(currentThread->GetName(), lockER);
    receptores->Append(receptor);
    buzon->Append(message);
    if (!emisores->IsEmpty())
    {
        DEBUG('t', "Thread: %s, signaling emisor\n", currentThread->GetName());
        Condition *emisor = emisores->Pop();
        DEBUG('t', "Voy a signalear %s\n", emisor->GetName());
        emisor->Signal();
    }
    DEBUG('t', "Thread: %s, going to wait, releasing lock\n", currentThread->GetName());
    receptor->Wait();
    delete receptor;
    lockER->Release();
    DEBUG('t', "Thread: %s, received\n", currentThread->GetName());
}