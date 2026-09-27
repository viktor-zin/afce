#include <stdio.h>
#include <stdlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define DEBUG 1

#ifdef DEBUG
#  define LOG(msg) fprintf(stderr, "[debug] %s\n", msg)
#else
#  define LOG(msg) ((void)0)
#endif

#if defined(_WIN32)
static const char *platform(void) { return "windows"; }
#else
static const char *platform(void) { return "unix"; }
#endif

int find_max(const int *a, size_t n)
{
    int m = a[0];
    for (size_t i = 1; i < n; i++)
        m = MAX(m, a[i]);
#ifdef DEBUG
    printf("max of %zu values: %d\n", n, m);
#endif
    return m;
}

int main(void)
{
    int values[] = {4, 8, 15, 16, 23, 42};
    LOG("started");
    printf("Platform: %s\n", platform());
    printf("Max: %d\n", find_max(values, ARRAY_LEN(values)));
#if DEBUG > 1
    printf("verbose\n");
#endif
    return 0;
}
