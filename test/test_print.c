#include <stdio.h>
#include <inttypes.h>
int main (void) {
    uint32_t a=1234;
    uint16_t b=5678;
    printf("%" PRIu32 "\n",a);
    printf("%" PRIu16 "\n",b);
    return 0;
}
