option(SALTSCORE_BUILD_URI "Build standalone re2c-generated URI parser" OFF)
option(SALTSCORE_BUILD_CNET "Build standalone CNet with required pinned providers" OFF)
option(SALTSCORE_CNET_STATIC_CRYPTO_ORIGINAL_OWNER_TEST
  "Qualify test-only original CNet Owner with sole static Salts::Crypto" OFF)
option(SALTSCORE_BUILD_WEBSOCKET_PARSER
  "Qualify CNet-private WebSocket frame parser" OFF)
option(SALTSCORE_BUILD_CSERDE "Build standalone C11 CSerde owner" OFF)
option(SALTSCORE_BUILD_TINYTEST "Build standalone C11 TinyTest owner" OFF)
option(SALTSCORE_BUILD_UNICODE "Build original pinned Unicode 17 owner" OFF)
option(SALTSCORE_BUILD_IDNA "Build original pinned Unicode 17 IDNA owner" OFF)
option(SALTSCORE_NATIVE_CPP_CONFORMANCE
  "Compile original native C++17 source contract against standalone Core" OFF)
option(SALTSCORE_BUILD_FS "Build standalone native FS owner" OFF)
option(SALTSCORE_BUILD_PROCESS "Build standalone Process owner" OFF)
option(SALTSCORE_CNET_OWNER_SOURCE_TESTS
  "Qualify original CNet Owner and actual retained NativeIO terminal" OFF)
option(SALTSCORE_BUILD_SCTP "Build standalone C11 SCTP Wire and Session" OFF)

option(SALTSCORE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(SALTSCORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(SALTSCORE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
