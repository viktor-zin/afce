#include <iostream>
#include <stdexcept>
#include <vector>

class Matrix
{
public:
    Matrix(int rows, int cols, double value = 0.0)
        : rows_(rows), cols_(cols), data_(rows, std::vector<double>(cols, value))
    {
    }

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    double &operator()(int r, int c) { return data_[r][c]; }
    double operator()(int r, int c) const { return data_[r][c]; }

    Matrix operator+(const Matrix &o) const
    {
        Matrix result(rows_, cols_);
        for (int i = 0; i < rows_; ++i)
            for (int j = 0; j < cols_; ++j)
                result(i, j) = (*this)(i, j) + o(i, j);
        return result;
    }

    Matrix operator*(const Matrix &o) const;
    static Matrix identity(int n);

private:
    int rows_, cols_;
    std::vector<std::vector<double>> data_;
};

Matrix Matrix::operator*(const Matrix &o) const
{
    if (cols_ != o.rows_)
        throw std::invalid_argument("size mismatch");
    Matrix result(rows_, o.cols_);
    for (int i = 0; i < rows_; ++i) {
        for (int j = 0; j < o.cols_; ++j) {
            double sum = 0;
            for (int k = 0; k < cols_; ++k)
                sum += data_[i][k] * o.data_[k][j];
            result(i, j) = sum;
        }
    }
    return result;
}

Matrix Matrix::identity(int n)
{
    Matrix m(n, n);
    for (int i = 0; i < n; ++i)
        m(i, i) = 1;
    return m;
}

std::ostream &operator<<(std::ostream &os, const Matrix &m)
{
    for (int i = 0; i < m.rows(); ++i) {
        for (int j = 0; j < m.cols(); ++j)
            os << m(i, j) << (j + 1 < m.cols() ? " " : "");
        os << '\n';
    }
    return os;
}

int main()
{
    Matrix a(2, 2, 1.5);
    Matrix b = Matrix::identity(2);
    std::cout << a + b << a * b;
    return 0;
}
