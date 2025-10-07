#include <array>
#include <vector>
#include <cstdint>  // For uint8_t

using namespace std;

// =============================================================================
// CONVOLUTIONAL CODING (K=7, Rate 1/2)
// =============================================================================

// Generator polynomials for K=7, rate 1/2 convolutional code (industry standard)
constexpr uint8_t G1 = 0b1011011; // 133 octal - Generator polynomial 1
constexpr uint8_t G2 = 0b1111001; // 171 octal - Generator polynomial 2
constexpr int CONSTRAINT_LENGTH = 7;
constexpr int NUM_STATES = 1 << (CONSTRAINT_LENGTH - 1); // 2^6 = 64 states
constexpr int CODE_RATE_NUM = 1;   // Numerator of code rate
constexpr int CODE_RATE_DEN = 2;   // Denominator of code rate

// Precomputed state transition tables for Viterbi decoder
struct ConvState {
    uint8_t next_state[2];    // Next state for input 0 and 1
    uint8_t output[2];        // Output bits for input 0 and 1
    uint8_t prev_state[2];    // Previous states that lead to this state
    uint8_t prev_input[2];    // Input bits from previous states
};

// Global state transition table (computed at startup)
static array<ConvState, NUM_STATES> state_table;
static bool state_table_initialized = false;

// Helper functions
constexpr uint8_t convolve(uint8_t data, uint8_t gen) {
    return __builtin_popcount(data & gen) & 1;
}

constexpr uint8_t hamming_distance(uint8_t a, uint8_t b) {
    return __builtin_popcount(a ^ b);
}

// Initialize state transition table
void init_conv_state_table() {
    if (state_table_initialized) return;
    
    for (int state = 0; state < NUM_STATES; state++) {
        for (int input = 0; input < 2; input++) {
            // Shift register: [input, state_bits]
            uint8_t shift_reg = (input << (CONSTRAINT_LENGTH - 1)) | state;
            
            // Compute outputs
            uint8_t out1 = convolve(shift_reg, G1);
            uint8_t out2 = convolve(shift_reg, G2);
            uint8_t output = (out1 << 1) | out2;
            
            // Next state (shift right, remove oldest bit)
            uint8_t next_state = shift_reg >> 1;
            
            state_table[state].next_state[input] = next_state;
            state_table[state].output[input] = output;
        }
    }
    
    // Compute reverse transitions (for Viterbi decoder)
    for (int state = 0; state < NUM_STATES; state++) {
        int count = 0;
        for (int prev_state = 0; prev_state < NUM_STATES; prev_state++) {
            for (int input = 0; input < 2; input++) {
                if (state_table[prev_state].next_state[input] == state) {
                    state_table[state].prev_state[count] = prev_state;
                    state_table[state].prev_input[count] = input;
                    count++;
                    if (count >= 2) break;
                }
            }
            if (count >= 2) break;
        }
    }
    
    state_table_initialized = true;
}

// =============================================================================
// CONVOLUTIONAL ENCODER
// =============================================================================

extern "C" int convolutional_encode(const bool* info_bits, int info_len, 
                                   bool* coded_bits, int* coded_len) {
    if (!info_bits || !coded_bits || !coded_len || info_len <= 0) return -1;
    
    init_conv_state_table();
    
    // Add tail bits to terminate trellis (K-1 zeros)
    int total_info_bits = info_len + (CONSTRAINT_LENGTH - 1);
    *coded_len = total_info_bits * CODE_RATE_DEN;
    
    uint8_t state = 0; // Start in zero state
    int coded_idx = 0;
    
    // Encode information bits
    for (int i = 0; i < info_len; i++) {
        int input = info_bits[i] ? 1 : 0;
        uint8_t output = state_table[state].output[input];
        
        // Output 2 bits (rate 1/2)
        coded_bits[coded_idx++] = (output >> 1) & 1;
        coded_bits[coded_idx++] = output & 1;
        
        state = state_table[state].next_state[input];
    }
    
    // Add tail bits (force to zero state)
    for (int i = 0; i < CONSTRAINT_LENGTH - 1; i++) {
        int input = 0; // Tail bits are always 0
        uint8_t output = state_table[state].output[input];
        
        coded_bits[coded_idx++] = (output >> 1) & 1;
        coded_bits[coded_idx++] = output & 1;
        
        state = state_table[state].next_state[input];
    }
    
    return 0; // Success
}

// =============================================================================
// VITERBI DECODER
// =============================================================================

