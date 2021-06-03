#include "channel.hh"
#include "barrier.hh"
#include "lock.hh"
#include "system.hh"

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
    barrera->Wait();
    DEBUG('t', "Thread: %s, going to send\n", currentThread->GetName());
    *buzon = message;
    barrera->Wait();
    lockEmisor->Release();
    // lockER->Acquire();
    // Condition *emisor = new Condition(currentThread->GetName(), lockER);
    // while (receptores->IsEmpty())
    // {
    //     emisores->Append(emisor);
    //     DEBUG('t', "Thread: %s, waiting for receptor\n", currentThread->GetName());
    //     emisor->Wait();
    //     DEBUG('t', "Thread: %s, there is a receptor\n", currentThread->GetName());
    // }
    // delete emisor;
    // DEBUG('t', "Thread: %s, going to send\n", currentThread->GetName());
    // Condition *receptor = receptores->Pop();
    // int *buffer = buzon->Pop();
    // *buffer = message;
    // receptor->Signal();
    // lockER->Release();
    DEBUG('t', "Thread: %s, sent\n", currentThread->GetName());
}

void Channel::Receive(int *message)
{
    DEBUG('t', "Thread: %s, receiving\n", currentThread->GetName());
    lockReceptor->Acquire();
    buzon = message;
    barrera->Wait();
    barrera->Wait();
    buzon = nullptr;
    lockReceptor->Release();
    // lockER->Acquire();
    // Condition *receptor = new Condition(currentThread->GetName(), lockER);
    // receptores->Append(receptor);
    // buzon->Append(message);
    // if (!emisores->IsEmpty())
    // {
    //     DEBUG('t', "Thread: %s, signaling emisor\n", currentThread->GetName());
    //     Condition *emisor = emisores->Pop();
    //     DEBUG('t', "Voy a signalear %s\n", emisor->GetName());
    //     emisor->Signal();
    // }
    // DEBUG('t', "Thread: %s, going to wait, releasing lock\n", currentThread->GetName());
    // receptor->Wait();
    // delete receptor;
    // lockER->Release();
    DEBUG('t', "Thread: %s, received\n", currentThread->GetName());
}