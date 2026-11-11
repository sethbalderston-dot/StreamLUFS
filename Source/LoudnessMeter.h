// ITU-R BS.1770-4 / EBU R128 / EBU Tech 3341+3342 loudness meter (stereo). No JUCE dependency.
#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lm {

constexpr float kFloor = -200.0f;   // "no reading yet"

inline double toLufs(double meanSquare) { return meanSquare > 0.0 ? -0.691 + 10.0 * std::log10(meanSquare) : -200.0; }

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double process(double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void clear() { z1 = z2 = 0; }
};

// Histogram of block loudness, 0.1 LU bins from -70 to +30 LUFS (constant memory, no allocation)
struct Histogram {
    static constexpr int N = 1000;
    std::array<uint32_t, N> cnt{};
    std::array<double, N> sumE{};
    void clear() { cnt.fill(0); sumE.fill(0.0); }
    void add(double e) {
        double L = toLufs(e);
        if (L < -70.0) return;                       // absolute gate
        int b = std::min(N - 1, (int) std::floor((L + 70.0) * 10.0));
        cnt[b]++; sumE[b] += e;
    }
    // stats after relative gate (offset below the ungated mean: 10 for integrated, 20 for LRA)
    bool gated(double relOffset, int& start, uint64_t& c, double& s) const {
        uint64_t c0 = 0; double s0 = 0;
        for (int i = 0; i < N; ++i) { c0 += cnt[i]; s0 += sumE[i]; }
        if (!c0) return false;
        double gate = toLufs(s0 / (double) c0) - relOffset;
        start = std::clamp((int) std::ceil((gate + 70.0) * 10.0), 0, N);
        c = 0; s = 0;
        for (int i = start; i < N; ++i) { c += cnt[i]; s += sumE[i]; }
        return c > 0;
    }
};

class LoudnessMeter {
public:
    struct Snapshot { float momentary = kFloor, shortTerm = kFloor, integrated = kFloor, lra = kFloor,
                            truePeak = kFloor, shortTermMax = kFloor, momentaryMax = kFloor; };

    void prepare(double sr) {
        sampleRate = sr;
        hopN = std::max(1, (int) std::lround(sr * 0.1));
        makeKWeighting(sr);
        makeTruePeakFilter();
        resetAll();
    }
    void requestReset() { resetFlag.store(true); }   // safe from any thread

    // audio thread
    void process(const float* l, const float* r, int n) {
        if (resetFlag.exchange(false)) resetAll();
        for (int i = 0; i < n; ++i) {
            double x[2] = { fin(l[i]), fin(r[i]) };
            for (int c = 0; c < 2; ++c) {
                double k = shelf[c].process(x[c]);
                k = hpf[c].process(k);
                sumSq[c] += k * k;
                trackTruePeak(c, x[c]);
            }
            if (++hopCount >= hopN) endHop();
        }
    }

    Snapshot getSnapshot() const {
        Snapshot s; s.momentary = mom; s.shortTerm = st; s.integrated = integ; s.lra = lra;
        s.truePeak = tpMax; s.shortTermMax = stMax; s.momentaryMax = momMax; return s;
    }

private:
    static double fin(float v) { return std::isfinite(v) ? (double) v : 0.0; }

