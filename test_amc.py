import unittest
import ctypes
import numpy as np
import os
import sys
import argparse
import time

# Load library with error handling
try:
    lib = ctypes.CDLL('./ber.so')
except OSError as e:
    print(f"Error loading ber.so: {e}")
    print("Make sure to run 'make shared' first to build the shared library")
    sys.exit(1)
lib.compute_ber.argtypes = [ctypes.c_int, ctypes.c_double, ctypes.c_longlong]
lib.compute_ber.restype = ctypes.c_double
lib.estimate_snr.argtypes = [ctypes.c_double, ctypes.c_longlong]
lib.estimate_snr.restype = ctypes.c_double

# Test function signatures (corrected)
lib.run_mod_demod_test.argtypes = [ctypes.c_char_p]
lib.run_mod_demod_test.restype = ctypes.c_int

lib.run_ber_edge_test.argtypes = [ctypes.c_char_p]
lib.run_ber_edge_test.restype = ctypes.c_int

lib.run_ber_accuracy_test.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double), ctypes.c_char_p]
lib.run_ber_accuracy_test.restype = ctypes.c_int

lib.run_snr_estimation_test.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double), ctypes.c_char_p]
lib.run_snr_estimation_test.restype = ctypes.c_int

lib.run_all_tests.argtypes = [ctypes.c_char_p]
lib.run_all_tests.restype = ctypes.c_int

