
#pragma once

#if defined(_MSC_VER)

    #define FORCEINLINE __forceinline
    #define NOINLINE    __declspec(noinline)
    #define NO_DISCARD  [[nodiscard]]
    #define RESTRICT    __restrict
    #define LIKELY      [[likely]]
    #define UNLIKELY    [[unlikely]]

#elif defined(__GNUC__) || defined(__clang__)

    #define FORCEINLINE inline __attribute__((always_inline))
    #define NOINLINE    __attribute__((noinline))
    #define NO_DISCARD  __attribute__((warn_unused_result))
    #define RESTRICT    __attribute__((restrict))
    #define LIKELY      [[likely]]
    #define UNLIKELY    [[unlikely]]

#else

    #error "Unsupported compiler"

#endif
