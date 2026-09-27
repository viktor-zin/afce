#include <stdio.h>

#define N 3

void read_matrix(int m[N][N])
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            scanf("%d", &m[i][j]);
}

void multiply(int a[N][N], int b[N][N], int c[N][N])
{
    int i, j, k;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            c[i][j] = 0;
            for (k = 0; k < N; k++)
                c[i][j] += a[i][k] * b[k][j];
        }
    }
}

void transpose(int m[N][N])
{
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++) {
            int t = m[i][j];
            m[i][j] = m[j][i];
            m[j][i] = t;
        }
}

int trace(int m[N][N])
{
    int s = 0;
    for (int i = 0; i < N; i++)
        s += m[i][i];
    return s;
}

int main(void)
{
    int a[N][N], b[N][N], c[N][N];
    read_matrix(a);
    read_matrix(b);
    multiply(a, b, c);
    transpose(c);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++)
            printf("%5d", c[i][j]);
        putchar('\n');
    }
    printf("trace = %d\n", trace(c));
    return 0;
}
