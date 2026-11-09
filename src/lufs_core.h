// lufs_core.h - loudness measurement core (ITU-R BS.1770-4 / EBU R128 / Tech 3341 / 3342)
//
// Method and structure follow Klangfreund's Ebu128LoudnessMeter
// (github.com/klangfreund/LUFSMeter, MIT, Copyright (c) 2018 Klangfreund, Samuel Gaehwiler):
// K-weighting -> mean square -> 400 ms / 3 s windows -> gating blocks every 100 ms ->
// histograms for integrated loudness and loudness range.
//
// Changes from the original (all deliberate):
//  - no JUCE; standard C++ only, no allocation after prepare()
//  - K-weighting coefficients are computed for the actual sample rate
//  - sliding windows updated per sample (resummed exactly once a second), not 50 ms bins
//  - 0.01 LU histogram bins, and each bin keeps its exact energy sum, so the integrated value
//    is not quantised (only the gate decision is, by at most 0.01 LU)
//  - 4x oversampled true peak (BS.1770-4 Annex 2 method; own Kaiser-windowed interpolator)
//
// Original licence (applies to the method/structure this file derives from):
//   The MIT License (MIT)  Copyright (c) 2018 Klangfreund, Samuel Gaehwiler
//   Permission is hereby granted, free of charge, to any person obtaining a copy of this
//   software and associated documentation files (the "Software"), to deal in the Software
//   without restriction, including without limitation the rights to use, copy, modify, merge,
//   publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
//   to whom the Software is furnished to do so, subject to the following conditions: The above
//   copyright notice and this permission notice shall be included in all copies or substantial
//   portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace lufs {

static constexpr double kFloorDb = -120.0;   // shown as "no signal"

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0; }
};

// Stage 1: head-related high shelf. Stage 2: RLB high pass. Computed for any sample rate.
inline void kweight_coeffs(double fs, Biquad& shelf, Biquad& hp) {
    const double pi = 3.14159265358979323846;
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan(pi * f0 / fs);
        const double Vh = std::pow(10.0, G / 20.0);
        const double Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelf.b1 = 2.0 * (K * K - Vh) / a0;
        shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0;
        shelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan(pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
        hp.a1 = 2.0 * (K * K - 1.0) / a0;
        hp.a2 = (1.0 - K / Q + K * K) / a0;
    }
}

// Histogram of gating-block loudness between -70 and +20 LUFS in 0.01 LU steps.
// Each bin keeps its block count and the exact sum of block energies.
struct GateHist {
    static constexpr int kBins = 9000;
    static constexpr double kLo = -70.0, kStep = 0.01;
    std::vector<double> e;
    std::vector<uint32_t> n;
    GateHist() : e(kBins, 0.0), n(kBins, 0) {}
    void clear() { std::fill(e.begin(), e.end(), 0.0); std::fill(n.begin(), n.end(), 0u); }
    static double to_lufs(double z) { return -0.691 + 10.0 * std::log10(z); }
    // z = weighted mean-square energy of the block. Blocks at or below -70 LUFS are dropped
    // (absolute gate).
    void add(double z) {
        if (!(z > 0.0)) return;
        const double l = to_lufs(z);
        if (!(l > kLo)) return;
        int i = (int)((l - kLo) / kStep);
        if (i >= kBins) i = kBins - 1;
        if (i < 0) i = 0;
        e[i] += z; n[i] += 1;
    }
    // Relative gate: threshold = ungated-above--70 mean loudness minus relGate LU.
    // Returns first bin passing the gate, or -1 if empty.
    int rel_bin(double relGateLU) const {
        double se = 0; uint64_t sn = 0;
        for (int i = 0; i < kBins; ++i) { se += e[i]; sn += n[i]; }
        if (sn == 0) return -1;
        const double thr = to_lufs(se / (double)sn) - relGateLU;
        int b = (int)std::ceil((thr - kLo) / kStep);
        if (b < 0) b = 0;
        return b >= kBins ? kBins : b;
    }
};

struct Readings {
    double momentary = kFloorDb, shortTerm = kFloorDb, integrated = kFloorDb;
    double lra = 0.0, lraLow = kFloorDb, lraHigh = kFloorDb;
    double maxMomentary = kFloorDb, maxShortTerm = kFloorDb;
    double truePeakL = kFloorDb, truePeakR = kFloorDb, truePeakMax = kFloorDb;   // dBTP, held
    double samplePeakL = kFloorDb, samplePeakR = kFloorDb;                        // dBFS, held
    double levelL = kFloorDb, levelR = kFloorDb;                                  // dBFS, falling bar
    double seconds = 0.0;
    bool clip = false;                                                            // held sample peak >= 0 dBFS
};

class Meter {
public:
    static constexpr int kPhases = 4;
    static constexpr int kTapsPerPhase = 24;
    static constexpr int kTaps = kPhases * kTapsPerPhase;   // 96

    Meter() { prepare(44100.0); }

