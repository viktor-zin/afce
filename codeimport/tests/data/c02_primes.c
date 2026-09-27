#include <stdio.h>
#include <math.h>

int is_prime(int n)
{
    int d;
    if (n < 2)
        return 0;
    if (n % 2 == 0)
        return n == 2;
    for (d = 3; d <= (int)sqrt(n); d += 2) {
        if (n % d == 0)
            return 0;
    }
    return 1;
}

int main(void)
{
    int limit, count = 0;
    scanf("%d", &limit);
    for (int i = 2; i <= limit; ++i) {
        if (!is_prime(i))
            continue;
        printf("%d ", i);
        count++;
        if (count == 100) {
            printf("...");
            break;
        }
    }
    printf("\nTotal: %d\n", count);
    return 0;
}
