# VocInput

Carrier signal generator for use with the Vocoder plug-in. Generates a sawtooth wave with optional pitch tracking from the input signal, plus breath noise modulated by sibilance detection. Designed to be placed before the Vocoder to provide the carrier signal on one channel while passing the voice (modulator) through on the other.

| Parameter | Description |
| --------- | ----------- |
| Tracking | Pitch tracking mode: Off (fixed pitch), Free (continuous tracking), or Quant (quantized to semitones) |
| Pitch | Base pitch offset in semitones |
| Breath | Amount of breath noise added to the carrier |
| S Thresh | Sibilance detection threshold - controls when breath noise replaces the pitched carrier |
| Max Freq | Maximum tracked pitch frequency |
