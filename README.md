# Adaptive Modulation & Coding (AMC) BER Playground

> **Origin Story:** After a late-night (around 10:30) conversation, we walked into an Embedded Systems Design class that basically nobody else showed up for. Instead of wasting the empty period, we hacked together the first C++ BER core and an early API on the spot. Dheeraj's questions and framing pushed the direction, Darshan's simple curiosity poked holes at the right moments, and this mini playground emerged.

---

## Overview

This mini project lets you:
- **Simulate Bit Error Rate (BER)** for BPSK, QPSK, and 16-QAM over AWGN channels
- **Compare simulated vs theoretical** performance curves
- **Generate CSV data** + publication-style plots
- **Experiment with adaptive modulation** decisions based on estimated SNR
- **Run fast C++ Monte Carlo kernels** with a lightweight Python front-end

Everything is intentionally minimal: one C++ source file (`ber.cpp`), one Python driver (`run_amc.py`), and a simple `Makefile`.

---

## Key Features

- **High-speed C++20 BER engine** (BPSK/QPSK/16-QAM)
- **Deterministic seeded BER function** (if compiled with `compute_ber_seeded`)
- **On-the-fly SNR estimation** using pilot symbols
- **Theoretical overlays**: BPSK/QPSK and 16-QAM approximations
- **Multiple run modes**: quick/no output, CSV only, plots, or full export
-  *Clean separation** between simulation core and Python presentation layer

---

## Project Structure

```
.
├── ber.cpp                 # C++ modulation/demod + BER + tests + SNR estimation
├── run_amc.py              # Python CLI for sweeping SNR and plotting/exporting
├── test_amc.py             # Benchmarking script
├── Makefile                # Minimal build + run targets
├── Makefile.full_backup    # Original extended build (kept for reference)
├── requirements.txt        # Python dependencies (numpy, matplotlib, scipy, etc.)
├── results.csv             # (Generated) BER sweep data
├── ber_*.png               # (Generated) Plots
├── LICENSE                 # MIT License
└── .gitignore             # Ignore build + output artifacts
```

---

## Dependencies

### C++
- **g++** with C++20 support (`-std=c++20`)
- Standard library with `<random>`, `<complex>`, `<cmath>`

### Python
Install dependencies (prefer virtualenv):

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

**Required packages:**
- `numpy` - Numerical computing
- `matplotlib` - Plotting and visualization
- `scipy` - Special functions (erfc, Q-function)
- `argparse` - Command-line interface (stdlib)

> **Note:** SciPy may warn if NumPy version drifts; functionality still works for these routines.

---

## Build & Test

### Build C++ test binary:
```bash
make
```

### Run C++ tests (mod/demod, BER accuracy, SNR estimation):
```bash
make test
```

### Build shared library for Python:
```bash
make shared
```

### Clean artifacts:
```bash
make clean
```

---

## ▶Run Modes (Make Targets)

| Target           | What it does                                        |
|------------------|-----------------------------------------------------|
| `make run`       | Quick simulation (no plots / CSV)                   |
| `make run-csv`   | Generates `results.csv`                            |
| `make run-plot`  | Interactive BER + SNR plots                        |
| `make run-full`  | CSV + saved plots (`ber_ber.png`, `ber_snr.png`)  |
| `make bench`     | Performance benchmark of BER kernel                |

---

## Direct Python Usage

After `make shared`:

**Full featured run:**
```bash
python run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 10 --snr-step 1 \
                  --bits 80000 --runs 2 --csv results.csv --save-prefix ber
```

**Minimal fast run (no plots):**
```bash
python run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 6 --snr-step 2 \
                  --bits 30000 --runs 1 --no-plot
```

**High-precision deep BER:**
```bash
python run_amc.py --mods 2,4 --snr-start 0 --snr-stop 12 --snr-step 0.5 \
                  --bits 5000000 --runs 3 --csv deep_ber.csv --save-prefix deep
```

---

## Output Artifacts

- **`results.csv`** — Columns: `SNR_dB`, `BER_mod2`, `BER_mod4`, `BER_mod16`
- **`ber_ber.png`** — BER vs Eb/N0 (simulated + theoretical curves)
- **`ber_snr.png`** — SNR estimation accuracy comparison
- **Adaptive modulation trace** — Printed to console (unless `--quiet`)

---

##  Theory & Mathematical Background

### Digital Modulation Fundamentals

Digital modulation maps information bits to complex symbols transmitted over a channel. The **Bit Error Rate (BER)** measures the probability that a received bit differs from the transmitted bit due to channel noise.

### Channel Model: AWGN

The **Additive White Gaussian Noise (AWGN)** channel model:

$$r = s + n$$

Where:
- $r$ = received signal
- $s$ = transmitted symbol
- $n \sim \mathcal{CN}(0, N_0/2)$ = complex Gaussian noise with power spectral density $N_0/2$ per dimension

### Energy-to-Noise Ratio

The signal-to-noise ratio per bit:

