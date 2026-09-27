#include <stdio.h>

static void swap(int *x, int *y)
{
    int t = *x;
    *x = *y;
    *y = t;
}

void selection_sort(int *a, int n)
{
    int i, j, min;
    for (i = 0; i < n - 1; i++) {
        min = i;
        for (j = i + 1; j < n; j++)
            if (a[j] < a[min])
                min = j;
        if (min != i)
            swap(&a[i], &a[min]);
    }
}

void insertion_sort(int *a, int n)
{
    for (int i = 1; i < n; i++) {
        int key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > key) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

int main(void)
{
    int a[] = {29, 10, 14, 37, 13}, b[] = {12, 11, 13, 5, 6};
    int n = sizeof(a) / sizeof(a[0]);
    selection_sort(a, n);
    insertion_sort(b, 5);
    for (int i = 0; i < n; i++)
        printf("%d %d\n", a[i], b[i]);
    return 0;
}
