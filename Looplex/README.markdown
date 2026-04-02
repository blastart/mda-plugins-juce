# Looplex

Live looping instrument with overdub capability. Records audio into a loop buffer with feedback control, allowing layered loop recording. Supports MIDI control: sustain pedal toggles recording, mod wheel attenuates feedback, and sostenuto/soft pedals resync the loop position. Audio is stored internally as 16-bit with dithering.

| Parameter | Description |
| --------- | ----------- |
| Max Del | Maximum loop length in seconds (requires Reset to take effect) |
| Reset | Toggle to reallocate the loop buffer when Max Del changes |
| Record | Toggle recording on/off (can also be controlled via MIDI sustain pedal) |
| In Mix | Input level |
| In Pan | Input panning - left, center, or right |
| Feedback | Loop feedback amount (can be attenuated via MIDI mod wheel) |
| Out Mix | Output level |
