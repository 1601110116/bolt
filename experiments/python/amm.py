#!/bin/env/python


import numpy as np
from sklearn.utils.extmath import randomized_svd


# ================================================================ samplings

def _compute_dim_scores(A, B, A_col_norms=None, B_row_norms=None):
    if A_col_norms is None:
        A_col_norms = np.linalg.norm(A, axis=0)
    if B_row_norms is None:
        B_row_norms = np.linalg.norm(B, axis=1)
    return A_col_norms * B_row_norms


def sketch_sq_sample(A, B, d):
    scores = _compute_dim_scores(A, B)
    probs = scores / np.sum(scores)
    D = A.shape[1]
    keep_idxs = np.random.choice(D, size=d, p=probs)

    weights = np.sqrt(d * probs[keep_idxs])
    return A[:, keep_idxs] / weights, B[keep_idxs] / weights.reshape(-1, 1)


def sketch_sq_deterministic(A, B, d):
    scores = _compute_dim_scores(A, B)
    D = A.shape[1]
    keep_idxs = np.argsort(scores)[::-d]

    weights = np.sqrt(d * (1. / D))  # uniform prob
    return A[:, keep_idxs] / weights, B[keep_idxs] / weights


def test_sketch_sq_sample():
    print("test_sketch_sq_sample")
    N, M, D = 100, 50, 200
    np.random.seed(1234)
    A = np.random.randint(5, size=(N, D)).astype(np.float32)
    B = np.random.randint(5, size=(D, M)).astype(np.float32)

    AB = A @ B
    orig_frob_sq = np.sum(AB * AB)

    prev_normed_err = np.inf
    for d in (10, 20, 30, 40, 50):
        A_hat, B_hat = sketch_sq_sample(A, B, d)
        # A_hat, B_hat = sketch_sq_deterministic(A, B, d)
        AB_hat = A_hat @ B_hat
        diffs = AB - AB_hat
        err_frob_sq = np.sum(diffs * diffs)
        normed_err_sq = err_frob_sq / orig_frob_sq
        print('d = {}, err = {:.3f}'.format(d, normed_err_sq))
        assert normed_err_sq < 1.
        assert normed_err_sq < prev_normed_err
        prev_normed_err = normed_err_sq


# ================================================================ TruncatedSVD

def svd_sketches(A, B, d, **kwargs):
    Ua, Sa, VTa = randomized_svd(A, n_components=d, **kwargs)
    Ub, Sb, VTb = randomized_svd(B, n_components=d, **kwargs)

    # print("truncated svd mat shapes:")
    # print(Ua.shape, Sa.shape, VTa.shape)
    # print(Ub.shape, Sb.shape, VTb.shape)

    return (Ua, np.diag(Sa) @ VTa), (Ub, np.diag(Sb) @ VTb)


def test_svd_sketches():
    print("test_svd_sketches")
    N, M, D = 100, 80, 50
    np.random.seed(1234)
    A = np.random.randint(5, size=(N, D)).astype(np.float32)
    B = np.random.randint(5, size=(D, M)).astype(np.float32)

    AB = A @ B
    orig_frob_sq = np.sum(AB * AB)

    prev_normed_err = np.inf
    # for d in [10]:
    for d in (1, 2, 4, 8, 16, 32):
        (Ua, SVTa), (Ub, SVTb) = svd_sketches(A, B, d)
        AB_hat = Ua @ (SVTa @ Ub) @ SVTb

        # print("fused mats shapes: ")
        # print(Ua.shape, SVTa.shape, Ub.shape, SVTb.shape)

        diffs = AB - AB_hat
        err_frob_sq = np.sum(diffs * diffs)
        normed_err_sq = err_frob_sq / orig_frob_sq
        print('d = {}, err = {:.5f}'.format(d, normed_err_sq))
        assert normed_err_sq < 1.
        assert normed_err_sq < prev_normed_err
        prev_normed_err = normed_err_sq


# ================================================================ FD-amm

def frequent_directions(A, d, variant=None):
    N, D = A.shape
    H = np.zeros((d, D))

    # for i in range(N):
    H[:d - 1] = A[:d - 1]
    for i in range(d - 1, N):
        H[-1] = A[i]
        try:
            U, S, Vt = np.linalg.svd(H, full_matrices=False)  # N x d, d, d x D
        except np.linalg.LinAlgError as e:
            print("SVD failed at iter ", i - (d - 1))
            print("H shape: ", H.shape)
            print("A shape: ", A.shape)
            print("d: ", d)
            # print("svd mat shape: ", U.shape, S.shape, Vt.shape)
            raise e
        # cutoff = S[d - 1]  # S is returned as a vector, not a diagonal mat
        if variant == 'robust':
            raise NotImplementedError()
        else:
            S = np.sqrt((S - S[-1]) ** 2)
            # print("new S shape: ", S.shape)
        H = np.diag(S) @ Vt  # d x D

    return H


