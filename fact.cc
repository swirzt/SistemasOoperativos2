#include <iostream>

long fib(long n)
{
    if (n == 0)
        return 1;
    else if (n == 1)
        return 1;
    else
        return fib(n - 1) + fib(n - 2);
}

int main()
{
    long x;
    std::cin >> x;
    x = fib(x);
    std::cout << x;
}