$$\frac{E_b}{N_0} = 10^{\left(\frac{E_b/N_0 \text{ (dB)}}{10}\right)}$$

Where:
- $E_b$ = energy per bit
- $N_0$ = noise power spectral density

For $M$-ary modulation:

$$\frac{E_s}{N_0} = \frac{E_b}{N_0} \cdot \log_2(M)$$

Where $E_s$ is the energy per symbol.

---

### BPSK (Binary Phase Shift Keying)

**Constellation:** Two points at $\{+1, -1\}$  
**Bits per symbol:** 1

**Theoretical BER:**

$$P_b = Q\left(\sqrt{2 \cdot \frac{E_b}{N_0}}\right) = \frac{1}{2}\text{erfc}\left(\sqrt{\frac{E_b}{N_0}}\right)$$

Where:
- $Q(x) = \frac{1}{2}\text{erfc}\left(\frac{x}{\sqrt{2}}\right)$ is the Q-function (tail probability of standard normal)
- $\text{erfc}(x) = \frac{2}{\sqrt{\pi}}\int_x^\infty e^{-t^2}dt$ is the complementary error function

**Intuition:** BPSK has maximum distance between symbols, making it the most robust binary modulation scheme.

---

### QPSK (Quadrature Phase Shift Keying)

**Constellation:** Four points at $\{\pm 1 \pm j\}/\sqrt{2}$  
**Bits per symbol:** 2

**Theoretical BER:**

$$P_b = Q\left(\sqrt{2 \cdot \frac{E_b}{N_0}}\right) = \frac{1}{2}\text{erfc}\left(\sqrt{\frac{E_b}{N_0}}\right)$$

**Key insight:** QPSK maintains the same BER as BPSK (per bit) because each dimension (I/Q) is essentially independent BPSK. However, QPSK transmits **twice the data rate** for the same bandwidth.

**Symbol Error Rate (SER):**

$$P_s = 1 - \left[1 - Q\left(\sqrt{\frac{E_s}{N_0}}\right)\right]^2 \approx 2Q\left(\sqrt{\frac{E_s}{N_0}}\right)$$

---

### 16-QAM (16-Quadrature Amplitude Modulation)

**Constellation:** 4×4 grid with symbols at $(\pm 1, \pm 1), (\pm 1, \pm 3), (\pm 3, \pm 1), (\pm 3, \pm 3)$ (normalized)  
**Bits per symbol:** 4

**Approximate BER** (assuming Gray coding):

$$P_b \approx \frac{3}{4} \cdot Q\left(\sqrt{\frac{4}{5} \cdot \frac{E_b}{N_0}}\right)$$

**More accurate approximation:**

$$P_b \approx \frac{1}{4}\left[3Q\left(\sqrt{\frac{4E_s}{10N_0}}\right) + Q\left(3\sqrt{\frac{4E_s}{10N_0}}\right)\right]$$

Where $E_s = 4E_b$ for 16-QAM.

**Trade-off:** 16-QAM transmits 4 bits per symbol (2× QPSK) but requires higher SNR for the same BER due to reduced minimum distance between symbols.

---

### Performance Comparison

| Modulation | Bits/Symbol | Relative SNR (dB) for BER=10⁻³ | Spectral Efficiency |
|------------|-------------|--------------------------------|---------------------|
| BPSK       | 1           | ~6.8 dB                        | 1 bit/s/Hz          |
| QPSK       | 2           | ~6.8 dB                        | 2 bit/s/Hz          |
| 16-QAM     | 4           | ~10.5 dB                       | 4 bit/s/Hz          |

**Key insight:** Higher-order modulation trades power efficiency for spectral efficiency.

---

### SNR Estimation via Pilot Symbols

The simulator estimates SNR using known pilot symbols:

$$\hat{\gamma} = \frac{\mathbb{E}[|s|^2]}{\mathbb{E}[|r - s|^2]}$$

Where:
- $s$ = known transmitted pilot
- $r$ = received pilot
- $|r - s|^2$ estimates noise variance

This enables **adaptive modulation**: switching between BPSK/QPSK/16-QAM based on channel quality.

---

## Statistical Notes

### Monte Carlo Simulation Accuracy

- **Low BER (< 10⁻⁵)** requires many bits for stable estimates
- Rule of thumb: Need ~100 errors for 10% accuracy → For BER=10⁻⁵, simulate ≥10⁷ bits
- **Confidence interval** (approximate): $\sigma_{BER} \approx \sqrt{\frac{BER(1-BER)}{N_{bits}}}$

### Practical Guidelines

| Target BER | Recommended Bits | Simulation Time |
|------------|------------------|-----------------|
| 10⁻² | 10,000 | < 1 second |
| 10⁻³ | 100,000 | ~1 second |
| 10⁻⁴ | 1,000,000 | ~10 seconds |
| 10⁻⁵ | 10,000,000 | ~100 seconds |

