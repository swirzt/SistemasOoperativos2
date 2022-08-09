
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

void itoa(int n, char *str)
{
    int m = n < 0 ? -n : n;

    int i = 0, r = 0;
    while (m)
    {
        r = m % 10;

        if (r >= 10)
            str[i++] = 65 + (r - 10);
        else
            str[i++] = 48 + r;

        m = m / 10;
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

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}