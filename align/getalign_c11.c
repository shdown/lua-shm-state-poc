#include <stdio.h>
#include <stddef.h>

int
main()
{
    printf("%d\n", (int) _Alignof(max_align_t));
}
