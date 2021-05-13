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
#include "threads/system.hh"

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
        DEBUG('e', "Starting to create file \n");
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr,
                                filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                  FILE_NAME_MAX_LEN);
            break;
        }
        printf("Ayudaaaaa %s\n", filename);
        DEBUG('e', "`Create` requested for file `%s`.\n", filename);
        ASSERT(fileSystem->Create(filename, 0));
        DEBUG('e', "Succesfully created a new file with name %s \n", filename);
        break;
    }

    case SC_REMOVE:
    {
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr,
                                filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                  FILE_NAME_MAX_LEN);
            break;
        }

        DEBUG('e', "Remove requested for file `%s`.\n", filename);
        ASSERT(fileSystem->Remove(filename));
        DEBUG('e', "Succesfully removed file with name %s \n", filename);

        break;
    }

    case SC_READ:
    {

        int bufferUsr = machine->ReadRegister(4);
        int size = machine->ReadRegister(5);
        int bytesRead;
        OpenFileId id = machine->ReadRegister(6);

        ASSERT(size > 0);
        ASSERT(id >= 0);

        char bufferSys[size];
        switch (id)
        {
        case CONSOLE_INPUT:
        {
            DEBUG('e', "Requested to read from console\n");
            synchconsole->ReadBuffer(bufferSys, size);
            DEBUG('e', "Read from console: %s \n", bufferSys);
            bytesRead = size;
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
            OpenFile *file = currentThread->getFile(id);
            bytesRead = file->Read(bufferSys, size);
            DEBUG('e', "Read from file: %s \n", bufferSys);
            break;
        }
        }

        WriteBufferToUser(bufferSys, bufferUsr, bytesRead);

        machine->WriteRegister(2, bytesRead);
        break;
    }

    case SC_WRITE:
    {

        int bufferUsr = machine->ReadRegister(4);
        int size = machine->ReadRegister(5);
        int bytesWritten;
        OpenFileId id = machine->ReadRegister(6);

        ASSERT(size > 0);
        ASSERT(id >= 0);

        char bufferSys[size + 1];
        ReadBufferFromUser(bufferUsr, bufferSys, size);
        DEBUG('e', "Quiero escribir en %d\n", id);
        switch (id)
        {
        case CONSOLE_INPUT:

            DEBUG('e', "Can't write to console input\n");
            bytesWritten = 0;
            break;

        case CONSOLE_OUTPUT:

            DEBUG('e', "Requested to write to console output \n");
            synchconsole->WriteBuffer(bufferSys, size);
            DEBUG('e', "Wrote to console output \n");
            bytesWritten = size;
            break;

        default:

            DEBUG('e', "Requested to read from file %d\n", id);
            OpenFile *file = currentThread->getFile(id);
            bytesWritten = file->Write(bufferSys, size);
            DEBUG('e', "Read from file: %s \n", bufferSys);
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
            DEBUG('e', "Execution termintated normally for thread %s \n", currentThread->GetName());
        }
        else
        {
            DEBUG('e', "Execution termintated abnormally for thread %s \n", currentThread->GetName());
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
            break;
        }
        else
        {
            OpenFile *archivo = fileSystem->Open(fileName);
            if (archivo != nullptr)
            {
                OpenFileId id = currentThread->createFile(archivo);
                DEBUG('e', "Guardamos el archivo %d\n", id);
                machine->WriteRegister(2, id);
            }
            else
            {
                DEBUG('e', "Invalid file \n");
                machine->WriteRegister(2, -1);
            }
        }

        break;
    }

    case SC_CLOSE: //Preguntar como devolver un valor
    {
        int fid = machine->ReadRegister(4);
        DEBUG('e', "`Close` requested for id %u.\n", fid);
        currentThread->removeFile(fid);
        machine->WriteRegister(2, 0);
        break;
    }

    default:
        fprintf(stderr, "Unexpected system call: id %d.\n", scid);
        ASSERT(false);
    }

    IncrementPC();
}

/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION, &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION, &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION, &DefaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION, &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION, &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
