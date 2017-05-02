
#include <cstring> // for memcpy
#include <iostream> // TODO rm after debug

// #include "bolt.hpp"
// #include "public.hpp"  // defines bolt wrapper class
// #include "nn_utils.hpp" // for knn_from_dists

#ifdef BLAZE
    #include "src/quantize/bolt.hpp"
    #include "src/include/public.hpp"  // defines bolt wrapper class
    #include "src/utils/nn_utils.hpp" // for knn_from_dists
#else
    #include "bolt.hpp"
    #include "public.hpp"  // defines bolt wrapper class
    #include "nn_utils.hpp" // for knn_from_dists
#endif


// ------------------------------------------------ BoltEncoder impl

// /** Only accepts values of 8,16,24,32, and returns true iff given one of these */
// bool BoltEncoder::set_nbytes(int nbytes) {
//     if (nbytes == 8 || nbytes == 16 || nbytes == 24 || nbytes == 32) {
//         _nbytes = nbytes;
//         return true;
//     }
//     return false;
// }


BoltEncoder::BoltEncoder(int nbytes, float scaleby):
    _nbytes(nbytes),
    _scaleby(scaleby),
    _offsets(2 * nbytes),
    _lut(16, 2 * nbytes)
{
    bool valid = (nbytes == 2 || nbytes == 8 || nbytes == 16 ||
        nbytes == 24 || nbytes == 32);
    if (!valid) {
        printf("ERROR: Received invalid nbytes %d; "
            "must be one of {2, 8, 16, 24, 32}.", nbytes);
        exit(1);
    }
    _offsets.setZero();
    _lut.setZero();
}

void BoltEncoder::set_offsets(const float* v, int len) {
    _offsets.resize(len);
    auto implied_nbytes = len / 2;
    assert(_nbytes > 0);
    assert(implied_nbytes == _nbytes);

    for (int i = 0; i < len; i++) {
        _offsets(i)  = v[i];
    }
}

void BoltEncoder::set_scale(float a) { _scaleby = a; }

bool BoltEncoder::set_centroids(const float* X, int m, int n) {
    _centroids.resize(m, n);
    auto implied_nbytes = m / (2 * 16);
    assert(_nbytes > 0);
    assert(implied_nbytes == _nbytes);

    int ncodebooks = 2 * _nbytes;
    int ncols = n * ncodebooks;

    // yep, python is feeding in the centroids correctly
//    RowMatrix<float> tmp(m, n);
//    for (int i = 0; i < m; i++) {
//        for (int j = 0; j < n; j++) {
//            tmp(i, j) = *(X + i * n + j);
//        }
//    }
//    std::cout << "cpp received centroids:\n" << tmp << "\n";

//    // is SWIG doing something weird? // TODO rm after debug
//    // --> this works; swig not doing anything too weird
//    for (int i = 0; i < m; i++) {
//        for (int j = 0; j < n; j++) {
//            _centroids(i, j) = *(X + (n * i) + j);
//        }
//    }
//    return false;

    // allow unrolling the loops for various nbytes; this will get called from
    // python, so this can't get inlined and they can't get unrolled otherwise
    switch (_nbytes) {
        case 2:
            bolt_encode_centroids<2>(X, ncols, _centroids.data());
            return true;
        case 8:
            bolt_encode_centroids<8>(X, ncols, _centroids.data());
            return true;
        case 16:
            bolt_encode_centroids<16>(X, ncols, _centroids.data());
            return true;
        case 24:
            bolt_encode_centroids<24>(X, ncols, _centroids.data());
            return true;
        case 32:
            bolt_encode_centroids<32>(X, ncols, _centroids.data());
            return true;
        default:
            return false;
    }
    return false;
}
bool BoltEncoder::set_centroids(const float* X, long m, long n) {
    return BoltEncoder::set_centroids(X, (int)m, (int)n);
}

