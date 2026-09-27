#include <array>
#include <iostream>

constexpr int W = 10;
constexpr int H = 8;
using Grid = std::array<std::array<bool, W>, H>;

int neighbours(const Grid &g, int r, int c)
{
    int count = 0;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0)
                continue;
            int rr = (r + dr + H) % H;
            int cc = (c + dc + W) % W;
            if (g[rr][cc])
                ++count;
        }
    }
    return count;
}

Grid step(const Grid &g)
{
    Grid next{};
    for (int r = 0; r < H; ++r)
        for (int c = 0; c < W; ++c) {
            const int n = neighbours(g, r, c);
            next[r][c] = g[r][c] ? (n == 2 || n == 3) : n == 3;
        }
    return next;
}

void print(const Grid &g)
{
    for (const auto &row : g) {
        for (bool cell : row)
            std::cout << (cell ? '#' : '.');
        std::cout << '\n';
    }
    std::cout << std::endl;
}

int main()
{
    Grid g{};
    g[1][2] = g[2][3] = g[3][1] = g[3][2] = g[3][3] = true;
    for (int generation = 0; generation < 4; ++generation) {
        std::cout << "Generation " << generation << ":\n";
        print(g);
        g = step(g);
    }
}
