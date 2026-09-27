#include <stdio.h>

void triangle(int n)
{
    int i, j;
    for (i = 1; i <= n; i++) {
        for (j = 0; j < n - i; j++)
            putchar(' ');
        for (j = 0; j < 2 * i - 1; j++)
            putchar('*');
        putchar('\n');
    }
}

void multiplication_table(int n)
{
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            if (j > i)
                break;
            printf("%4d", i * j);
        }
        printf("\n");
    }
}

int find_pair(int target)
{
    int i, j;
    for (i = 1; i < 100; i++)
        for (j = i; j < 100; j++)
            if (i * j == target)
                goto found;
    return 0;
found:
    printf("%d * %d = %d\n", i, j, target);
    return 1;
}

int main(void)
{
    int n = 5;
    triangle(n);
    multiplication_table(9);
    for (int k = 1; k <= 20; k++) {
        if (k % 3 == 0)
            continue;
        if (k > 15)
            break;
        printf("%d ", k);
    }
    printf("\n");
    find_pair(391);
    return 0;
}
