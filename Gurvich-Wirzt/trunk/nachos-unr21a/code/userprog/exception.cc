/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.
#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "filesys/open_file_data.hh"
#include "threads/system.hh"
#include "args.hh"

#include <stdio.h>

/// Agregamoos sistema de archivos, no sabemos si alguien mas loo necesita, lo hacemos global para este archivo

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

// Initializes a forked thread
void initialize(void *args)
{
    currentThread->space->InitRegisters(); // Initialize registers
    currentThread->space->RestoreState();  // Copy the father's state

    if (args != nullptr)
    {
        unsigned argc = WriteArgs((char **)args);
        machine->WriteRegister(4, argc);
        int sp = machine->ReadRegister(STACK_REG);
        machine->WriteRegister(5, sp);
        machine->WriteRegister(STACK_REG, sp - 24); // Restamos 24 como nos recomienda el archivo args.hh
    }

    machine->Run(); // Run the program
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid)
    {

    case SC_HALT:
        DEBUG('e', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
        break;

    case SC_CREATE:
    {
        DEBUG('f', "Starting to create file \n");
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr,
                                filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                  FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }
        if (fileSystem->Create(filename))
        {
            DEBUG('e', "Succesfully created a new file with name %s \n", filename);
            machine->WriteRegister(2, 0);
        }
        else
        {
            DEBUG('e', "Error when creating a new file with name %s \n", filename);
            machine->WriteRegister(2, -1);
        }
        break;
    }

    case SC_REMOVE:
    {
        DEBUG('e', "Starting to remove file \n");
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr,
                                filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                  FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }

        if (fileSystem->Remove(filename))
        {
            DEBUG('e', "Succesfully removed file with name %s \n", filename);
            machine->WriteRegister(2, 0);
        }
        else
        {
            DEBUG('e', "Error when removing file with name %s \n", filename);
            machine->WriteRegister(2, -1);
        }
        break;
    }

    case SC_READ:
    {

        int bufferUsr = machine->ReadRegister(4);
        int size = machine->ReadRegister(5);
        OpenFileId id = machine->ReadRegister(6);

        int bytesRead;

        if (id < 0)
        {
            DEBUG('e', "File id non existant \n");
            machine->WriteRegister(2, 0);
            break;
        }

        if (size <= 0)
        {
            DEBUG('e', "Read size incorrect\n");
            machine->WriteRegister(2, 0);
            break;
        }

        char bufferSys[size];
        switch (id)
        {
        case CONSOLE_INPUT:
        {
            synchconsole->ReadBuffer(bufferSys, size);
            char algo = bufferSys[size]; // Esto hace que el debug imprima lindo
            bufferSys[size] = '\0';
            bufferSys[size] = algo; // Dejo el buffer como estaba
            bytesRead = size;
            if (bytesRead != 0)
                WriteBufferToUser(bufferSys, bufferUsr, bytesRead);
            break;
        }
        case CONSOLE_OUTPUT:
        {
            DEBUG('e', "Can't read from console output \n");
            bytesRead = 0;
            break;
        }
        default:
        {
            DEBUG('e', "Requested to read from file %d\n", id);
            if (!currentThread->fileExists(id))
            {
                DEBUG('e', "File non existant \n");
                bytesRead = 0;
            }
            else
            {
                OpenFile *file = currentThread->getFile(id);
                bytesRead = file->Read(bufferSys, size);
                if (bytesRead != 0)
                    WriteBufferToUser(bufferSys, bufferUsr, bytesRead);
            }
            break;
        }
        }

        machine->WriteRegister(2, bytesRead);
        break;
    }

    case SC_WRITE:
    {

        int bufferUsr = machine->ReadRegister(4);
        int size = machine->ReadRegister(5);
        int bytesWritten;
        OpenFileId id = machine->ReadRegister(6);

        if (id < 0)
        {
            DEBUG('e', "File id non existant \n");
            machine->WriteRegister(2, 0);
            break;
        }

        if (size <= 0)
        {
            DEBUG('e', "Write size incorrect\n");
            machine->WriteRegister(2, 0);
            break;
        }

        char bufferSys[size];
        ReadBufferFromUser(bufferUsr, bufferSys, size);
        switch (id)
        {
        case CONSOLE_INPUT:

            DEBUG('e', "Can't write to console input\n");
            bytesWritten = 0;
            break;

        case CONSOLE_OUTPUT:
            synchconsole->WriteBuffer(bufferSys, size);
            bytesWritten = size;
            break;

        default:

            DEBUG('e', "Requested to write to file %d\n", id);
            if (!currentThread->fileExists(id))
            {
                DEBUG('e', "File non existant \n");
                bytesWritten = 0;
            }
            else
            {
                OpenFile *file = currentThread->getFile(id);
                bytesWritten = file->Write(bufferSys, size);
                DEBUG('e', "Write to file: %s \n", bufferSys);
            }
            break;
        }
        machine->WriteRegister(2, bytesWritten);
        break;
    }
    case SC_EXIT:
    {
        int returnValue = machine->ReadRegister(4);
        if (returnValue == 0)
        {
            DEBUG('e', "Execution terminated normally for thread %s \n", currentThread->GetName());
        }
        else
        {
            DEBUG('e', "Execution terminated abnormally for thread %s return: %d\n", currentThread->GetName(), returnValue);
        }
        currentThread->Finish(returnValue);
        break;
    }

    case SC_OPEN:
    {

        int fileNameAddr = machine->ReadRegister(4);

        if (fileNameAddr == 0)
        {
            DEBUG('e', "File does not exists \n");
            machine->WriteRegister(2, -1);
            break;
        }

        char fileName[FILE_NAME_MAX_LEN + 1];

        if (!ReadStringFromUser(fileNameAddr, fileName, FILE_NAME_MAX_LEN + 1))
        {
            DEBUG('e', "FileName too big \n");
            machine->WriteRegister(2, -1);
        }
        else
        {
            OpenFile *archivo = fileSystem->Open(fileName);
            if (archivo != nullptr)
            {
                OpenFileId id = currentThread->createFile(archivo);
                if (id == -1)
                {
                    DEBUG('e', "Can't save file\n");
                    delete archivo; // Lo eliminamos porque no lo vamos a usar
                    machine->WriteRegister(2, -1);
                }
                else
                {
                    DEBUG('e', "Opened file id: %d\n", id);
                    machine->WriteRegister(2, id);
                }
            }
            else
            {
                DEBUG('e', "Invalid file \n");
                machine->WriteRegister(2, -1);
            }
        }
        break;
    }

    case SC_CLOSE:
    {
        int fid = machine->ReadRegister(4);
        DEBUG('e', "`Close` requested for id %u.\n", fid);
        OpenFile *archivo = currentThread->removeFile(fid);

        if (archivo == nullptr)
        {
            DEBUG('e', "Can't close file %u", fid);

            machine->WriteRegister(2, -1);
        }
        else
        {
            DEBUG('e', "Closed file %u", fid);
#ifndef FILESYS_STUB
            const char *name = archivo->GetName();

            OpenFileData *data;
            openFilesData->get(name, &data);

            data->dataLock->Acquire();
            data->numOpens--;
            if (data->numOpens > 0)
            {
                DEBUG('f', "Quedan %d lectores en %s\n", data->numOpens, name);
                data->dataLock->Release();
            }
            else
            {
                DEBUG('f', "Soy el ultimo que lee en %s\n", name);
                data->dataLock->Release();
                openFilesData->remove(name);
                delete data;
            }
#else
            delete archivo;
#endif

            machine->WriteRegister(2, 0);
        }

        break;
    }
    case SC_JOIN:
    {
        SpaceId id = machine->ReadRegister(4);
        if (!activeThreads->HasKey(id))
        {
            DEBUG('e', "Thread does not exists\n");
            machine->WriteRegister(2, -1);
        }
        else
        {
            DEBUG('e', "Started to join thread: %d\n", id);
            Thread *hilo = activeThreads->Get(id);
            int status = hilo->Join();
            DEBUG('e', "Thread joined\n");
            machine->WriteRegister(2, status);
        }
        break;
    }

    case SC_EXEC:
    {

        DEBUG('e', "Exec requested \n");
        int filenameAddr = machine->ReadRegister(4);
        int argsAddr = machine->ReadRegister(5);
        int joinable = machine->ReadRegister(6);
        char **args = nullptr;
        if (argsAddr)
            args = SaveArgs(argsAddr);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        // Este tiene que ser un puntero para no perder informacion cuando salimos de la pila
        // New nos lo almacena en heap
        char *filename = new char[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr,
                                filename, FILE_NAME_MAX_LEN + 1))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                  FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }

        OpenFile *archivo = fileSystem->Open(filename);
        DEBUG('e', "Filename is %s \n", filename);

        if (archivo == nullptr)
        {
            DEBUG('e', "Invalid file \n");
            machine->WriteRegister(2, -1);
        }
        else
        {
            DEBUG('e', "Creating a new thread\n");
            Thread *hilo = new Thread(filename, joinable, 0);
#ifdef SWAP
            AddressSpace *memoria = new AddressSpace(archivo, hilo->pid);
#else
            AddressSpace *memoria = new AddressSpace(archivo, 0);
#endif
            hilo->space = memoria;
            hilo->Fork(initialize, args);
            DEBUG('e', "Initialized new exec thread \n");
#ifdef FILESYS_STUB
#ifndef DEMAND_LOADING
            delete archivo;
#endif
#endif
            machine->WriteRegister(2, hilo->pid);
        }
        break;
    }

    default:
        fprintf(stderr, "Unexpected system call: id %d.\n", scid);
        ASSERT(false);
    }

    IncrementPC();
}

