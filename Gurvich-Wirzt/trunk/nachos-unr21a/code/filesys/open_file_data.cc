#include "open_file_data.hh"
#include "open_file.hh"

OpenFileData::OpenFileData(OpenFile *fil)
{
    file = fil;
    numReaders = 0;
    numWriters = 0;
    writerActive = false;
    lock = new Lock(fil->GetName());
    condition = new Condition(fil->GetName(), lock);
    deleted = false;
    deleteLock = new Lock(fil->GetName());
}

OpenFileData::~OpenFileData()
{
    delete lock;
    delete condition;
    delete deleteLock;
}