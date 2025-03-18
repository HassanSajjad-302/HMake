
import <iostream>;
import "A.hpp";
const char *getAName()
{
    return "My Name is Library A\n";
}
int main()
{
    std::cout << getAName();
    std::cout << getBName();
}
