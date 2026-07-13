#include <assert.h>
#include <stdint.h>

int main(void) {
    _Static_assert(sizeof(uint32_t) == 4U, "uint32_t must be exactly 32 bits");
    _Static_assert(sizeof(uint8_t) == 1U, "uint8_t must be exactly 8 bits");

    assert(UINT32_MAX == UINT32_C(4294967295));
    return 0;
}
