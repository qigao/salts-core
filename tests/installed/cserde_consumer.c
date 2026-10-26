#include <cserde/cserde.h>
int main(void) {
    cserde_token value = {0};
    value.kind = CSERDE_NULL;
    return cserde_token_valid(&value) ? 0 : 1;
}
