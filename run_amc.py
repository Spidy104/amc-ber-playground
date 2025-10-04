import ctypes
import argparse
import math
import csv
import sys
import time
import os
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
import scipy.special as sp

# Load C++ library
lib = ctypes.CDLL('./ber.so')
lib.compute_ber.argtypes = [ctypes.c_int, ctypes.c_double, ctypes.c_longlong]
lib.compute_ber.restype = ctypes.c_double
# Seeded version (may not exist in older builds)
_seeded_func = getattr(lib, 'compute_ber_seeded', None)
if _seeded_func is not None:
    _seeded_func.argtypes = [ctypes.c_int, ctypes.c_double, ctypes.c_longlong, ctypes.c_ulonglong]
    _seeded_func.restype = ctypes.c_double
    HAS_SEEDED = True
else:
    HAS_SEEDED = False

lib.estimate_snr.argtypes = [ctypes.c_double, ctypes.c_longlong]
lib.estimate_snr.restype = ctypes.c_double

# Theoretical helpers
def qfunc(x):
    return 0.5 * sp.erfc(x / np.sqrt(2))

def theor_ber_bpsk_qpsk(ebno_db):
    ebno_lin = 10 ** (ebno_db / 10)
    return qfunc(np.sqrt(2 * ebno_lin))

def theor_ber_16qam(ebno_db):
    ebno_lin = 10 ** (ebno_db / 10)
    sqrt_term = np.sqrt(2 * ebno_lin / 5)
    return (1/4) * (3 * qfunc(sqrt_term) + qfunc(3 * sqrt_term))

THEOR_FUN = {2: theor_ber_bpsk_qpsk, 4: theor_ber_bpsk_qpsk, 16: theor_ber_16qam}

# Simulation wrappers
def simulate_ber(mod, snr_db, bits, runs=1, seed=None):
    if seed is not None and HAS_SEEDED:
        # Derive per-run seeds for reproducibility but variation
        base = seed & 0xFFFFFFFFFFFFFFFF
        vals = []
        for r in range(runs):
            s = (base + r * 0x9E3779B185EBCA87) & 0xFFFFFFFFFFFFFFFF
            vals.append(lib.compute_ber_seeded(mod, snr_db, bits, ctypes.c_ulonglong(s)))
        return float(np.mean(vals))
    else:
        return float(np.mean([lib.compute_ber(mod, snr_db, bits) for _ in range(runs)]))

def simulate_snr(true_snr_db, pilots, runs=5):
    return float(np.mean([lib.estimate_snr(true_snr_db, pilots) for _ in range(runs)]))

# Adaptive threshold finder
def find_min_snr_for_ber(mod, target_ber, bits, runs=2, low=0.0, high=30.0, tol=0.1, seed=None):
    while high - low > tol:
        mid = 0.5 * (low + high)
        ber = simulate_ber(mod, mid, bits, runs=runs, seed=seed)
        if ber <= target_ber:
            high = mid
        else:
            low = mid
    return high

# AMC decision logic
def choose_mod(est_snr_db, thresh_qpsk, thresh_16qam):
    if est_snr_db < thresh_qpsk:
        return 'NONE'
    elif est_snr_db < thresh_16qam:
        return 'QPSK'
    else:
        return '16QAM'

# Plotting
def plot_ber(snrs, sim_map, mods, show_theor=True, save=None, num_bits=None):
    plt.figure(figsize=(9, 5.5))
    for m in mods:
        label = {2: 'BPSK', 4: 'QPSK', 16: '16QAM'}[m]
        plt.semilogy(snrs, sim_map[m], '--', label=f'{label} (Sim)')
        if show_theor:
            theor = THEOR_FUN[m](snrs)
            plt.semilogy(snrs, theor, label=f'{label} (Theor)')
    plt.xlabel('Eb/N0 (dB)')
    plt.ylabel('BER')
    
    # Add title with simulation parameters
    if num_bits is not None:
        plt.title(f'BER vs Eb/N0\n(Simulated with {num_bits:,} bits per SNR point)')
    else:
        plt.title('BER vs Eb/N0')
    
    # Add grid lines for easier reading
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.ylim(1e-8, 1)
    
    # Make legend larger and place outside if needed
    plt.legend(loc='upper right', bbox_to_anchor=(1.1, 1.0))
    if save:
        plt.savefig(save, dpi=140, bbox_inches='tight')
    else:
        plt.show()


def plot_snr_est(snrs, est_snrs, pilots, save=None):
    plt.figure(figsize=(6.5, 4.2))
    plt.plot(snrs, snrs, '--', label='True SNR')
    plt.plot(snrs, est_snrs, label='Estimated SNR')
    plt.xlabel('True Eb/N0 (dB)')
    plt.ylabel('Estimated Eb/N0 (dB)')
    plt.title(f'SNR Estimation\n(Using {pilots:,} pilot symbols)')
    
    # Add grid lines for easier reading
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    
    # Improve legend positioning
    plt.legend(loc='upper left')
    if save:
        plt.savefig(save, dpi=140, bbox_inches='tight')
    else:
        plt.show()