    void makeKWeighting(double fs) {
        const double PI = 3.14159265358979323846;
        { // stage 1: high-shelf (head model)
            double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            double K = std::tan(PI * f0 / fs), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
            double a0 = 1.0 + K / Q + K * K;
            for (auto& b : shelf) {
                b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0;
                b.a1 = 2.0 * (K * K - 1.0) / a0;       b.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        { // stage 2: RLB high-pass
            double f0 = 38.13547087602444, Q = 0.5003270373238773;
            double K = std::tan(PI * f0 / fs), a0 = 1.0 + K / Q + K * K;
            for (auto& b : hpf) { b.b0 = 1; b.b1 = -2; b.b2 = 1; b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0; }
        }
    }

    // 4x oversampling polyphase windowed-sinc (49 taps, Blackman) for BS.1770 true-peak estimate
    void makeTruePeakFilter() {
        const double PI = 3.14159265358979323846;
        for (int n = 0; n < kTaps; ++n) {
            double t = (n - 24) / 4.0;
            double sinc = (n == 24) ? 1.0 : std::sin(PI * t) / (PI * t);
            double w = 0.42 - 0.5 * std::cos(2 * PI * n / 48.0) + 0.08 * std::cos(4 * PI * n / 48.0);
            fir[n] = sinc * w;
        }
    }
    void trackTruePeak(int c, double x) {
        auto& h = hist[c]; auto& pos = histPos[c];
        pos = (pos + 1) & 15; h[pos] = x;
        double peak = std::fabs(x);
        for (int p = 1; p < 4; ++p) {           // phase 0 == original sample
            double y = 0; int k = 0;
            for (int n = p; n < kTaps; n += 4, ++k) y += h[(pos - k) & 15] * fir[n];
            peak = std::max(peak, std::fabs(y));
        }
        if (peak > tpLin) tpLin = peak;
    }

    void endHop() {
        double E = (sumSq[0] + sumSq[1]) / (double) hopCount;
        sumSq[0] = sumSq[1] = 0; hopCount = 0;
        ring[ringPos] = E; ringPos = (ringPos + 1) % 30; ringFilled = std::min(ringFilled + 1, 30);

        if (ringFilled >= 4) {                           // momentary: 400 ms, 75 % overlap
            double m = 0; for (int k = 0; k < 4; ++k) m += ring[(ringPos - 1 - k + 60) % 30]; m /= 4.0;
            mom = (float) toLufs(m); momMax = std::max(momMax.load(), mom.load());
            histI.add(m);
            int s; uint64_t c; double e;
            if (histI.gated(10.0, s, c, e)) integ = (float) toLufs(e / (double) c);
        }
        if (ringFilled >= 30) {                          // short-term: 3 s
            double m = 0; for (double v : ring) m += v; m /= 30.0;
            st = (float) toLufs(m); stMax = std::max(stMax.load(), st.load());
            histS.add(m);
            int s; uint64_t c; double e;
            if (histS.gated(20.0, s, c, e)) {            // EBU Tech 3342: 10th..95th percentile
                uint64_t cum = 0, lo = 0, hi = 0; bool gl = false, gh = false;
                for (int i = s; i < Histogram::N; ++i) {
                    cum += histS.cnt[i];
                    if (!gl && cum >= 0.10 * (double) c) { lo = i; gl = true; }
                    if (!gh && cum >= 0.95 * (double) c) { hi = i; gh = true; }
                }
                lra = (float) ((double) (hi - lo) * 0.1);
            }
        }
        tpMax = tpLin > 0 ? (float) (20.0 * std::log10(tpLin)) : kFloor;
    }

    void resetAll() {
        for (auto& b : shelf) b.clear();
        for (auto& b : hpf) b.clear();
        for (auto& h : hist) h.fill(0.0);
        histPos = { 0, 0 }; sumSq = { 0, 0 }; hopCount = 0; ring.fill(0.0); ringPos = 0; ringFilled = 0;
        histI.clear(); histS.clear(); tpLin = 0;
        mom = st = integ = lra = tpMax = stMax = momMax = kFloor;
    }

    static constexpr int kTaps = 49;
    double sampleRate = 48000; int hopN = 4800, hopCount = 0;
    std::array<Biquad, 2> shelf, hpf;
    std::array<double, 2> sumSq{};
    std::array<double, 30> ring{}; int ringPos = 0, ringFilled = 0;
    Histogram histI, histS;
    std::array<double, kTaps> fir{};
    std::array<std::array<double, 16>, 2> hist{}; std::array<int, 2> histPos{};
    double tpLin = 0;
    std::atomic<bool> resetFlag{ false };
    std::atomic<float> mom{ kFloor }, st{ kFloor }, integ{ kFloor }, lra{ kFloor }, tpMax{ kFloor }, stMax{ kFloor }, momMax{ kFloor };
};

} // namespace lm
