#include <stdio.h>

unsigned long long factorial(int n)
{
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

unsigned long long factorial_iter(int n)
{
    unsigned long long f = 1;
    int i;
    for (i = 2; i <= n; i++)
        f *= i;
    return f;
}

long fib(int n)
{
    long a = 0, b = 1, t;
    while (n-- > 0) {
        t = a + b;
        a = b;
        b = t;
    }
    return a;
}

int fib_rec(int n) { return n < 2 ? n : fib_rec(n - 1) + fib_rec(n - 2); }

int main(void)
{
    int n;
    printf("n = ");
    if (scanf("%d", &n) != 1 || n < 0) {
        fprintf(stderr, "invalid input\n");
        return 1;
    }
    printf("%d! = %llu\n", n, factorial(n));
    printf("%d! = %llu (loop)\n", n, factorial_iter(n));
    for (int i = 0; i <= n && i < 20; i++)
        printf("fib(%d) = %ld, %d\n", i, fib(i), fib_rec(i));
    return 0;
}
