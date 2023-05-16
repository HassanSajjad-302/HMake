import "lib1/public-lib1.hpp";
import "lib2/public-lib2.hpp";

int main()
{
    std::cout << getValueLib1() + getValueLib2() << std::endl;
}