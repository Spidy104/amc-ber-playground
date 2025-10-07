#ifndef CODING_H
#define CODING_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convolutional encoder with constraint length K=7, rate 1/2
 * @param info_bits Input information bits
 * @param info_len Number of information bits
 * @param coded_bits Output coded bits (must have space for 2*info_len bits)
 * @param coded_len Pointer to store actual number of coded bits
 * @return 0 on success, non-zero on error
 */
int convolutional_encode(const bool* info_bits, int info_len, 
                        bool* coded_bits, int* coded_len);

/**
 * Viterbi decoder for K=7, rate 1/2 convolutional code
 * @param received_llr Log-likelihood ratios of received bits
 * @param received_len Number of received LLR values (should be 2*info_len)
 * @param decoded_bits Output decoded information bits
 * @param decoded_len Pointer to store number of decoded bits
 * @return 0 on success, non-zero on error
 */
int viterbi_decode(const double* received_llr, int received_len,
                  bool* decoded_bits, int* decoded_len);

#ifdef __cplusplus
}
#endif

#endif // CODING_H