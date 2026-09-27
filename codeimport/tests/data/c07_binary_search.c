#include <stdio.h>

int binary_search(const int *a, int n, int key)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == key)
            return mid;
        else if (a[mid] < key)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

int search_rec(const int *a, int lo, int hi, int key)
{
    int mid;
    if (lo > hi)
        return -1;
    mid = (lo + hi) / 2;
    if (a[mid] == key)
        return mid;
    if (a[mid] > key)
        return search_rec(a, lo, mid - 1, key);
    return search_rec(a, mid + 1, hi, key);
}

int main(void)
{
    int data[] = {1, 3, 5, 7, 9, 11, 13};
    int key, pos;
    scanf("%d", &key);
    pos = binary_search(data, 7, key);
    if (pos >= 0)
        printf("Found at %d\n", pos);
    else
        puts("Not found");
    printf("%d\n", search_rec(data, 0, 6, key));
    return 0;
}
