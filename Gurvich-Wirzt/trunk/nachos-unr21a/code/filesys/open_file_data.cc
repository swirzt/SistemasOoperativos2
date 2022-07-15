#include "open_file_data.hh"
#include "open_file.hh"
#include "file_system.hh"
#include "threads/system.hh"

OpenFileData::OpenFileData(OpenFile *fil)
{
    file = fil;
    numOpens = 1;
    numReaders = 0;
    numWriters = 0;
    writerActive = false;
    lock = new Lock(fil->GetName());
    condition = new Condition(fil->GetName(), lock);
    deleted = false;
    dataLock = new Lock(fil->GetName());
}

OpenFileData::~OpenFileData()
{
    ASSERT(numOpens == 0);
    if (deleted)
    {
        DEBUG('f', "Eliminando archivo %s\n", file->GetName());
        // Sabemos que nadie mas va a abrir el archivo porque deleted esta en true
        fileSystem->Remove(file->GetName());
    }
    delete file;
    delete lock;
    delete condition;
    delete dataLock;
}