#include <stdio.h>

int main(void)
{
    int n, a[100];
    printf("n = ");
    scanf("%d", &n);
    for (int i = 0; i < n; i++)
        scanf("%d", &a[i]);

    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (a[j] > a[j + 1]) {
                int t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
                swapped = 1;
            }
        }
        if (!swapped)
            break;
    }

    for (int i = 0; i < n; i++)
        printf("%d ", a[i]);
    return 0;
}
