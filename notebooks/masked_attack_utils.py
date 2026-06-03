import numpy as np
import matplotlib.pyplot as plt
from IPython.display import display, HTML


# Trace segmentation 

def pair_idx(r, c, v=78):
    assert 0 <= r < c < v
    return r * (2 * v - r - 1) // 2 + (c - r - 1)

def call_idx(r, c, k, which, v=78):
    p = pair_idx(r, c, v)
    return 16 * p + 2 * k + which


def calls_per_share(v=78, o=8):
    return (v * (v - 1) // 2) * o * 2


def call_idx_unmasked(r, c, k, which, v=78, o=8):
    assert 0 <= r < c < v
    assert 0 <= k < o
    assert which in (0, 1)

    p = pair_idx(r, c, v=v)
    return (2 * o) * p + 2 * k + which


def call_idx_masked(r, c, k, which, share, v=78, o=8, n_shares=3):
    assert 0 <= share < n_shares

    return (
        share * calls_per_share(v=v, o=o)
        + call_idx_unmasked(r, c, k, which, v=v, o=o)
    )


def oil_segment_indices_for_share(z, k, share, v=78, o=8, n_shares=3):
    assert 0 <= z < v
    assert 0 <= k < o
    assert 0 <= share < n_shares

    result = []

    for r in range(z):
        result.append(
            call_idx_masked(
                r, z, k,
                which=0,
                share=share,
                v=v,
                o=o,
                n_shares=n_shares,
            )
        )

    for c in range(z + 1, v):
        result.append(
            call_idx_masked(
                z, c, k,
                which=1,
                share=share,
                v=v,
                o=o,
                n_shares=n_shares,
            )
        )

    return sorted(result)


def build_segment_records(full_trace, peaks):
    peaks = np.asarray(peaks, dtype=int)
    peaks = np.sort(peaks)

    records = []

    for i in range(len(peaks) - 1):
        s = int(peaks[i])
        e = int(peaks[i + 1])

        records.append({
            "global_idx": i,
            "start": s,
            "end": e,
            "length": e - s,
            "segment": np.asarray(full_trace[s:e], dtype=float),
        })

    if len(peaks) > 0 and peaks[-1] < len(full_trace):
        s = int(peaks[-1])
        e = int(len(full_trace))

        records.append({
            "global_idx": len(records),
            "start": s,
            "end": e,
            "length": e - s,
            "segment": np.asarray(full_trace[s:e], dtype=float),
        })

    return records

def merge_oil_segments_for_share(oil_records,
                                 allowed_lengths=None,
                                 target_len=None):
    selected = []

    for rec in oil_records:
        L = rec["length"]

        if allowed_lengths is not None and L not in allowed_lengths:
            continue

        selected.append(np.asarray(rec["segment"], dtype=float))

    if len(selected) == 0:
        return np.empty((0, 0), dtype=float)

    if target_len is None:
        target_len = min(len(seg) for seg in selected)

    merged = []

    for seg in selected:
        if len(seg) >= target_len:
            merged.append(seg[:target_len])

    if len(merged) == 0:
        return np.empty((0, 0), dtype=float)

    return np.asarray(merged, dtype=float)


def build_segment_records(full_trace, peaks):
    peaks = np.asarray(peaks, dtype=int)
    peaks = np.sort(peaks)

    records = []

    for i in range(len(peaks) - 1):
        s = int(peaks[i])
        e = int(peaks[i + 1])

        records.append({
            "global_idx": i,
            "start": s,
            "end": e,
            "length": e - s,
            "segment": np.asarray(full_trace[s:e], dtype=float),
        })

    if len(peaks) > 0 and peaks[-1] < len(full_trace):
        s = int(peaks[-1])
        e = int(len(full_trace))

        records.append({
            "global_idx": len(records),
            "start": s,
            "end": e,
            "length": e - s,
            "segment": np.asarray(full_trace[s:e], dtype=float),
        })

    return records


def get_oil_segment_records_for_share(segment_records, z, k, share,
                                      v=78, o=8, n_shares=3):

    idxs = oil_segment_indices_for_share(
        z=z,
        k=k,
        share=share,
        v=v,
        o=o,
        n_shares=n_shares,
    )

    record_map = {rec["global_idx"]: rec for rec in segment_records}

    selected = []
    for idx in idxs:
        if idx in record_map:
            selected.append(record_map[idx].copy())

    return selected
    

# Precalculations
def upper_tri_bs_index(r, c, v):
    assert 0 <= r <= c < v
    bs = 0
    for rr in range(r):
        bs += v - rr
    bs += (c - r)
    return bs

def get_P1_block(attack_P1, r, c, limbs, v):
    bs = upper_tri_bs_index(r, c, v)
    return extract_p1_entry_by_bs(attack_P1, limbs, bs)

def extract_p1_entry_by_bs(attack_P1, limbs, bs):
    start = bs * limbs
    end = start + limbs
    return np.array(attack_P1[start:end], dtype=np.uint64)


def get_entries_for_coeff_share(acc_state_share, attack_P1,
                                v, o, limbs, z, k0):

    dest_rows = []
    p1_blocks = []
    acc_blocks = []

    for row in range(v):
        if row == z:
            continue

        r0 = min(z, row)
        c0 = max(z, row)

        p1_block = get_P1_block(attack_P1, r0, c0, limbs, v)
        acc_block = acc_state_share[row, k0, :].copy()

        dest_rows.append(row)
        p1_blocks.append(p1_block)
        acc_blocks.append(acc_block)

    P1_entries = np.array(p1_blocks, dtype=np.uint64)
    acc_init = np.array(acc_blocks, dtype=np.uint64)

    return dest_rows, P1_entries, acc_init

# CPA modeling
def hw(x):
    return bin(int(x)).count("1")

LSB_MASK = 0x1111111111111111

def mul_table(b):
    b = int(b) & 0x0F
    x = (b * 0x08040201) & 0xffffffff
    high = x & 0xf0f0f0f0
    return (x ^ (high >> 4) ^ (high >> 3)) & 0xffffffff


def m_vec_mul_add_model(in_vec, a, acc):
    in_vec = np.array(in_vec, dtype=np.uint64)
    acc = np.array(acc, dtype=np.uint64).copy()
    tab = mul_table(a)

    for i, val in enumerate(in_vec):
        val = int(val)

        v = (
            ((val & LSB_MASK) * (tab & 0xff))
            ^ (((val >> 1) & LSB_MASK) * ((tab >> 8) & 0xf))
            ^ (((val >> 2) & LSB_MASK) * ((tab >> 16) & 0xf))
            ^ (((val >> 3) & LSB_MASK) * ((tab >> 24) & 0xf))
        ) & 0xffffffffffffffff

        acc[i] ^= np.uint64(v)

    return acc

def build_hypothesis_hw(P1_entries, acc_init, limbs=5):
    NUM_KEYS = 16
    N = P1_entries.shape[0]   

    hyp = np.zeros((NUM_KEYS, N * limbs), dtype=float)

    for k in range(NUM_KEYS):
        idx = 0
        for i in range(N):
            p1 = np.array(P1_entries[i], dtype=np.uint64)
            acc_before = np.array(acc_init[i], dtype=np.uint64)

            acc_after = m_vec_mul_add_model(p1, k, acc_before.copy())

            for limb in range(limbs):
                hyp[k, idx] = hw(int(acc_after[limb]) & 0xffffffff)
                idx += 1

    return hyp

def normalize_limbs(x, limbs):
    x = np.array(x, dtype=float).copy()
    for i in range(limbs):
        xi = x[i::limbs]
        xi -= np.mean(xi)
        s = np.std(xi)
        if s > 1e-12:
            xi /= s
        x[i::limbs] = xi
    return x

def update_acc_for_coeff(acc_state, guessed_coeff, z, k0, attack_P1, v, limbs):
    for c in range(v):
        if c == z:
            continue

        r0 = min(z, c)
        c0 = max(z, c)
        p1_block = get_P1_block(attack_P1, r0, c0, limbs, v)

        acc_before = acc_state[c, k0, :].copy()
        acc_after = m_vec_mul_add_model(p1_block, guessed_coeff, acc_before)
        acc_state[c, k0, :] = acc_after

    return acc_state

# Visualize

def plot_abs_share_correlations_n(corrs, guesses):
    keys = np.arange(16)

    guess_oil = 0
    for g in guesses:
        guess_oil ^= int(g)

    plt.figure(figsize=(9, 4))
    colors = ["mediumseagreen", "steelblue", "orange"]

    for s, corr in enumerate(corrs):
        plt.plot(
            keys,
            np.abs(corr),
            color=colors[s % len(colors)],
            label=f"s{s}: {guesses[s]}"
        )

        plt.scatter(
            [guesses[s]],
            [abs(corr[guesses[s]])],
            s=70,
            color=colors[s % len(colors)],
        )

    plt.axvline(
        guess_oil,
        linestyle="--",
        color="crimson",
        linewidth=2,
        label=f"comb: {guess_oil}"
    )

    plt.xlabel("Share guess / combined oil")
    plt.ylabel("|Corr|")
    plt.grid(True, alpha=0.25)
    plt.legend()
    plt.show()

def check_oil_vector(recovered_vector, true_O, column, o=8):
    true_vector = np.array(true_O, dtype=np.int64).reshape(-1, o)[:, column]
    recovered_vector = np.array(recovered_vector, dtype=np.int64)
    true_vector = true_vector[:len(recovered_vector)]

    print("Recovered oil vector:")
    print(recovered_vector)
    print("True oil vector:")
    print(true_vector)

    ok = np.all(recovered_vector == true_vector)
    num_correct = np.sum(recovered_vector == true_vector)
    total = len(recovered_vector)

    if ok:
        display(HTML(f"""
        <div class="alert alert-success" style="background-color: #eefaf1; color: #14532d; border-color: #bbf7d0;">
            <strong>Match:</strong> Oil vector recovered correctly ({num_correct}/{total} correct).
        </div>
        """))
    else:
        display(HTML(f"""
        <div class="alert alert-danger" style="background-color: #fff1f2; color: #7f1d1d; border-color: #fecdd3;">
            <strong>Mismatch:</strong> Oil vector recovery failed ({num_correct}/{total} correct).
        </div>
        """))


def check_oil_matrix(recovered_O_matrix, true_O, v, o):
    true_O_matrix = np.array(true_O, dtype=np.int64).reshape(v, o)
    recovered_O_matrix = np.array(recovered_O_matrix, dtype=np.int64)

    ok = np.all(recovered_O_matrix == true_O_matrix)
    num_correct = np.sum(recovered_O_matrix == true_O_matrix)
    total = v * o

    if ok:
        display(HTML(f"""
        <div class="alert alert-success" style="background-color: #eefaf1; color: #14532d; border-color: #bbf7d0;">
            <strong>Match:</strong> Full oil matrix recovered correctly ({num_correct}/{total} correct).
        </div>
        """))
    else:
        display(HTML(f"""
        <div class="alert alert-danger" style="background-color: #fff1f2; color: #7f1d1d; border-color: #fecdd3;">
            <strong>Mismatch:</strong> Oil matrix recovery failed ({num_correct}/{total} correct).
        </div>
        """))


# CPA on one coeff
def run_cpa(power, hyp, limbs):
    power = normalize_limbs(power, limbs)
    corr = np.zeros(hyp.shape[0])

    for k in range(hyp.shape[0]):
        h = normalize_limbs(hyp[k], limbs)
        if np.std(power) < 1e-12 or np.std(h) < 1e-12:
            continue
        c = np.corrcoef(power, h)[0, 1]
        if not np.isnan(c):
            corr[k] = c
    return corr


def recover_oil_share(
    segment_records,
    acc_state_share,
    attack_P1,
    v,
    o,
    limbs,
    z,
    k,
    share,
    poi_list,
    target_len=308,
    n_shares=2,
    allowed_lengths = None,
):

    if len(poi_list) != limbs:
        raise ValueError(f"poi_list must have length {limbs}")

    oil_records = get_oil_segment_records_for_share(
        segment_records=segment_records,
        z=z,
        k=k,
        share=share,
        v=v,
        o=o,
        n_shares=n_shares,
    )

    segments = merge_oil_segments_for_share(
        oil_records,
        allowed_lengths=allowed_lengths,
        target_len=target_len,
    )

    segments = np.asarray(segments, dtype=float)

    if len(segments) == 0:
        raise ValueError(f"No segments found for z={z}, k={k}, share={share}")
        

    _, p1, acc_init = get_entries_for_coeff_share(
        acc_state_share=acc_state_share,
        attack_P1=attack_P1,
        v=v,
        o=o,
        limbs=limbs,
        z=z,
        k0=k,
    )

    if len(segments) != len(p1) or len(segments) != len(acc_init):
        raise ValueError(
            f"Mismatch: segments={len(segments)}, "
            f"p1={len(p1)}, acc_init={len(acc_init)}"
        )

    hyp = build_hypothesis_hw(
        P1_entries=p1,
        acc_init=acc_init,
        limbs=limbs,
    )

    power_parts = []
    poi_lists = []

    for limb in range(limbs):
        poi = int(poi_list[limb])

        if poi < 0 or poi >= segments.shape[1]:
            raise ValueError(
                f"POI {poi} for limb {limb} outside "
                f"segmentlength {segments.shape[1]}")

        poi_lists.append(np.array([poi], dtype=np.int64))

        p = segments[:, poi]
        p = (p - np.mean(p)) / (np.std(p) + 1e-12)

        power_parts.append(p)

    N = len(power_parts[0])
    power = np.empty(limbs * N, dtype=float)

    for limb in range(limbs):
        power[limb::limbs] = power_parts[limb]

    corr = run_cpa(power, hyp, limbs)
    guess = int(np.argmax(np.abs(corr)))

    debug = {
        "share": share,
        "z": z,
        "k": k,
        "poi_list": poi_list,
        "segments_shape": segments.shape,
        "hyp_shape": hyp.shape,
        "power_shape": power.shape,
        "allowed_lengths": allowed_lengths,
    }

    return guess, corr, poi_lists, power, hyp, debug

def recover_oil_from_n_shares(
    segment_records,
    acc_states,
    attack_P1,
    v,
    o,
    limbs,
    z,
    k,
    poi_lists,
    target_len=308,
    n_shares=None,
    allowed_lengths=None,
):

    guesses = []
    corrs = []
    debugs = []

    for share in range(n_shares):
        guess, corr, poi_lists_out, power, hyp, debug = recover_oil_share(
            segment_records=segment_records,
            acc_state_share=acc_states[share],
            attack_P1=attack_P1,
            v=v,
            o=o,
            limbs=limbs,
            z=z,
            k=k,
            share=share,
            poi_list=poi_lists[share],
            target_len=target_len,
            n_shares=n_shares,
            allowed_lengths=allowed_lengths,
        )

        guesses.append(int(guess))
        corrs.append(corr)
        debugs.append(debug)

    guess_oil = 0
    for g in guesses:
        guess_oil ^= g

    debug = {
        "guesses": guesses,
        "guess_oil": guess_oil,
        "corrs": corrs,
        "debugs": debugs,
        "n_shares": n_shares,
        "z": z,
        "k": k,
    }

    return guess_oil, guesses, corrs, debug
