#include "PluginEditor.h"
using namespace juce;

namespace {
const Colour kBg(0xff14171c), kPanel(0xff1e232b), kText(0xffe6e9ee), kDim(0xff8b94a3), kAccent(0xff4aa3ff);
Colour statusColour(plat::Status s) {
    switch (s) { case plat::Status::Good: return Colour(0xff3ecf8e); case plat::Status::Warn: return Colour(0xffffc145);
                 case plat::Status::Bad: return Colour(0xffff5c5c); default: return Colour(0xff5b6472); }
}
String val(float v, int dp = 1) { return v < -150.f ? String("--") : String(v, dp); }
constexpr int kRowY = 262, kRowH = 26;
}

LufsEditor::LufsEditor(LufsProcessor& p) : AudioProcessorEditor(&p), proc(p) {
    setSize(900, 700);
    addAndMakeVisible(resetButton);
    resetButton.onClick = [this] { proc.meter.requestReset(); history.clear(); };
    startTimerHz(30);
}

void LufsEditor::resized() { resetButton.setBounds(getWidth() - 90, 12, 76, 26); }

Rectangle<int> LufsEditor::rowBounds(int i) const { return { 16, kRowY + 24 + i * kRowH, getWidth() - 32, kRowH - 2 }; }

void LufsEditor::timerCallback() {
    snap = proc.meter.getSnapshot();
    if (++tick % 3 == 0) { history.push_back({ snap.momentary, snap.shortTerm }); if (history.size() > 600) history.pop_front(); }
    repaint();
}

void LufsEditor::mouseDown(const MouseEvent& e) {
    for (int i = 0; i < (int) plat::all().size(); ++i)
        if (rowBounds(i).contains(e.getPosition())) { proc.selectedPlatform = i; repaint(); }
}

