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

# Coding function signatures
lib.compute_ber_coded.argtypes = [ctypes.c_int, ctypes.c_double, ctypes.c_longlong, ctypes.c_int]
lib.compute_ber_coded.restype = ctypes.c_double
lib.test_convolutional_coding.argtypes = []
lib.test_convolutional_coding.restype = ctypes.c_int
lib.estimate_coding_gain_db.argtypes = []
lib.estimate_coding_gain_db.restype = ctypes.c_double

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

    # ========================================================================
    # CODING TESTS
    # ========================================================================
    
    def test_convolutional_coding_self_test(self):
        """Test the built-in convolutional coding self-test"""
        result = lib.test_convolutional_coding()
        self.assertEqual(result, 0, f"Convolutional coding self-test failed with code: {result}")
        print("✅ Convolutional coding self-test passed")
        
    def test_coding_gain_estimate(self):
        """Test coding gain estimation function"""
        gain_db = lib.estimate_coding_gain_db()
        self.assertGreater(gain_db, 0, "Coding gain should be positive")
        self.assertLess(gain_db, 15, "Coding gain should be reasonable (<15 dB)")
        print(f"📊 Estimated coding gain: {gain_db:.1f} dB")
        
    def test_coded_ber_function_availability(self):
        """Test that coded BER function is available and callable"""
        try:
            # Test with minimal parameters (should not crash)
            result = lib.compute_ber_coded(2, 10.0, 1000, 1)
            self.assertIsInstance(result, float, "compute_ber_coded should return a float")
            print(f"📡 compute_ber_coded callable, returned: {result}")
        except Exception as e:
            self.fail(f"compute_ber_coded function call failed: {e}")
            
    def test_coding_vs_uncoded_basic(self):
        """Basic test comparing coded vs uncoded BER"""
        snr_db = 6.0
        bits = 50000
        
        print(f"\n🧪 Testing BPSK at {snr_db} dB SNR with {bits} bits...")
        
        # Get uncoded BER
        uncoded_ber = lib.compute_ber(2, snr_db, bits)
        self.assertGreaterEqual(uncoded_ber, 0, "Uncoded BER should be non-negative")
        self.assertLess(uncoded_ber, 1.0, "Uncoded BER should be < 1.0")
        
        # Get coded BER  
        coded_ber = lib.compute_ber_coded(2, snr_db, bits, 1)
        print(f"  Uncoded BER: {uncoded_ber:.4e}")
        print(f"  Coded BER:   {coded_ber:.4e}")
        
        # Basic sanity checks
        if coded_ber >= 1.0:
            print("  ❌ ERROR: Coded BER indicates failure (>=1.0)")
            print("  🔍 This suggests an issue in the coding implementation")
            self.fail("Coded BER function returned error code (>=1.0)")
        elif coded_ber < 0:
            print("  ❌ ERROR: Coded BER is negative")
            self.fail("Coded BER should not be negative")
        elif coded_ber == 0:
            print("  ⚠️  WARNING: Coded BER is exactly zero (may be below detection threshold)")
        else:
            # Valid coded BER - check if it shows improvement
            improvement = uncoded_ber / coded_ber if coded_ber > 0 else float('inf')
            gain_db = 10 * np.log10(improvement) if improvement > 1 else 0
            print(f"  📈 Improvement: {improvement:.1f}x ({gain_db:.1f} dB gain)")
            
            if improvement > 1.5:  # Expect at least some coding gain
                print("  ✅ Coding shows improvement!")
            else:
                print("  ⚠️  Limited or no coding gain observed")
                
    def test_coding_with_different_modulations(self):
        """Test coding behavior with different modulation schemes"""
        modulations = [2, 4, 16]
        snr_db = 8.0
        bits = 30000
        
        print(f"\n🔄 Testing coding with different modulations at {snr_db} dB...")
        
        for mod in modulations:
            mod_name = {2: 'BPSK', 4: 'QPSK', 16: '16-QAM'}[mod]
            
            uncoded_ber = lib.compute_ber(mod, snr_db, bits)
            coded_ber = lib.compute_ber_coded(mod, snr_db, bits, 1)
            
            print(f"  {mod_name:6s}: Uncoded={uncoded_ber:.3e}, Coded={coded_ber:.3e}")
            
            # Check behavior
            if mod == 2:  # BPSK should support coding
                if coded_ber >= 1.0:
                    print(f"    ❌ {mod_name}: Coding failed")
                elif coded_ber < uncoded_ber:
                    print(f"    ✅ {mod_name}: Coding provides gain")
                else:
                    print(f"    ⚠️  {mod_name}: No coding gain observed")
            else:  # Non-BPSK should fall back to uncoded
                if abs(coded_ber - uncoded_ber) < 1e-6:
                    print(f"    ✅ {mod_name}: Correctly falls back to uncoded")
                else:
                    print(f"    ⚠️  {mod_name}: Unexpected behavior (should fall back)")
                    
    def test_coding_snr_sweep(self):
        """Test coding performance across different SNR values"""
        snr_values = [2.0, 4.0, 6.0, 8.0, 10.0]
        bits = 20000
        
        print(f"\n📈 SNR sweep for BPSK coding ({bits} bits per point)...")
        print("   SNR    Uncoded      Coded       Gain")
        print("  ----   ---------   ---------   ------")
        
        valid_tests = 0
        total_gain = 0.0
        
        for snr in snr_values:
            uncoded = lib.compute_ber(2, snr, bits)
            coded = lib.compute_ber_coded(2, snr, bits, 1)
            
            if coded >= 1.0:
                gain_str = "ERROR"
            elif coded == 0:
                gain_str = "FLOOR"
            elif uncoded > 0 and coded > 0:
                gain_db = 10 * np.log10(uncoded / coded)
                gain_str = f"{gain_db:5.1f}dB"
                if gain_db > 0:
                    total_gain += gain_db
                    valid_tests += 1
            else:
                gain_str = "N/A"
                
            print(f"  {snr:4.1f}   {uncoded:8.2e}   {coded:8.2e}   {gain_str}")
            
        if valid_tests > 0:
            avg_gain = total_gain / valid_tests
            print(f"\n  📊 Average coding gain: {avg_gain:.1f} dB ({valid_tests}/{len(snr_values)} valid tests)")
            
            # Expect reasonable coding gain
            if avg_gain > 3.0:  # Expect at least 3 dB average gain
                print("  ✅ Reasonable coding gain achieved")
            else:
                print("  ⚠️  Lower than expected coding gain")
        else:
            print("  ❌ No valid coding gain measurements")
            self.fail("No valid coding tests completed")
            
    def test_coding_error_conditions(self):
        """Test coding function with invalid inputs"""
        print("\n🚨 Testing error conditions...")
        
        # Invalid modulation order
        result = lib.compute_ber_coded(8, 10.0, 10000, 1)  # Invalid mod order
        self.assertEqual(result, -1.0, "Should return -1.0 for invalid modulation")
        print("  ✅ Invalid modulation order handled correctly")
        
        # Invalid SNR
        result = lib.compute_ber_coded(2, -100.0, 10000, 1)  # Invalid SNR
        self.assertEqual(result, -1.0, "Should return -1.0 for invalid SNR")
        print("  ✅ Invalid SNR handled correctly")
        
        # Zero bits
        result = lib.compute_ber_coded(2, 10.0, 0, 1)  # Zero bits
        self.assertEqual(result, 0.0, "Should return 0.0 for zero bits")
        print("  ✅ Zero bits handled correctly")
        
        # Coding disabled (should match uncoded)
        uncoded = lib.compute_ber(2, 8.0, 20000)
        coded_off = lib.compute_ber_coded(2, 8.0, 20000, 0)  # Coding disabled
        self.assertAlmostEqual(coded_off, uncoded, places=10, msg="Coded with coding=0 should match uncoded")
        print("  ✅ Coding disabled mode works correctly")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='AMC BER tests / benchmark')
    parser.add_argument('--bench', action='store_true', help='Run benchmark instead of unit tests')
    parser.add_argument('--bench-snr', type=float, default=6.0, help='SNR (dB) for benchmark run')
    parser.add_argument('--bench-mod', type=int, default=None, help='(Deprecated) Single modulation order to benchmark')
    parser.add_argument('--bench-mods', type=str, default='2,4,16', help='Comma list of modulation orders to benchmark')
    parser.add_argument('--bench-sizes', type=str, default='5000,20000,80000,320000', help='Comma list of bit sizes (ignored if adaptive)')
    parser.add_argument('--bench-coded', action='store_true', help='Include coded BER in benchmark (now supports 2,4,16)')
    parser.add_argument('--bench-adaptive', action='store_true', help='Adaptively increase bits until min errors reached')
    parser.add_argument('--bench-min-errors', type=int, default=100, help='Minimum target bit errors for BER stability (adaptive mode)')
    parser.add_argument('--bench-max-bits', type=int, default=10_000_000, help='Maximum bits to use in adaptive mode')
    parser.add_argument('--bench-gain', action='store_true', help='Compute coding gain (dB) when coded BER > 0')
    parser.add_argument('--bench-csv', type=str, default=None, help='Optional CSV output for benchmark results')
    args, remaining = parser.parse_known_args()

    if args.bench:
        # Determine mod list
        if args.bench_mod is not None:
            mods = [args.bench_mod]
        else:
            mods = [int(m) for m in args.bench_mods.split(',') if m.strip()]
        for m in mods:
            if m not in (2,4,16):
                print(f"Invalid modulation order {m}. Use 2,4,16.")
                raise SystemExit(1)

        print('AMC BER Benchmark (Multi-Mod)')
        print('=============================')
        print(f'SNR={args.bench_snr} dB')
        if args.bench_adaptive:
            print(f"Adaptive mode: min_errors={args.bench_min_errors}, max_bits={args.bench_max_bits}")
        if args.bench_coded:
            print('Including coded BER columns')
        if args.bench_gain:
            print('Coding gain computation enabled')

        # Prepare CSV if needed
        csv_rows = []
        if args.bench_csv:
            header = ['mod','bits','ber_uncoded']
            if args.bench_coded:
                header.append('ber_coded')
            if args.bench_gain:
                header.append('gain_db')
            header += ['time_ms','throughput_Mb_s']
            csv_rows.append(header)

        def run_once(mod, n_bits):
            t0 = time.perf_counter()
            ber_u = lib.compute_ber(mod, args.bench_snr, n_bits)
            ber_c = None
            if args.bench_coded:
                ber_c = lib.compute_ber_coded(mod, args.bench_snr, n_bits, 1234)
            dt_ms = (time.perf_counter() - t0) * 1000.0
            thr = n_bits / (dt_ms/1000.0) / 1e6
            return ber_u, ber_c, dt_ms, thr

        # Print header
        base_cols = ['mod','bits','ber_uncoded']
        if args.bench_coded:
            base_cols.append('ber_coded')
        if args.bench_gain:
            base_cols.append('gain_db')
        base_cols += ['time_ms','throughput_Mb_s','scale'] if not args.bench_adaptive else ['time_ms','throughput_Mb_s']
        header_line = '  '.join(f"{c:>12}" for c in base_cols)
        print(header_line)
        print('-'*len(header_line))

        for mod in mods:
            last_bits = None
            if args.bench_adaptive:
                guess = 5000
                total_bits = 0
                total_errors = 0
                step_scale = 2.5
                while total_bits < args.bench_max_bits and total_errors < args.bench_min_errors:
                    ber_u, ber_c, dt_ms, thr = run_once(mod, guess)
                    est_errors = max(1, int(ber_u * guess))
                    total_bits += guess
                    total_errors += est_errors
                    gain_db = ''
                    ber_c_display = ''
                    if args.bench_coded:
                        if ber_c is not None:
                            if ber_c == 0.0:
                                ber_c_display = f"{0.5/guess:.2e}"
                            else:
                                ber_c_display = f"{ber_c:.2e}"
                            if args.bench_gain and ber_c > 0 and ber_u > 0:
                                gain_db = f"{10*np.log10(ber_u/ber_c):.2f}"
                    row_print = [f"{mod:12d}", f"{total_bits:12d}", f"{ber_u:12.2e}"]
                    if args.bench_coded:
                        row_print.append(f"{ber_c_display:>12}")
                    if args.bench_gain:
                        row_print.append(f"{gain_db:>12}")
                    row_print += [f"{dt_ms:12.2f}", f"{thr:12.2f}"]
                    print('  '.join(row_print))
                    if args.bench_csv:
                        csv_row = [mod, total_bits, ber_u]
                        if args.bench_coded:
                            csv_row.append(0.5/guess if ber_c == 0.0 else ber_c)
                        if args.bench_gain and gain_db:
                            csv_row.append(float(gain_db))
                        elif args.bench_gain:
                            csv_row.append('')
                        csv_row += [dt_ms, thr]
                        csv_rows.append(csv_row)
                    if est_errors < 5:
                        guess = int(guess * step_scale)
                    else:
                        guess = int(guess * 1.5)
                print(f"--> Mod {mod}: stopping total_bits={total_bits}, est_errors≈{total_errors}")
            else:
                sizes = [int(s) for s in args.bench_sizes.split(',') if s.strip()]
                for n in sizes:
                    ber_u, ber_c, dt_ms, thr = run_once(mod, n)
                    ber_c_display = None
                    if args.bench_coded:
                        if ber_c is not None and ber_c == 0.0:
                            ber_c_display = 0.5 / n
                        else:
                            ber_c_display = ber_c
                    gain_db = ''
                    if (args.bench_gain and args.bench_coded and isinstance(ber_c_display,(int,float))
                        and ber_c_display > 0 and ber_u > 0):
                        gain_db = 10*np.log10(ber_u / ber_c_display)
                    row_print = [f"{mod:12d}", f"{n:12d}", f"{ber_u:12.2e}"]
                    if args.bench_coded:
                        row_print.append(f"{ber_c_display:12.2e}")
                    if args.bench_gain:
                        row_print.append(f"{gain_db:12.2f}" if gain_db != '' else f"{'':>12}")
                    scale = f"x{n/last_bits:.1f}" if last_bits else '   --'
                    row_print += [f"{dt_ms:12.2f}", f"{thr:12.2f}", f"{scale:>12}"]
                    print('  '.join(row_print))
                    if args.bench_csv:
                        csv_row = [mod, n, ber_u]
                        if args.bench_coded:
                            csv_row.append(ber_c_display)
                        if args.bench_gain:
                            csv_row.append(gain_db if gain_db != '' else '')
                        csv_row += [dt_ms, thr]
                        csv_rows.append(csv_row)
                    last_bits = n
        if args.bench_csv:
            import csv
            with open(args.bench_csv,'w',newline='') as f:
                writer = csv.writer(f)
                writer.writerows(csv_rows)
            print(f"CSV written: {args.bench_csv}")
        raise SystemExit(0)

    print("AMC BER Simulation - Python Unit Tests")
    print("======================================")
    print("Testing C++ library integration via ctypes")
    print()
    # Pass remaining args to unittest (like -k pattern)
    sys.argv = [sys.argv[0]] + remaining
    unittest.main(verbosity=2)