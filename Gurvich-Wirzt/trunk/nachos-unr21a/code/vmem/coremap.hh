/// A data structure defining a bitmap -- an array of bits each of which can
/// be either on or off.  It is also known as a bit array, bit set or bit
/// vector.
///
/// The bitmap is represented as an array of unsigned integers, on which we
/// do modulo arithmetic to find the bit we are interested in.
///
/// The data structure is parameterized with with the number of bits being
/// managed.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_COREMAP__HH
#define NACHOS_COREMAP__HH

#include "lib/utility.hh"

/// A “bitmap” -- an array of bits, each of which can be independently set,
/// cleared, and tested.
///
/// Most useful for managing the allocation of the elements of an array --
/// for instance, disk sectors, or main memory pages.
///
/// Each bit represents whether the corresponding sector or page is in use
/// or free.
class Coremap
{
public:
    /// Initialize a Coremap with `nitems` pairs; all pairs  are cleared.
    ///
    /// * `nitems` is the number of items in the coremap.
    Coremap(unsigned nitems);

    /// Uninitialize a coremap.
    ~Coremap();

    /// Set the “nth” entry.
    void Mark(unsigned phys, unsigned vpn, unsigned pid);

    /// Clear the “nth” entry.
    void Clear(unsigned phys);

    bool Test(unsigned phys);

    /// Return the index of a empty pair, and as a side effect, set the bit.
    ///
    /// If no pairs are clear, return (-1,-1).
    int Find(unsigned vpn, unsigned pid);

    unsigned CountClear();

    unsigned *GetOwner(unsigned phys);

private:
    /// Number of entries in the bitmap.
    unsigned numEntries;

    /// Pair storage.
    unsigned **map;
};

#endif
