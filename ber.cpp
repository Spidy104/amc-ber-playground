#include <algorithm> // STL algorithms
#include <array>     // std::array
#include <cmath>
#include <complex>
#include <cstring>    // For strcpy in error buffer
#include <functional> // std::function
#include <numeric>    // transform_reduce
#include <optional>   // std::optional
#include <random>
#include <utility> // For std::pair
#include <vector>

#include "coding.h"



using namespace std;

using cdouble = complex<double>;

// Use mathematical constants (C++20 style but with fallback)
constexpr double M_SQRT2_INV = 0.7071067811865476;   // 1.0 / sqrt(2.0)
constexpr double M_SQRT10_INV = 0.31622776601683794; // 1.0 / sqrt(10.0)
constexpr double scale_qpsk = M_SQRT2_INV;
constexpr double scale_16qam = M_SQRT10_INV;
constexpr int MAX_ERR_MSG = 256;

// Reintroduce traits instead of concepts for wider compiler compatibility
template <typename T>
using is_numeric = std::enable_if_t<std::is_arithmetic_v<T>, int>;

// Forward declarations
vector<cdouble> modulate_impl(const vector<bool> &bits, int mod_order);

// 16-QAM Gray mapping (per component, treated as a 4-PAM with Gray code)
// Adjacent amplitude levels must differ by exactly one bit to minimize the
// bit error impact of small symbol decision errors.
// Conventional Gray-ordered amplitude sequence (from +3 to -3) is:
//   +3   +1   -1   -3
//  bits: 00   01   11   10   (each neighbor differs by 1 bit)
// Our lookup uses the bit-pair (msb, lsb) combined as index = (msb<<1)|lsb.
// Index enumeration order is therefore: 00, 01, 10, 11
// To preserve the Gray relationships we intentionally store the table in this
// index order (NOT in amplitude order). That means:
//   index 0 (00) -> +3
//   index 1 (01) -> +1
//   index 2 (10) -> -3  (note: amplitude order would expect -1 here; this is
//                        why the array looks "unsorted")
//   index 3 (11) -> -1
// This still implements the correct Gray mapping because demodulation maps the
// quantized amplitude levels back to the same bit pairs:
//   +3 -> 00, +1 -> 01, -1 -> 11, -3 -> 10
// The apparent “out of order” placement of -3 and -1 in the lookup array is
// deliberate and NOT a bug.
constexpr std::array<double, 4> qam_levels = {3.0, 1.0, -3.0, -1.0};

constexpr double get_level(bool msb, bool lsb) noexcept {
  // Convert bool pair to index: 00->0, 01->1, 10->2, 11->3
  const size_t index =
      (static_cast<size_t>(msb) << 1) | static_cast<size_t>(lsb);
  return qam_levels[index];
}

// Compile-time validation of Gray mapping
static_assert(get_level(false, false) == 3.0, "16QAM Gray mapping error (00)");
static_assert(get_level(false, true) == 1.0, "16QAM Gray mapping error (01)");
static_assert(get_level(true, false) == -3.0, "16QAM Gray mapping error (10)");
static_assert(get_level(true, true) == -1.0, "16QAM Gray mapping error (11)");

constexpr double demod_level(double val) noexcept {
  return (val > 2.0) ? 3.0 : (val > 0.0) ? 1.0 : (val > -2.0) ? -1.0 : -3.0;
}

// Modern validation with constexpr array
constexpr std::array<int, 3> valid_mod_orders = {2, 4, 16};

constexpr bool is_valid_mod_order(int mod_order) noexcept {
  return std::find(valid_mod_orders.begin(), valid_mod_orders.end(),
                   mod_order) != valid_mod_orders.end();
}

// Modern error handling approach
[[nodiscard]] std::optional<vector<cdouble>> try_modulate(
  const vector<bool> &bits, int mod_order) {
  // Validate inputs using constexpr function
  if (!is_valid_mod_order(mod_order)) [[unlikely]] {
    return std::nullopt;
  }

  int bits_per_sym = static_cast<int>(log2(mod_order));
  if (bits.size() < static_cast<size_t>(bits_per_sym)) [[unlikely]] {
    return std::nullopt;
  }

  return modulate_impl(bits, mod_order);
}