void LufsEditor::paint(Graphics& g) {
    g.fillAll(kBg);
    const auto& P = plat::all();
    const auto& sel = P[(size_t) jlimit(0, (int) P.size() - 1, proc.selectedPlatform)];

    g.setColour(kText); g.setFont(Font(FontOptions(20.f, Font::bold)));
    g.drawText("STREAM LUFS", 16, 10, 300, 30, Justification::left);
    g.setColour(kDim); g.setFont(Font(FontOptions(12.f)));
    g.drawText("ITU-R BS.1770-4 K-weighted  |  put me last on the master bus, play the full track", 16, 36, 600, 16, Justification::left);

    // readouts
    struct R { const char* name; String v; const char* unit; Colour c; };
    float plr = (snap.truePeak > -150.f && snap.integrated > -150.f) ? snap.truePeak - snap.integrated : lm::kFloor;
    R rs[] = { { "INTEGRATED", val(snap.integrated), "LUFS", kAccent }, { "SHORT-TERM", val(snap.shortTerm), "LUFS", kText },
               { "MOMENTARY", val(snap.momentary), "LUFS", kText }, { "LOUDNESS RANGE", val(snap.lra), "LU", kText },
               { "TRUE PEAK", val(snap.truePeak), "dBTP", snap.truePeak > -1.f ? Colour(0xffff5c5c) : kText },
               { "PEAK / LOUDNESS", val(plr), "dB", kText } };
    int w = (getWidth() - 32 - 5 * 8) / 6;
    for (int i = 0; i < 6; ++i) {
        Rectangle<int> b(16 + i * (w + 8), 62, w, 78);
        g.setColour(kPanel); g.fillRoundedRectangle(b.toFloat(), 8.f);
        g.setColour(kDim); g.setFont(Font(FontOptions(11.f))); g.drawText(rs[i].name, b.withHeight(22).reduced(10, 0).translated(0, 6), Justification::left);
        g.setColour(rs[i].c); g.setFont(Font(FontOptions(30.f, Font::bold))); g.drawText(rs[i].v, b.withTrimmedTop(24).withHeight(36).reduced(10, 0), Justification::left);
        g.setColour(kDim); g.setFont(Font(FontOptions(11.f))); g.drawText(rs[i].unit, b.withTrimmedTop(58).reduced(10, 0), Justification::left);
    }

    // history graph
    Rectangle<float> gr(16.f, 150.f, (float) getWidth() - 32.f, 100.f);
    g.setColour(kPanel); g.fillRoundedRectangle(gr, 8.f);
    auto yOf = [&](float v) { return gr.getBottom() - 6.f - (jlimit(-40.f, 0.f, v) + 40.f) / 40.f * (gr.getHeight() - 12.f); };
    g.setColour(kDim.withAlpha(0.35f)); g.setFont(Font(FontOptions(10.f)));
    for (int db = -40; db <= 0; db += 10) { g.drawHorizontalLine((int) yOf((float) db), gr.getX() + 30, gr.getRight() - 6); g.drawText(String(db), (int) gr.getX() + 4, (int) yOf((float) db) - 6, 26, 12, Justification::left); }
    g.setColour(statusColour(plat::Status::Good).withAlpha(0.8f));
    g.drawHorizontalLine((int) yOf(sel.target), gr.getX() + 30, gr.getRight() - 6);
    g.setFont(Font(FontOptions(10.f))); g.drawText("target " + String(sel.target, 0), (int) gr.getRight() - 90, (int) yOf(sel.target) - 13, 84, 12, Justification::right);
    if (history.size() > 1) {
        for (int pass = 0; pass < 2; ++pass) {
            Path path; bool started = false; float x0 = gr.getX() + 32.f, dx = (gr.getWidth() - 40.f) / 600.f;
            for (size_t i = 0; i < history.size(); ++i) {
                float v = pass == 0 ? history[i].first : history[i].second; if (v < -150.f) continue;
                float x = x0 + dx * (float) (600 - history.size() + i);
                if (!started) { path.startNewSubPath(x, yOf(v)); started = true; } else path.lineTo(x, yOf(v));
            }
            g.setColour(pass == 0 ? kDim : kAccent); g.strokePath(path, PathStrokeType(pass == 0 ? 1.f : 2.f));
        }
    }

    // platform table
    g.setColour(kDim); g.setFont(Font(FontOptions(11.f)));
    const int cx[] = { 30, 290, 380, 500, 620, 740 };
    const char* hd[] = { "PLATFORM (click one)", "TARGET", "YOU vs TARGET", "TP CEILING", "TP MARGIN", "STATUS" };
    for (int i = 0; i < 6; ++i) g.drawText(hd[i], cx[i], kRowY, 200, 20, Justification::left);
    for (int i = 0; i < (int) P.size(); ++i) {
        auto rb = rowBounds(i); const auto& p = P[(size_t) i]; auto st = plat::evaluate(p, snap);
        g.setColour(i == proc.selectedPlatform ? Colour(0xff2a3340) : kPanel); g.fillRoundedRectangle(rb.toFloat(), 6.f);
        if (i == proc.selectedPlatform) { g.setColour(kAccent); g.drawRoundedRectangle(rb.toFloat().reduced(.5f), 6.f, 1.f); }
        g.setColour(kText); g.setFont(Font(FontOptions(13.f)));
        float d = snap.integrated - p.target, m = p.tpCeiling - snap.truePeak; bool has = snap.integrated > -150.f;
        String stext = st == plat::Status::Idle ? "waiting" : st == plat::Status::Good ? "OK" : st == plat::Status::Warn ? "Adjust" : "Fix";
        String cols[] = { p.name, String(p.target, 0) + " LUFS", has ? (d > 0 ? "+" : "") + String(d, 1) + " LU" : "--",
                          String(p.tpCeiling, 1) + " dBTP", has ? (m >= 0 ? "+" : "") + String(m, 1) + " dB" : "--", stext };
        for (int c = 0; c < 6; ++c) g.drawText(cols[c], cx[c] + (c == 5 ? 4 : 0), rb.getY(), 250, rb.getHeight(), Justification::left);
        g.setColour(statusColour(st)); g.fillEllipse((float) cx[5] - 14.f, (float) rb.getCentreY() - 5.f, 10.f, 10.f);
    }

    // advice
    int adY = kRowY + 24 + (int) P.size() * kRowH + 8;
    Rectangle<int> ad(16, adY, getWidth() - 32, getHeight() - adY - 12);
    g.setColour(kPanel); g.fillRoundedRectangle(ad.toFloat(), 8.f);
    g.setColour(kAccent); g.setFont(Font(FontOptions(13.f, Font::bold)));
    g.drawText("WHAT TO ADJUST FOR " + String(sel.name).toUpperCase(), ad.getX() + 12, ad.getY() + 6, ad.getWidth() - 24, 18, Justification::left);
    String text; for (auto& l : plat::advice(sel, snap)) text << String(CharPointer_UTF8("\xe2\x80\xa2 ")) << String(CharPointer_UTF8(l.c_str())) << "\n";
    g.setColour(kText); g.setFont(Font(FontOptions(12.5f)));
    g.drawFittedText(text, ad.reduced(12, 0).withTrimmedTop(28).withTrimmedBottom(4), Justification::topLeft, 12, 1.0f);
}
