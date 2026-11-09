// LUFest engine (placeholder name): loudness meter effect for Force / MPC OS.
// Provides mpc_engine() for wrapper/vst2_wrap.c. In the float-passthrough build (-DPASSTHROUGH_F32, see
// wrapper_f32.patch) the wrapper copies the host's audio through untouched and hands the same float blocks
// to mpc_meter_f32(); the plain int16 process() below is then never called by the wrapper.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
extern "C" {
#include "engine.h"
}
#include "lufs_core.h"

namespace {

struct Inst {
    lufs::Meter m;
    double fs = 44100.0;
};

constexpr int kSegs = 30;                 // segments per bar, 2 dB each, -58 .. 0 dBFS
constexpr double kSegDb0 = -60.0, kSegStep = 2.0;

void fmt_db(char* b, int n, double v) {
    if (v <= lufs::kFloorDb + 0.05) snprintf(b, (size_t)n, "-inf");
    else snprintf(b, (size_t)n, "%.1f", v);
}

// segment k (1..kSegs) of a bar: lit up to the level, plus one lit segment at the held peak
bool seg_lit(int k, double level_db, double peak_db) {
    const double thr = kSegDb0 + kSegStep * k;
    if (level_db >= thr - 1e-9) return true;
    int pk = (int)std::floor((peak_db - kSegDb0) / kSegStep + 1e-9);
    if (pk > kSegs) pk = kSegs;
    return pk >= 1 && k == pk;
}

// "sgl_12" / "syr_3" / "srl_2" (zone g=green 1..21, y=yellow 1..6, r=red 1..3; channel l/r); "<key>_on" sets *on.
// idx = overall segment number 1..30 (green 1-21 = -58..-18 dB, yellow 22-27 = -16..-6 dB, red 28-30 = -4..0 dB)
bool parse_seg(const char* key, int* ch, int* idx, bool* on) {
    if (key[0] != 's' || (key[1] != 'g' && key[1] != 'y' && key[1] != 'r') || (key[2] != 'l' && key[2] != 'r') || key[3] != '_') return false;
    const int base = key[1] == 'g' ? 0 : key[1] == 'y' ? 21 : 27, cnt = key[1] == 'g' ? 21 : key[1] == 'y' ? 6 : 3;
    *ch = key[2] == 'r' ? 1 : 0;
    char* end = nullptr;
    long v = strtol(key + 4, &end, 10);
    if (end == key + 4 || v < 1 || v > cnt) return false;
    *idx = base + (int)v;
    if (*end == '\0') { *on = false; return true; }
    if (strcmp(end, "_on") == 0) { *on = true; return true; }
    return false;
}

void* create(const char*) { return new Inst(); }
void destroy(void* i) { delete static_cast<Inst*>(i); }
void midi(void*, const uint8_t*, int) {}

void set_param(void* i, const char* k, const char* v) {
    Inst* s = static_cast<Inst*>(i);
    if (!strcmp(k, "reset") && atof(v) > 0.5) s->m.reset();   // the readouts themselves are read-only
}

int get_param(void* i, const char* k, char* b, int n) {
    Inst* s = static_cast<Inst*>(i);
    const lufs::Readings& r = s->m.readings();
    if (!strcmp(k, "mom"))   { fmt_db(b, n, r.momentary);   return (int)strlen(b); }
    if (!strcmp(k, "st"))    { fmt_db(b, n, r.shortTerm);   return (int)strlen(b); }
    if (!strcmp(k, "integ")) { fmt_db(b, n, r.integrated);  return (int)strlen(b); }
    if (!strcmp(k, "maxm"))  { fmt_db(b, n, r.maxMomentary); return (int)strlen(b); }
    if (!strcmp(k, "maxs"))  { fmt_db(b, n, r.maxShortTerm); return (int)strlen(b); }
    if (!strcmp(k, "tp"))    { fmt_db(b, n, r.truePeakMax);  return (int)strlen(b); }
    if (!strcmp(k, "pkl"))   { fmt_db(b, n, r.samplePeakL);  return (int)strlen(b); }
    if (!strcmp(k, "pkr"))   { fmt_db(b, n, r.samplePeakR);  return (int)strlen(b); }
    if (!strcmp(k, "lra")) {
        if (r.lraLow <= lufs::kFloorDb + 0.05) snprintf(b, (size_t)n, "--");   // not enough gated audio yet
        else snprintf(b, (size_t)n, "%.1f", r.lra);
        return (int)strlen(b);
    }
    if (!strcmp(k, "time")) {
        const long t = (long)r.seconds;
        if (t >= 3600) snprintf(b, (size_t)n, "%ld:%02ld:%02ld", t / 3600, (t / 60) % 60, t % 60);
        else snprintf(b, (size_t)n, "%ld:%02ld", t / 60, t % 60);
        return (int)strlen(b);
    }
    {
        static const char* const L[][2] = {{"lbl_mom","MOM LUFS"},{"lbl_st","SHORT LUFS"},{"lbl_integ","INTEG LUFS"},{"lbl_lra","LRA LU"},
            {"lbl_tp","TRUE PK"},{"lbl_maxm","MAX M"},{"lbl_maxs","MAX S"},{"lbl_pkl","PEAK L"},{"lbl_pkr","PEAK R"},{"lbl_time","TIME"}};
        for (const auto& e : L) if (!strcmp(k, e[0])) { snprintf(b, (size_t)n, "%s", e[1]); return (int)strlen(b); }
    }
    if (!strcmp(k, "clip_1"))    { snprintf(b, (size_t)n, "CLIP"); return (int)strlen(b); }
    if (!strcmp(k, "clip_1_on")) { snprintf(b, (size_t)n, "%d", r.clip ? 1 : 0); return (int)strlen(b); }
    int ch, idx; bool on;
    if (parse_seg(k, &ch, &idx, &on)) {
        if (!on) { b[0] = '\0'; return 0; }   // the tile carries no text
        const double lvl = ch ? r.levelR : r.levelL, pk = ch ? r.samplePeakR : r.samplePeakL;
        snprintf(b, (size_t)n, "%d", seg_lit(idx, lvl, pk) ? 1 : 0);
        return 1;
    }
    return 0;
}

// The wrapper insists on an effect port providing process(); in the float-passthrough build it is not used.
void render(void*, int16_t* out, int frames) { memset(out, 0, (size_t)frames * 4); }
void process(void*, const int16_t* in, int16_t* out, int frames) { memcpy(out, in, (size_t)frames * 4); }

const mpc_engine_t ENGINE = { create, destroy, midi, set_param, get_param, render, process, nullptr };

}  // namespace

extern "C" {

const mpc_engine_t* mpc_engine(void) { return &ENGINE; }

// float audio from the wrapper (PASSTHROUGH_F32): any block size, called under the engine lock
void mpc_meter_f32(void* inst, const float* l, const float* r, int n) {
    static_cast<Inst*>(inst)->m.process(l, r, n);
}

// the host's real sample rate (effSetSampleRate); the meter is re-prepared for it
void mpc_meter_rate(void* inst, double fs) {
    Inst* s = static_cast<Inst*>(inst);
    if (fs >= 8000.0 && fs <= 192000.0 && fs != s->fs) { s->fs = fs; s->m.prepare(fs); }
}

}
