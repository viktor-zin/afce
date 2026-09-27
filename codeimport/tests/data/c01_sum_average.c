/* Лабораторная работа 1: сумма и среднее арифметическое */
#include <stdio.h>

int main(void)
{
    int n, i;
    double x, sum = 0.0;

    printf("Enter the number of values: ");
    scanf("%d", &n);
    if (n <= 0) {
        printf("Nothing to do\n");
        return 1;
    }
    for (i = 0; i < n; i++) {
        printf("x[%d] = ", i + 1);
        scanf("%lf", &x);
        sum += x;
    }
    printf("Sum = %.2f\n", sum);
    printf("Average = %.3f\n", sum / n);
    return 0;
}