# CSV output
def write_csv(path, snrs, mods, sim_map):
    path = Path(path)
    with path.open('w', newline='') as f:
        w = csv.writer(f)
        header = ['SNR_dB'] + [f'BER_mod{m}' for m in mods]
        w.writerow(header)
        for i, snr in enumerate(snrs):
            row = [snr] + [sim_map[m][i] for m in mods]
            w.writerow(row)

# Main CLI
def main():
    parser = argparse.ArgumentParser(description='Adaptive Modulation BER/SNR Simulation CLI')
    parser.add_argument('--mods', type=str, default='2,4,16', help='Comma list of modulation orders (2,4,16)')
    parser.add_argument('--snr-start', type=float, default=0.0)
    parser.add_argument('--snr-stop', type=float, default=20.0)
    parser.add_argument('--snr-step', type=float, default=1.0)
    parser.add_argument('--bits', type=int, default=500000, help='Number of bits per simulation')
    parser.add_argument('--runs', type=int, default=2, help='Monte Carlo runs per SNR point')
    parser.add_argument('--pilots', type=int, default=200, help='Pilot symbols for SNR estimation')
    parser.add_argument('--find-thresholds', action='store_true', help='Estimate AMC thresholds for QPSK/16QAM @ target BER')
    parser.add_argument('--target-ber', type=float, default=1e-5)
    parser.add_argument('--thresh-bits', type=int, default=1_000_000)
    parser.add_argument('--seed', type=int, default=None, help='Deterministic seed (enables seeded BER if available)')
    parser.add_argument('--csv', type=str, default=None, help='Write BER results to CSV file')
    parser.add_argument('--no-plot', action='store_true', help='Disable plotting')
    parser.add_argument('--save-prefix', type=str, default=None, help='Prefix to save plot images instead of showing')
    parser.add_argument('--quiet', action='store_true')

    args = parser.parse_args()

    mods = [int(x) for x in args.mods.split(',') if x.strip()]
    for m in mods:
        if m not in (2,4,16):
            print(f"Error: Unsupported modulation order: {m}. Supported: 2, 4, 16")
            return 2
    
    # Validate parameters
    if args.bits <= 0:
        print("Error: Number of bits must be positive")
        return 2
    if args.runs <= 0:
        print("Error: Number of runs must be positive") 
        return 2
    if args.snr_stop < args.snr_start:
        print("Error: SNR stop must be >= SNR start")
        return 2
    if args.snr_step <= 0:
        print("Error: SNR step must be positive")
        return 2

    snrs = np.arange(args.snr_start, args.snr_stop + 1e-9, args.snr_step)
    sim_map = {m: [] for m in mods}

    t0 = time.time()
    for m in mods:
        for snr in snrs:
            ber = simulate_ber(m, float(snr), args.bits, runs=args.runs, seed=args.seed)
            # Handle zero BER gracefully for log plots - use actual zero or very small value
            if ber == 0.0:
                # For zero BER, estimate theoretical limit based on bit count
                min_ber = 0.5 / args.bits  # Minimum detectable BER
                sim_map[m].append(min_ber)
            else:
                sim_map[m].append(ber)
        if not args.quiet:
            print(f"Completed modulation {m}")
    elapsed = time.time() - t0

    if args.csv:
        try:
            write_csv(args.csv, snrs, mods, sim_map)
            if not args.quiet:
                print(f"Wrote CSV: {args.csv}")
        except Exception as e:
            print(f"Error writing CSV file: {e}")
            return 3

    # SNR estimation curve
    est_snrs = [simulate_snr(float(s), args.pilots) for s in snrs]

    # Threshold finding
    thresh_qpsk = thresh_16qam = None
    if args.find_thresholds and 4 in mods and 16 in mods:
        if not args.quiet:
            print("Finding AMC thresholds...")
        thresh_qpsk = find_min_snr_for_ber(4, args.target_ber, args.thresh_bits, seed=args.seed)
        thresh_16qam = find_min_snr_for_ber(16, args.target_ber, args.thresh_bits, seed=args.seed)
        if not args.quiet:
            print(f"Threshold QPSK: {thresh_qpsk:.2f} dB | 16QAM: {thresh_16qam:.2f} dB")

    # Decide modulation for a few sample estimated SNRs (demonstration)
    if thresh_qpsk is not None and thresh_16qam is not None:
        for true_snr in [5,10,15,20]:
            est = simulate_snr(true_snr, args.pilots)
            mod_choice = choose_mod(est, thresh_qpsk, thresh_16qam)
            if not args.quiet:
                print(f"True SNR {true_snr:.1f} dB -> est {est:.2f} dB => {mod_choice}")

    if not args.no_plot:
        try:
            save_prefix = args.save_prefix
            if save_prefix:
                plot_ber(snrs, sim_map, mods, save=f"{save_prefix}_ber.png", num_bits=args.bits)
                plot_snr_est(snrs, est_snrs, args.pilots, save=f"{save_prefix}_snr.png")
            else:
                plot_ber(snrs, sim_map, mods, num_bits=args.bits)
                plot_snr_est(snrs, est_snrs, args.pilots)
        except Exception as e:
            print(f"Warning: Plotting failed: {e}")
            print("Continuing without plots...")

    if not args.quiet:
        print(f"Completed in {elapsed:.2f} s")

    return 0

if __name__ == '__main__':
    sys.exit(main())