    void prepare(double sampleRate) {
        fs_ = sampleRate;
        for (int c = 0; c < 2; ++c) kweight_coeffs(fs_, shelf_[c], hp_[c]);
        n400_ = (int)std::lround(fs_ * 0.4);
        n3s_ = (int)std::lround(fs_ * 3.0);
        hop_ = (int)std::lround(fs_ * 0.1);
        resum_ = (int)std::lround(fs_);
        stride_ = std::max(1, (int)std::lround(fs_ * 0.0005));   // M/S refreshed every ~0.5 ms
        for (int c = 0; c < 2; ++c) ring_[c].assign((size_t)n3s_, 0.0);
        design_interp();
        fall_db_per_sample_ = 24.0 / fs_;
        reset();
    }

    void reset() {
        for (int c = 0; c < 2; ++c) {
            shelf_[c].clear(); hp_[c].clear();
            std::fill(ring_[c].begin(), ring_[c].end(), 0.0);
            sum400_[c] = sum3_[c] = 0.0;
            std::memset(hist_[c], 0, sizeof(hist_[c]));
            tpMax_[c] = 0.0; spMax_[c] = 0.0; level_[c] = 0.0;
        }
        pos_ = 0; seen_ = 0; sinceHop_ = 0; sinceResum_ = 0; sinceStride_ = 0; hpos_ = 0;
        gI_.clear(); gL_.clear();
        r_ = Readings();
        lvlDb_[0] = lvlDb_[1] = kFloorDb;
    }

    // Left/right float samples, nominal full scale = 1.0. Audio is not modified.
    void process(const float* l, const float* r, int n) {
        const float* in[2] = {l, r};
        for (int i = 0; i < n; ++i) {
            double sq[2];
            for (int c = 0; c < 2; ++c) {
                const double x = in[c][i];
                // sample peak and true peak
                const double ax = std::fabs(x);
                if (ax > spMax_[c]) spMax_[c] = ax;
                if (ax > blockPk_[c]) blockPk_[c] = ax;
                tp_push(c, x);
                // K-weighting and square
                const double y = hp_[c].process(shelf_[c].process(x));
                sq[c] = y * y;
                // windows
                const int old3 = pos_;                                 // sample leaving the 3 s window
                int old400 = pos_ - n400_; if (old400 < 0) old400 += n3s_;
                sum3_[c] += sq[c] - ring_[c][(size_t)old3];
                sum400_[c] += sq[c] - ring_[c][(size_t)old400];
                ring_[c][(size_t)pos_] = sq[c];
            }
            if (++pos_ >= n3s_) pos_ = 0;
            ++seen_;
            // exact re-sum once a second so float drift cannot build up
            if (++sinceResum_ >= resum_) { sinceResum_ = 0; resum(); }
            // momentary / short-term refresh
            if (++sinceStride_ >= stride_) { sinceStride_ = 0; refresh_ms(); }
            // gating blocks every 100 ms
            if (++sinceHop_ >= hop_) {
                sinceHop_ = 0;
                if (seen_ >= (uint64_t)n400_) gI_.add(z400());
                if (seen_ >= (uint64_t)n3s_) gL_.add(z3s());
                update_gated();
            }
        }
        // level bars: instant rise, fall 24 dB/s; sample-peak hold
        for (int c = 0; c < 2; ++c) {
            const double pk = blockPk_[c];
            const double db = pk > 0.0 ? std::max(kFloorDb, 20.0 * std::log10(pk)) : kFloorDb;
            lvlDb_[c] = std::max(db, lvlDb_[c] - fall_db_per_sample_ * n);
            blockPk_[c] = 0.0;
        }
        publish_peaks();
        r_.seconds = (double)seen_ / fs_;
    }

    const Readings& readings() const { return r_; }
    void reset_peaks_only() {
        for (int c = 0; c < 2; ++c) { tpMax_[c] = 0.0; spMax_[c] = 0.0; }
        publish_peaks();
    }
    double sample_rate() const { return fs_; }

private:
    double fs_ = 44100.0;
    int n400_ = 0, n3s_ = 0, hop_ = 0, resum_ = 0, stride_ = 0;
    Biquad shelf_[2], hp_[2];
    std::vector<double> ring_[2];
    double sum400_[2] = {0, 0}, sum3_[2] = {0, 0};
    int pos_ = 0, sinceHop_ = 0, sinceResum_ = 0, sinceStride_ = 0;
    uint64_t seen_ = 0;
    GateHist gI_, gL_;
    Readings r_;
    // true peak
    double h_[kTaps] = {0};
    double hist_[2][kTapsPerPhase] = {{0}};
    int hpos_ = 0;
    double tpMax_[2] = {0, 0}, spMax_[2] = {0, 0}, level_[2] = {0, 0}, blockPk_[2] = {0, 0};
    double lvlDb_[2] = {kFloorDb, kFloorDb};
    double fall_db_per_sample_ = 0.0;

    double z400() const { return (sum400_[0] + sum400_[1]) / (double)n400_; }
    double z3s() const { return (sum3_[0] + sum3_[1]) / (double)n3s_; }
    static double lufs(double z) { return z > 0.0 ? std::max(kFloorDb, -0.691 + 10.0 * std::log10(z)) : kFloorDb; }