bool BoltEncoder::set_data(const float* X, int m, int n) {
    assert(_nbytes > 0);
    assert(m > 0);
    assert(n > 2 * _nbytes); // equal would probably also work, but play safe
    _ncodes = m;
    int64_t nblocks = ceil(m / 32.0);
    _codes.resize(nblocks * 32, _nbytes);
    _codes.bottomRows(32).setZero(); // ensure mem initialized past end of X

    switch (_nbytes) {
        case 2:
            bolt_encode<2>(X, m, n, _centroids.data(), _codes.data());
            return true;
        case 8:
            bolt_encode<8>(X, m, n, _centroids.data(), _codes.data());
            return true;
        case 16:
            bolt_encode<16>(X, m, n, _centroids.data(), _codes.data());
            return true;
        case 24:
            bolt_encode<24>(X, m, n, _centroids.data(), _codes.data());
            return true;
        case 32:
            bolt_encode<32>(X, m, n, _centroids.data(), _codes.data());
            return true;
        default:
            return false;
    }
    return false;
}
bool BoltEncoder::set_data(const float* X, long m, long n) {
    return BoltEncoder::set_data(X, (int)m, (int)n);
}

bool BoltEncoder::set_codes(const uint8_t* codes, int m, int n) {
    auto implied_nbytes = n / 2;
    assert(implied_nbytes == _nbytes);
    assert(m > 0);
    _ncodes = m;
    int64_t nblocks = ceil(m / 32.0);
    Eigen::Map<const RowMatrix<uint8_t> > buff_wrapper(codes, m, n);

    _codes.resize(nblocks * 32, 2 * _nbytes);
    _codes.bottomRows(32).setZero(); // ensure mem initialized past end of X
    _codes.topRows(m) = buff_wrapper;

    return true;
}
bool BoltEncoder::set_codes(const RowMatrix<uint8_t>& codes) {
    return set_codes(codes.data(), (int)codes.rows(), (int)codes.cols());
}


void _naive_lut(const float* q, int len, int nbytes,
    const RowMatrix<float>& centroids, const RowVector<float>& offsets,
    float scaleby, ColMatrix<uint8_t>& lut_out) {

    auto ncodebooks = 2 * nbytes;
    auto ncentroids = 16;
    auto subvect_len = centroids.cols();
    auto codebook_sz = subvect_len * ncentroids;
    for (int m = 0; m < ncodebooks; m++) {
        const float* block_ptr = centroids.data() + m * codebook_sz;
        for (int i = 0; i < ncentroids; i++) {
            float dist_sq = 0;
            for (int j = 0; j < subvect_len; j++) {
                auto col = m * subvect_len + j;
                auto diff = block_ptr[j * ncentroids + i] - q[col];
                dist_sq += diff * diff;
            }
            int dist_int = static_cast<int>(dist_sq * scaleby + offsets(m));
            lut_out(i, m) = std::max(0, std::min(255, dist_int));
            // dist_sq = std::fmax(0., std::fmin(dist_sq, 255.));
            // lut_out(i, m) = static_cast<uint8_t>(dist_sq);
        }
    }
}

