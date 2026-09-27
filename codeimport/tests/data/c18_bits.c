#include <stdio.h>

unsigned count_bits(unsigned x)
{
    unsigned count = 0;
    while (x) {
        count += x & 1u;
        x >>= 1;
    }
    return count;
}

int is_power_of_two(unsigned x) { return x && !(x & (x - 1)); }

void print_binary(unsigned x)
{
    for (int i = 31; i >= 0; i--) {
        putchar((x >> i) & 1 ? '1' : '0');
        if (i % 8 == 0 && i != 0)
            putchar(' ');
    }
    putchar('\n');
}

int main(void)
{
    unsigned x = 0xF0u, mask = 0x0F;
    x |= mask;
    x ^= 0x3;
    x <<= 2;
    x &= ~0x10u;
    printf("x = %u (0x%X), bits = %u\n", x, x, count_bits(x));
    print_binary(x);
    printf("%s\n", is_power_of_two(64) ? "yes" : "no");
    return 0;
}