    void resum() {
        for (int c = 0; c < 2; ++c) {
            double s3 = 0, s4 = 0;
            for (int i = 0; i < n3s_; ++i) s3 += ring_[c][(size_t)i];
            for (int i = 1; i <= n400_; ++i) {
                int k = pos_ - i; if (k < 0) k += n3s_;
                s4 += ring_[c][(size_t)k];
            }
            sum3_[c] = s3; sum400_[c] = s4;
        }
    }

    void refresh_ms() {
        if (seen_ >= (uint64_t)n400_) {
            r_.momentary = lufs(z400());
            if (r_.momentary > r_.maxMomentary) r_.maxMomentary = r_.momentary;
        }
        if (seen_ >= (uint64_t)n3s_) {
            r_.shortTerm = lufs(z3s());
            if (r_.shortTerm > r_.maxShortTerm) r_.maxShortTerm = r_.shortTerm;
        }
    }

    void update_gated() {
        // integrated: relative gate -10 LU
        {
            const int b = gI_.rel_bin(10.0);
            if (b >= 0 && b < GateHist::kBins) {
                double se = 0; uint64_t sn = 0;
                for (int i = b; i < GateHist::kBins; ++i) { se += gI_.e[i]; sn += gI_.n[i]; }
                if (sn) r_.integrated = lufs(se / (double)sn);
            }
        }
        // loudness range: relative gate -20 LU, 10th to 95th percentile
        {
            const int b = gL_.rel_bin(20.0);
            if (b >= 0 && b < GateHist::kBins) {
                uint64_t total = 0;
                for (int i = b; i < GateHist::kBins; ++i) total += gL_.n[i];
                if (total) {
                    uint64_t cum = 0; int lo = -1, hi = -1;
                    for (int i = b; i < GateHist::kBins; ++i) {
                        cum += gL_.n[i];
                        if (lo < 0 && (double)cum >= 0.10 * (double)total) lo = i;
                        if (hi < 0 && (double)cum >= 0.95 * (double)total) { hi = i; break; }
                    }
                    if (lo >= 0 && hi >= 0) {
                        r_.lraLow = GateHist::kLo + (lo + 0.5) * GateHist::kStep;
                        r_.lraHigh = GateHist::kLo + (hi + 0.5) * GateHist::kStep;
                        r_.lra = std::max(0.0, r_.lraHigh - r_.lraLow);
                    }
                }
            }
        }
    }

    static double to_db(double a) { return a > 0.0 ? std::max(kFloorDb, 20.0 * std::log10(a)) : kFloorDb; }
    void publish_peaks() {
        r_.truePeakL = to_db(tpMax_[0]); r_.truePeakR = to_db(tpMax_[1]);
        r_.truePeakMax = std::max(r_.truePeakL, r_.truePeakR);
        r_.samplePeakL = to_db(spMax_[0]); r_.samplePeakR = to_db(spMax_[1]);
        r_.levelL = lvlDb_[0]; r_.levelR = lvlDb_[1];
        r_.clip = (spMax_[0] >= 1.0 || spMax_[1] >= 1.0);
    }

    // 4x interpolation filter: Kaiser-windowed sinc, cutoff at the input Nyquist, 96 taps.
    void design_interp() {
        const double pi = 3.14159265358979323846;
        const double beta = 9.0;
        auto i0 = [](double x) {
            double s = 1, t = 1;
            for (int k = 1; k < 60; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
            return s;
        };
        const double c = (kTaps - 1) / 2.0;
        const double den = i0(beta);
        for (int k = 0; k < kTaps; ++k) {
            const double t = (k - c) / (double)kPhases;
            const double sinc = (std::fabs(t) < 1e-12) ? 1.0 : std::sin(pi * t) / (pi * t);
            const double u = (k - c) / c;
            const double w = i0(beta * std::sqrt(std::max(0.0, 1.0 - u * u))) / den;
            h_[k] = sinc * w;
        }
        // make each phase sum to exactly 1 (unity DC gain on every interpolated point)
        for (int p = 0; p < kPhases; ++p) {
            double s = 0;
            for (int j = 0; j < kTapsPerPhase; ++j) s += h_[kPhases * j + p];
            if (s != 0.0) for (int j = 0; j < kTapsPerPhase; ++j) h_[kPhases * j + p] /= s;
        }
        std::memset(hist_, 0, sizeof(hist_));
    }

    void tp_push(int c, double x) {
        double* hist = hist_[c];
        // hist[0] = newest sample
        for (int j = kTapsPerPhase - 1; j > 0; --j) hist[j] = hist[j - 1];
        hist[0] = x;
        for (int p = 0; p < kPhases; ++p) {
            double acc = 0;
            for (int j = 0; j < kTapsPerPhase; ++j) acc += h_[kPhases * j + p] * hist[j];
            const double a = std::fabs(acc);
            if (a > tpMax_[c]) tpMax_[c] = a;
        }
    }
};

}  // namespace lufs