template<int Reduction>
void lut(const float* q, int len, int nbytes,
    const RowMatrix<float>& centroids, const RowVector<float>& offsets,
    float scaleby, ColMatrix<uint8_t>& lut_out)
{
    assert(scaleby > 0.);
    assert(offsets.data() != nullptr);

    // _naive_lut(q, len, nbytes, centroids, offsets, scaleby, lut_out);
    // return;

    std::cout << "nbytes: " << nbytes << "\n";

    //    static constexpr int ncentroids = 16;
    //    int ncodebooks = nbytes * 2;
    // ColMatrix<uint8_t>& lut_out(ncentroids, ncodebooks);
    auto lut_ptr = lut_out.data();

    // create lookup table and then scan with it
    switch (nbytes) {
        case 2:
            // TODO uncomment after debug
           bolt_lut<2, Reduction>(q, len, centroids.data(), offsets.data(),
               scaleby, lut_ptr);

            // bolt_lut<2, Reduction>(q, len, centroids.data(), lut_ptr);
            break;
        case 8:
            bolt_lut<8, Reduction>(q, len, centroids.data(), offsets.data(),
                scaleby, lut_ptr);
            break;
        case 16:
            bolt_lut<16, Reduction>(q, len, centroids.data(), offsets.data(),
                scaleby, lut_ptr);
            break;
        case 24:
            bolt_lut<24, Reduction>(q, len, centroids.data(), offsets.data(),
                scaleby, lut_ptr);
            break;
        case 32:
            bolt_lut<32, Reduction>(q, len, centroids.data(), offsets.data(),
                scaleby, lut_ptr);
            break;
        default:
            break;
    }
//    return lut_out;
}
void BoltEncoder::lut_l2(const float* q, int len) {
    lut<Reductions::DistL2>(q, len, _nbytes, _centroids,
        _offsets, _scaleby, _lut);
}
void BoltEncoder::lut_dot(const float* q, int len) {
    lut<Reductions::DotProd>(q, len, _nbytes, _centroids,
        _offsets, _scaleby, _lut);
}
void BoltEncoder::lut_l2(const RowVector<float>& q) {
    lut_l2(q.data(), static_cast<int>(q.size()));
}
void BoltEncoder::lut_dot(const RowVector<float>& q) {
    lut_dot(q.data(), static_cast<int>(q.size()));
}

template<int Reduction=Reductions::DistL2, class dist_t>
void query(const float* q, int len, int nbytes,
    const RowMatrix<float>& centroids,
    const RowVector<float>& offsets, float scaleby,
    const RowMatrix<uint8_t>& codes,
    // ColMatrix<float> _centroids, RowMatrix<uint8_t> _codes,
    int64_t ncodes, ColMatrix<uint8_t>& lut_tmp, dist_t* dists)
{
    int ncodebooks = nbytes * 2;
    assert(nbytes > 0);
    assert(scaleby > 0);
    assert(codes.rows() > 0);
    assert(codes.cols() > 0);
    assert(len == (centroids.cols() * ncodebooks));

    int64_t N = codes.rows(); // number of codes stored
    int64_t nblocks = ceil(N / 32.0);

    // static constexpr int ncentroids = 16;
    // ColMatrix<uint8_t> lut_out(ncentroids, ncodebooks);
    // auto lut_ptr = lut_out.data();
    auto lut_ptr = lut_tmp.data();
    assert(lut_ptr != nullptr);

    auto codes_ptr = codes.data();
    assert(codes_ptr != nullptr);

    // create lookup table and then scan with it
    switch (nbytes) {
        case 2:
//             bolt_lut<2, Reduction>(q, len, _centroids.data(), lut_ptr);


            // TODO rm
//            lut<Reduction>(q, len, nbytes, centroids, offsets, scaleby, lut_tmp);

            bolt_lut<2, Reduction>(q, len, centroids.data(), offsets.data(), scaleby,
                                   lut_ptr);
            bolt_scan<2, true>(codes.data(), lut_ptr, dists, nblocks);
            break;
        case 8:
            bolt_lut<8, Reduction>(q, len, centroids.data(), offsets.data(), scaleby,
                                   lut_ptr);
            bolt_scan<8, true>(codes.data(), lut_ptr, dists, nblocks);
            break;
        case 16:
            bolt_lut<16, Reduction>(q, len, centroids.data(), offsets.data(), scaleby,
                                    lut_ptr);
            bolt_scan<16, true>(codes.data(), lut_ptr, dists, nblocks);
            break;
        case 24:
            bolt_lut<24, Reduction>(q, len, centroids.data(), offsets.data(), scaleby,
                                    lut_ptr);
            bolt_scan<24, true>(codes.data(), lut_ptr, dists, nblocks);
            break;
        case 32:
            bolt_lut<32, Reduction>(q, len, centroids.data(), offsets.data(), scaleby,
                                    lut_ptr);
            bolt_scan<32, true>(codes.data(), lut_ptr, dists, nblocks);
            break;
        default:
            break;
    }
}

