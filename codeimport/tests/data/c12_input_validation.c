#include <stdio.h>

static void flush_line(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

int read_int_in_range(int lo, int hi)
{
    int value, ok;
    do {
        printf("Enter a number from %d to %d: ", lo, hi);
        ok = scanf("%d", &value);
        if (ok != 1) {
            printf("That is not a number!\n");
            flush_line();
            continue;
        }
        if (value < lo || value > hi)
            printf("Out of range\n");
    } while (ok != 1 || value < lo || value > hi);
    return value;
}

int main(void)
{
    int age = read_int_in_range(1, 120);
    char answer;
    printf("Age accepted: %d\n", age);
    printf("Continue? (y/n) ");
    flush_line();
    answer = getchar();
    if (answer == 'y' || answer == 'Y')
        printf("OK\n");
    return 0;
}
