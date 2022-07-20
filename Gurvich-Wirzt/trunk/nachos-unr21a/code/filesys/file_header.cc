/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>

FileHeader::FileHeader()
{
    indirect = nullptr;
    raw.numBytes = 0;
    raw.numSectors = 0;
}

FileHeader::~FileHeader()
{
    if (indirect)
        delete indirect;
}

/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
bool FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
    ASSERT(freeMap != nullptr);

    if (fileSize > MAX_FILE_SIZE)
    {
        return false;
    }

    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);
    // Cantidad de Inodos necesarios
    unsigned numInodes = DivRoundUp(raw.numSectors, NUM_DIRECT);
    // Quiero ubicar todos los Inodos y la informacion del archivo
    if (freeMap->CountClear() < raw.numSectors + numInodes)
    {
        return false; // Not enough space.
    }

    unsigned i;
    for (i = 0; i < raw.numSectors && i < NUM_DIRECT; i++)
    {
        raw.dataSectors[i] = freeMap->Find();
    }

    if (raw.numSectors >= NUM_DIRECT)
    {
        raw.nextInode = freeMap->Find();
        indirect = new FileHeader();
        FileHeader *temp = indirect;
        for (; i < raw.numSectors; i += NUM_DIRECT)
        {
            temp->IndirectAllocate(freeMap, fileSize - (i * SECTOR_SIZE), raw.numSectors - i);
            if (i + NUM_DIRECT < raw.numSectors)
            {
                temp->SetNextInode(freeMap->Find());
                temp->indirect = new FileHeader();
                temp = temp->indirect;
            }
        }
    }
    return true;
}

void FileHeader::IndirectAllocate(Bitmap *freeMap, unsigned filesize, unsigned rest)
{
    DEBUG('f', "Alocando %u bytes en este fileHeader\n", rest);
    raw.numBytes = filesize;
    raw.numSectors = rest;
    unsigned toWrite = rest > NUM_DIRECT ? NUM_DIRECT : rest;

    for (unsigned i = 0; i < toWrite; i++)
    {
        raw.dataSectors[i] = freeMap->Find();
    }
}

void FileHeader::SetNextInode(unsigned sector)
{
    raw.nextInode = sector;
}

bool FileHeader::ExtendFile(Bitmap *freeMap, unsigned extra)
{

    // SI agregamos mas sectores hay que hacer una actualizacion heredada en cada header
    unsigned cantSectors = DivRoundUp(extra + raw.numBytes, SECTOR_SIZE) - raw.numSectors;
    DEBUG('f', "Necesito agregar %d sectores\n", cantSectors);
    // Entra todo en el primer FileHeader, no hay que actualizar la cadena
    if (raw.numSectors + cantSectors < NUM_DIRECT)
    {
        DEBUG('f', "Entra todo en el FileHeader\n");
        if (freeMap->CountClear() < cantSectors)
        {
            DEBUG('f', "No tengo sectores para ocupar");
            return false;
        }

        for (unsigned i = raw.numSectors; i < raw.numSectors + cantSectors; i++)
        {
            DEBUG('f', "Voy a ocupar un sector \n");
            raw.dataSectors[i] = freeMap->Find();
            DEBUG('f', "Ocupe el sector %u \n", raw.dataSectors[i]);
        }

        raw.numSectors += cantSectors;
        raw.numBytes += extra;
        return true;
    }
    else
    {
        if (indirect)
        {
            DEBUG('f', "Tengo otro FileHeader, voy a seguir por ahi\n");
            if (indirect->ExtendFile(freeMap, extra))
            {
                raw.numSectors += cantSectors;
                raw.numBytes += extra;
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            unsigned numInodes = DivRoundUp(cantSectors, NUM_DIRECT);
            DEBUG('f', "Necesito %u nuevos FileHeaders\n", numInodes);
            if (freeMap->CountClear() < cantSectors + numInodes)
            {
                return false;
            }

            raw.nextInode = freeMap->Find();
            indirect = new FileHeader();
            FileHeader *temp = indirect;
            for (unsigned i = 0; i < cantSectors; i += NUM_DIRECT)
            {
                temp->IndirectAllocate(freeMap, extra - (i * SECTOR_SIZE), cantSectors - i);
                if (i + NUM_DIRECT < cantSectors)
                {
                    temp->SetNextInode(freeMap->Find());
                    temp->indirect = new FileHeader();
                    temp = temp->indirect;
                }
            }
            return true;
        }
    }
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    for (unsigned i = 0; i < raw.numSectors && i < NUM_DIRECT; i++)
    {
        ASSERT(freeMap->Test(raw.dataSectors[i])); // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }
    if (indirect)
    {
        indirect->Deallocate(freeMap);
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *)&raw);
    if (raw.numSectors >= NUM_DIRECT)
    {
        indirect = new FileHeader();
        indirect->FetchFrom(raw.nextInode);
    }
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void FileHeader::WriteBack(unsigned sector)
{
    synchDisk->WriteSector(sector, (char *)&raw);
    if (raw.numSectors >= NUM_DIRECT)
    {
        indirect->WriteBack(raw.nextInode);
    }
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    if (offset >= SECTOR_SIZE * NUM_DIRECT)
    {
        return indirect->ByteToSector(offset - SECTOR_SIZE * NUM_DIRECT);
    }
    return raw.dataSectors[offset / SECTOR_SIZE];
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void FileHeader::Print(const char *title)
{
    char *data = new char[SECTOR_SIZE];

    if (title == nullptr)
    {
        printf("File header:\n");
    }
    else
    {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    for (unsigned i = 0; i < raw.numSectors && i < NUM_DIRECT; i++)
    {
        printf("%u ", raw.dataSectors[i]);
    }
    printf("\n");

    for (unsigned i = 0, k = 0; i < raw.numSectors && i < NUM_DIRECT; i++)
    {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++)
        {
            if (isprint(data[j]))
            {
                printf("%c", data[j]);
            }
            else
            {
                printf("\\%X", (unsigned char)data[j]);
            }
        }
        printf("\n");
    }
    delete[] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}
