/// Routines to manage a bitmap -- an array of bits each of which can be
/// either on or off.  Represented as an array of integers.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "coremap.hh"

#include <stdio.h>

/// Initialize a coremap with `nitems` entries, so that every entry is clear.  It
/// can be added somewhere on a list.
///
/// * `nitems` is the number of entries in the bitmap.
Coremap::Coremap(unsigned nitems)
{
    ASSERT(nitems > 0);

    numEntries = nitems;
    map = new int *[numEntries];
    for (unsigned i = 0; i < numEntries; i++)
    {
        map[i] = new int[2];
        map[i][0] = -1;
        map[i][1] = -1;
    }
}

/// De-allocate a coremap.
Coremap::~Coremap()
{
    for (unsigned i = 0; i < numEntries; i++)
    {
        delete[] map[i];
    }
    delete[] map;
}

/// Set the “nth” entry in a coremap.
///
/// * `which` is the number of the entry to be set.
void Coremap::Mark(unsigned phys, unsigned vpn, unsigned pid)
{
    ASSERT(phys < numEntries);
    map[phys][0] = vpn;
    map[phys][1] = pid;
}

/// Clear the “nth” entry in a coremap.
///
/// * `phys` is the number of the entry to be cleared.
void Coremap::Clear(unsigned phys)
{
    ASSERT(phys < numEntries);
    map[phys][0] = -1;
    map[phys][1] = -1;
}

/// Return true if the “nth” entry is set.
///
/// * `which` is the number of the entry to be tested.
bool Coremap::Test(unsigned phys)
{
    ASSERT(phys < numEntries);
    return map[phys][0] != -1;
}
/// Return the number of the first entry which is clear.  As a side effect, set
/// the entry (markit with the physical page and pid).  (In other words, find and allocate an entry.)
///
/// If no entries are clear, return (-1,-1).
int Coremap::Find(unsigned vpn, unsigned pid)
{
    for (unsigned i = 0; i < numEntries; i++)
    {
        if (!Test(i))
        {
            Mark(i, vpn, pid);
            return i;
        }
    }
    return -1;
}

/// Return the number of clear entries in the bitmap.  (In other words, how many
/// entries are unallocated?)
unsigned Coremap::CountClear()
{
    unsigned count = 0;

    for (unsigned i = 0; i < numEntries; i++)
    {
        if (!Test(i))
        {
            count++;
        }
    }
    return count;
}

/// Return the info of an entry
int *Coremap::GetOwner(unsigned phys)
{
    return map[phys];
}

void Coremap::Print()
{
    printf("Coremap de size %d\n", numEntries);
    for (unsigned i = 0; i < numEntries; i++)
    {
        printf("%d %d\n", map[i][0], map[i][1]);
    }
    printf("-------------------------\n");
}