[[nodiscard]] vector<cdouble> modulate(const vector<bool> &bits,
                                      int mod_order) {
  auto result = try_modulate(bits, mod_order);
  return result ? *result : vector<cdouble>{};
}

[[nodiscard]] vector<cdouble> modulate_impl(const vector<bool> &bits,
                                           int mod_order) {
  int bits_per_sym = static_cast<int>(log2(mod_order));
  size_t num_sym = bits.size() / bits_per_sym;
  vector<cdouble> symbols(num_sym);

  if (num_sym == 0) {
    return symbols;
  }

  if (mod_order == 2) {
    for (size_t i = 0; i < num_sym; ++i) {
      symbols[i] = bits[i] ? cdouble(-1.0) : cdouble(1.0);
    }
  } else if (mod_order == 4) {
    for (size_t i = 0; i < num_sym; ++i) {
      // Add bounds checking
      if (2 * i + 1 >= bits.size())
        break;
      double re = bits[2 * i] ? -1.0 : 1.0;
      double im = bits[2 * i + 1] ? -1.0 : 1.0;
      symbols[i] = cdouble(re, im) * scale_qpsk;
    }
  } else if (mod_order == 16) {
    for (size_t i = 0; i < num_sym; ++i) {
      // Add bounds checking
      if (4 * i + 3 >= bits.size())
        break;
      double re = get_level(bits[4 * i], bits[4 * i + 2]);
      double im = get_level(bits[4 * i + 1], bits[4 * i + 3]);
      symbols[i] = cdouble(re, im) * scale_16qam;
    }
  }
  return symbols;
}

[[nodiscard]] vector<bool> demodulate(const vector<cdouble> &symbols,
                                     int mod_order) {
  int bits_per_sym = static_cast<int>(log2(mod_order));
  size_t num_sym = symbols.size();
  vector<bool> bits(num_sym * bits_per_sym);

  if (num_sym == 0) {
    return bits; // Return empty vector if no symbols
  }

  if (mod_order == 2) {
    for (size_t i = 0; i < num_sym; ++i) {
      bits[i] = (real(symbols[i]) < 0.0);
    }
  } else if (mod_order == 4) {
    for (size_t i = 0; i < num_sym; ++i) {
      // Add bounds checking
      if (2 * i + 1 >= bits.size())
        break;
      cdouble y = symbols[i] / scale_qpsk;
      bits[2 * i] = (real(y) < 0.0);
      bits[2 * i + 1] = (imag(y) < 0.0);
    }
  } else if (mod_order == 16) {
    for (size_t i = 0; i < num_sym; ++i) {
      // Add bounds checking
      if (4 * i + 3 >= bits.size())
        break;
      cdouble z = symbols[i] / scale_16qam;
      double re_level = demod_level(real(z));
      double im_level = demod_level(imag(z));
      // Modern lookup approach with constexpr mapping
      constexpr auto level_to_bits = [](double level) -> pair<bool, bool> {
        switch (static_cast<int>(level)) {
        case 3:
          return {false, false};
        case 1:
          return {false, true};
        case -1:
          return {true, true};
        case -3:
        default:
          return {true, false};
        }
      };
      auto [msb_re, lsb_re] = level_to_bits(re_level);
      bits[4 * i] = msb_re;
      bits[4 * i + 2] = lsb_re;
      auto [msb_im, lsb_im] = level_to_bits(im_level);
      bits[4 * i + 1] = msb_im;
      bits[4 * i + 3] = lsb_im;
    }
  }
  return bits;
}

// Modern utility functions
template <typename T, typename = is_numeric<T>>
[[nodiscard]] constexpr T db_to_linear(T db_value) noexcept {
  return std::pow(T{10.0}, db_value / T{10.0});
}

template <typename T, typename = is_numeric<T>>
[[nodiscard]] constexpr T linear_to_db(T linear_value) noexcept {
  return T{10.0} * std::log10(linear_value);
}

// Modern random bit generator
class BitGenerator {
private:
  std::mt19937 gen_;
  std::uniform_int_distribution<int> dist_;

public:
  BitGenerator() : gen_(std::random_device{}()), dist_(0, 1) {}

  bool operator()() { return static_cast<bool>(dist_(gen_)); }
};

