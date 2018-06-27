#include <cstddef>
#include <cstdio>

int
main()
{
    std::printf("%zu\n", alignof(std::max_align_t));
}
