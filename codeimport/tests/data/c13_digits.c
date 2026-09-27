#include <stdio.h>
#include <stdlib.h>

int digit_sum(long n)
{
    int s = 0;
    n = labs(n);
    while (n > 0) {
        s += n % 10;
        n /= 10;
    }
    return s;
}

long reverse_number(long n)
{
    long r = 0;
    while (n != 0) {
        r = r * 10 + n % 10;
        n /= 10;
    }
    return r;
}

int count_digits(long n)
{
    int count = 0;
    do {
        count++;
        n /= 10;
    } while (n != 0);
    return count;
}

int is_armstrong(int n)
{
    int k = count_digits(n), t = n, sum = 0;
    while (t > 0) {
        int d = t % 10, p = 1;
        for (int i = 0; i < k; i++)
            p *= d;
        sum += p;
        t /= 10;
    }
    return sum == n;
}

int main(void)
{
    long n;
    scanf("%ld", &n);
    printf("sum=%d reversed=%ld digits=%d\n", digit_sum(n), reverse_number(n), count_digits(n));
    for (int i = 1; i < 1000; i++)
        if (is_armstrong(i))
            printf("%d\n", i);
    return EXIT_SUCCESS;
}
