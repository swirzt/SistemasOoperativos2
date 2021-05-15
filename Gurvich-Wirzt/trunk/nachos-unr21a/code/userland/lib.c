
unsigned strlenn(const char *s)
{
    unsigned i = 0;
    while (*s++)
        i++;
    return i;
}

void putsNachos(const char *s)
{

    Write(s, strlenn(s), 1);
}

void swap(char *a, char *b)
{
    char c = *a;
    *a = *b;
    *b = c;
}

void reverse(char *s, unsigned i, unsigned j)
{
    while (i < j)
        swap(&s[i++], &s[j--]);
}

void move1(char *s, unsigned size)
{
    for (unsigned i = size; i > 0; i--)
        s[i] = s[i - 1];
}

// void itoa(int n, char *str)
// {
//     int negativo = n < 0;
//     if (negativo)
//         n = -n;
//     int i = 0;
//     int finished = 0;
//     while (!finished)
//     {
//         int m = n % 10;
//         str[i++] = m + 48;

//         if (m == n)
//         {
//             reverse(str, 0, i);
//             finished = 1;
//         }
//         n = n / 10;
//     }
//     if (negativo)
//     {
//         move1(str, i++);
//         str[0] = '-';
//     }
//     str[i] = '\0';
//     // putsNachos("ITOA\n");
//     // putsNachos(str);
// }

static int abs(int n)
{
    if (n < 0)
        return -n;
    return n;
}

// No la usamos
void itoa(int n, char *str)
{

    int base = 10;
    int nvalue = abs(n);

    int i = 0, r = 0;
    while (nvalue)
    {
        r = nvalue % base;

        if (r >= 10)
            str[i++] = 65 + (r - 10);
        else
            str[i++] = 48 + r;

        nvalue = nvalue / base;
    }

    if (i == 0)
    {
        str[i++] = '0';
    }

    if (n < 0)
    {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse(str, 0, i - 1);
}
