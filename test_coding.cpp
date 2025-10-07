#include <iostream>
#include <vector>
#include <cassert>
#include "coding.h"

using namespace std;

void test_small_encoding() {
    cout << "=== Testing Small Input (4 bits) ===" << endl;
    
    // Test with 4 information bits
    bool info_bits[] = {true, false, true, false};  // 1010
    int info_len = 4;
    
    // K=7 means K-1=6 tail bits, so total = 4+6=10 info bits
    // Rate 1/2 means output = 10*2 = 20 coded bits
    int max_coded_len = 2 * (info_len + 6);
    bool* coded_bits = new bool[max_coded_len];
    int actual_coded_len;
    
    cout << "Input bits: ";
    for (int i = 0; i < info_len; i++) {
        cout << (info_bits[i] ? "1" : "0");
    }
    cout << endl;
    
    // Test encoding
    int encode_result = convolutional_encode(info_bits, info_len, coded_bits, &actual_coded_len);
    cout << "Encode result: " << encode_result << endl;
    cout << "Expected coded length: " << max_coded_len << ", actual: " << actual_coded_len << endl;
    
    if (encode_result == 0) {
        cout << "Coded bits: ";
        for (int i = 0; i < actual_coded_len; i++) {
            cout << (coded_bits[i] ? "1" : "0");
            if (i % 2 == 1) cout << " ";  // Group by pairs
        }
        cout << endl;
        
        // Test decoding with perfect channel (no noise)
        vector<double> llr(actual_coded_len);
        for (int i = 0; i < actual_coded_len; i++) {
            // LLR convention: decoder adds LLR when output=1, subtracts when output=0
            // So for correct decoding: LLR = +10 when bit=1, -10 when bit=0
            llr[i] = coded_bits[i] ? 10.0 : -10.0;
        }
        
        bool* decoded_bits = new bool[info_len];
        int decoded_len;
        int decode_result = viterbi_decode(llr.data(), actual_coded_len, decoded_bits, &decoded_len);
        
        cout << "Decode result: " << decode_result << endl;
        cout << "Decoded length: " << decoded_len << endl;
        
        if (decode_result == 0) {
            cout << "Decoded bits: ";
            for (int i = 0; i < decoded_len; i++) {
                cout << (decoded_bits[i] ? "1" : "0");
            }
            cout << endl;
            
            // Check if decoding matches original
            bool match = true;
            int compare_len = min(info_len, decoded_len);
            for (int i = 0; i < compare_len; i++) {
                if (info_bits[i] != decoded_bits[i]) {
                    match = false;
                    break;
                }
            }
            
            cout << "Perfect channel test: " << (match ? "PASS" : "FAIL") << endl;
        }
        
        delete[] decoded_bits;
    }
    
    delete[] coded_bits;
    cout << endl;
}

void test_medium_encoding() {
    cout << "=== Testing Medium Input (100 bits) ===" << endl;
    
    int info_len = 100;
    
    // Generate simple pattern
    bool* info_bits = new bool[info_len];
    for (int i = 0; i < info_len; i++) {
        info_bits[i] = (i % 3 == 0);  // Pattern: 100100100...
    }
    
    int max_coded_len = 2 * (info_len + 6);
    bool* coded_bits = new bool[max_coded_len];
    int actual_coded_len;
    
    cout << "Testing with " << info_len << " information bits..." << endl;
    
    // Test encoding
    int encode_result = convolutional_encode(info_bits, info_len, coded_bits, &actual_coded_len);
    cout << "Encode result: " << encode_result << endl;
    cout << "Expected coded length: " << max_coded_len << ", actual: " << actual_coded_len << endl;
    
    if (encode_result == 0) {
        // Test decoding with perfect channel
        vector<double> llr(actual_coded_len);
        for (int i = 0; i < actual_coded_len; i++) {
            // LLR convention: decoder adds LLR when output=1, subtracts when output=0
            llr[i] = coded_bits[i] ? 10.0 : -10.0;
        }
        
        bool* decoded_bits = new bool[info_len];
        int decoded_len;
        int decode_result = viterbi_decode(llr.data(), actual_coded_len, decoded_bits, &decoded_len);
        
        cout << "Decode result: " << decode_result << endl;
        cout << "Decoded length: " << decoded_len << endl;
        
        if (decode_result == 0) {
            // Check error rate
            int errors = 0;
            int compare_len = min(info_len, decoded_len);
            for (int i = 0; i < compare_len; i++) {
                if (info_bits[i] != decoded_bits[i]) {
                    errors++;
                }
            }
            
            double error_rate = (double)errors / compare_len;
            cout << "Error rate: " << errors << "/" << compare_len << " = " << error_rate << endl;
            cout << "Perfect channel test: " << (error_rate == 0.0 ? "PASS" : "FAIL") << endl;
        }
        
        delete[] decoded_bits;
    }
    
    delete[] info_bits;
    delete[] coded_bits;
    cout << endl;
}

