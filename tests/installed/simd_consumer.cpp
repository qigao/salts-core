/* Independent C++17 installed consumer. Only Salts::SIMD is required. */
#include <salts/simd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

static_assert(sizeof(cmeta_v128) == 16u, "128-bit public carrier");
static_assert(alignof(cmeta_v128) == 16u, "16-byte public alignment");
static_assert(std::is_same_v<cmeta_v128, salts_simd_v128>, "one native carrier");
static_assert(std::is_standard_layout_v<cmeta_v128>, "standard-layout carrier");
static_assert(std::is_trivially_copyable_v<cmeta_v128>, "caller-owned value bytes");
static_assert(std::is_standard_layout_v<salts_simd_desc>, "plain lane descriptor");
static_assert(offsetof(salts_simd_v128, bytes) == 0u, "bytes start at zero");

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "SIMD C++17 consumer: %s:%d: %s\n", \
                 __FILE__, __LINE__, #condition); \
    return 1; \
} } while (false)

/* All signatures are checked independently of the producer. Volatile pointer
 * reads also require every C-linkage entry to exist in the installed library. */
#define SIMD_EXECUTION_APIS(X) \
    X(cmeta_simd_v128_load, void, (cmeta_v128 *, const void *)) \
    X(cmeta_simd_v128_store, void, (void *, const cmeta_v128 *)) \
    X(cmeta_simd_splat, bool, (const salts_simd_desc *, cmeta_v128 *, cmeta_simd_scalar)) \
    X(cmeta_simd_load_splat, bool, (const salts_simd_desc *, cmeta_v128 *, const void *)) \
    X(cmeta_simd_load_extend, bool, (const salts_simd_desc *, cmeta_v128 *, const void *)) \
    X(cmeta_simd_load_zero, bool, (uint16_t, cmeta_v128 *, const void *)) \
    X(cmeta_simd_extract_lane, bool, (const salts_simd_desc *, const cmeta_v128 *, uint16_t, cmeta_simd_scalar *)) \
    X(cmeta_simd_replace_lane, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, uint16_t, cmeta_simd_scalar)) \
    X(cmeta_simd_shuffle_bytes, bool, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *, const uint8_t *)) \
    X(cmeta_simd_swizzle_bytes, bool, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_unary, bool, (const salts_simd_desc *, cmeta_simd_unary_op, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_binary, bool, (const salts_simd_desc *, cmeta_simd_binary_op, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_compare, bool, (const salts_simd_desc *, cmeta_simd_compare_op, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_shift, bool, (const salts_simd_desc *, cmeta_simd_shift_op, cmeta_v128 *, const cmeta_v128 *, uint32_t)) \
    X(cmeta_simd_saturating_binary, bool, (const salts_simd_desc *, cmeta_simd_saturating_op, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_reduce, bool, (const salts_simd_desc *, cmeta_simd_reduce_op, const cmeta_v128 *, uint32_t *)) \
    X(cmeta_simd_narrow, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_extend_half, bool, (const salts_simd_desc *, cmeta_simd_half, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_extmul_half, bool, (const salts_simd_desc *, cmeta_simd_half, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_extadd_pairwise, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_q15mulr_sat, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_dot_pairwise, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_convert, bool, (const salts_simd_desc *, const salts_simd_desc *, cmeta_simd_convert_op, cmeta_simd_lane_policy, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_select, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_splat, void, (cmeta_v128 *, int32_t)) \
    X(cmeta_simd_i32x4_add, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_sub, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_mul, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_eq, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_f32x4_add, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_f32x4_mul, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_v128_bitselect, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *))

#define DECLARE_API(name, result, arguments) \
    static_assert(std::is_same_v<decltype(&name), result (*) arguments>, \
                  #name " public C linkage signature"); \
    static result (*const volatile linked_##name) arguments = &name;
SIMD_EXECUTION_APIS(DECLARE_API)
#undef DECLARE_API

static int check_exported_descriptors() {
    struct expected_shape {
        const salts_simd_desc *desc;
        salts_simd_lane_kind kind;
        uint16_t bits;
        bool mask;
    };
    const expected_shape shapes[] = {
        {&salts_simd_i8x16, SALTS_SIMD_LANE_I8, 8u, false},
        {&salts_simd_u8x16, SALTS_SIMD_LANE_U8, 8u, false},
        {&salts_simd_i16x8, SALTS_SIMD_LANE_I16, 16u, false},
        {&salts_simd_u16x8, SALTS_SIMD_LANE_U16, 16u, false},
        {&salts_simd_i32x4, SALTS_SIMD_LANE_I32, 32u, false},
        {&salts_simd_u32x4, SALTS_SIMD_LANE_U32, 32u, false},
        {&salts_simd_i64x2, SALTS_SIMD_LANE_I64, 64u, false},
        {&salts_simd_u64x2, SALTS_SIMD_LANE_U64, 64u, false},
        {&salts_simd_f32x4, SALTS_SIMD_LANE_F32, 32u, false},
        {&salts_simd_f64x2, SALTS_SIMD_LANE_F64, 64u, false},
        {&salts_simd_b8x16, SALTS_SIMD_LANE_BOOL, 8u, true},
        {&salts_simd_b16x8, SALTS_SIMD_LANE_BOOL, 16u, true},
        {&salts_simd_b32x4, SALTS_SIMD_LANE_BOOL, 32u, true},
        {&salts_simd_b64x2, SALTS_SIMD_LANE_BOOL, 64u, true}
    };
    static_assert(sizeof(shapes) / sizeof(shapes[0]) == 14u, "all DLL data exports");
    for (const auto &shape : shapes) {
        CHECK(salts_simd_desc_valid(shape.desc));
        CHECK(shape.desc->lane_kind == shape.kind);
        CHECK(shape.desc->lane_bits == shape.bits);
        CHECK(shape.desc->lane_count == 128u / shape.bits);
        CHECK(shape.desc->is_mask == shape.mask);
    }
    CHECK(!salts_simd_desc_valid(nullptr));
    return 0;
}

static int check_numeric_and_masks() {
    constexpr uint32_t a[4] = {UINT32_MAX, 1u, UINT32_C(0x80000000), 7u};
    constexpr uint32_t b[4] = {1u, UINT32_MAX, UINT32_C(0x80000000), 9u};
    constexpr uint32_t sum[4] = {0u, 0u, 0u, 16u};
    constexpr uint32_t mask_words[4] = {UINT32_MAX, 0u, UINT32_MAX, 0u};
    constexpr uint32_t selected[4] = {UINT32_MAX, UINT32_MAX, UINT32_C(0x80000000), 9u};
    constexpr int16_t sat_a[8] = {32760, -32760, 1, -1, 100, -100, 300, -300};
    constexpr int16_t sat_b[8] = {100, -100, 2, -2, 50, -50, 100, -100};
    constexpr int16_t sat_sum[8] = {32767, -32768, 3, -3, 150, -150, 400, -400};
    cmeta_v128 left{}, right{}, out{}, mask{};
    uint32_t words[4]{};
    int16_t shorts[8]{};

    cmeta_simd_v128_load(&left, a);
    cmeta_simd_v128_load(&right, b);
    CHECK(cmeta_simd_binary(&salts_simd_u32x4, SALTS_SIMD_BINARY_ADD,
                            &out, &left, &right));
    cmeta_simd_v128_store(words, &out);
    CHECK(std::memcmp(words, sum, sizeof(words)) == 0);
    cmeta_simd_v128_load(&mask, mask_words);
    cmeta_simd_v128_bitselect(&left, &left, &right, &mask);
    cmeta_simd_v128_store(words, &left);
    CHECK(std::memcmp(words, selected, sizeof(words)) == 0);

    cmeta_simd_v128_load(&left, sat_a);
    cmeta_simd_v128_load(&right, sat_b);
    CHECK(cmeta_simd_saturating_binary(&salts_simd_i16x8,
                                       SALTS_SIMD_SATURATING_ADD,
                                       &out, &left, &right));
    cmeta_simd_v128_store(shorts, &out);
    CHECK(std::memcmp(shorts, sat_sum, sizeof(shorts)) == 0);
    return 0;
}

static int check_rejected_operations() {
    cmeta_v128 value{}, out{};
    unsigned char before[16];
    std::memset(out.bytes, 0xa6, sizeof(out.bytes));
    std::memcpy(before, out.bytes, sizeof(before));
    CHECK(!cmeta_simd_unary(&salts_simd_u32x4, SALTS_SIMD_UNARY_ABS,
                            &out, &value));
    CHECK(std::memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_binary(&salts_simd_i32x4, SALTS_SIMD_BINARY_DIV,
                             &out, &value, &value));
    CHECK(std::memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_binary(nullptr, SALTS_SIMD_BINARY_ADD,
                             &out, &value, &value));
    CHECK(std::memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_load_splat(&salts_simd_i8x16, &out, nullptr));
    CHECK(std::memcmp(before, out.bytes, sizeof(before)) == 0);
    cmeta_simd_scalar scalar{};
    scalar.u64 = UINT64_C(0x123456789abcdef0);
    CHECK(!cmeta_simd_extract_lane(&salts_simd_i32x4, &value, 4u, &scalar));
    CHECK(scalar.u64 == UINT64_C(0x123456789abcdef0));
    return 0;
}

static int check_byte_boundaries_and_alias() {
    struct guarded_value {
        unsigned char before[16];
        cmeta_v128 value;
        unsigned char after[16];
    } guarded{};
    unsigned char source[18]{}, destination[18]{};
    std::memset(&guarded, 0x3c, sizeof(guarded));
    std::memset(destination, 0x7d, sizeof(destination));
    for (std::size_t i = 0; i < sizeof(source); ++i)
        source[i] = static_cast<unsigned char>(i * 13u);
    source[8] = 0;
    cmeta_simd_v128_load(&guarded.value, source + 1);
    cmeta_simd_v128_load(&guarded.value, guarded.value.bytes);
    cmeta_simd_v128_store(guarded.value.bytes, &guarded.value);
    cmeta_simd_v128_store(destination + 1, &guarded.value);
    CHECK(std::memcmp(destination + 1, source + 1, 16u) == 0);
    CHECK(destination[0] == 0x7du && destination[17] == 0x7du);
    CHECK(cmeta_simd_unary(&salts_simd_u8x16, SALTS_SIMD_UNARY_NOT,
                           &guarded.value, &guarded.value));
    for (std::size_t i = 0; i < 16u; ++i) {
        CHECK(guarded.before[i] == 0x3cu && guarded.after[i] == 0x3cu);
        CHECK(guarded.value.bytes[i] == static_cast<unsigned char>(source[i + 1u] ^ 0xffu));
    }
    return 0;
}

int main() {
#define CHECK_LINK(name, result, arguments) CHECK(linked_##name != nullptr);
    SIMD_EXECUTION_APIS(CHECK_LINK)
#undef CHECK_LINK
    if (check_exported_descriptors() || check_numeric_and_masks() ||
        check_rejected_operations() || check_byte_boundaries_and_alias())
        return 1;
    std::puts("PASS installed SIMD C++17: 32 functions, 14 descriptors, numeric and byte contracts");
    return 0;
}

#undef SIMD_EXECUTION_APIS
#undef CHECK
