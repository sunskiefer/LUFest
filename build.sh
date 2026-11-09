#!/usr/bin/env bash
# Cross-compile LUFest for the Force (ARMv7 hard-float, glibc 2.31) with Zig. Needs: python3 -m pip install ziglang
# and params.h from the skin generator (tools/gen_vst.py of mpc-vst-plugins @ 3845bea with tools_patch applied, run on vst.json).
set -euo pipefail
T="-target arm-linux-gnueabihf.2.31 -mcpu=cortex_a17"; Z="python3 -m ziglang"
BUILD=${BUILD:-build}; mkdir -p "$BUILD/obj"
$Z c++ $T -O2 -fPIC -fvisibility=hidden -std=gnu++11 -DPASSTHROUGH_F32 -Isrc -I"$BUILD" -Iwrapper_patched -c src/engine.cpp -o "$BUILD/obj/engine.o"
$Z cc  $T -O2 -fPIC -fvisibility=hidden -std=gnu11  -DPASSTHROUGH_F32 -I"$BUILD" -Iwrapper_patched -c wrapper_patched/vst2_wrap.c -o "$BUILD/obj/vst2_wrap.o"
$Z c++ $T -O2 -shared -fPIC -fvisibility=hidden -s "$BUILD/obj/engine.o" "$BUILD/obj/vst2_wrap.o" -lm -lpthread -Wl,--no-undefined -o "$BUILD/lufest.so"
