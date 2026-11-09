# LUFest user guide

## Setup
1. Install LUFest (see README) and restart the Force app/MPC OS as the installer does.
2. Open the master channel, add LUFest as an effect in its effect slots.
3. Play. The readouts and bars follow the master.

## Reading it
| Readout | Meaning |
|---|---|
| MOM LUFS | momentary loudness, 400 ms window |
| SHORT LUFS | short-term loudness, 3 s window |
| INTEG LUFS | integrated loudness since RESET (absolute gate -70 LUFS, relative gate -10 LU) |
| LRA LU | loudness range (10th to 95th percentile, relative gate -20 LU); "--" until enough audio |
| MAX M / MAX S | highest momentary / short-term since RESET |
| TRUE PK | highest true peak (dBTP, 4x oversampled) since RESET |
| PEAK L / PEAK R | held sample peaks in dBFS |
| TIME | measured time since RESET |
Level bars: 2 dB per segment from -58 to 0 dBFS; instant rise, 24 dB/s fall, one held-peak segment. Green up to -18, yellow -16 to -6, red -4 to 0.
CLIP lights red when a sample reaches full scale. RESET clears everything above.

## Is it before or after the master fader? (please test)
Play a steady tone, note SHORT LUFS, move the master fader down 6 dB. If the reading drops about 6 LU it is measuring after the fader; if it does not move, before it.
Report the result so it can be written here.

## Layout
Positions are in `layout.conf`; to move something, change that control's line only.