extern "C" int viterbi_decode(const double* received_llr, int received_len,
                             bool* decoded_bits, int* decoded_len) {
    if (!received_llr || !decoded_bits || !decoded_len || received_len <= 0) return -1;
    if (received_len % 2 != 0) return -2; // Must be even (rate 1/2)
    
    init_conv_state_table();
    
    int num_stages = received_len / 2;
    int info_len = num_stages - (CONSTRAINT_LENGTH - 1); // Remove tail bits
    if (info_len <= 0) return -3;
    
    *decoded_len = info_len;
    
    // Viterbi decoder using log-domain
    constexpr double NEG_INF = -1e30;
    vector<vector<double>> path_metrics(num_stages + 1, vector<double>(NUM_STATES, NEG_INF));
    vector<vector<uint8_t>> path_history(num_stages + 1, vector<uint8_t>(NUM_STATES, 0));
    
    // Initialize (start in zero state)
    path_metrics[0][0] = 0.0;
    
    // Forward pass
    for (int stage = 0; stage < num_stages; stage++) {
        double llr0 = received_llr[2 * stage];     // LLR for first output bit
        double llr1 = received_llr[2 * stage + 1]; // LLR for second output bit
        
        for (int state = 0; state < NUM_STATES; state++) {
            if (path_metrics[stage][state] == NEG_INF) continue;
            
            for (int input = 0; input < 2; input++) {
                uint8_t next_state = state_table[state].next_state[input];
                uint8_t output = state_table[state].output[input];
                
                // Compute branch metric (correlation with received LLRs)
                double branch_metric = 0.0;
                if ((output >> 1) & 1) branch_metric += llr0; else branch_metric -= llr0;
                if (output & 1) branch_metric += llr1; else branch_metric -= llr1;
                
                double new_metric = path_metrics[stage][state] + branch_metric;
                
                // Select survivor path
                if (new_metric > path_metrics[stage + 1][next_state]) {
                    path_metrics[stage + 1][next_state] = new_metric;
                    path_history[stage + 1][next_state] = (state << 1) | input;
                }
            }
        }
    }
    
    // Backward pass (traceback from zero state)
    uint8_t current_state = 0;
    for (int stage = num_stages; stage > 0; stage--) {
        uint8_t history = path_history[stage][current_state];
        uint8_t prev_state = history >> 1;
        uint8_t input = history & 1;
        
        // Only store information bits (not tail bits)
        if (stage <= info_len) {
            decoded_bits[stage - 1] = (input == 1);
        }
        
        current_state = prev_state;
    }
    
    return 0; // Success
}

// =============================================================================
// SOFT DECISION HELPER
// =============================================================================

extern "C" void hard_to_soft_llr(const double* received_symbols, int num_symbols,
                                 double* llr_output, double noise_variance) {
    // Convert received symbols to LLR for BPSK: LLR = 2*r/σ²
    double factor = 2.0 / noise_variance;
    for (int i = 0; i < num_symbols; i++) {
        llr_output[i] = received_symbols[i] * factor;
    }
}

// =============================================================================
// CODING GAIN ESTIMATION
// =============================================================================

extern "C" double estimate_coding_gain_db() {
    // Theoretical coding gain for K=7, rate 1/2 convolutional code
    // Free distance = 10, coding gain ≈ 10*log10(10*0.5) ≈ 7 dB
    return 7.0; // Approximate coding gain in dB
}

// =============================================================================
// TEST AND VALIDATION FUNCTIONS
// =============================================================================

extern "C" int test_convolutional_coding() {
    init_conv_state_table();
    
    // Test with simple pattern
    const int test_len = 10;
    bool info_bits[test_len] = {1, 0, 1, 1, 0, 1, 0, 0, 1, 1};
    bool coded_bits[100];
    int coded_len;
    
    // Encode
    if (convolutional_encode(info_bits, test_len, coded_bits, &coded_len) != 0) {
        return -1;
    }
    
    // Convert to soft LLR (perfect channel)
    vector<double> llr(coded_len);
    for (int i = 0; i < coded_len; i++) {
        llr[i] = coded_bits[i] ? 10.0 : -10.0; // High confidence
    }
    
    // Decode
    bool decoded_bits[test_len];
    int decoded_len;
    if (viterbi_decode(llr.data(), coded_len, decoded_bits, &decoded_len) != 0) {
        return -2;
    }
    
    // Verify
    if (decoded_len != test_len) return -3;
    for (int i = 0; i < test_len; i++) {
        if (decoded_bits[i] != info_bits[i]) return -4;
    }
    
    return 0; // Success - perfect decode
}