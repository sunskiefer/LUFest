# Third-party notices

LUFest's own code is licensed under PolyForm Noncommercial License 1.0.0 (see LICENSE): free to use, not for sale.
The parts below belong to their authors and keep their own licences.

## 1. Klangfreund LUFSMeter (Ebu128LoudnessMeter) - MIT
`src/lufs_core.h` follows the method and structure of Klangfreund's Ebu128LoudnessMeter
(https://github.com/klangfreund/LUFSMeter) and was rewritten (no JUCE; sample-rate-aware K-weighting; per-sample windows;
fine histogram; true peak). The original licence applies to what it derives from:

> The MIT License (MIT)
> Copyright (c) 2018 Klangfreund, Samuel Gaehwiler
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## 2. mpc-vst-plugins wrapper (sd88me; built from the sunskiefer fork at 3845bea) - MIT
`wrapper_patched/` holds copies of the wrapper files (vst2_wrap.c, engine.h, plugin_dir.h, popup.h). Only vst2_wrap.c is changed, by the
opt-in PASSTHROUGH_F32 patch (`wrapper_patched/PASSTHROUGH_F32.patch`). These files stay MIT:

```
MIT License

Copyright (c) 2026 sd88me

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 3. Skin tool patch
`tools_patch/shadow_skin_on_colour.patch` is a small patch to the framework's tools/shadow_skin.py (MIT, same notice as above):
it adds a per-list lit colour `on=<hex>`. The framework itself is not included here.
