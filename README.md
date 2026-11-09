# LUFest

Loudness (LUFS) meter for the Akai Force / MPC OS (VST2 effect). Put it on the master channel's effects; it measures what passes through and does not change the audio.
Vendor: L'Cronx. Free, not for sale (PolyForm Noncommercial 1.0.0).

**Status: 0.1.1, first release, test build. It has not been run on a Force yet and its loudness numbers have not been checked against a reference.** See "Not verified".

## What it shows
- MOM LUFS (400 ms), SHORT LUFS (3 s), INTEG LUFS (gated, since RESET), LRA LU, MAX M, MAX S, TIME
- TRUE PK (4x oversampled, dBTP), PEAK L / PEAK R (held sample peaks, dBFS)
- L and R level bars with a held peak segment: green up to -18 dBFS, yellow -16 to -6, red -4 to 0; red CLIP lamp
- RESET button: clears integrated, LRA, maxes, peaks and time
- Readout tiles use a 36 px label over a 64 px number; the ten readouts are on one Q-Link set

## Install
Download `LUFest-0.1.1-mpc-armv7.zip` from the Releases page, unzip, copy it to the Force and run `install.sh` as root (details in INSTALL.md inside the zip).
Needs MPC OS 3.x. Uninstall with `uninstall.sh`.

## Not verified
- Loudness numbers have not been checked against EBU/ITU test signals or a reference meter.
- Never run on a Force: loading, master insert, real screen refresh (host polls text about every 100 ms, tile bars about every 10 ms, screen update throttled to 50 ms), CPU load.
- Whether the master insert hears the signal before or after the master fader: test in docs/USER-GUIDE.md.
- Headphone/cue output is not measured. Sample rate is assumed 44.1 kHz until the host reports one.

## How it is built
Zig cross-compile for ARMv7 hard-float, glibc 2.31 (see `build.sh`). Uses a patched copy of the mpc-vst-plugins wrapper (`PASSTHROUGH_F32`) so audio passes through as float, untouched, and the meter sees overs, true peaks and the host sample rate. A small patch to the skin tool adds a per-list lit colour for the three-colour bars.

## Licence
Own code: PolyForm Noncommercial 1.0.0 (see LICENSE). Third-party code stays with its authors under their own licences: Klangfreund LUFSMeter (MIT) and the mpc-vst-plugins wrapper (MIT). See NOTICE.md.
