# Adaptive Modulation & Coding (AMC) BER Playground

> Origin story: After a late-night (around 10:30) conversation, we walked into an Embedded Systems Design class that basically nobody else showed up for. Instead of wasting the empty period, we hacked together the first C++ BER core and an early API on the spot. Dheeraj’s questions and framing pushed the direction, Darshan’s simple curiosity poked holes at the right moments, and this mini playground emerged.

## Overview
This mini project lets you:
- Simulate Bit Error Rate (BER) for BPSK, QPSK, and 16-QAM over AWGN
- Compare simulated vs theoretical performance curves
- Generate CSV data + publication-style plots
- Experiment with adaptive modulation decisions based on estimated SNR
- Run fast C++ Monte Carlo kernels with a lightweight Python front-end

Everything is intentionally minimal: one C++ source file (`ber.cpp`), one Python driver (`run_amc.py`), and a simple `Makefile`.

## Key Features
- High-speed C++20 BER engine (BPSK/QPSK/16-QAM)
- Deterministic seeded BER function (if compiled with `compute_ber_seeded`)
- On-the-fly SNR estimation using pilot symbols
- Theoretical overlays: BPSK/QPSK and 16-QAM approximations
- Multiple run modes: quick/no output, CSV only, plots, or full export
- Clean separation between simulation core and Python presentation layer

## Project Structure
```
ber.cpp          # C++ modulation/demod + BER + tests + SNR estimation
run_amc.py       # Python CLI for sweeping SNR and plotting / exporting
Makefile         # Minimal build + run targets
Makefile.full_backup # Original extended build (kept for reference)
requirements.txt # Python dependencies (numpy, matplotlib, scipy, etc.)
results.csv      # (Generated) BER sweep data
ber_*.png        # (Generated) Plots
.gitignore       # Ignore build + output artifacts
```

## Dependencies
### C++
- g++ with C++20 support

### Python
Install deps (prefer venv):
```
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```
> Note: SciPy may warn if NumPy version drifts; functionality still works for these routines.

## Build & Test
Build C++ test binary:
```
make
```
Run C++ tests (mod/demod, BER accuracy, SNR estimation):
```
make test
```
Build shared library for Python:
```
make shared
```
Clean artifacts:
```
make clean
```

## ▶ Run Modes (Make Targets)
| Target      | What it does                                      |
|-------------|----------------------------------------------------|
| `make run`       | Quick simulation (no plots / CSV)                |
| `make run-csv`   | Generates `results.csv`                          |
| `make run-plot`  | Interactive BER + SNR plots                      |
| `make run-full`  | CSV + saved plots (`ber_ber.png`, `ber_snr.png`) |

## Direct Python Usage
After `make shared`:
```
python run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 10 --snr-step 1 --bits 80000 --runs 2 --csv results.csv --save-prefix ber
```
Minimal fast run (no plots):
```
python run_amc.py --mods 2,4,16 --snr-start 0 --snr-stop 6 --snr-step 2 --bits 30000 --runs 1 --no-plot
```

## Output Artifacts
- `results.csv` — columns: SNR_dB, BER_mod2, BER_mod4, BER_mod16
- `ber_ber.png` — BER vs Eb/N0 (sim + theory)
- `ber_snr.png` — SNR estimation accuracy
- Adaptive modulation decision trace printed (if not `--quiet`)

## Theory Quick Reference
For $E_b/N_0$ in dB: $E_b/N_0(\text{linear}) = 10^{\frac{E_b/N_0(\text{dB})}{10}}$

BPSK / QPSK BER:
$$P_b = Q\left(\sqrt{2 E_b/N_0}\right) = \tfrac{1}{2}\operatorname{erfc}\left(\sqrt{E_b/N_0}\right)$$

16-QAM (approx):
$$P_b \approx \frac{1}{4}\left(3 Q\big(\sqrt{\tfrac{2}{5} E_b/N_0}\big) + Q\big(3\sqrt{\tfrac{2}{5} E_b/N_0}\big)\right)$$

Where $Q(x) = \tfrac{1}{2}\operatorname{erfc}\left(\tfrac{x}{\sqrt{2}}\right)$.

## Statistical Notes
- Low BER (< 1e-5) requires many bits for stable estimates
- Threshold adaptation example uses estimated SNR vs true SNR trace
- Use `--runs > 1` to average out Monte Carlo noise
- Increase `--bits` for deeper BER (ex: 5e6 for ~1e-6 region)

## CLI Options (excerpt)
```
--mods 2,4,16        Modulation orders to simulate
--snr-start / stop / step
--bits               Bits per SNR point (adjusted to symbol multiple)
--runs               Repeats per SNR for averaging
--csv file.csv       Export results
--save-prefix prefix Save plots as prefix_*.png
--no-plot            Disable plotting
--quiet              Suppress progress prints
--pilots N           Pilot symbols for SNR estimation
```
Run `python run_amc.py -h` for full list.

## Troubleshooting
| Symptom | Cause | Fix |
|---------|-------|-----|
| SciPy NumPy version warning | Version mismatch | Pin versions in `requirements.txt` |
| `compute_ber_seeded` missing | Older build | Re-run `make shared` |
| Blank/No plot window (Wayland) | Backend issue | Try `MPLBACKEND=Agg` or save plots instead |
| Very flat BER curve | Too few bits | Increase `--bits` or `--runs` |

## Possible Enhancements
- Add 64-QAM / 256-QAM
- Constellation diagram plotting
- Parallel SNR sweep (multiprocessing)
- BLER / Frame simulation layer
- Simple adaptive rate controller
- CI workflow with GitHub Actions

## Benchmark
You can measure raw BER kernel performance (Python overhead minimal) with:

```
make bench
```

Sample output (BPSK @ 6 dB):
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

Interpretation:
- bits: number of simulated bits passed through the Monte Carlo kernel
- ber: measured BER at that SNR (statistical noise expected for small sizes)
- time(ms): wall‑clock time for that call
- throughput: effective simulated bit rate (higher for larger batches)
- scale: growth factor relative to previous size

Customize (examples):
```
python test_amc.py --bench --bench-mod 16 --bench-snr 8 --bench-sizes 10000,40000,160000
```
You can adjust sizes in the Makefile or invoke directly as above.

## Credits
- Core coding: You (rapid prototyping in the empty class) + Darshan(simple question to make some changes in the API)
- Concept & initial spark: [Dheeraj](https://github.com/dj-49)
- Question prompts & catalyst: Darshan
- Early session: Empty Embedded Systems Design class + previous night’s 10:30 chat
- Energy source: Improvised build > unattended lecture

## License
This project is licensed under the **MIT License**.

See the [`LICENSE`](./LICENSE) file for full text.


---
Enjoy hacking the air interface. Open an issue / add Darshan's handle later.
