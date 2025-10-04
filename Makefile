### Minimal Makefile (simplified) ###
# Original full version saved as Makefile.full_backup

CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic

TARGET := ber_tests
SRC := ber.cpp test_main.cpp

.PHONY: all test clean help shared run run-csv run-plot run-full bench

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TARGET)
	./$(TARGET)

shared: ber.cpp
	$(CXX) $(CXXFLAGS) -shared -fPIC -o ber.so ber.cpp -lm

clean:
	rm -f $(TARGET) ber.so *.o

help:
	@echo "Minimal targets:"
	@echo "  make / make all  -> build tests"
	@echo "  make test        -> run C++ tests"
	@echo "  make shared      -> build ber.so for Python"
	@echo "  make run         -> quick simulation (no outputs)"
	@echo "  make run-csv     -> simulation with CSV output (results.csv)"
	@echo "  make run-plot    -> simulation with plots (interactive)"
	@echo "  make run-full    -> simulation with CSV + saved plots"
	@echo "  make bench       -> quick timing of core BER kernel"
	@echo "  make clean       -> remove build outputs"
	@echo "Restore full system: copy Makefile.full_backup over Makefile manually."

run: shared
	@echo "Running BER simulation..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 6 --snr-step 2 --bits 30000 --runs 1 --no-plot

run-csv: shared
	@echo "Running BER simulation with CSV output..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 8 --snr-step 1 --bits 50000 --runs 1 --csv results.csv --no-plot

run-plot: shared
	@echo "Running BER simulation with plots..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 8 --snr-step 1 --bits 50000 --runs 1

run-full: shared
	@echo "Running BER simulation with CSV and plots..."
	python3 run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 10 --snr-step 1 --bits 80000 --runs 2 --csv results.csv --save-prefix ber

bench: shared
	@echo "Benchmarking BER core (BPSK @ 6 dB, scaling bits)..."
	@python3 test_amc.py --bench --bench-snr 6.0 --bench-mod 2 --bench-sizes 5000,20000,80000,320000