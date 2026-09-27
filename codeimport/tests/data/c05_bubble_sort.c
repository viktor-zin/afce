#include <stdio.h>
#include <stdbool.h>

#define SIZE 10

void print_array(const int a[], int n)
{
    for (int i = 0; i < n; i++)
        printf("%d ", a[i]);
    printf("\n");
}

void bubble_sort(int a[], int n)
{
    bool swapped;
    int pass = 0;
    do {
        swapped = false;
        for (int j = 0; j < n - 1 - pass; j++) {
            if (a[j] > a[j + 1]) {
                int tmp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = tmp;
                swapped = true;
            }
        }
        pass++;
    } while (swapped);
}

int main(void)
{
    int a[SIZE] = {5, 3, 9, 1, 7, 2, 8, 6, 4, 0};
    printf("Before: ");
    print_array(a, SIZE);
    bubble_sort(a, SIZE);
    printf("After:  ");
    print_array(a, SIZE);
    return 0;
}
