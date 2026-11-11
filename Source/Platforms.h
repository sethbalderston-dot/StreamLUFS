// Streaming targets + the "what should I adjust" advice engine. No JUCE dependency.
// Targets are playback-normalisation levels reported by the services / industry guides (checked Nov 2026).
// Platforms change these occasionally and several (Deezer, SoundCloud, Tidal, Amazon, YouTube) don't publish an
// official number, so treat them as editable: change the table below and rebuild.
#pragma once
#include "LoudnessMeter.h"
#include <cstdio>
#include <string>
#include <vector>

namespace plat {

struct Platform { const char* name; float target; float tpCeiling; bool boostsQuiet; const char* note; };

inline const std::vector<Platform>& all() {
    static const std::vector<Platform> p = {
        { "Spotify (Normal)", -14.f, -1.f, true,  "Turns loud tracks down and quiet tracks up. Listener can also pick Loud (-11) or Quiet (-19)." },
        { "Spotify (Loud)",   -11.f, -2.f, true,  "Listener setting. Spotify advises -2 dBTP when your master is louder than -14." },
        { "Apple Music",      -16.f, -1.f, true,  "Sound Check; turns tracks up or down toward -16." },
        { "YouTube / YT Music", -14.f, -1.f, false, "Attenuates loud tracks only; quiet tracks are left alone." },
        { "Amazon Music",     -14.f, -2.f, false, "Normalisation is a listener toggle. -2 dBTP is the safer ceiling." },
        { "Tidal",            -14.f, -1.f, false, "Normalisation is on by default (album-aware)." },
        { "Deezer",           -15.f, -1.f, true,  "-15 relayed by Deezer staff; unofficial and may change." },
        { "SoundCloud",       -14.f, -1.f, false, "Behaviour is inconsistent; many guides say it may not normalise at all." },
        { "EBU R128 (broadcast)", -23.f, -1.f, true, "Not streaming: TV/radio delivery. Tolerance +/-0.5 LU." },
    };
    return p;
}

enum class Status { Idle, Good, Warn, Bad };

inline Status evaluate(const Platform& p, const lm::LoudnessMeter::Snapshot& s) {
    if (s.integrated < -150.f) return Status::Idle;
    float d = std::fabs(s.integrated - p.target);
    Status a = d <= 1.0f ? Status::Good : d <= 2.5f ? Status::Warn : Status::Bad;
    float over = s.truePeak - p.tpCeiling;
    Status b = over <= 0.f ? Status::Good : over <= 1.f ? Status::Warn : Status::Bad;
    return (int) a > (int) b ? a : b;
}

inline std::string fmt(const char* f, double a = 0, double b = 0, double c = 0) {
    char buf[512]; std::snprintf(buf, sizeof buf, f, a, b, c); return buf;
}

inline std::vector<std::string> advice(const Platform& p, const lm::LoudnessMeter::Snapshot& s) {
    std::vector<std::string> L;
    if (s.integrated < -150.f) {
        L.push_back("Play the WHOLE song (or the loudest section you care about) from the start. Integrated LUFS needs the full track to be meaningful.");
        L.push_back("Put this plugin LAST on your master bus, after the limiter. Hit Reset before each full pass.");
        return L;
    }
    const double d = s.integrated - p.target;
    if (d > 1.0) {
        L.push_back(fmt("LOUD: %.1f LU above %.0f LUFS. The service will turn you down ~%.1f dB, so the extra level only costs punch.", d, p.target, d));
        L.push_back(fmt("FIX: pull the limiter input/threshold (or master fader into it) down ~%.1f dB until Integrated reads about %.0f.", d, p.target));
        L.push_back("If it loses energy: back off master-bus compression ratio/clipper first - that's where over-loudness usually comes from.");
        L.push_back("K-weighting boosts highs (+4 dB above ~2 kHz): harsh 3-8 kHz (vocal sibilance, hats, cymbals) inflates LUFS - de-ess/trim there.");
    } else if (d < -1.0) {
        L.push_back(fmt("QUIET: %.1f LU below %.0f LUFS.", -d, p.target));
        if (p.boostsQuiet) L.push_back("This service can turn you UP, but only if the listener has normalisation on - and it can't recover dynamics you didn't get.");
        else L.push_back("This service will NOT turn you up - you'll play quieter than the tracks around you.");
        L.push_back(fmt("FIX: raise limiter input/gain ~%.1f dB (watch gain reduction; keep it musical).", -d));
        L.push_back("Sub/bass under ~100 Hz adds little LUFS but eats headroom: high-pass below 25-30 Hz, check kick vs. bass, and add presence (2-5 kHz) rather than low end.");
    } else {
        L.push_back(fmt("ON TARGET: %.1f LUFS integrated is within 1 LU of %.0f. No loudness change needed.", s.integrated, p.target));
    }
    const double over = s.truePeak - p.tpCeiling;
    if (over > 0.0) {
        L.push_back(fmt("PEAKS: true peak %.1f dBTP is %.1f dB over the %.1f dBTP ceiling. Lower the limiter ceiling to that value with true-peak/oversampling ON.", s.truePeak, over, p.tpCeiling));
        L.push_back("Lossy encodes (AAC/Ogg) add up to ~1 dB of overshoot, which is why the ceiling is below 0.");
    } else {
        L.push_back(fmt("Peaks OK: %.1f dBTP, %.1f dB under the %.1f ceiling.", s.truePeak, -over, p.tpCeiling));
    }
    const double plr = s.truePeak - s.integrated;
    if (plr < 7.0) L.push_back(fmt("DENSITY: peak-to-loudness is only %.1f dB - heavily limited. Expect flat transients; ease compression, rebalance drums.", plr));
    else if (plr > 14.0) L.push_back(fmt("DYNAMIC: peak-to-loudness is %.1f dB - very open. Fine for acoustic/classical; for pop/EDM consider more bus glue.", plr));
    if (s.shortTermMax > -150.f && s.shortTermMax - s.integrated > 4.0)
        L.push_back(fmt("BALANCE: loudest 3 s is %.1f LU over the song average - sections differ a lot. Lift quiet sections or ride the chorus down if unintended.", s.shortTermMax - s.integrated));
    if (s.lra > -150.f) {
        if (s.lra > 12.0) L.push_back(fmt("Loudness range %.1f LU is wide - quiet parts may vanish in cars/earbuds. Compress vocals/mix bus a little.", s.lra));
        else if (s.lra < 3.0 && s.lra >= 0.0) L.push_back(fmt("Loudness range %.1f LU is very flat - no dynamic contrast between sections.", s.lra));
    }
    return L;
}

} // namespace plat
