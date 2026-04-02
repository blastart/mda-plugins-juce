# TalkBox

High quality vocoder effect using linear predictive coding (LPC). Takes the spectral envelope from one input (the modulator, typically a voice) and applies it to another input (the carrier, typically a synth or guitar), creating the classic "talking instrument" effect. Uses overlapping Hanning windows and Levinson-Durbin recursion for robust spectral analysis.

| Parameter | Description |
| --------- | ----------- |
| Wet | Effect output level |
| Dry | Dry (unprocessed carrier) output level |
| Carrier | Selects which input channel is the carrier: Left or Right |
| Quality | LPC analysis order - higher values give more detailed spectral matching |
