# Tracker

Attempt to track the pitch of the input signal and use it to drive a synthesizer oscillator or resonant filter. The pitch tracker uses zero-crossing detection with a low-pass filter to extract the fundamental frequency. Can generate sine, square, sawtooth, ring-modulated, or EQ (resonant filter) outputs that follow the input pitch.

| Parameter | Description |
| --------- | ----------- |
| Mode | Synthesis mode: Sine, Square, Saw, Ring (modulate input), or EQ (resonant filter at tracked pitch) |
| Dynamics | Amount of dynamics (envelope following) applied to the synthesized signal |
| Mix | Wet / dry mix |
| Glide | Pitch tracking smoothness - lower values track pitch changes more quickly |
| Transpose | Pitch offset for the synthesized signal in semitones |
| Maximum | Maximum tracking frequency - raise to track higher pitched sources |
| Trigger | Threshold level for pitch detection |
| Output | Level trim |
