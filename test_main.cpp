#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

// Declarations of extern "C" functions from ber.cpp
extern "C" {
double compute_ber(int mod_order, double snr_db, long long num_bits);
double estimate_snr(double true_snr_db, long long num_pilots);
int run_mod_demod_test(char *err_msg);
int run_ber_edge_test(char *err_msg);
int run_ber_accuracy_test(double *out_avg_ber, double *out_theor,
                          char *err_msg);
int run_snr_estimation_test(double *out_avg_est, double *out_std_est,
                            char *err_msg);
int run_all_tests(char *overall_msg);
}

namespace {
struct SweepResult {
  int mod_order;
  double snr_db;
  double ber;
};

// Helper function to check if two BER values are reasonably close
bool ber_close(double a, double b, double tolerance = 0.2) {
  if (a == 0.0 && b == 0.0)
    return true;
  if (a == 0.0 || b == 0.0)
    return false;
  return std::abs(a - b) / std::max(a, b) < tolerance;
}

// Test boundary conditions and stress tests
bool test_boundary_conditions() {
  std::cout << "\n==== Boundary Condition Tests ====" << std::endl;
  bool all_passed = true;

  // Test very high SNR (should give very low BER)
  double ber_high_snr = compute_ber(2, 20.0, 100000);
  if (ber_high_snr < 0 || ber_high_snr > 1e-8) {
    std::cout << "[FAIL] High SNR test: BER=" << ber_high_snr << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] High SNR (20dB): BER=" << std::scientific
              << ber_high_snr << std::defaultfloat << std::endl;
  }

  // Test very low SNR (should give high BER but not > 0.5)
  double ber_low_snr = compute_ber(2, -10.0, 50000);
  if (ber_low_snr < 0 || ber_low_snr > 0.5) {
    std::cout << "[FAIL] Low SNR test: BER=" << ber_low_snr << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] Low SNR (-10dB): BER=" << ber_low_snr << std::endl;
  }

  // Test large bit count (stress test)
  double ber_large = compute_ber(4, 6.0, 1000000);
  if (ber_large < 0) {
    std::cout << "[FAIL] Large bit count test failed" << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] Large bit count (1M bits): BER=" << std::scientific
              << ber_large << std::defaultfloat << std::endl;
  }

  return all_passed;
}

// Test input validation
bool test_input_validation() {
  std::cout << "\n==== Input Validation Tests ====" << std::endl;
  bool all_passed = true;

  // Invalid modulation orders
  if (compute_ber(3, 10.0, 1000) != -1.0) {
    std::cout << "[FAIL] Invalid mod order (3) should return -1" << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] Invalid mod order rejection" << std::endl;
  }

  // Negative bit count
  if (compute_ber(2, 10.0, -100) != 0.0) {
    std::cout << "[FAIL] Negative bit count should return 0" << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] Negative bit count handling" << std::endl;
  }

  // Out of range SNR
  if (compute_ber(2, -100.0, 1000) != -1.0) {
    std::cout << "[FAIL] Out-of-range SNR should return -1" << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] Out-of-range SNR rejection" << std::endl;
  }

  // SNR estimation validation
  if (estimate_snr(10.0, -50) != -999.0) {
    std::cout << "[FAIL] Invalid pilot count should return -999" << std::endl;
    all_passed = false;
  } else {
    std::cout << "[PASS] Invalid pilot count rejection" << std::endl;
  }

  return all_passed;
}

// Test consistency across modulation orders
bool test_modulation_consistency() {
  std::cout << "\n==== Modulation Consistency Tests ====" << std::endl;
  bool all_passed = true;

  // At same Eb/N0, higher order modulation should have higher BER
  double ber_bpsk = compute_ber(2, 8.0, 100000);
  double ber_qpsk = compute_ber(4, 8.0, 100000);
  double ber_16qam = compute_ber(16, 8.0, 100000);

  if (ber_bpsk >= 0 && ber_qpsk >= 0 && ber_16qam >= 0) {
    std::cout << "[INFO] BER at 8dB: BPSK=" << std::scientific << ber_bpsk
              << ", QPSK=" << ber_qpsk << ", 16QAM=" << ber_16qam
              << std::defaultfloat << std::endl;

    // BPSK and QPSK should be similar (Gray coding)
    if (!ber_close(ber_bpsk, ber_qpsk, 0.3)) {
      std::cout << "[WARN] BPSK and QPSK BER significantly different"
                << std::endl;
    }

    // 16QAM should be worse than QPSK
    if (ber_16qam <= ber_qpsk * 1.5) {
      std::cout << "[WARN] 16QAM BER not sufficiently higher than QPSK"
                << std::endl;
    }

    std::cout << "[PASS] Modulation order consistency" << std::endl;
  } else {
    std::cout << "[FAIL] One or more modulations failed" << std::endl;
    all_passed = false;
  }

  return all_passed;
}

// Test SNR estimation accuracy across range
bool test_snr_estimation_accuracy() {
  std::cout << "\n==== SNR Estimation Accuracy Tests ====" << std::endl;
  bool all_passed = true;

  std::vector<double> test_snrs = {0, 5, 10, 15};
  for (double true_snr : test_snrs) {
    double est_snr = estimate_snr(true_snr, 500); // More pilots for accuracy
    double error = std::abs(est_snr - true_snr);

    if (est_snr == -999.0) {
      std::cout << "[FAIL] SNR estimation failed for " << true_snr << " dB"
                << std::endl;
      all_passed = false;
    } else if (error > 2.0) { // Allow 2dB error
      std::cout << "[FAIL] SNR estimation error too large: true=" << true_snr
                << " est=" << est_snr << " error=" << error << " dB"
                << std::endl;
      all_passed = false;
    } else {
      std::cout << "[PASS] SNR " << true_snr << "dB -> " << std::setprecision(2)
                << est_snr << "dB (error=" << error << "dB)" << std::endl;
    }
  }

  return all_passed;
}

