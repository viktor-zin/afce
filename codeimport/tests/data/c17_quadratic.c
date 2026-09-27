#include <stdio.h>
#include <math.h>

int solve(double a, double b, double c, double *x1, double *x2)
{
    double d;
    if (fabs(a) < 1e-12) {
        if (fabs(b) < 1e-12)
            return -1;
        *x1 = -c / b;
        return 1;
    }
    d = b * b - 4 * a * c;
    if (d < 0)
        return 0;
    else if (d == 0) {
        *x1 = *x2 = -b / (2 * a);
        return 1;
    } else {
        *x1 = (-b + sqrt(d)) / (2 * a);
        *x2 = (-b - sqrt(d)) / (2 * a);
        return 2;
    }
}

int main(void)
{
    double a, b, c, x1 = 0, x2 = 0;
    printf("a b c: ");
    scanf("%lf %lf %lf", &a, &b, &c);
    int roots = solve(a, b, c, &x1, &x2);
    switch (roots) {
    case -1:
        printf("Infinite or no solutions\n");
        break;
    case 0:
        printf("No real roots\n");
        break;
    case 1:
        printf("x = %.4f\n", x1);
        break;
    default:
        printf("x1 = %.4f, x2 = %.4f\n", x1, x2);
        break;
    }
    return 0;
}
