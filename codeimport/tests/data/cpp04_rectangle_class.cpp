#include <iostream>

class Rectangle
{
public:
    Rectangle() : width_(0), height_(0) { ++count_; }
    Rectangle(double w, double h) : width_{w}, height_{h} { ++count_; }
    ~Rectangle() { --count_; }

    double width() const { return width_; }
    double height() const { return height_; }
    void setWidth(double w)
    {
        if (w >= 0)
            width_ = w;
    }
    double area() const { return width_ * height_; }
    double perimeter() const;
    bool isSquare() const noexcept { return width_ == height_; }
    void scale(double k);
    static int count() { return count_; }

    friend std::ostream &operator<<(std::ostream &os, const Rectangle &r)
    {
        return os << "Rectangle(" << r.width_ << " x " << r.height_ << ")";
    }

private:
    double width_;
    double height_;
    static int count_;
};

int Rectangle::count_ = 0;

double Rectangle::perimeter() const
{
    return 2 * (width_ + height_);
}

void Rectangle::scale(double k)
{
    width_ *= k;
    height_ *= k;
}

int main()
{
    Rectangle a(3, 4), b;
    b.setWidth(5);
    a.scale(2);
    std::cout << a << " area=" << a.area() << " perimeter=" << a.perimeter() << std::endl;
    std::cout << std::boolalpha << "square: " << a.isSquare() << std::endl;
    std::cout << "objects: " << Rectangle::count() << std::endl;
    return 0;
}