extern "C" double compute_ber(int mod_order, double snr_db,
                              long long num_bits) {
  // Validate modulation order
  if (mod_order != 2 && mod_order != 4 && mod_order != 16) [[unlikely]]
    return -1.0;

  // Validate SNR range (reasonable bounds)
  if (snr_db < -50.0 || snr_db > 50.0) [[unlikely]]
    return -1.0;

  int bits_per_sym = static_cast<int>(log2(mod_order));

  // Adjust num_bits to be divisible by bits_per_sym
  if (num_bits % bits_per_sym != 0)
    num_bits -= num_bits % bits_per_sym;

  // Return 0 BER for non-positive number of bits
  if (num_bits <= 0) [[unlikely]]
    return 0.0;

  // Reasonable upper limit for number of bits to prevent excessive memory usage
  if (num_bits > 100000000LL) [[unlikely]]
    return -1.0;
  // Modern STL approach for bit generation
  random_device rd;
  mt19937 gen(rd());
  vector<bool> bits(static_cast<size_t>(num_bits));
  // Use modern generator directly
  BitGenerator bit_gen;
  std::generate(bits.begin(), bits.end(), std::ref(bit_gen));

  vector<cdouble> symbols = modulate(bits, mod_order);
  double ebno_lin = db_to_linear(snr_db);
  double esno_lin = static_cast<double>(bits_per_sym) * ebno_lin;
  double n0 = 1.0 / esno_lin;
  double sigma = sqrt(n0 / 2.0);
  normal_distribution<double> noise_dist(0.0, sigma);

  // Modern STL approach with algorithm
  std::for_each(symbols.begin(), symbols.end(), [&](cdouble &s) {
    s += cdouble(noise_dist(gen), noise_dist(gen));
  });

  vector<bool> rx_bits = demodulate(symbols, mod_order);

  // Modern STL approach for error counting
  const auto errors =
      std::transform_reduce(bits.begin(), bits.end(), rx_bits.begin(), 0LL,
                            std::plus<>{},        // reduction operation
                            std::not_equal_to<>{} // transform operation
      );

  return static_cast<double>(errors) / static_cast<double>(num_bits);
}

// Deterministic (seeded) BER computation for reproducible simulation sequences
extern "C" double compute_ber_seeded(int mod_order, double snr_db,
                                     long long num_bits,
                                     unsigned long long seed) {
  if (mod_order != 2 && mod_order != 4 && mod_order != 16) [[unlikely]]
    return -1.0;
  if (snr_db < -50.0 || snr_db > 50.0) [[unlikely]]
    return -1.0;
  int bits_per_sym = static_cast<int>(log2(mod_order));
  if (num_bits % bits_per_sym != 0)
    num_bits -= num_bits % bits_per_sym;
  if (num_bits <= 0) [[unlikely]]
    return 0.0;
  if (num_bits > 100000000LL) [[unlikely]]
    return -1.0;
  

  mt19937_64 gen(seed);
  uniform_int_distribution<int> bit_dist(0, 1);
  vector<bool> bits(static_cast<size_t>(num_bits));
  for (size_t i = 0; i < bits.size(); ++i)
    bits[i] = static_cast<bool>(bit_dist(gen));

  vector<cdouble> symbols = modulate(bits, mod_order);
  double ebno_lin = db_to_linear(snr_db);
  double esno_lin = static_cast<double>(bits_per_sym) * ebno_lin;
  double n0 = 1.0 / esno_lin;
  double sigma = sqrt(n0 / 2.0);
  normal_distribution<double> noise_dist(0.0, sigma);
  for (auto &s : symbols) {
    s += cdouble(noise_dist(gen), noise_dist(gen));
  }
  vector<bool> rx_bits = demodulate(symbols, mod_order);
  const auto errors = std::transform_reduce(
      bits.begin(), bits.end(), rx_bits.begin(), 0LL, std::plus<>{},
      std::not_equal_to<>{});
  return static_cast<double>(errors) / static_cast<double>(num_bits);
}

vector<cdouble> generate_pilots(size_t num_pilots) {
  // Direct initialization is already optimal, but we can make it more explicit
  return vector<cdouble>(num_pilots, cdouble(1.0, 0.0));
}

