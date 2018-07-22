#include <stdio.h>
#include <stddef.h>

int
main()
{
#ifdef __BIGGEST_ALIGNMENT__
    printf("%d\n", (int) __BIGGEST_ALIGNMENT__);
#else
    int a = 2 * sizeof(size_t);
    int b = __alignof__(long double);
    printf("%d\n", a > b ? a : b);
#endif
}
