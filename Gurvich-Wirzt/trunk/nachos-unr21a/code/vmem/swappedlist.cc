#include "swappedlist.hh"

SwappedList::SwappedList(unsigned nitems)
{
    size = nitems;
    saved = 0;
    swapped = new unsigned[size];
}

SwappedList::~SwappedList()
{
    delete swapped;
}

int SwappedList::Add(unsigned item)
{
    swapped[saved++] = item;
    return saved - 1;
}

int SwappedList::Find(unsigned item)
{
    for (unsigned i = 0; i < saved; i++)
    {
        if (swapped[i] == item)
            return i;
    }
    return -1;
}