/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "file_system.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "lib/utility.hh"
#include "threads/system.hh"
#include "threads/lock.hh"
#include "threads/condition.hh"
#include "open_file_data.hh"

#include <stdio.h>
#include <string.h>

/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    dirSize = NUM_DIR_ENTRIES;
    DEBUG('f', "Initializing the file system.\n");
    if (format)
    {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        Directory *dir = new Directory(dirSize, DIRECTORY_SECTOR, DIRECTORY_SECTOR);
        FileHeader *mapH = new FileHeader();
        FileHeader *dirH = new FileHeader();

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.
        freeMapFile = new OpenFile(FREE_MAP_SECTOR, "FreeMap");
        openFilesData->insert("FreeMap", new OpenFileData(freeMapFile));

        directoryFile = new OpenFile(DIRECTORY_SECTOR, "Directory");
        rootDirectoryFile = directoryFile;
        openFilesData->insert("Directory", new OpenFileData(directoryFile));

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f'))
        {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    }
    else
    {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile = new OpenFile(FREE_MAP_SECTOR, "FreeMap");
        openFilesData->insert("FreeMap", new OpenFileData(freeMapFile));

        directoryFile = new OpenFile(DIRECTORY_SECTOR, "Directory");
        rootDirectoryFile = directoryFile;
        openFilesData->insert("Directory", new OpenFileData(directoryFile));
    }
    DEBUG('f', "File system initialization done.\n");
}

FileSystem::~FileSystem()
{
    openFilesData->remove("FreeMap");
    delete freeMapFile;
    openFilesData->remove("Directory");
    delete directoryFile;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool FileSystem::Create(const char *name)
{
    ASSERT(name != nullptr);

    DEBUG('f', "Creating file %s\n", name);

    Directory *dir = new Directory(dirSize);
    dir->FetchFrom(directoryFile);

    bool success;

    if (dir->Find(name) != -1)
    {
        success = false; // File is already in directory.
    }
    else
    {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
        // Find a sector to hold the file header.
        if (sector == -1)
        {
            DEBUG('f', "No free space for file header.\n");
            success = false; // No free block for file header.
        }
        else if (!dir->Add(name, sector, false))
        {
            DEBUG('f', "No free entry for file in directory.\n");
            success = false; // No space in directory.
        }
        else
        {
            DEBUG('f', "Sector allocated for file header in %d.\n", sector);
            FileHeader *h = new FileHeader();
            h->WriteBack(sector);
            freeMap->WriteBack(freeMapFile);
            dir->WriteBack(directoryFile);
            delete h;
            success = true;
        }

        delete freeMap;
    }
    delete dir;
    return success;
}

bool FileSystem::CreateDirectory(const char *name)
{
    ASSERT(name != nullptr);

    DEBUG('f', "Creating directory %s\n", name);

    Directory *dir = new Directory(dirSize);
    dir->FetchFrom(directoryFile);

    bool success;

    if (dir->Find(name) != -1)
    {
        success = false; // File is already in directory.
    }
    else
    {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
        // Find a sector to hold the file header.
        if (sector == -1)
        {
            DEBUG('f', "No free space for file header.\n");
            success = false; // No free block for file header.
        }
        else if (!dir->Add(name, sector, true))
        {
            DEBUG('f', "No free entry for file in directory.\n");
            success = false; // No space in directory.
        }
        else
        {
            DEBUG('f', "Sector allocated for file header in %d.\n", sector);
            FileHeader *h = new FileHeader();
            h->WriteBack(sector);
            OpenFile *newdirFile = new OpenFile(sector, h, name);
            OpenFileData *temp = new OpenFileData(newdirFile);
            openFilesData->insert(name, temp);
            Directory *newdir = new Directory(NUM_DIR_ENTRIES, directoryFile->GetSector(), sector);
            freeMap->WriteBack(freeMapFile);
            newdir->WriteBack(newdirFile);
            openFilesData->remove(name);
            dir->WriteBack(directoryFile);

            delete temp; // En su destruccion se hace delete de newDirFIle y h
            delete newdir;
            success = true;
        }

        delete freeMap;
    }
    delete dir;
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);
    OpenFileData *data;
    openFilesDataLock->Acquire();
    if (openFilesData->get(name, &data))
    {
        if (data->deleted)
        {
            DEBUG('f', "Quise abrir el archivo %s pero esta marcado para borrarse\n", name);
            openFilesDataLock->Release();
            return nullptr;
        }
        else
        {
            DEBUG('f', "Abriendo el archivo %s ya abierto por otro hilo\n", name);
            data->dataLock->Acquire();
            data->numOpens++;
            data->file->AddSeekPosition(0);
            data->dataLock->Release();
            openFilesDataLock->Release();
            return data->file;
        }
    }
    openFilesDataLock->Release();

    Directory *dir = new Directory(dirSize);
    OpenFile *openFile = nullptr;

    DEBUG('f', "Opening file %s\n", name);
    dir->FetchFrom(directoryFile);
    int sector = dir->Find(name);
    if (sector >= 0)
    {
        DEBUG('f', "File %s found in directory.\n", name);
        openFile = new OpenFile(sector, name); // `name` was found in directory.
    }
    delete dir;
    if (openFile != nullptr)
    {
        data = new OpenFileData(openFile);

        openFilesData->insert(openFile->GetName(), data);
    }

    return openFile; // Return null if not found.
}

