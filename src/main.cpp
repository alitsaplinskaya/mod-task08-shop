
#include "task.h"
int main()
{
    setlocale(LC_ALL, "rus");

    my_shop shop(5, 75, 600, 12, 5);
    std::cout << "Start workflow" << std::endl;
    shop.start(20000);
}
