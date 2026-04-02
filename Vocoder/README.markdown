# Vocoder

Attempt to make the carrier signal (e.g. synth, noise) take on the spectral characteristics of the modulator signal (e.g. voice). Uses a bank of 4th-order band-pass filters (8 or 16 bands) with envelope followers to analyze the modulator spectrum, then applies the same spectral envelope to the carrier. Includes high frequency bypass for retaining sibilance and clarity.

| Parameter | Description |
| --------- | ----------- |
| Mod In | Selects which input channel is the modulator: Left or Right |
| Output | Level trim |
| Hi Thru | Amount of high frequency signal passed through unprocessed (adds sibilance and clarity) |
| Hi Band | Level of the high frequency band in the vocoder |
| Envelope | Envelope follower speed - controls how quickly the vocoder responds to the modulator. Very low settings freeze the current spectral shape |
| Filter Q | Filter resonance / bandwidth - higher values give narrower, more resonant bands |
| Mid Freq | Shifts the center frequencies of the filter bank |
| Quality | Number of frequency bands: 8 Band or 16 Band |
