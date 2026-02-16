# ADC → SD Card Performance Benchmark

## Test Configuration

**SPI Clock**
- `CLK_SLOW = 2 MHz`
- `CLK_FAST = 4 MHz`

**Sampling Parameters**
- `SAMPLE_RATE = 4000 Hz` (4 kS/s)
- `BUF_SIZE = 1024 samples`
- Sample format: `uint16_t`

---

## Measured Results

| Metric | Value |
|--------|-------|
Time to fill buffer | **256 ms** |
Time to write buffer to SD card | **~9 ms** |
Data per buffer | **2048 bytes** |
Effective data rate | **~8 KB/s** |

---

## Validation Against Theory

The expected acquisition time is: 1024 samples / 4000 samples/s = 256 ms

The measured value matches the theoretical result, confirming that:
- Sampling timing is stable and deterministic.
- No observable interference from SD card operations.

---

## System Utilization

The SD card write consumes only: 9 ms / 256 ms ≈ 3.5%

This leaves approximately:9 ms / 256 ms ≈ 3.5%


This leaves approximately:
≈ 247 ms idle time per acquisition cycle
≈ 96% timing headroom

## Summary
This benchmark demonstrates that the implemented ADC-to-SD logging pipeline operates with a large timing safety margin.  
At 4 kS/s, the system uses only a small fraction of the available storage bandwidth, making it suitable for scaling to higher data rates or more complex real-time tasks.
