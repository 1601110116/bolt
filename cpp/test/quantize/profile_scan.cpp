//
//  profile_scan.cpp
//  Bolt
//
//  Created by DB on 12/10/19.
//  Copyright © 2019 D Blalock. All rights reserved.
//

#ifdef BLAZE
    #include "src/quantize/product_quantize.hpp"
    #include "test/quantize/amm_common.hpp"
#else
    #include "amm_common.hpp"
    #include "bit_ops.hpp"
    #include "product_quantize.hpp"
#endif

static constexpr int kNtrialsScan = 20;

// void _profile_amm_popcount(const char* dset_name, int N, int D, int M) {
void _profile_scan_popcount(int nrows, int nbytes) {
    static constexpr int M = 1;
    int ncols = nbytes / 8;

    RowMatrix<uint64_t> A(nrows, ncols); A.setRandom();
    ColMatrix<uint64_t> B(ncols, M); B.setRandom();
    ColMatrix<uint16_t> out(nrows, M); out.setZero();

    std::string msg(string_with_format(
        "%-22s, N C B:, %7d,  -1, %2d,\t", "popcount scan", nrows, nbytes));

    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        out.data(), out.size(),
        (bgemm(A.data(), B.data(), nrows, ncols, M, out.data()) ));
}

TEST_CASE("popcnt scan timing", "[amm][scan][popcount][profile][old]") {

    // the takeaway from this function is that not tiling the inner loops
    // wrecks everything else when D is unknown, and does basically the same
    // as <2, 1> and <3, 1> when it is known (these 2 are the only other real
    // competitors)

    // static constexpr int nbytes = 128;
    // static constexpr int nbytes = 64;
    // static constexpr int nbytes = 32;
    static constexpr int nbytes = 16;
    // static constexpr int nbytes = 8;
    // static constexpr int nbytes = 256;
    static constexpr int D = nbytes / 8;
    // static constexpr int D = 10;
    // int D = 1;
    // int D = 10;
    // int N = 12 * 1024;
    // int N = 1024;
    // int N = 100;
    // int N = 12 * 1024;
    int N = 50 * 1000;
    // int N = 11 * 1024;
    // int N = 5 * 1024;
    // int N = 4 * 1024;
    // int M = 24;
    int M = 12;
    // int M = 10;
    // int M = 1;
    // int M = 6;
    // int M = 6;

    RowMatrix<uint64_t> A(N, D); A.setRandom();
    ColMatrix<uint64_t> B(D, M); B.setRandom();
    ColMatrix<uint16_t> out(N, M); out.setZero();
    // ColMatrix<uint16_t> out(N * 10, M * 10);

    // printf("min A val: %llu\n", A.minCoeff());
    // printf("max A val: %llu\n", A.maxCoeff());
    // printf("min B val: %llu\n", B.minCoeff());
    // printf("max B val: %llu\n", B.maxCoeff());

    // REPEATED_PROFILE_DIST_COMPUTATION(1, "just printf", 1,
    //     out.data(), out.size(), printf("just printf\n"));
    // // REPEATED_PROFILE_DIST_COMPUTATION(3, "popcount amm D=1, 1,1", 5,
    // REPEATED_PROFILE_DIST_COMPUTATION(, "popcount amm D=1, 1,1", 1,

    // _bgemm<1, 1>(A.data(), B.data(), N, D, M, out.data());

    // int max_idx = -1;
    // int val_at_idx = -1;
    // for (int i = 0; i < out.size(); i++) {
    //     auto val = out.data()[i];
    //     if (val > 0) { max_idx = i; val_at_idx = val; }
    // }
    // printf("largest nonzero idx of out: %d; val = %d\n", max_idx, val_at_idx);

        // (do {
        //     _bgemm<1, 1>(A.data(), B.data(), N, D, M, out.data());
        //     printf("out min, max = %d, %d", out.minCoeff(), out.maxCoeff());
        // } while(0) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm", kNtrials,
        out.data(), out.size(),
        (bgemm(A.data(), B.data(), N, D, M, out.data()) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 1,1", kNtrials,
        out.data(), out.size(),
        (_bgemm<1, 1, D>(A.data(), B.data(), N, D, M, out.data()) ));
        // (_bgemm<1, 1, -1>(A.data(), B.data(), N, D, M, out.data()) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 1,2", kNtrials,
        out.data(), out.size(),
        (_bgemm<1, 2, D>(A.data(), B.data(), N, D, M, out.data()) ));
        // (_bgemm<1, 2, -1>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 1,3", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<1, 3, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 1,4", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<1, 4, D>(A.data(), B.data(), N, D, M, out.data()) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 2,1", kNtrials,
        out.data(), out.size(),
        (_bgemm<2, 1, D>(A.data(), B.data(), N, D, M, out.data()) ));
        // (_bgemm<2, 1, -1>(A.data(), B.data(), N, D, M, out.data()) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 2,2", kNtrials,
        out.data(), out.size(),
        (_bgemm<2, 2, D>(A.data(), B.data(), N, D, M, out.data()) ));
        // (_bgemm<2, 2, -1>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 2,3", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<2, 3, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 2,4", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<2, 4, D>(A.data(), B.data(), N, D, M, out.data()) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 3,1", kNtrials,
        out.data(), out.size(),
        (_bgemm<3, 1, D>(A.data(), B.data(), N, D, M, out.data()) ));
        // (_bgemm<3, 1, -1>(A.data(), B.data(), N, D, M, out.data()) ));
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 3,2", kNtrials,
        out.data(), out.size(),
        (_bgemm<3, 2, D>(A.data(), B.data(), N, D, M, out.data()) ));
        // (_bgemm<3, 2, -1>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 3,3", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<3, 3, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 3,4", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<3, 4, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 4,1", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<4, 1, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 4,2", kNtrials,
    // //     out.data(), out.size(),
    // //     (_bgemm<4, 2, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 4,3", kNtrials,
    // //     out.data(), out.size(),
    // //     (_bgemm<4, 3, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 4,4", kNtrials,
    // //     out.data(), out.size(),
    // //     (_bgemm<4, 4, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 5,1", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<5, 1, D>(A.data(), B.data(), N, D, M, out.data()) ));
    // REPEATED_PROFILE_DIST_COMPUTATION(kNreps, "popcount amm D=1, 6,1", kNtrials,
    //     out.data(), out.size(),
    //     (_bgemm<6, 1, D>(A.data(), B.data(), N, D, M, out.data()) ));
}