unsigned int tlbSelection = 0;

#ifdef USE_TLB
static void
PageFaultHandler(ExceptionType _et)
{
    int addr = machine->ReadRegister(BAD_VADDR_REG);
    int vpn = addr / PAGE_SIZE;
    TranslationEntry *entry = &(currentThread->space->pageTable[vpn]);
#ifdef DEMAND_LOADING
    if (!entry->valid)
        currentThread->space->LoadPage(vpn);
#endif
    TranslationEntry entrytlb = machine->GetMMU()->tlb[tlbSelection % TLB_SIZE];
    if (entrytlb.valid)
    {
        unsigned mmuvpn = entrytlb.virtualPage;
        currentThread->space->pageTable[mmuvpn] = entrytlb;
    }
    machine->GetMMU()->tlb[tlbSelection % TLB_SIZE] = *entry;
    tlbSelection++;
    stats->tlbMiss++;
}
#endif

static void ReadOnlyHandler(ExceptionType _et)
{
    DEBUG('e', "Tried to read from a read only page");
    // PREGUNTA PARA DAMIAN
    ASSERT(false); // Esto mata al SO, deberia solo matar al programa que intento?
    return;
}

/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION, &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION, &SyscallHandler);
#ifdef USE_TLB
    machine->SetHandler(PAGE_FAULT_EXCEPTION, &PageFaultHandler);
#else
    machine->SetHandler(PAGE_FAULT_EXCEPTION, &DefaultHandler);
#endif
    machine->SetHandler(READ_ONLY_EXCEPTION, &ReadOnlyHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION, &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
