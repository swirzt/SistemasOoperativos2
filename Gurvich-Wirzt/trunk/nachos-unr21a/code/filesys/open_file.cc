/// Routines to manage an open Nachos file.  As in UNIX, a file must be open
/// before we can read or write to it.  Once we are all done, we can close it
/// (in Nachos, by deleting the `OpenFile` data structure).
///
/// Also as in UNIX, for convenience, we keep the file header in memory while
/// the file is open.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "open_file.hh"
#include "file_header.hh"
#include "threads/system.hh"
#include "lib/linkedlist.hh"
#include "threads/system.hh"
#include "lib/utility.hh"
#include "filesys/open_file_data.hh"
#include <string.h>

/// Open a Nachos file for reading and writing.  Bring the file header into
/// memory while the file is open.
///
/// * `sector` is the location on disk of the file header for this file.
OpenFile::OpenFile(int sector, const char *name)
{
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    seekPositionList = new LinkedList<int, unsigned>(intcomp);
    seekPositionList->insert(currentThread->pid, 0);
    char *temp = new char[strlen(name) + 1];
    strcpy(temp, name);
    hdrSector = sector;
    filename = temp;
}

OpenFile::OpenFile(int sector, FileHeader *head, const char *name)
{
    hdr = head;
    seekPositionList = new LinkedList<int, unsigned>(intcomp);
    seekPositionList->insert(currentThread->pid, 0);
    hdrSector = sector;
    char *temp = new char[strlen(name) + 1];
    strcpy(temp, name);
    filename = temp;
}

/// Close a Nachos file, de-allocating any in-memory data structures.
OpenFile::~OpenFile()
{
    delete hdr;
    delete seekPositionList;
}

/// Change the current location within the open file -- the point at which
/// the next `Read` or `Write` will start from.
///
/// * `position` is the location within the file for the next `Read`/`Write`.
void OpenFile::Seek(unsigned position)
{
    ASSERT(position <= hdr->FileLength());
    seekPositionList->update(currentThread->pid, position);
}

/// OpenFile::Read/Write
///
/// Read/write a portion of a file, starting from `seekPosition`.  Return the
/// number of bytes actually written or read, and as a side effect, increment
/// the current position within the file.
///
/// Implemented using the more primitive `ReadAt`/`WriteAt`.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.

int OpenFile::Read(char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);
    unsigned seekPosition;
    // Si no estas registrado para abrir el archivo, no funciona
    ASSERT(seekPositionList->get(currentThread->pid, &seekPosition));

    int result = ReadAt(into, numBytes, seekPosition);
    seekPositionList->update(currentThread->pid, seekPosition + result);
    return result;
}

int OpenFile::Write(const char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);
    unsigned seekPosition;
    // Si no estas registrado para abrir el archivo, no funciona
    ASSERT(seekPositionList->get(currentThread->pid, &seekPosition));

    int result = WriteAt(into, numBytes, seekPosition);
    seekPositionList->update(currentThread->pid, seekPosition + result);
    return result;
}

/// OpenFile::ReadAt/WriteAt
///
/// Read/write a portion of a file, starting at `position`.  Return the
/// number of bytes actually written or read, but has no side effects (except
/// that `Write` modifies the file, of course).
///
/// There is no guarantee the request starts or ends on an even disk sector
/// boundary; however the disk only knows how to read/write a whole disk
/// sector at a time.  Thus:
///
/// For ReadAt:
///     We read in all of the full or partial sectors that are part of the
///     request, but we only copy the part we are interested in.
/// For WriteAt:
///     We must first read in any sectors that will be partially written, so
///     that we do not overwrite the unmodified portion.  We then copy in the
///     data that will be modified, and write back all the full or partial
///     sectors that are part of the request.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.
/// * `position` is the offset within the file of the first byte to be
///   read/written.

int OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position)
{
    return ReadAt(into, numBytes, position, false);
}

int OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position, bool isSys)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    char *buf;

    if (position >= fileLength)
    {
        return 0; // Check request.
    }
    if (position + numBytes > fileLength)
    {
        numBytes = fileLength - position;
    }
    DEBUG('f', "Reading %u bytes at %u, from file of length %u. Filename: %s\n",
          numBytes, position, fileLength, filename);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors = 1 + lastSector - firstSector;

    OpenFileData *sdata;
    if (!isSys)
    {
        openFilesDataLock->Acquire();
        ASSERT(openFilesData->get(this->hdrSector, &sdata));
        openFilesDataLock->Release();

        // Algoritmo para empezar a leer ----
        sdata->lock->Acquire();
        while (sdata->numWriters > 0 || sdata->writerActive)
            sdata->condition->Wait();
        sdata->numReaders++;
        sdata->lock->Release();
        // fin de empezar a leer ----
    }

    // Read in all the full and partial sectors that we need.
    buf = new char[numSectors * SECTOR_SIZE];
    for (unsigned i = firstSector; i <= lastSector; i++)
    {
        synchDisk->ReadSector(hdr->ByteToSector(i * SECTOR_SIZE),
                              &buf[(i - firstSector) * SECTOR_SIZE]);
    }

    // Copy the part we want.
    memcpy(into, &buf[position - firstSector * SECTOR_SIZE], numBytes);

    if (!isSys)
    {
        // algoritmo para terminar de leer
        sdata->lock->Acquire();
        sdata->numReaders--;
        if (sdata->numReaders == 0)
            sdata->condition->Broadcast();
        sdata->lock->Release();
        // fin de terminar de leer
    }

    delete[] buf;
    return numBytes;
}

int OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position)
{
    return WriteAt(from, numBytes, position, false);
}

int OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position, bool isSys)
{
    ASSERT(from != nullptr);
    ASSERT(numBytes > 0);

    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;

    // Necesitamos agregar mas data sectors (y headers si tenemos que agrandarlo)
    if (position + numBytes > fileLength)
    {
        DEBUG('f', "Me pase de largo, voy a agrandar el archivo %s, su tamaño actual es %u\n", filename, fileLength);
        unsigned left = position + numBytes - fileLength;
        if (!fileSystem->Extend(hdr, left))
        {
            return 0; // Could not write
        }
        fileLength = hdr->FileLength();
        hdr->WriteBack(hdrSector);
    }

    DEBUG('f', "Writing %u bytes at %u, from file of length %u. Filename: %s\n",
          numBytes, position, fileLength, filename);

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors = 1 + lastSector - firstSector;

    buf = new char[numSectors * SECTOR_SIZE];

    firstAligned = position == firstSector * SECTOR_SIZE;
    lastAligned = position + numBytes == (lastSector + 1) * SECTOR_SIZE;

    // Read in first and last sector, if they are to be partially modified.
    if (!firstAligned)
    {
        ReadAt(buf, SECTOR_SIZE, firstSector * SECTOR_SIZE, isSys);
    }
    if (!lastAligned && (firstSector != lastSector || firstAligned))
    {
        ReadAt(&buf[(lastSector - firstSector) * SECTOR_SIZE],
               SECTOR_SIZE, lastSector * SECTOR_SIZE, isSys);
    }

    // Copy in the bytes we want to change.
    memcpy(&buf[position - firstSector * SECTOR_SIZE], from, numBytes);

    OpenFileData *sdata;
    if (!isSys)
    {
        openFilesDataLock->Acquire();
        ASSERT(openFilesData->get(this->hdrSector, &sdata));
        openFilesDataLock->Release();

        // Algoritmo para empezar a escribir ----
        sdata->lock->Acquire();
        sdata->numWriters++;
        while (sdata->numReaders > 0 || sdata->writerActive)
            sdata->condition->Wait();
        sdata->numWriters--;
        sdata->writerActive = true;
        sdata->lock->Release();
        // fin de empezar a escribir ----
    }
    // Write modified sectors back.
    for (unsigned i = firstSector; i <= lastSector; i++)
    {
        synchDisk->WriteSector(hdr->ByteToSector(i * SECTOR_SIZE),
                               &buf[(i - firstSector) * SECTOR_SIZE]);
    }

    if (!isSys)
    { // Algoritmo para terminar de escribir ----
        sdata->lock->Acquire();
        sdata->writerActive = false;
        sdata->condition->Broadcast();
        sdata->lock->Release();
        // fin de terminar de escribir ----
    }

    delete[] buf;
    return numBytes;
}

/// Return the number of bytes in the file.
unsigned
OpenFile::Length() const
{
    return hdr->FileLength();
}

void OpenFile::AddSeekPosition(unsigned position)
{
    seekPositionList->insert(currentThread->pid, position);
}

const char *OpenFile::GetName() const
{
    return filename;
}

int OpenFile::GetSector() const
{
    return hdrSector;
}