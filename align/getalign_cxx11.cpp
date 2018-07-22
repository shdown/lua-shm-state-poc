#include <cstddef>
#include <cstdio>
using namespace std;

int
main()
{
    printf("%zu\n", alignof(max_align_t));
}
