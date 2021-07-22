#ifndef NACHOS_SWAPPEDLIST__HH
#define NACHOS_SWAPPEDLIST__HH

class SwappedList
{
public:
    SwappedList(unsigned nitems);
    ~SwappedList();

    int Add(unsigned item);

    int Find(unsigned item);

    void printSwapped();

private:
    unsigned size;
    unsigned saved;
    unsigned *swapped;
};

#endif