void FileSystem::Close(OpenFile *file)
{
    const char *name = file->GetName();

    OpenFileData *data;
    openFilesDataLock->Acquire();
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
        openFilesData->remove(name);
        data->dataLock->Release();
        delete data;
    }
    openFilesDataLock->Release();
}

bool FileSystem::CleanFile(const char *name)
{
    Directory *dir = new Directory(dirSize);
    dir->FetchFrom(directoryFile);
    int sector = dir->Find(name);
    if (sector == -1)
    {
        delete dir;
        return false; // file not found
    }
    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap); // Remove data blocks.
    freeMap->Clear(sector);     // Remove header block.
    dir->Remove(name);

    freeMap->WriteBack(freeMapFile); // Flush to disk.
    dir->WriteBack(directoryFile);   // Flush to disk.
    delete fileH;
    delete dir;
    delete freeMap;
    return true;
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool FileSystem::Remove(const char *name)
{
    ASSERT(name != nullptr);
    OpenFileData *data;
    openFilesDataLock->Acquire();
    if (openFilesData->get(name, &data))
    {
        data->dataLock->Acquire();
        data->deleted = true;
        data->dataLock->Release();
        openFilesDataLock->Acquire();
        return true;
    }
    else
    {
        openFilesDataLock->Acquire();
        return CleanFile(name);
    }
}

bool FileSystem::Extend(FileHeader *hdr, unsigned size)
{
    DEBUG('f', "Extending file by %u bytes\n", size);
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    bool res = hdr->ExtendFile(freeMap, size);
    freeMap->WriteBack(freeMapFile);
    delete freeMap;
    return res;
}

/// List all the files in the file system directory.
void FileSystem::List()
{
    Directory *dir = new Directory(dirSize);

    dir->FetchFrom(directoryFile);
    dir->List();
    delete dir;
}

unsigned FileSystem::getDirSize()
{
    return dirSize;
}

void FileSystem::setDirSize(unsigned size)
{
    dirSize = size;
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector))
    {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value)
    {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check."))
    {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++)
    {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++)
    {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    unsigned dirSize = fileSystem->getDirSize();
    const char *knownNames[dirSize];

    for (unsigned i = 0; i < dirSize; i++)
    {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse)
        {
            if (strlen(e->name) > FILE_NAME_MAX_LEN)
            {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++)
            {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0)
                {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated)
            {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(dirSize);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void FileSystem::Print()
{
    FileHeader *bitH = new FileHeader;
    FileHeader *dirH = new FileHeader;
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    Directory *dir = new Directory(dirSize);

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}

bool FileSystem::ChangeDirectory(const char *name)
{
    DEBUG('f', "Changing directory to \"%s\".\n", name);
    // Directory *dir = new Directory(dirSize);
    // dir->FetchFrom(directoryFile);

    char path[strlen(name) + 1]; // Lo almacena en heap o en pila? Creemos que en pila, gracias GCC
    const char *rest = get_filepath(name, path);

    OpenFile *newDir = Open(path);

    if (newDir)
    {
        OpenFile *temp = directoryFile;
        directoryFile = newDir;
        if (strcmp(rest, "") == 0)
        {
            DEBUG('f', "Changed directory to \"%s\".\n", path);
            Close(temp);
            return true;
        }
        else
        {
            if (ChangeDirectory(rest))
            {
                Close(temp);
                return true;
            }
            else
            {
                Close(newDir);
                directoryFile = temp;
                return false;
            }
        }
    }
    else
    {
        DEBUG('f', "Directory \"%s\" not found.\n", path);
        return false;
    }
}
