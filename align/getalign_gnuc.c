#include <stdio.h>
#include <stddef.h>

int
main()
{
    int a = 2 * sizeof(size_t);
    int b = __alignof__(long double);
    printf("%d\n", a > b ? a : b);
}