// TODO version that writes to argout array to avoid unnecessary copy
// TODO allow specifying safe (no overflow) scan
template<int Reduction=Reductions::DistL2>
RowVector<uint16_t> query_all(const float* q, int len, int nbytes,
    // ColMatrix<float> _centroids, RowMatrix<uint8_t> _codes, int64_t _ncodes)
    const RowMatrix<float>& centroids,
    const RowVector<float>& offsets, float scaleby,
    const RowMatrix<uint8_t>& codes, int64_t ncodes,
    ColMatrix<uint8_t>& lut_tmp)
{
    RowVector<uint16_t> dists(codes.rows()); // need 32B alignment, so can't use stl vector
    query(q, len, nbytes, centroids, offsets, scaleby, codes, ncodes,
        lut_tmp, dists.data());

    // okay, ya, swig is returning the right values
    std::cout << "cpp first 20 distances:\n[";
    for (int i = 0; i < 20; i++) {
        std::cout << dists(i) << " ";
    }
    std::cout << "]\n";
    
    return dists;
    
    // copy computed distances into a vector to return; would be nice
    // to write directly into the vector, but can't guarantee 32B alignment
    // without some hacks
//    vector<uint16_t> ret(ncodes);
    
    // ret.reserve(_ncodes);
//    std::memcpy(ret.data(), dists.data(), ncodes * sizeof(uint16_t));
//    printf("ncodes, ret.size(), %lld, %lld\n", _ncodes, ret.size());
//    return ret;
}

template<int Reduction=Reductions::DistL2>
vector<int64_t> query_knn(const float* q, int len, int nbytes,
    const RowMatrix<float>& centroids,
    const RowVector<float>& offsets, float scaleby,
    // ColMatrix<float> _centroids, RowMatrix<uint8_t> _codes,
    const RowMatrix<uint8_t>& codes, int64_t ncodes,
    int k, ColMatrix<uint8_t>& lut_tmp)
{
    RowVector<uint16_t> dists(codes.rows()); // need 32B alignment, so can't use stl vector
    query(q, len, nbytes, centroids, offsets, scaleby, codes, ncodes,
        lut_tmp, dists.data());

    // extract and return the k nearest neighbors
    vector<nn::Neighbor> neighbors = nn::knn_from_dists(dists.data(), codes.rows(), k);
    vector<int64_t> ret(k);
    for (int i = 0; i < k; i++) {
        ret[k] = static_cast<int64_t>(neighbors[i].idx);
    }
    return ret;
}


//vector<uint16_t> BoltEncoder::dists_sq(const float* q, int len) {
RowVector<uint16_t> BoltEncoder::dists_sq(const float* q, int len) {
    return query_all<Reductions::DistL2>(
        q, len, _nbytes, _centroids, _offsets, _scaleby, _codes, _ncodes, _lut);
}
RowVector<uint16_t> BoltEncoder::dot_prods(const float* q, int len) {
    return query_all<Reductions::DotProd>(
        q, len, _nbytes, _centroids, _offsets, _scaleby, _codes, _ncodes, _lut);
}

vector<int64_t> BoltEncoder::knn_l2(const float* q, int len, int k) {
    return query_knn<Reductions::DistL2>(
        q, len, _nbytes, _centroids, _offsets, _scaleby, _codes, _ncodes,
        k, _lut);
}
vector<int64_t> BoltEncoder::knn_mips(const float* q, int len, int k) {
    return query_knn<Reductions::DotProd>(
        q, len, _nbytes, _centroids, _offsets, _scaleby, _codes, _ncodes,
        k, _lut);
}


// simple getters
ColMatrix<uint8_t> BoltEncoder::get_lut() { return _lut; }
RowVector<float> BoltEncoder::get_offsets() { return _offsets; }
float BoltEncoder::get_scale() { return _scaleby; }
