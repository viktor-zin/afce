#include <stdio.h>

/* Prints the larger of two numbers. */
static int max(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

int main(void)
{
    int a, b;
    scanf("%d %d", &a, &b);
    if (a > b)
        printf("%d\n", a);
    else
        printf("%d\n", b);
    printf("max = %d\n", max(a, b));
    return 0;
}