void test_large_encoding() {
    cout << "=== Testing Large Input (1000 bits) ===" << endl;
    
    int info_len = 1000;
    
    // Generate alternating pattern
    bool* info_bits = new bool[info_len];
    for (int i = 0; i < info_len; i++) {
        info_bits[i] = (i % 2 == 0);  // Pattern: 101010...
    }
    
    int max_coded_len = 2 * (info_len + 6);
    bool* coded_bits = new bool[max_coded_len];
    int actual_coded_len;
    
    cout << "Testing with " << info_len << " information bits..." << endl;
    cout << "Allocated coded array size: " << max_coded_len << endl;
    
    // Test encoding
    int encode_result = convolutional_encode(info_bits, info_len, coded_bits, &actual_coded_len);
    cout << "Encode result: " << encode_result << endl;
    cout << "Expected coded length: " << max_coded_len << ", actual: " << actual_coded_len << endl;
    
    if (encode_result == 0 && actual_coded_len <= max_coded_len) {
        cout << "Encoding successful - no buffer overflow" << endl;
        
        // Test decoding with perfect channel
        vector<double> llr(actual_coded_len);
        for (int i = 0; i < actual_coded_len; i++) {
            // LLR convention: decoder adds LLR when output=1, subtracts when output=0
            llr[i] = coded_bits[i] ? 10.0 : -10.0;
        }
        
        bool* decoded_bits = new bool[info_len];
        int decoded_len;
        int decode_result = viterbi_decode(llr.data(), actual_coded_len, decoded_bits, &decoded_len);
        
        cout << "Decode result: " << decode_result << endl;
        cout << "Decoded length: " << decoded_len << endl;
        
        if (decode_result == 0) {
            // Check first 10 and last 10 bits
            cout << "First 10 bits - Original: ";
            for (int i = 0; i < 10 && i < info_len; i++) {
                cout << (info_bits[i] ? "1" : "0");
            }
            cout << ", Decoded: ";
            for (int i = 0; i < 10 && i < decoded_len; i++) {
                cout << (decoded_bits[i] ? "1" : "0");
            }
            cout << endl;
            
            // Check error rate
            int errors = 0;
            int compare_len = min(info_len, decoded_len);
            for (int i = 0; i < compare_len; i++) {
                if (info_bits[i] != decoded_bits[i]) {
                    errors++;
                }
            }
            
            double error_rate = (double)errors / compare_len;
            cout << "Error rate: " << errors << "/" << compare_len << " = " << error_rate << endl;
            cout << "Perfect channel test: " << (error_rate == 0.0 ? "PASS" : "FAIL") << endl;
        }
        
        delete[] decoded_bits;
    } else {
        cout << "Encoding failed or buffer overflow detected!" << endl;
    }
    
    delete[] info_bits;
    delete[] coded_bits;
    cout << endl;
}

int main() {
    cout << "Convolutional Coding Test Suite" << endl;
    cout << "================================" << endl;
    
    // Test with increasing sizes to find the breaking point
    test_small_encoding();
    test_medium_encoding(); 
    test_large_encoding();
    
    cout << "All tests completed." << endl;
    return 0;
}