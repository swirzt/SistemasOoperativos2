/// Copyright (c) 2018-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "utility.hh"
#include <string.h>

Debug debug;

bool intcomp(int a, int b)
{
    return a == b;
}

bool strcomp(const char *s1, const char *s2)
{
    return !strcmp(s1, s2);
}