// note that this is the scan time for a single query; ie, matrix-vector prod
void _profile_scan(int nrows, int nbytes) {
    int nblocks = nrows / 32;

    int ncodebooks = 2 * nbytes;
    int ncodebooks_pq = nbytes;

    // for mithral / bolt
    ColMatrix<uint8_t> codes16(nrows, ncodebooks); codes16.setRandom();
    codes16 = codes16.unaryExpr(
        [=](const uint8_t x) { return (uint8_t)(x % 16); });
    ColMatrix<uint8_t> luts16(16, ncodebooks); luts16.setRandom();
    luts16 = luts16.array() / ncodebooks; // make max lut value small
    RowVector<uint8_t> dists_u8(nrows);
    RowVector<uint16_t> dists_u16(nrows);

    // for pq / opq
    ColMatrix<uint8_t> codes256(nrows, ncodebooks_pq);
    codes256.setRandom();
    codes256 = codes256.unaryExpr(
        [=](const uint8_t x) { return (uint8_t)(x % 256); });
    ColMatrix<float> luts_f32(256, ncodebooks_pq); luts_f32.setRandom();
    RowVector<float> dists_f32(nrows); dists_f32.setRandom();

    std::string msg;
    auto fmt_as_cppstring = string_with_format(
        "%%-22s, N C B:, %7d, %%3d, %2d,\t", nrows, ncodebooks_pq);
    auto fmt = fmt_as_cppstring.c_str();

    // int nbytes16 = ncodebooks / 2;
    // int nbytes256 = ncodebooks;

    // ------------------------ mithral
    RowVector<uint8_t> dists_u8_x2(nrows * 2); // to handle upcast
    msg = string_with_format(fmt, "mithral scan upcast4", ncodebooks);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_u8_x2.data(), nrows,
        (mithral_scan<4>(codes16.data(), nblocks, ncodebooks,
                      luts16.data(), dists_u8_x2.data())));
    msg = string_with_format(fmt, "mithral scan upcast8", ncodebooks);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_u8_x2.data(), nrows,
        (mithral_scan(codes16.data(), nblocks, ncodebooks,
                      luts16.data(), dists_u8_x2.data())));
    msg = string_with_format(fmt, "mithral scan upcast16", ncodebooks);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_u8_x2.data(), nrows,
        (mithral_scan<16>(codes16.data(), nblocks, ncodebooks,
                      luts16.data(), dists_u8_x2.data())));

    // ------------------------ bolt
    static constexpr bool signed_luts = true; // bolt uses signed for dotprod
    msg = string_with_format(fmt, "bolt scan uint8", ncodebooks);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_u8.data(), nrows,
        (bolt_scan<false, signed_luts>(codes16.data(), nblocks, ncodebooks,
                                       luts16.data(), dists_u8.data())));
    msg = string_with_format(fmt, "bolt scan uint16", ncodebooks);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_u16.data(), nrows,
        (bolt_scan<false, signed_luts>(codes16.data(), nblocks, ncodebooks,
                                       luts16.data(), dists_u16.data())));
    msg = string_with_format(fmt, "bolt scan safe uint16", ncodebooks);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_u16.data(), nrows,
        (bolt_scan<true, signed_luts>(codes16.data(), nblocks, ncodebooks,
                                      luts16.data(), dists_u16.data())));

    // ------------------------ pq (same as opq)
    msg = string_with_format(fmt, "pq scan", ncodebooks_pq);
    REPEATED_PROFILE_DIST_COMPUTATION(kNreps, msg, kNtrialsScan,
        dists_f32.data(), nrows,
        pq_scan_8b(codes256.data(), nrows, ncodebooks_pq, luts_f32.data(),
                   dists_f32.data()));
}


TEST_CASE("popcount scan timing", "[amm][scan][popcount][profile]") {
    // static constexpr int nrows = 100 * 1024;
    static constexpr int nrows = 1000 * 1000;

    // printf("algo, _0, N, B, _1, latency0, _2, latency1, _3, latency2, _4\n");
    printf("algo, _0, N, C, B, _1, latency0, _2, latency1, _3, latency2, _4,"
           " latency3, _5, latency4, _6\n");
    std::vector<int> all_nbytes {8, 16, 32, 64};
    for (auto b : all_nbytes) {
        _profile_scan_popcount(nrows, b);
    }
}

TEST_CASE("vq scan timing", "[amm][scan][profile]") {
    // static constexpr int nrows = 100 * 1024;
    static constexpr int nrows = 1000 * 1000;

    // printf("algo, _0, N, C, B, _1, latency0, _2, latency1, _3, latency2, _4\n");

    // std::vector<int> all_ncodebooks {4, 8, 16, 32, 64};
    // for (auto c : all_ncodebooks) {
    std::vector<int> all_nbytes {8, 16, 32, 64};
    for (auto b : all_nbytes) {
        _profile_scan(nrows, b);
    }
}
