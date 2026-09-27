#include <stdio.h>

/* Euclid's algorithm, iterative version */
int gcd(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int gcd_rec(int a, int b)
{
    if (b == 0)
        return a;
    return gcd_rec(b, a % b);
}

long lcm(int a, int b)
{
    return (long)a / gcd(a, b) * b;
}

int main()
{
    int a, b;
    printf("a, b: ");
    scanf("%d %d", &a, &b);
    printf("gcd = %d\n", gcd(a, b));
    printf("gcd (recursive) = %d\n", gcd_rec(a, b));
    printf("lcm = %ld\n", lcm(a, b));
    return 0;
}