def fd_amm_sketches(A, B, d):
    G = np.hstack((A.T, B))   # D x (N + M)
    H = frequent_directions(G, d)
    assert H.shape == (d, A.shape[0] + B.shape[1])
    C = H[:, :A.shape[0]]  # d x N
    D = H[:, A.shape[0]:]  # d x M
    return C.T, D


def test_fd_amm_sketches():
    print("test_fd_amm_sketches")
    N, M, D = 100, 80, 50
    np.random.seed(1234)
    A = np.random.randint(5, size=(N, D)).astype(np.float32)
    B = np.random.randint(5, size=(D, M)).astype(np.float32)

    AB = A @ B
    orig_frob_sq = np.sum(AB * AB)

    prev_normed_err = np.inf
    for d in (1, 2, 4, 8, 16, 32):
        A_hat, B_hat = fd_amm_sketches(A, B, d)
        AB_hat = A_hat @ B_hat

        diffs = AB - AB_hat
        err_frob_sq = np.sum(diffs * diffs)
        normed_err_sq = err_frob_sq / orig_frob_sq
        print('d = {}, err = {:.5f}'.format(d, normed_err_sq))
        assert normed_err_sq < 1.
        assert normed_err_sq < prev_normed_err
        prev_normed_err = normed_err_sq


# ================================================================ Co-occurring

def cooccur_sketches(A, B, d):
    N, D = A.shape
    B = B.T
    M, _ = B.shape
    assert B.shape[1] == D

    X = np.copy(A[:, :d])   # N x d
    # Y = np.copy(B[:d].T)    # M x d
    Y = np.copy(B[:, :d])    # M x d

    # mid_idx = d - 2  # does this make it work better for large d? EDIT: nope
    mid_idx = d // 2
    ntrailing_zeros = d - mid_idx
    # print("mid_idx: ", mid_idx)
    # print("ntrailing_zeros: ", ntrailing_zeros)

    i = 0
    while i < D:
        Qx, Rx = np.linalg.qr(X)  # N x d, d x d
        Qy, Ry = np.linalg.qr(Y)  # M x d, d x d
        prod = Rx @ Ry.T          # d x d
        U, S, Vt = np.linalg.svd(prod, full_matrices=False)  # d x d, d, d x d
        cutoff = S[mid_idx]
        S = np.sqrt(np.maximum(S - cutoff, 0))

        X = Qx @ (U @ np.diag(S))
        # Y = Qy @ (Vt @ np.diag(S))
        Y = Qy @ (Vt.T @ np.diag(S))

        end_dim = min(D, i + ntrailing_zeros)
        ncols_to_copy = end_dim - i
        end_col = mid_idx + ncols_to_copy

        # print("S: ", S)
        # print("i, end_dim, ncols_to_copy, end_col = ",
        #       i, end_dim, ncols_to_copy, end_col)

        # X[:, mid_idx:end_col] = A[:, i:end_dim]
        # Y[:, mid_idx:end_col] = B[i:end_dim].T
        X[:, mid_idx:end_col] = np.copy(A[:, i:end_dim])
        # Y[:, mid_idx:end_col] = np.copy(B[i:end_dim].T)
        Y[:, mid_idx:end_col] = np.copy(B[:, i:end_dim])
        i = end_dim

    return X, Y.T


def test_cooccur_sketches():
    print("test_cooccur_sketches")
    # so this doesn't have monotonically better acc as d increases; seems to
    # run into issues with d being a large fraction of D, possibly because
    # then it doesn't have many iterations and it's just zeroing out a ton of
    # the singular vectors

    N, M, D = 100, 80, 50
    # N, M, D = 100, 80, 160
    np.random.seed(1234)
    A = np.random.randint(5, size=(N, D)).astype(np.float32)
    B = np.random.randint(5, size=(D, M)).astype(np.float32)

    AB = A @ B
    orig_frob_sq = np.sum(AB * AB)

    # prev_normed_err = np.inf
    # for d in [4]:
    for d in (2, 4, 8, 16, 32):
        # A_hat, B_hat = fd_amm_sketches(A, B, d)
        A_hat, B_hat = cooccur_sketches(A, B, d)
        AB_hat = A_hat @ B_hat

        # print("fused mats shapes: ")
        # print(Ua.shape, SVTa.shape, Ub.shape, SVTb.shape)

        diffs = AB - AB_hat
        err_frob_sq = np.sum(diffs * diffs)
        normed_err_sq = err_frob_sq / orig_frob_sq
        print('d = {}, err = {:.5f}'.format(d, normed_err_sq))
        assert normed_err_sq < 1.
        # assert normed_err_sq < prev_normed_err
        # prev_normed_err = normed_err_sq


# ================================================================ main

# def main():
#     pass


if __name__ == '__main__':
    test_sketch_sq_sample()
    test_svd_sketches()
    test_fd_amm_sketches()
    test_cooccur_sketches()
