//
//  avx_utils.hpp
//  Dig
//
//  Created by DB on 2017-2-7
//  Copyright (c) 2016 DB. All rights reserved.
//

#ifndef __AVX_UTILS_HPP
#define __AVX_UTILS_HPP

#include "immintrin.h"

static_assert(__AVX2__, "AVX 2 is required! Try --march=native or -mavx2");

namespace {

// ================================================================ Types

// TODO this is probably not the best file for this
struct Reductions {
    enum { DotProd, DistL2, DistL1 };
};

// ================================================================ Functions

// ------------------------------------------------ avx utils adapted from eigen
// see Eigen/src/Core/arch/AVX/PacketMath.h

// float predux_max(const __m256& a)
// {
//     // 3 + 3 cycles
//     auto tmp = _mm256_max_ps(a, _mm256_permute2f128_ps(a,a,1));
//     // 3 + 3 cycles (_MM_SHUFFLE is a built-in macro that generates constants)
//     tmp = _mm256_max_ps(tmp, _mm256_shuffle_ps(tmp,tmp,_MM_SHUFFLE(1,0,3,2)));
//     // 1 cycle + 3 cycles + 1 cycle
//     return pfirst(_mm256_max_ps(tmp, _mm256_shuffle_ps(tmp,tmp,1)));
// }

static inline float pfirst(const __m256& a) {
  return _mm_cvtss_f32(_mm256_castps256_ps128(a));
}

static inline int32_t pfirst(const __m256i& a) {
    return _mm_cvtsi128_si32(_mm256_castsi256_si128(a));
}

// int32_t predux_min(const __m256i a) {
//     auto tmp = _mm256_min_epi32(a, _mm256_permute2x128_si256(a,a,1));
//     auto tmp2 = _mm256_min_epi32(tmp, _mm256_shuffle_epi32(tmp,tmp,_MM_SHUFFLE(1,0,3,2)));
//     return pfirst(_mm256_min_epi32(tmp2, _mm256_shuffle_epi32(tmp2,tmp2,1)));
//     // return _mm256_extract_epi32(tmp2)
// }

// returns a * b + c, elementwise
inline __m256 fma(__m256 a, __m256 b, __m256 c) {
    __m256 res = c;
    __asm__("vfmadd231ps %[a], %[b], %[c]" : [c] "+x" (res) : [a] "x" (a), [b] "x" (b));
    return res;
}

// ------------------------------------------------ other avx utils

template<class T>
static int8_t msb_idx_u32(T x) {
    // return 8*sizeof(uint32_t) - 1 - __builtin_clzl((uint32_t)x);
    // XXX if sizeof(uinsigned int) != 4, this will break
    static_assert(sizeof(unsigned int) == 4,
        "XXX Need to use different builtin for counting leading zeros");
    return ((uint32_t)31) - __builtin_clz((uint32_t)x);
}

template<class T>
static inline __m256i load_si256i(T* ptr) {
    return _mm256_load_si256((__m256i*)ptr);
}
template<class T>
static inline __m128i load_si128i(T* ptr) {
    return _mm_load_si128((__m128i*)ptr);
}

template<class T>
static inline __m256i stream_load_si256i(T* ptr) {
    return _mm256_stream_load_si256((__m256i*)ptr);
}

static inline __m256i broadcast_min(const __m256i a) {
    // swap upper and lower 128b halves, then min with original
    auto tmp = _mm256_min_epi32(a, _mm256_permute2x128_si256(a,a,1));
    // swap upper and lower halves within each 128b half, then min
    auto tmp2 = _mm256_min_epi32(tmp, _mm256_shuffle_epi32(tmp, _MM_SHUFFLE(1,0,3,2)));
    // alternative elements have min of evens and min of odds, respectively;
    // so swap adjacent pairs of elements within each 128b half
    return _mm256_min_epi32(tmp2, _mm256_shuffle_epi32(tmp2, _MM_SHUFFLE(2,3,0,1)));
}

static inline __m256i packed_epu16_to_unpacked_epu8(const __m256i& a, const __m256i& b) {
    // indices to undo packus_epi32 followed by packus_epi16, once we've
    // swapped 64-bit blocks 1 and 2
    static const __m256i shuffle_idxs = _mm256_set_epi8(
        31-16, 30-16, 29-16, 28-16, 23-16, 22-16, 21-16, 20-16,
        27-16, 26-16, 25-16, 24-16, 19-16, 18-16, 17-16, 16-16,
        15- 0, 14- 0, 13- 0, 12- 0, 7 - 0, 6 - 0, 5 - 0, 4 - 0,
        11- 0, 10- 0, 9 - 0, 8 - 0, 3 - 0, 2 - 0, 1 - 0, 0 - 0);

    auto dists_uint8 = _mm256_packus_epi16(a, b);
    // undo the weird shuffling caused by the pack operations
    auto dists_perm = _mm256_permute4x64_epi64(
        dists_uint8, _MM_SHUFFLE(3,1,2,0));
    return _mm256_shuffle_epi8(dists_perm, shuffle_idxs);
}

// based on https://stackoverflow.com/a/51779212/1153180
template<bool Signed=true>
static inline __m256i pack_ps_epi8_or_epu8(const __m256& x0, const __m256& x1,
                                           const __m256& x2, const __m256& x3)
{
    __m256i a = _mm256_cvtps_epi32(x0);
    __m256i b = _mm256_cvtps_epi32(x1);
    __m256i c = _mm256_cvtps_epi32(x2);
    __m256i d = _mm256_cvtps_epi32(x3);
    __m256i ab = _mm256_packs_epi32(a,b);
    __m256i cd = _mm256_packs_epi32(c,d);
    __m256i abcd = _mm256_undefined_si256();
    if (Signed) {
        abcd = _mm256_packs_epi16(ab, cd);
    } else {
        abcd = _mm256_packus_epi16(ab, cd);
    }
    // packed to one vector, but in [ a_lo, b_lo, c_lo, d_lo | a_hi, b_hi, c_hi, d_hi ] order
    // if you can deal with that in-memory format (e.g. for later in-lane unpack), great, you're done

    // but if you need sequential order, then vpermd:
    __m256i lanefix = _mm256_permutevar8x32_epi32(abcd, _mm256_setr_epi32(0,4, 1,5, 2,6, 3,7));
    return lanefix;
}

template<bool Signed=true>
static inline __m256i load_4xf32_as_32xepi8_or_epu8(
    const float* x, const __m256& scales, const __m256& offsets)
{
    auto x0 = fma(_mm256_loadu_ps(x), scales, offsets);
    auto x1 = fma(_mm256_loadu_ps(x + 8), scales, offsets);
    auto x2 = fma(_mm256_loadu_ps(x + 16), scales, offsets);
    auto x3 = fma(_mm256_loadu_ps(x + 24), scales, offsets);
    return pack_ps_epi8_or_epu8<Signed>(x0, x1, x2, x3);
}


// assumes N % 32 == 0, D % NReadCols == 0, M >= 2
template<int NReadCols, int NWriteCols>
static inline void _sgemm_colmajor_narrow_padded(
    const float* A, const float *B, int N, int D, int M, float* out)
{
    static const int packet_sz = 8;
    int nblocks_N = N / 32;
    int nstripes_D = D / NWriteCols;
    int nstripes_M = M / NWriteCols;

    int in_cols[NReadCols];
    int out_cols[NWriteCols];
    const float* a_col_starts[NReadCols];
    const float* b_col_starts[NWriteCols];
    float* out_col_starts[NWriteCols];
    const float* a_col_ptrs[NReadCols];
    const float* b_col_ptrs[NWriteCols];
    float* out_col_ptrs[NWriteCols];
    // __m256 a_subs[NReadCols];
    __m256 b_subs[NReadCols * NWriteCols];
    // for (int i = 0; i < NReadCols; i++) {
    //     a_subs[i] = _mm256_undefined_ps();
    // }
    for (int i = 0; i < NReadCols * NWriteCols; i++) {
        b_subs[i] = _mm256_undefined_ps();
    }
    for (int i = 0; i < N * D; i++) { out[i] = 0; }  // zero output

    for (int m = 0; m < NWriteCols; m++) { // for each group of output cols
        for (int j = 0; j < nstripes_D; j++) { // for each group of input cols
            // set col start ptrs and current ptrs for simplicity
            for (int jj = 0; jj < NReadCols; jj++) {
                in_cols[jj] = j * NReadCols + jj;
                a_col_starts[jj] = A + in_cols[jj] * N;
                a_col_ptrs[jj] = a_col_starts[jj];
            }
            for (int mm = 0; mm < NWriteCols; mm++) {
                auto out_col = m * NWriteCols + mm;
                out_cols[mm] = out_col;
                b_col_starts[mm] = B + D * out_col;
                out_col_starts[mm] = out + N * out_col;
                // b_col_ptrs[mm] = b_col_starts[mm];
                out_col_ptrs[mm] = out_col_starts[mm];
            }

            // load up coeffs for this group of input dims, for all out cols
            for (int jj = 0; jj < NReadCols; jj++) {
                for (int mm = 0; mm < NWriteCols; mm++) {
                    auto b_row = in_cols[jj];
                    auto bval = *(b_col_starts[mm] + b_row);
                    b_subs[jj * NWriteCols + mm] = _mm256_set1_ps(bval);
                }
            }

            for (int b = 0; b < nblocks_N; b++) {   // for each block of rows
                // load up sums-so-far from current output
                __m256 sums[NWriteCols];
                for (int mm = 0; mm < NWriteCols; mm++) {
                    auto out_ptr = out_col_ptrs[mm];
                    sums[mm] = _mm256_load_ps(out_ptr);
                }
                // load input from each col, and update partial sums for
                // each output
                for (int jj = 0; jj < NReadCols; jj++) {  // for each in col
                    auto a_ptr = a_col_ptrs[jj];
                    auto avec = _mm256_load_ps(a_ptr);
                    a_col_ptrs[jj] += packet_sz;

                    for (int mm = 0; mm < NWriteCols; mm++) { // each out col
                        auto bvec = b_subs[jj * NWriteCols + mm];
                        sums[mm] = fma(avec, bvec, sums[mm]);
                    }
                }
                // write back partial sums and increment output
                for (int mm = 0; mm < NWriteCols; mm++) {
                    auto out_ptr = out_col_ptrs[mm];
                    __m256 sum = sums[mm];
                    sums[mm] = _mm256_store_ps(out_ptr, sum);
                    out_col_ptrs[mm] += packet_sz;
                }
            }
        }
    }
}

static inline void sgemm_colmajor_narrow_padded(
    const float* A, const float *B, int N, int D, int M, float* out)
{
    _sgemm_colmajor_narrow_padded<4, 2>(A, B, N, D, M, out);
}

} // anon namespace

#endif // __AVX_UTILS_HPP

