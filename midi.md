# MIDI

## Patch programs

|Bank|Name|
|----|----|
|0|Buzzer waveform 0|
|1|Buzzer waveform 1|
|2|Buzzer waveform 2|
|3|Buzzer waveform 3|
|4|Buzzer waveform 4|
|5|Buzzer waveform 5|
|6|Buzzer waveform 6|
|7|Buzzer waveform 7|

| Program | Name                                   | Tone  | Noise | Buzzer |
|---------|----------------------------------------|-------|-------|--------|
| 0       | Square                                 | Note  | Off   | Off    |
| 1       | Noise                                  | Off   | On    | Off    |
| 2       | Square + Noise                         | Note  | On    | Off    |
| 3       | Buzzer                                 | Off   | Off   | Note   |
| 4       | Buzzer + Square                        | Note  | Off   | Note   |
| 5       | Buzzer + Noise                         | Off   | On    | Note   |
| 6       | Buzzer + Square + Noise                | Note  | On    | Note   |
| 7       | Square + Buzzer                        | Note  | Off   | Note   |
| 8       | Square + Buzzer + Noise                | Note  | On    | Note   |
| 9       | Square + Fixed Buzzer                  | Note  | Off   | Fixed  |
| 10      | Square + Fixed Buzzer + Noise          | Note  | On    | Fixed  |
| 11      | Buzzer + Fixed Square                  | Fixed | Off   | Note   |
| 12      | Buzzer + Fixed Square + Noise          | Fixed | On    | Note   |

## CCs

| CC | Function          |
|----|-------------------|
| 70 | Noise period      |
| 71 | Fixed Tone freq.  |
| 72 | Fixed Buzzer freq.|
| 73 | Detune (semitones)|
| 74 | Detune (fine)     |
| 75 | Attack            |
| 76 | Decay             |
| 77 | Sustain           |
| 78 | Release           |
| 79 | FM Attack         |
| 90 | SyncBuzzer period |
| 91 | SID max           |
| 92 | SID min           |
| 93 | SID duty          |
|    |                   |
|    |                   |
|    |                   |
|    |                   |
|    |                   |
|    |                   |
|    |                   |

