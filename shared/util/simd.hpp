#pragma once

#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace ud {

enum class SimdLevel {
    None,
    SSE42,
    AVX2,
    AVX512
};

inline SimdLevel detect_simd_level() {
#ifdef _WIN32
    int cpuInfo[4] = {-1};
    __cpuid(cpuInfo, 1);
    bool has_sse42 = (cpuInfo[2] & (1 << 20)) != 0;

    __cpuid(cpuInfo, 7);
    bool has_avx2 = (cpuInfo[1] & (1 << 5)) != 0;
    bool has_avx512f = (cpuInfo[1] & (1 << 16)) != 0;

    if (has_avx512f) return SimdLevel::AVX512;
    if (has_avx2) return SimdLevel::AVX2;
    if (has_sse42) return SimdLevel::SSE42;
#else
    unsigned int eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    bool has_sse42 = (ecx & bit_SSE4_2) != 0;

    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    bool has_avx2 = (ebx & bit_AVX2) != 0;
    bool has_avx512f = (ebx & bit_AVX512F) != 0;

    if (has_avx512f) return SimdLevel::AVX512;
    if (has_avx2) return SimdLevel::AVX2;
    if (has_sse42) return SimdLevel::SSE42;
#endif
    return SimdLevel::None;
}

// Global cached SIMD level
inline SimdLevel get_simd_level() {
    static SimdLevel level = detect_simd_level();
    return level;
}

inline void* aligned_alloc_simd(size_t size, size_t alignment = 64) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return nullptr;
    }
    return ptr;
#endif
}

inline void aligned_free_simd(void* ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

} // namespace ud
