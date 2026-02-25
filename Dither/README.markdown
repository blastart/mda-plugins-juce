# Dither

Range of dither types for word-length reduction.

| Parameter | Description |
| --------- | ----------- |
| Word Length | Output word length 8 - 24 bits |
| Dither | see below for the possible modes |
| Dither Amp | Dither amplitude - is at optimum level but can be turned down to reduce background hiss at the expense of dither level "pumping" caused by the input signal type and level |
| DC Trim | Fine tune DC offset - can help get best dither sound for silent or very quiet inputs |
| Zoom | Allows the signal to be faded into the noise floor at a clearly audible level so dither settings can be "auditioned". Note that some (perceptual) properties of dither will change when listened to in this way |

Dither Modes:

- OFF: Truncation
- TRI: Triangular PDF dither
- HP-TRI: High-pass Triangular PDF dither (a good general purpose dither)
- N-SHAPE: Second-order noise-shaped dither (for final mastering to 8 or 16 bits)

## Technical notes

When a waveform is rounded to the nearest 16 (or whatever)-bit value this causes distortion. Dither allows you to exchange this rough sounding signal-dependant distortion for a smooth background hiss.

Some sort of dither should always be used when reducing the word length of digital audio, such as from 24-bit to 16-bit. In many cases the background noise in a recording will act as dither, but dither will still be required on fades and on very clean recordings such as purely synthetic sources.

Noise shaping makes the background hiss of dither sound quieter, but adds more high-frequency noise than 'ordinary' dither. This high frequency noise can be a problem if a recording is later processed in any way (including gain changes) especially if noise shaping is applied a second time.

If you are producing an *absolutely final* master at 16 bits or less, use noise shaped dither. In all other situations use a non-noise-shaped dither such as high-pass-triangular. When mastering for MP3 or other compressed formats be aware that noise shaping may take some of the encoder's 'attention' away from the real signal at high frequencies.

No gain changes should be applied after this plug-in. Make sure any master output fader is set to 0.0 dB in the host application.

**Very technical note** This plug-in follows Steinberg's convention that a signal level of 1.0 corresponds to a 16-bit output of 32768. If your host application does not allow this exact gain setting the effectiveness of dither may be reduced (check for harmonic distortion of a 1 kHz sine wave using a spectrum analyser).