**Tips:**
- Use `--runs > 1` to average out Monte Carlo noise
- Increase `--bits` for deeper BER regions
- Threshold adaptation example uses estimated SNR vs true SNR trace
- For publication-quality curves, use `--bits 5000000` and `--runs 5`

---

## CLI Options

```
--mods 2,4,16              Modulation orders to simulate (2=BPSK, 4=QPSK, 16=16QAM)
--snr-start FLOAT          Starting Eb/N0 in dB (default: 0)
--snr-stop FLOAT           Ending Eb/N0 in dB (default: 10)
--snr-step FLOAT           Step size in dB (default: 1)
--bits INT                 Bits per SNR point (adjusted to symbol multiple)
--runs INT                 Repeats per SNR for averaging (default: 1)
--csv FILE                 Export results to CSV file
--save-prefix PREFIX       Save plots as PREFIX_*.png
--no-plot                  Disable interactive plotting
--quiet                    Suppress progress prints
--pilots INT               Number of pilot symbols for SNR estimation
--bench                    Run performance benchmark
--bench-mod INT            Modulation for benchmark (default: 2)
--bench-snr FLOAT          SNR for benchmark (default: 6.0)
--bench-sizes LIST         Bit sizes for benchmark (comma-separated)
```

Run `python run_amc.py -h` for full list.

---

## Benchmarking

Measure raw BER kernel performance:

```bash
make bench
```

**Sample output (BPSK @ 6 dB):**
```
AMC BER Benchmark
=================
Modulation: 2, SNR=6.0 dB
    bits         ber   time(ms)   throughput(Mbit/s)  scale
-------------------------------------------------------------
    5000    1.00e-03       0.66                 7.53  --
   20000    2.80e-03       3.03                 6.61  x4.0
   80000    2.37e-03      11.62                 6.88  x4.0
  320000    2.28e-03      30.67                10.43  x4.0
```

**Interpretation:**
- **bits**: Number of simulated bits in Monte Carlo kernel
- **ber**: Measured BER at that SNR (statistical noise expected for small sizes)
- **time(ms)**: Wall-clock time for that call
- **throughput**: Effective simulated bit rate (higher for larger batches)
- **scale**: Growth factor relative to previous size

**Custom benchmarks:**
```bash
python test_amc.py --bench --bench-mod 16 --bench-snr 8 --bench-sizes 10000,40000,160000
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| SciPy NumPy version warning | Version mismatch | Pin versions in `requirements.txt` |
| `compute_ber_seeded` missing | Older build | Re-run `make shared` |
| Blank/No plot window (Wayland) | Backend issue | Try `MPLBACKEND=Agg` or use `--save-prefix` |
| Very flat BER curve | Too few bits | Increase `--bits` or `--runs` |
| Import error for C++ module | Library not built | Run `make shared` first |
| High noise in low SNR | Statistical variance | Increase `--bits` and `--runs` |

---

## Possible Enhancements

### Short-term
- [ ] Add 64-QAM / 256-QAM modulation schemes
- [ ] Constellation diagram plotting with decision boundaries
- [ ] Parallel SNR sweep using Python multiprocessing
- [ ] Error vector magnitude (EVM) calculation

---

## Example Workflows

### Quick exploration (fast):
```bash
make run
```

### Generate publication plot:
```bash
python run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 12 --snr-step 0.5 \
                  --bits 1000000 --runs 5 --csv paper_results.csv \
                  --save-prefix paper --quiet
```

### Compare theory vs simulation:
```bash
python run_amc.py --mods 2 --snr-start 0 --snr-stop 10 --snr-step 0.5 \
                  --bits 500000 --runs 3 --save-prefix bpsk_validation
```

### Benchmark performance:
```bash
make bench
# Or custom:
python test_amc.py --bench --bench-mod 4 --bench-snr 8 --bench-sizes 50000,200000,800000
```

---

## Credits

- **Core coding**: You (rapid prototyping in the empty class) + Darshan (simple questions to refine the API)
- **Concept & initial spark**: [Dheeraj](https://github.com/dj-49)
- **Question prompts & catalyst**: Darshan
- **Early session**: Empty Embedded Systems Design class + previous night's 10:30 chat
- **Energy source**: Improvised build > unattended lecture

---

## License

This project is licensed under the **MIT License**.

See the [`LICENSE`](./LICENSE) file for full text.

---

## References

### Books
- Proakis & Salehi, *Digital Communications* (5th Ed.)
- Goldsmith, *Wireless Communications*
- Sklar, *Digital Communications: Fundamentals and Applications*

### Standards
- IEEE 802.11 (WiFi) - Adaptive modulation in OFDM
- 3GPP LTE/5G - Link adaptation specifications

### Online Resources
- [Wireless Pi - BER Curves](https://wirelesspi.com/)
- [CommPy - Digital Communications in Python](https://github.com/veeresht/CommPy)

---

**⚡ Built during an empty classroom session, refined with curiosity and late-night conversations.**