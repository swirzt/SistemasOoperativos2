#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

#include "condition.hh"
#include "lock.hh"

class Channel
{
public:
    Channel(const char *debugname);

    ~Channel();

    void Send(int message);
    void Receive(int *message);
    const char *getName();

private:
    List<Condition *> *emisores;
    List<Condition *> *receptores;
    List<int *> *mensajes;
    Lock *lockER;
    const char *name;
};

#endif