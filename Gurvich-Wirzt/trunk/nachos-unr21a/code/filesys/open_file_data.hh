#ifndef FILESYS_STUB
#ifndef NACHOS_FILESYS_OPENFILEDATA__HH
#define NACHOS_FILESYS_OPENFILEDATA__HH

#include "open_file.hh"
#include "threads/lock.hh"
#include "threads/condition.hh"

class OpenFileData
{
public:
    OpenFileData(OpenFile *fil);
    ~OpenFileData();

    OpenFile *file;
    unsigned int numOpens;
    unsigned int numReaders;
    unsigned int numWriters;
    bool writerActive;
    Lock *lock;
    Condition *condition;
    bool deleted;
};

#endif
#endif