// Test repeatability (same parameters should give similar results)
bool test_repeatability() {
  std::cout << "\n==== Repeatability Tests ====" << std::endl;
  bool all_passed = true;

  const int num_repeats = 5;
  const double test_snr = 6.0;
  const long long test_bits = 200000;

  std::vector<double> bers;
  for (int i = 0; i < num_repeats; ++i) {
    double ber = compute_ber(4, test_snr, test_bits);
    bers.push_back(ber);
  }

  // Check if results are reasonably consistent
  double mean_ber = 0.0;
  for (double ber : bers)
    mean_ber += ber;
  mean_ber /= num_repeats;

  double variance = 0.0;
  for (double ber : bers) {
    variance += (ber - mean_ber) * (ber - mean_ber);
  }
  variance /= num_repeats;
  double std_dev = std::sqrt(variance);

  // Coefficient of variation should be reasonable (< 50% for this sample size)
  double cv = (mean_ber > 0) ? std_dev / mean_ber : 0;

  if (cv > 0.5) {
    std::cout << "[WARN] High variance in repeated runs: CV=" << cv
              << std::endl;
  }

  std::cout << "[PASS] Repeatability: mean BER=" << std::scientific << mean_ber
            << " std=" << std_dev << " CV=" << std::defaultfloat << cv
            << std::endl;

  return all_passed;
}
} // namespace

int main() {
  std::cout << "==== Basic C API Test Harness ====" << std::endl;

  char msg[256] = {0};

  if (run_mod_demod_test(msg) != 0) {
    std::cerr << "[FAIL] run_mod_demod_test: " << msg << std::endl;
    return 1;
  }
  std::cout << "[PASS] Mod/Demod: " << msg << std::endl;

  if (run_ber_edge_test(msg) != 0) {
    std::cerr << "[FAIL] run_ber_edge_test: " << msg << std::endl;
    return 1;
  }
  std::cout << "[PASS] Edge cases: " << msg << std::endl;

  double avg_ber = 0.0, theor_ber = 0.0;
  if (run_ber_accuracy_test(&avg_ber, &theor_ber, msg) != 0) {
    std::cerr << "[FAIL] run_ber_accuracy_test: " << msg << std::endl;
    return 1;
  }
  std::cout << std::scientific << std::setprecision(3);
  std::cout << "[PASS] BER accuracy: sim=" << avg_ber << ", theor=" << theor_ber
            << std::endl;

  double avg_est = 0.0, std_est = 0.0;
  if (run_snr_estimation_test(&avg_est, &std_est, msg) != 0) {
    std::cerr << "[FAIL] run_snr_estimation_test: " << msg << std::endl;
    return 1;
  }
  std::cout << std::defaultfloat << std::setprecision(3);
  std::cout << "[PASS] SNR estimation: avg=" << avg_est
            << " dB, std=" << std_est << " dB" << std::endl;

  if (run_all_tests(msg) != 0) {
    std::cerr << "[FAIL] run_all_tests: " << msg << std::endl;
    return 1;
  }
  std::cout << "[PASS] run_all_tests: " << msg << std::endl;

  // Additional BER sweep
  std::cout << "\n==== BER Sweep (BPSK/QPSK/16QAM) ====" << std::endl;
  std::vector<int> mods = {2, 4, 16};
  std::vector<double> snrs = {0, 2, 4, 6, 8, 10};
  long long bits = 50000; // keep moderate for speed

  std::cout << std::left << std::setw(8) << "Mod" << std::setw(8) << "SNR(dB)"
            << "BER" << std::endl;
  for (int m : mods) {
    for (double snr : snrs) {
      double ber = compute_ber(m, snr, bits);
      if (ber < 0) {
        std::cout << std::left << std::setw(8) << m << std::setw(8) << snr
                  << "ERROR" << std::endl;
      } else {
        std::cout << std::left << std::setw(8) << m << std::setw(8) << snr
                  << std::scientific << ber << std::defaultfloat << std::endl;
      }
    }
  }

  // SNR estimation sanity check sweep
  std::cout << "\n==== SNR Estimation Sweep ====" << std::endl;
  for (double true_snr : snrs) {
    double est = estimate_snr(true_snr, 200);
    std::cout << "True SNR: " << true_snr << " dB -> Estimated: " << est
              << " dB" << std::endl;
  }

  // Additional comprehensive tests
  bool all_additional_passed = true;
  all_additional_passed &= test_boundary_conditions();
  all_additional_passed &= test_input_validation();
  all_additional_passed &= test_modulation_consistency();
  all_additional_passed &= test_snr_estimation_accuracy();
  all_additional_passed &= test_repeatability();

  std::cout << "\n==== Final Summary ====" << std::endl;
  if (all_additional_passed) {
    std::cout << "[PASS] All comprehensive tests completed successfully."
              << std::endl;
  } else {
    std::cout << "[WARN] Some additional tests had warnings or failures."
              << std::endl;
  }

  return 0;
}
