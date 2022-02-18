#include "swappedlist.hh"
#include <stdio.h>

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
    int oldSaved = saved;
    swapped[saved] = item;
    saved++;
    return oldSaved;
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

void SwappedList::printSwapped()
{
    printf("Páginas guardadas:\n");
    for (unsigned i = 0; i < saved; i++)
    {
        printf("%d, ", swapped[i]);
    }
    printf("\n");
}