void add_awgn(vector<cdouble> &symbols, double esno_lin, mt19937 &gen) {
  const double n0 = 1.0 / esno_lin;
  const double sigma = sqrt(n0 / 2.0);
  normal_distribution<double> noise_dist(0.0, sigma);

  // Use STL algorithm for consistency
  std::for_each(symbols.begin(), symbols.end(), [&](cdouble &s) {
    s += cdouble(noise_dist(gen), noise_dist(gen));
  });
}

extern "C" double estimate_snr(double true_snr_db, long long num_pilots) {
  // Validate inputs
  if (num_pilots <= 0) [[unlikely]]
    return -999.0; // Return error value instead of 0
  if (true_snr_db < -50.0 || true_snr_db > 50.0) [[unlikely]]
    return -999.0;
  if (num_pilots > 1000000LL) [[unlikely]]
    return -999.0; // Reasonable upper limit
  random_device rd;
  mt19937 gen(rd());
  vector<cdouble> tx_pilots = generate_pilots(static_cast<size_t>(num_pilots));
  double ebno_lin = db_to_linear(true_snr_db);
  double esno_lin = ebno_lin; // 1 bit/sym
  vector<cdouble> rx_pilots = tx_pilots;
  add_awgn(rx_pilots, esno_lin, gen);

  // Modern STL approach for noise variance calculation
  const double noise_var =
      std::transform_reduce(
          rx_pilots.begin(), rx_pilots.end(), tx_pilots.begin(), 0.0,
          std::plus<>{}, // reduction operation
          [](const cdouble &rx, const cdouble &tx) { return norm(rx - tx); }) /
      static_cast<double>(num_pilots);

  double est_esno_lin = 1.0 / noise_var;
  double est_ebno_lin = est_esno_lin;
  return linear_to_db(est_ebno_lin);
}

// Internal theoretical helpers (not exposed)
inline double qfunc(double x) { return 0.5 * erfc(x / sqrt(2.0)); }

inline double theor_ber_bpsk_qpsk(double ebno_db) {
  double ebno_lin = pow(10.0, ebno_db / 10.0);
  return qfunc(sqrt(2.0 * ebno_lin));
}

inline double theor_ber_16qam(double ebno_db) {
  double ebno_lin = pow(10.0, ebno_db / 10.0);
  double sqrt_term = sqrt(2.0 * ebno_lin / 5.0);
  return (1.0 / 4.0) * (3.0 * qfunc(sqrt_term) + qfunc(3.0 * sqrt_term));
}

// Test Functions (extern "C" for Python calling)
extern "C" int run_mod_demod_test(char *err_msg) {
  // BPSK
  vector<bool> bits_bpsk = {false, true};
  auto syms_bpsk = modulate(bits_bpsk, 2);
  auto rx_bpsk = demodulate(syms_bpsk, 2);
  if (rx_bpsk != bits_bpsk) {
    strcpy(err_msg, "Mod/Demod BPSK failed");
    return 1;
  }
  // QPSK
  vector<bool> bits_qpsk = {false, false, true, true};
  auto syms_qpsk = modulate(bits_qpsk, 4);
  auto rx_qpsk = demodulate(syms_qpsk, 4);
  if (rx_qpsk != bits_qpsk) {
    strcpy(err_msg, "Mod/Demod QPSK failed");
    return 1;
  }
  // 16QAM
  vector<bool> bits_16qam = {false, false, false, false};
  auto syms_16qam = modulate(bits_16qam, 16);
  auto rx_16qam = demodulate(syms_16qam, 16);
  if (rx_16qam != bits_16qam) {
    strcpy(err_msg, "Mod/Demod 16QAM failed");
    return 1;
  }
  strcpy(err_msg, "All mod/demod tests passed");
  return 0;
}

extern "C" int run_ber_edge_test(char *err_msg) {
  if (compute_ber(2, 0.0, 0LL) != 0.0) {
    strcpy(err_msg, "BER zero bits failed");
    return 1;
  }
  if (compute_ber(3, 0.0, 100LL) != -1.0) {
    strcpy(err_msg, "BER invalid mod failed");
    return 1;
  }
  strcpy(err_msg, "BER edge cases passed");
  return 0;
}

