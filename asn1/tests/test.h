#ifndef SALTS_ASN1_CONFORMANCE_TEST_H
#define SALTS_ASN1_CONFORMANCE_TEST_H

/* Local assertions for the original, moved BER/DER conformance cases.
 * Test-only; no dependency on TinyTest, SaltsCore or an installed Crypto DSO.
 * Every spec is one executable and each it() is executed exactly once.
 */
#include <stdio.h>
#include <string.h>

static int salts_asn1_case(const char *name)
{
    fprintf(stdout, "case: %s\n", name);
    return 1;
}

#define spec(_name) int main(void)
#define it(_name) if (salts_asn1_case(_name))
#define check_true(_expression) do { \
    if (!(_expression)) { \
        fprintf(stderr, "%s:%d: assertion failed: %s\n", \
                __FILE__, __LINE__, #_expression); \
        return 1; \
    } \
} while (0)
#define salts_check_equal_scalar(_left, _right) do { \
    if (!((_left) == (_right))) { \
        fprintf(stderr, "%s:%d: equality failed: %s == %s\n", \
                __FILE__, __LINE__, #_left, #_right); \
        return 1; \
    } \
} while (0)
#define salts_check_equal_bytes(_left, _right, _size) do { \
    const void *_a = (_left); \
    const void *_b = (_right); \
    size_t _n = (_size); \
    if ((_n && (!_a || !_b)) || (_n && memcmp(_a, _b, _n) != 0)) { \
        fprintf(stderr, "%s:%d: byte comparison failed: %s == %s\n", \
                __FILE__, __LINE__, #_left, #_right); \
        return 1; \
    } \
} while (0)
#define SALTS_ASN1_TEST_SELECT(_1, _2, _3, NAME, ...) NAME
#if defined(__cplusplus)
/* MSVC's traditional C++ preprocessor forwards __VA_ARGS__ as one
 * argument and incorrectly selects `unused`. The migrated C++17
 * conformance uses only the original two-scalar-operand assertion. */
#define check_equal(_left, _right) salts_check_equal_scalar(_left, _right)
#else
#define check_equal(...) SALTS_ASN1_TEST_SELECT( \
    __VA_ARGS__, salts_check_equal_bytes, salts_check_equal_scalar, unused)( __VA_ARGS__ )
#endif

#endif
