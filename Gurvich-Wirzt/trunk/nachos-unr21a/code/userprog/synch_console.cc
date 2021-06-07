#include "synch_console.hh"

static void
readHandler(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *consola = (SynchConsole *)arg;
    consola->ReadAvail();
}

static void
writeHandler(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *consola = (SynchConsole *)arg;
    consola->WriteDone();
}

SynchConsole::SynchConsole()
{
    consola = new Console(nullptr, nullptr, readHandler, writeHandler, this);
    readAvail = new Semaphore("Console semaphore for reading", 0);
    writeDone = new Semaphore("Console WriteDone", 0);
    lockWrite = new Lock("Lock for the console write");
    lockRead = new Lock("Lock for the console read");
}

SynchConsole::~SynchConsole()
{
    delete consola;
    delete lockWrite;
    delete lockRead;
    delete writeDone;
    delete readAvail;
}

void SynchConsole::ReadAvail()
{
    readAvail->V();
}
void SynchConsole::WriteDone()
{
    writeDone->V();
}

void SynchConsole::WriteChar(const char *buffer)
{
    ASSERT(buffer != nullptr);
    consola->PutChar(*buffer);
    writeDone->P();
}

void SynchConsole::ReadChar(char *buffer)
{
    ASSERT(buffer != nullptr);
    readAvail->P();
    *buffer = consola->GetChar();
}

void SynchConsole::WriteBuffer(const char *buffer, int size)
{
    lockWrite->Acquire();
    for (int i = 0; i < size; i++)
    {
        WriteChar(buffer + i);
    }
    lockWrite->Release();
}

void SynchConsole::ReadBuffer(char *buffer, int size)
{
    lockRead->Acquire();
    for (int i = 0; i < size; i++)
    {
        ReadChar(buffer + i);
    }
    lockRead->Release();
}