extern "C" int run_ber_accuracy_test(double *out_avg_ber, double *out_theor,
                                     char *err_msg) {
  const int num_runs = 5; // Fewer runs but more bits per run for stability
  const double test_snr_db = 9.0; // Slightly lower SNR to increase error events
  double theor = theor_ber_bpsk_qpsk(test_snr_db);

  // Choose number of bits so that expected total errors >= 200 for good
  // statistics expected_errors ≈ theor * num_bits * num_runs
  long long num_bits = 200000;          // initial guess
  const long long max_bits = 5'000'000; // safety cap
  while (theor * num_bits * num_runs < 200.0 && num_bits < max_bits) {
    num_bits *= 2;
  }

  double avg_ber = 0.0;
  for (int r = 0; r < num_runs; ++r) {
    avg_ber += compute_ber(2, test_snr_db, num_bits);
  }
  avg_ber /= num_runs;

  // Relative tolerance tightened because statistical variance reduced by higher
  // error count
  double tol_rel = 0.15; // 15%
  if (theor > 0 && std::abs(avg_ber - theor) / theor > tol_rel) {
    snprintf(err_msg, MAX_ERR_MSG,
             "BER BPSK accuracy failed: SNR=%.1f dB sim=%.2e theor=%.2e "
             "bits/run=%lld",
             test_snr_db, avg_ber, theor, num_bits);
    return 1;
  }
  *out_avg_ber = avg_ber;
  *out_theor = theor;
  snprintf(err_msg, MAX_ERR_MSG,
           "BER accuracy passed (SNR=%.1f dB, bits/run=%lld)", test_snr_db,
           num_bits);
  return 0;
}

extern "C" int run_snr_estimation_test(double *out_avg_est, double *out_std_est,
                                       char *err_msg) {
  const int num_runs = 20;
  const long long num_pilots = 100;
  double true_snr = 10.0;
  double sum_est = 0.0, sum_sq_diff = 0.0;
  for (int r = 0; r < num_runs; ++r) {
    double est = estimate_snr(true_snr, num_pilots);
    sum_est += est;
    sum_sq_diff += (est - true_snr) * (est - true_snr);
  }
  double avg_est = sum_est / num_runs;
  double var_est = sum_sq_diff / num_runs;
  double std_est = sqrt(var_est);
  double tol_mean = 0.5; // dB
  double tol_std = 1.0;  // dB
  if (abs(avg_est - true_snr) > tol_mean || std_est > tol_std) {
    snprintf(err_msg, MAX_ERR_MSG,
             "SNR est failed: avg=%.2f, std=%.2f (true=%.1f)", avg_est, std_est,
             true_snr);
    return 1;
  }
  *out_avg_est = avg_est;
  *out_std_est = std_est;
  strcpy(err_msg, "SNR estimation passed");
  return 0;
}

extern "C" int run_all_tests(char *overall_msg) {
  if (!overall_msg)
    return -1; // Null pointer check

  char err_buf[MAX_ERR_MSG] = {0};
  int failures = 0;

  // Run each test and stop on first failure for clearer error reporting
  failures += run_mod_demod_test(err_buf);
  if (failures > 0) {
    snprintf(overall_msg, MAX_ERR_MSG, "Mod/Demod test failed: %s", err_buf);
    return failures;
  }

  failures += run_ber_edge_test(err_buf);
  if (failures > 0) {
    snprintf(overall_msg, MAX_ERR_MSG, "BER edge test failed: %s", err_buf);
    return failures;
  }

  double avg_ber, theor_ber;
  failures += run_ber_accuracy_test(&avg_ber, &theor_ber, err_buf);
  if (failures > 0) {
    snprintf(overall_msg, MAX_ERR_MSG,
             "BER accuracy test failed: %s (sim=%.2e, theor=%.2e)", err_buf,
             avg_ber, theor_ber);
    return failures;
  }

  double avg_est, std_est;
  failures += run_snr_estimation_test(&avg_est, &std_est, err_buf);
  if (failures > 0) {
    snprintf(overall_msg, MAX_ERR_MSG,
             "SNR estimation test failed: %s (avg=%.2f, std=%.2f)", err_buf,
             avg_est, std_est);
    return failures;
  }

  strncpy(overall_msg, "All tests passed!", MAX_ERR_MSG - 1);
  overall_msg[MAX_ERR_MSG - 1] = '\0'; // Ensure null termination
  return 0;
}