class TestAMC(unittest.TestCase):
    def setUp(self):
        self.err_buf = ctypes.create_string_buffer(256)
        self.avg_ber = ctypes.c_double()
        self.theor_ber = ctypes.c_double()
        self.avg_est = ctypes.c_double()
        self.std_est = ctypes.c_double()

    def test_mod_demod(self):
        err_msg = ctypes.create_string_buffer(256)
        result = lib.run_mod_demod_test(err_msg)
        self.assertEqual(result, 0, f"Mod/Demod failed: {err_msg.value.decode()}")
        print(f"Mod/Demod: {err_msg.value.decode()}")

    def test_ber_edge(self):
        err_msg = ctypes.create_string_buffer(256)
        result = lib.run_ber_edge_test(err_msg)
        self.assertEqual(result, 0, f"BER edge failed: {err_msg.value.decode()}")
        print(f"BER Edge: {err_msg.value.decode()}")

    def test_ber_accuracy(self):
        """Test BER computation accuracy against theoretical values"""
        err_msg = ctypes.create_string_buffer(256)
        result = lib.run_ber_accuracy_test(ctypes.byref(self.avg_ber), ctypes.byref(self.theor_ber), err_msg)
        self.assertEqual(result, 0, f"BER accuracy failed: {err_msg.value.decode()}")
        print(f"BER Accuracy: sim={self.avg_ber.value:.2e}, theor={self.theor_ber.value:.2e}")
        
        # Additional validation
        self.assertGreater(self.avg_ber.value, 0, "Simulated BER should be positive")
        self.assertGreater(self.theor_ber.value, 0, "Theoretical BER should be positive")
        self.assertLess(abs(self.avg_ber.value - self.theor_ber.value) / self.theor_ber.value, 0.2, 
                       "Simulated and theoretical BER should be within 20%")

    def test_snr_estimation(self):
        """Test SNR estimation functionality"""
        err_msg = ctypes.create_string_buffer(256)
        result = lib.run_snr_estimation_test(ctypes.byref(self.avg_est), ctypes.byref(self.std_est), err_msg)
        self.assertEqual(result, 0, f"SNR est failed: {err_msg.value.decode()}")
        print(f"SNR Est: avg={self.avg_est.value:.2f} dB, std={self.std_est.value:.2f} dB")
        
        # Additional validation
        self.assertGreater(self.std_est.value, 0, "Standard deviation should be positive")
        self.assertLess(self.std_est.value, 2.0, "Standard deviation should be reasonable (<2dB)")

    def test_all(self):
        """Test the comprehensive test suite"""
        err_msg = ctypes.create_string_buffer(256)
        result = lib.run_all_tests(err_msg)
        self.assertEqual(result, 0, f"All tests failed: {err_msg.value.decode()}")
        print(f"Overall: {err_msg.value.decode()}")

    def test_compute_ber_basic(self):
        """Test basic BER computation functionality"""
        # Test BPSK at moderate SNR
        ber = lib.compute_ber(2, 6.0, 100000)
        self.assertGreaterEqual(ber, 0, "BER should be non-negative")
        self.assertLessEqual(ber, 0.5, "BER should not exceed 0.5")
        print(f"BPSK BER at 6dB: {ber:.4e}")
        
        # Test QPSK
        ber_qpsk = lib.compute_ber(4, 6.0, 100000)
        self.assertGreaterEqual(ber_qpsk, 0, "QPSK BER should be non-negative")
        print(f"QPSK BER at 6dB: {ber_qpsk:.4e}")
        
        # Test 16QAM
        ber_16qam = lib.compute_ber(16, 6.0, 100000)
        self.assertGreaterEqual(ber_16qam, 0, "16QAM BER should be non-negative")
        self.assertGreater(ber_16qam, ber_qpsk, "16QAM should have higher BER than QPSK at same SNR")
        print(f"16QAM BER at 6dB: {ber_16qam:.4e}")

    def test_compute_ber_validation(self):
        """Test BER computation input validation"""
        # Invalid modulation order
        ber = lib.compute_ber(3, 10.0, 1000)
        self.assertEqual(ber, -1.0, "Invalid modulation order should return -1")
        
        # Negative bit count
        ber = lib.compute_ber(2, 10.0, -100)
        self.assertEqual(ber, 0.0, "Negative bit count should return 0")
        
        # Out of range SNR
        ber = lib.compute_ber(2, -100.0, 1000)
        self.assertEqual(ber, -1.0, "Out-of-range SNR should return -1")
        
        print("Input validation tests passed")

    def test_estimate_snr_basic(self):
        """Test basic SNR estimation functionality"""
        # Test at known SNR
        true_snr = 8.0
        est_snr = lib.estimate_snr(true_snr, 500)
        self.assertNotEqual(est_snr, -999.0, "SNR estimation should not fail")
        
        error = abs(est_snr - true_snr)
        self.assertLess(error, 3.0, "SNR estimation error should be < 3dB")
        print(f"SNR estimation: true={true_snr}dB, est={est_snr:.2f}dB, error={error:.2f}dB")

    def test_estimate_snr_validation(self):
        """Test SNR estimation input validation"""
        # Invalid pilot count
        est_snr = lib.estimate_snr(10.0, -50)
        self.assertEqual(est_snr, -999.0, "Invalid pilot count should return -999")
        
        # Out of range SNR
        est_snr = lib.estimate_snr(-100.0, 100)
        self.assertEqual(est_snr, -999.0, "Out-of-range SNR should return -999")
        
        print("SNR estimation validation tests passed")

    def test_ber_monotonicity(self):
        """Test that BER decreases with increasing SNR"""
        snrs = [2, 4, 6, 8]
        bers = []
        
        for snr in snrs:
            ber = lib.compute_ber(2, snr, 50000)
            bers.append(ber)
            self.assertGreaterEqual(ber, 0, f"BER at {snr}dB should be non-negative")
        
        # Check monotonicity (allowing for some statistical variation)
        for i in range(1, len(bers)):
            if bers[i] > 0 and bers[i-1] > 0:  # Skip zero BER cases
                ratio = bers[i] / bers[i-1]
                self.assertLess(ratio, 2.0, f"BER should generally decrease with SNR: {bers[i-1]:.2e} -> {bers[i]:.2e}")
        
        print(f"BER monotonicity test: {[f'{b:.2e}' for b in bers]}")

    def test_edge_max_bits_limit(self):
        """Ensure overly large bit requests are rejected ( > 100,000,000 )"""
        # Using a value just above the enforced cap
        ber = lib.compute_ber(2, 5.0, 150_000_000)
        self.assertEqual(ber, -1.0, "Should reject excessive number of bits")

    def test_edge_zero_bits(self):
        """Zero bits should return BER=0 and not error"""
        ber = lib.compute_ber(2, 5.0, 0)
        self.assertEqual(ber, 0.0, "Zero bits should yield BER 0.0")

    def test_edge_high_snr_boundary(self):
        """Highest allowed SNR (50 dB) should return valid small BER"""
        ber = lib.compute_ber(2, 50.0, 200000)
        self.assertTrue(ber >= 0.0, "High SNR boundary should not error")

    def test_edge_invalid_high_snr(self):
        """SNR above allowed range should return error (-1)"""
        ber = lib.compute_ber(2, 51.0, 10000)
        self.assertEqual(ber, -1.0, "Out of range SNR should be rejected")

    def test_bits_alignment_truncation(self):
        """If num_bits not divisible by bits/symbol, it should truncate quietly"""
        # For QPSK (2 bits/sym), provide odd number of bits
        full = 10001
        ber = lib.compute_ber(4, 6.0, full)
        # Should still compute (not -1). Can't assert value but ensure non-negative
        self.assertGreaterEqual(ber, 0.0)

    def test_pilot_count_effect_on_snr_est(self):
        """Higher pilot counts should reduce std deviation of SNR estimate statistically"""
        # Perform multiple estimates with low vs higher pilot counts
        import statistics
        low_pilots = 50
        high_pilots = 800
        runs = 8
        low_estimates = [lib.estimate_snr(8.0, low_pilots) for _ in range(runs)]
        high_estimates = [lib.estimate_snr(8.0, high_pilots) for _ in range(runs)]
        # Filter out any error sentinel values
        low_estimates = [e for e in low_estimates if e > -500]
        high_estimates = [e for e in high_estimates if e > -500]
        if len(low_estimates) > 1 and len(high_estimates) > 1:
            low_std = statistics.pstdev(low_estimates)
            high_std = statistics.pstdev(high_estimates)
            self.assertLessEqual(high_std, low_std * 1.2, "Higher pilot count should not increase variance markedly")

    def test_modulation_parity_similarity(self):
        """BPSK and QPSK BER should be close at same Eb/N0"""
        snrs = [4.0, 6.0, 8.0]
        for snr in snrs:
            ber_bpsk = lib.compute_ber(2, snr, 120000)
            ber_qpsk = lib.compute_ber(4, snr, 120000)
            if ber_bpsk > 0 and ber_qpsk > 0:
                ratio = max(ber_bpsk, ber_qpsk) / min(ber_bpsk, ber_qpsk)
                self.assertLess(ratio, 2.5, f"BPSK vs QPSK BER diverged too much at {snr} dB (ratio={ratio:.2f})")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='AMC BER tests / benchmark')
    parser.add_argument('--bench', action='store_true', help='Run benchmark instead of unit tests')
    parser.add_argument('--bench-snr', type=float, default=6.0, help='SNR (dB) for benchmark BPSK run')
    parser.add_argument('--bench-mod', type=int, default=2, help='Modulation order to benchmark (2,4,16)')
    parser.add_argument('--bench-sizes', type=str, default='5000,20000,80000,320000', help='Comma list of bit sizes')
    args, remaining = parser.parse_known_args()

    if args.bench:
        if args.bench_mod not in (2,4,16):
            print('Invalid modulation order for benchmark. Use 2,4, or 16.')
            raise SystemExit(1)
        sizes = [int(s) for s in args.bench_sizes.split(',') if s.strip()]
        print('AMC BER Benchmark')
        print('=================')
        print(f'Modulation: {args.bench_mod}, SNR={args.bench_snr} dB')
        header = f"{'bits':>10}  {'ber':>10}  {'time(ms)':>9}  {'throughput(Mbit/s)':>19}  scale"
        print(header)  
        print('-'*len(header))
        last = None
        for n in sizes:
            t0 = time.perf_counter()
            ber = lib.compute_ber(args.bench_mod, args.bench_snr, n)
            dt_ms = (time.perf_counter() - t0) * 1000.0
            rate = n / (dt_ms/1000.0) / 1e6
            scale = f"x{n/last:.1f}" if last else '--'
            print(f"{n:10d}  {ber:10.2e}  {dt_ms:9.2f}  {rate:19.2f}  {scale}")
            last = n
        raise SystemExit(0)

    print("AMC BER Simulation - Python Unit Tests")
    print("======================================")
    print("Testing C++ library integration via ctypes")
    print()
    # Pass remaining args to unittest (like -k pattern)
    sys.argv = [sys.argv[0]] + remaining
    unittest.main(verbosity=2)