#include "LoudnessMeter.h"
#include "Platforms.h"
#include <cstdio>
#include <vector>
using namespace lm;
static int fails = 0;
static const double PI = 3.14159265358979;
static void check(const char* n, double v, double expect, double tol) {
    bool ok = std::fabs(v - expect) <= tol; if (!ok) ++fails;
    std::printf("%-46s %8.2f (expect %.2f +/- %.2f) %s\n", n, v, expect, tol, ok ? "PASS" : "FAIL");
}
static std::vector<float> sine(double fs, double sec, double dbfs, double f = 997.0) {
    int N = (int) (fs * sec); std::vector<float> a(N);
    for (int i = 0; i < N; ++i) a[i] = (float) (std::pow(10.0, dbfs / 20.0) * std::sin(2 * PI * f * i / fs));
    return a;
}
int main() {
    for (double fs : { 44100.0, 48000.0, 96000.0 }) {
        LoudnessMeter m; m.prepare(fs); auto a = sine(fs, 30, -23);
        m.process(a.data(), a.data(), (int) a.size());
        auto s = m.getSnapshot(); char buf[64];
        std::snprintf(buf, 64, "997 Hz -23 dBFS stereo Integrated @%.0f", fs); check(buf, s.integrated, -23.0, 0.1);
        std::snprintf(buf, 64, "  Short-term @%.0f", fs); check(buf, s.shortTerm, -23.0, 0.1);
        std::snprintf(buf, 64, "  Momentary @%.0f", fs); check(buf, s.momentary, -23.0, 0.1);
    }
    { LoudnessMeter m; m.prepare(48000); auto a = sine(48000, 20, -23); std::vector<float> z(a.size(), 0.f);
      m.process(a.data(), z.data(), (int) a.size()); check("Left only -23 dBFS -> ~-26 LUFS", m.getSnapshot().integrated, -26.0, 0.1); }
    { LoudnessMeter m; m.prepare(48000); int N = 96000; std::vector<float> a(N);
      for (int i = 0; i < N; ++i) a[i] = (float) std::sin(PI / 2 * i + PI / 4);
      m.process(a.data(), a.data(), N); check("True peak, inter-sample 0 dBTP (sample pk -3)", m.getSnapshot().truePeak, 0.0, 0.3); }
    { LoudnessMeter m; m.prepare(48000); auto a = sine(48000, 20, -23); std::vector<float> z(a.size(), 0.f);
      m.process(a.data(), a.data(), (int) a.size()); m.process(z.data(), z.data(), (int) z.size());
      check("Gating ignores trailing silence", m.getSnapshot().integrated, -23.0, 0.1); }
    { LoudnessMeter m; m.prepare(48000); auto q = sine(48000, 20, -33), l = sine(48000, 20, -23);
      m.process(q.data(), q.data(), (int) q.size()); m.process(l.data(), l.data(), (int) l.size());
      check("LRA of -33/-23 two-level signal", m.getSnapshot().lra, 10.0, 0.6); }
    { LoudnessMeter::Snapshot s; s.integrated = -10; s.truePeak = 0; s.shortTermMax = -8; s.lra = 5;
      std::printf("\nSample advice (-10 LUFS, 0 dBTP vs Spotify):\n"); for (auto& x : plat::advice(plat::all()[0], s)) std::printf("  - %s\n", x.c_str()); }
    std::printf("\n%s\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return fails;
}