// Convolutional coding-enabled BER computation
extern "C" double compute_ber_coded(int mod_order, double snr_db, long long num_bits, int seed) {
  // Support BPSK (2), QPSK (4), and 16-QAM (16) coded paths.
  if (mod_order != 2 && mod_order != 4 && mod_order != 16) {
    return compute_ber(mod_order, snr_db, num_bits); // Fallback
  }

  int info_bits_count = static_cast<int>(num_bits);
  // For 16-QAM ensure coded length divisible by 4 (coded bits per symbol)
  if (mod_order == 16 && info_bits_count % 2 != 0) {
    --info_bits_count; // Make even so (info_bits+6) is even => coded_len multiple of 4
    if (info_bits_count <= 0) return -0.15;
  }
  if (info_bits_count <= 0) return -0.1;

  // Rate 1/2 convolutional code with tail bits (K=7 => 6 tail bits)
  const int constraint_tail = 6;
  int coded_len = 2 * (info_bits_count + constraint_tail);
  if (coded_len <= 0) return -0.2;
  if (coded_len > 200'000'000) return -0.25; // Safety cap

  // Generate info bits
  mt19937 gen(seed);
  uniform_int_distribution<int> bit_dist(0, 1);
  vector<bool> orig_bits(info_bits_count);
  for (int i = 0; i < info_bits_count; ++i) orig_bits[i] = bit_dist(gen);

  // Encode
  bool* info_arr = new bool[info_bits_count];
  for (int i = 0; i < info_bits_count; ++i) info_arr[i] = orig_bits[i];
  bool* coded_arr = new bool[coded_len];
  int actual_coded_len = 0;
  int enc_status = convolutional_encode(info_arr, info_bits_count, coded_arr, &actual_coded_len);
  if (enc_status != 0 || actual_coded_len != coded_len) {
    delete[] info_arr; delete[] coded_arr; return -1.0;
  }

  vector<bool> coded_bits(coded_len);
  for (int i = 0; i < coded_len; ++i) coded_bits[i] = coded_arr[i];
  delete[] info_arr; delete[] coded_arr;

  // Map coded bits to symbols based on modulation
  vector<cdouble> symbols;
  if (mod_order == 2) {
    symbols.reserve(coded_len);
    for (int i = 0; i < coded_len; ++i) {
      symbols.emplace_back(coded_bits[i] ? -1.0 : 1.0, 0.0);
    }
  } else if (mod_order == 4) {
    symbols.reserve(coded_len / 2);
    for (int i = 0; i < coded_len; i += 2) {
      bool b0 = coded_bits[i];
      bool b1 = coded_bits[i + 1];
      double re = b0 ? -1.0 : 1.0;
      double im = b1 ? -1.0 : 1.0;
      symbols.emplace_back(re * scale_qpsk, im * scale_qpsk);
    }
  } else { // 16-QAM coded path (4 coded bits per symbol)
    symbols.reserve(coded_len / 4);
    // Mapping matches modulate_impl: (b0,b2) -> real, (b1,b3) -> imag via get_level
    for (int i = 0; i < coded_len; i += 4) {
      if (i + 3 >= coded_len) break; // Should not happen due to adjustment
      bool b0 = coded_bits[i];
      bool b1 = coded_bits[i + 1];
      bool b2 = coded_bits[i + 2];
      bool b3 = coded_bits[i + 3];
      double re = get_level(b0, b2);
      double im = get_level(b1, b3);
      symbols.emplace_back(re * scale_16qam, im * scale_16qam);
    }
  }

  // Noise scaling: esno_lin = R * k * ebno_lin, where k = coded bits per symbol
  constexpr double code_rate = 0.5;
  double ebno_lin = pow(10.0, snr_db / 10.0);
  int coded_bits_per_symbol = (mod_order == 2) ? 1 : (mod_order == 4 ? 2 : 4);
  double esno_lin = ebno_lin * code_rate * coded_bits_per_symbol;
  double n0 = 1.0 / esno_lin;
  normal_distribution<double> noise_dist(0.0, sqrt(n0 / 2.0));
  for (auto &sym : symbols) {
    sym += cdouble(noise_dist(gen), noise_dist(gen));
  }

  // Generate LLRs (order must match coded bit emission order)
  vector<double> llr(coded_len);
  double llr_scale = 2.0 / n0; // Base scale
  if (mod_order == 2) {
    for (int i = 0; i < coded_len; ++i) {
      llr[i] = -real(symbols[i]) * llr_scale;
    }
  } else if (mod_order == 4) { // QPSK
    int idx = 0;
    for (auto &sym : symbols) {
      llr[idx++] = -real(sym) * llr_scale;
      llr[idx++] = -imag(sym) * llr_scale;
    }
  } else { // 16-QAM: compute two LLRs per dimension (msb, lsb) using exact log-sum-exp over 4-PAM levels
    // Precompute intrinsic amplitude levels after scaling
    constexpr array<double,4> pam_levels = {3.0,1.0,-3.0,-1.0}; // indices 0..3 correspond to bit pairs 00,01,10,11? (As defined in get_level)
    // Need mapping consistent with get_level: index = (msb<<1)|lsb => levels {+3,+1,-3,-1}
    constexpr array<double,4> levels = {3.0,1.0,-3.0,-1.0};
    double two_sigma2 = n0; // since each complex dimension variance = n0/2 => 2*sigma^2 = n0
    int idx = 0;
    for (auto &sym : symbols) {
      double rI = real(sym) / scale_16qam; // de-normalize
      double rQ = imag(sym) / scale_16qam;
      // Function to compute two LLRs for a real value over 4-PAM Gray mapping
      auto fill_llrs = [&](double x, double &llr_msb, double &llr_lsb){
        double metric[4];
        for (int k=0;k<4;++k){
          double d = x - levels[k];
          metric[k] = - (d*d) / two_sigma2; // log-likelihood up to constant
        }
        // Gray mapping: indices -> bit pairs (msb,lsb): 0->(0,0),1->(0,1),2->(1,0),3->(1,1)
        // Compute log-sum-exp for msb=0 (indices 0,1) and msb=1 (2,3)
        auto lse2 = [](double a, double b){ double m = max(a,b); return m + log(exp(a-m)+exp(b-m)); };
        double l_ms0 = lse2(metric[0], metric[1]);
        double l_ms1 = lse2(metric[2], metric[3]);
        // LLR = log P(bit=0) - log P(bit=1)
        double llr_std_msb = l_ms0 - l_ms1;
        // For lsb: indices with lsb=0 are 0 (00) and 2 (10); lsb=1 are 1 (01) and 3 (11)
        double l_ls0 = lse2(metric[0], metric[2]);
        double l_ls1 = lse2(metric[1], metric[3]);
        double llr_std_lsb = l_ls0 - l_ls1;
        // Invert sign to align with current decoder expectation
        llr_msb = -llr_std_msb;
        llr_lsb = -llr_std_lsb;
      };
      double llrI_msb, llrI_lsb, llrQ_msb, llrQ_lsb;
      fill_llrs(rI, llrI_msb, llrI_lsb);
      fill_llrs(rQ, llrQ_msb, llrQ_lsb);
      // Order must match coded bit emission: (b0,b1,b2,b3) where get_level used (b0,b2) for I and (b1,b3) for Q
      // Emission order from encoder is sequential; we mapped groups of 4 bits as b0,b1,b2,b3.
      llr[idx++] = llrI_msb; // b0 (msb of I pair)
      llr[idx++] = llrQ_msb; // b1 (msb of Q pair)
      llr[idx++] = llrI_lsb; // b2 (lsb of I pair)
      llr[idx++] = llrQ_lsb; // b3 (lsb of Q pair)
    }
  }

  // Decode
  bool* decoded_arr = new bool[info_bits_count];
  int decoded_len = 0;
  int dec_status = viterbi_decode(llr.data(), coded_len, decoded_arr, &decoded_len);
  if (dec_status != 0) { delete[] decoded_arr; return -10.0 - dec_status; }
  if (decoded_len <= 0 || decoded_len > info_bits_count) { delete[] decoded_arr; return -0.3; }

  int bit_errors = 0;
  int cmp_len = std::min(decoded_len, info_bits_count);
  for (int i = 0; i < cmp_len; ++i) if (orig_bits[i] != decoded_arr[i]) ++bit_errors;
  delete[] decoded_arr;
  return static_cast<double>(bit_errors) / static_cast<double>(cmp_len);
}

// Extern C interface for Python
extern "C" {
  double py_compute_ber_coded(int mod_order, double snr_db, long long num_bits, int seed) {
    return compute_ber_coded(mod_order, snr_db, num_bits, seed);
  }
}