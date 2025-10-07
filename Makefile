### Minimal Makefile (simplified) ###
# Original full version saved as Makefile.full_backup

CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic

# ---------------------------------------------------------------------------
# Configurable simulation scales (override on command line as needed):
#   make run-coded BITS=4000000 SNR_STOP=14
# ---------------------------------------------------------------------------
BITS        ?= 1000000          # Default bits for quick runs
DEEP_BITS   ?= 20000000         # Deep run bit count
ULTRA_BITS  ?= 50000000         # Ultra deep floor probing
RUNS        ?= 1
DEEP_RUNS   ?= 2
ULTRA_RUNS  ?= 3
SNR_START   ?= 0
SNR_STOP    ?= 12
SNR_STEP    ?= 1
DEEP_STOP   ?= 18
FULL_STOP   ?= 16
ULTRA_STOP  ?= 20

TARGET := ber_tests
SRC := ber.cpp test_main.cpp

.PHONY: all test clean help shared run run-csv run-plot run-full bench bench-multi bench-gain bench-csv bench-all bench-16qam

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TARGET)
	./$(TARGET)

shared: ber.cpp coding.cpp
	$(CXX) $(CXXFLAGS) -shared -fPIC -o ber.so ber.cpp coding.cpp -lm

# Test standalone coding functions
test-coding: test_coding.cpp coding.cpp
	$(CXX) $(CXXFLAGS) -o test_coding test_coding.cpp coding.cpp -lm

run-test-coding: test-coding
	./test_coding

clean:
	rm -f $(TARGET) ber.so test_coding *.o *.png *.csv

help:
	@echo "Minimal targets:"
	@echo "  make / make all  -> build tests"
	@echo "  make test        -> run C++ tests"
	@echo "  make shared      -> build ber.so for Python"
	@echo "  make run         -> quick simulation (no outputs)"
	@echo "  make run-csv     -> simulation with CSV output (results.csv)"
	@echo "  make run-plot    -> simulation with plots (interactive)"
	@echo "  make run-full    -> simulation with CSV + saved plots"
	@echo "  make bench       -> legacy quick single-mod benchmark"
	@echo "  make bench-multi -> multi-mod benchmark (uses --bench-mods)"
	@echo "  make bench-gain  -> multi-mod benchmark incl. coding gain"
	@echo "  make bench-csv   -> multi-mod benchmark exporting bench_multi.csv"
	@echo "  make bench-all   -> gain + CSV + multi-mod"
	@echo "  make bench-16qam -> focused 16-QAM benchmark (higher SNR)"
	@echo "  make clean       -> remove build outputs"
	@echo "Restore full system: copy Makefile.full_backup over Makefile manually."

run: shared
	@echo "Running BER simulation (bits=$(BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start $(SNR_START) --snr-stop $(SNR_STOP) --snr-step 2 --bits $(BITS) --runs $(RUNS) --no-plot

run-csv: shared
	@echo "Running BER simulation with CSV output (bits=$(BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 15 --snr-step 1 --bits $(BITS) --runs $(RUNS) --csv results.csv --no-plot

run-plot: shared
	@echo "Running BER simulation with plots (bits=$(BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 15 --snr-step 1 --bits $(BITS) --runs $(RUNS)

run-full: shared
	@echo "Running BER simulation (full) bits=$(BITS) runs=$(RUNS)..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop $(FULL_STOP) --snr-step 0.5 --bits $(BITS) --runs $(RUNS) --csv results.csv --save-prefix ber

run-deep: shared
	@echo "Running deep BER simulation (bits=$(DEEP_BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop $(DEEP_STOP) --snr-step 0.5 --bits $(DEEP_BITS) --runs $(DEEP_RUNS) --csv results_deep.csv --save-prefix ber_deep

run-ultra: shared
	@echo "Running ultra-deep BER simulation (bits=$(ULTRA_BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop $(ULTRA_STOP) --snr-step 0.5 --bits $(ULTRA_BITS) --runs $(ULTRA_RUNS) --csv results_ultra.csv --save-prefix ber_ultra

run-coded: shared
	@echo "Running coded BER simulation (bits=$(BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start $(SNR_START) --snr-stop $(SNR_STOP) --snr-step $(SNR_STEP) --bits $(BITS) --runs $(RUNS) --coding --save-prefix ber_coded

run-coded-only: shared
	@echo "Running coded-only BER simulation (bits=$(BITS))..."
	python3 run_amc.py --mods 2,4,16 --snr-start $(SNR_START) --snr-stop $(SNR_STOP) --snr-step $(SNR_STEP) --bits $(BITS) --runs $(RUNS) --coding --coded-only --save-prefix ber_coded_only

bench: shared
	@echo "Benchmarking BER core (BPSK @ 6 dB, scaling bits)..."
	@python3 test_amc.py --bench --bench-snr 6.0 --bench-mod 2 --bench-sizes 5000,20000,80000,320000

# ---------------------------------------------------------------------------
# Enhanced benchmarking (multi-mod & coding gain)
# Overridable variables:
#   BENCH_SNRS   : comma list of SNRs (currently single value used per run)
#   BENCH_SNR    : primary SNR point (fallback for single-run targets)
#   BENCH_MODS   : modulation orders to include
#   BENCH_SIZES  : bit chunk sizes (accumulated)
# Example:
#   make bench-multi BENCH_SNR=8 BENCH_MODS=2,4 BENCH_SIZES=10000,40000
# ---------------------------------------------------------------------------
BENCH_SNR   ?= 6.0
BENCH_MODS  ?= 2,4,16
BENCH_SIZES ?= 5000,20000,80000,320000

bench-multi: shared
	@echo "Multi-mod benchmark (mods=$(BENCH_MODS), SNR=$(BENCH_SNR), sizes=$(BENCH_SIZES))..."
	@python3 test_amc.py --bench --bench-snr $(BENCH_SNR) --bench-mods $(BENCH_MODS) --bench-sizes $(BENCH_SIZES)

bench-gain: shared
	@echo "Multi-mod benchmark with coding gain (mods=$(BENCH_MODS), SNR=$(BENCH_SNR))..."
	@python3 test_amc.py --bench --bench-snr $(BENCH_SNR) --bench-mods $(BENCH_MODS) --bench-sizes $(BENCH_SIZES) --bench-gain

bench-csv: shared
	@echo "Multi-mod benchmark with CSV export (bench_multi.csv)..."
	@python3 test_amc.py --bench --bench-snr $(BENCH_SNR) --bench-mods $(BENCH_MODS) --bench-sizes $(BENCH_SIZES) --bench-csv bench_multi.csv

bench-all: shared
	@echo "Comprehensive benchmark (CSV + gain) ..."
	@python3 test_amc.py --bench --bench-snr $(BENCH_SNR) --bench-mods $(BENCH_MODS) --bench-sizes $(BENCH_SIZES) --bench-gain --bench-csv bench_multi.csv

bench-16qam: shared
	@echo "Focused 16-QAM benchmark (higher SNR=12 dB)..."
	@python3 test_amc.py --bench --bench-snr 12.0 --bench-mods 16 --bench-sizes 10000,40000,160000 --bench-gain --bench-csv bench_16qam.csv