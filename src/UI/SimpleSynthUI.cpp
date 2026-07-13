// SimpleSynthUI.cpp — UI side: a row of five NanoVG-drawn rotary knobs (Gain + ADSR) bound to
// the plugin parameters. Basic on purpose — real synth-panel drawing (arc value-ring, bevelled
// body, indicator, label + live readout) but restrained, so each control can be richened later.
//
// Knobs are drawn from NanoVG vector primitives (no image assets) and edited by vertical drag,
// the conventional knob gesture. Ranges/labels come from the shared SimpleSynthParams table.

#include "DistrhoUI.hpp"

#include <cmath>
#include <cstdio>

#include "SimpleSynthParams.h"

START_NAMESPACE_DISTRHO

using namespace simplesynth;

// 270° sweep with a 90° gap at the bottom, in NanoVG angle convention (0 = east, CW positive).
static constexpr float kArcStart = 0.75f * M_PI;         // 135°
static constexpr float kArcSweep = 1.50f * M_PI;         // 270°

class SimpleSynthUI : public UI
{
public:
    SimpleSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        loadSharedResources(); // bundled "sans" font
        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i] = kParams[i].def;
        setGeometryConstraints(360, 220, false, false);
    }

protected:
    /* -- host -> UI ------------------------------------------------------- */
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount)
        {
            fValues[index] = value;
            repaint();
        }
    }

    /* -- drawing ---------------------------------------------------------- */
    void onNanoDisplay() override
    {
        const float w = getWidth();
        const float h = getHeight();

        // panel background
        beginPath();
        rect(0, 0, w, h);
        fillColor(24, 26, 31);
        fill();

        for (uint32_t i = 0; i < kParamCount; ++i)
            drawKnob(i);
    }

    /* -- interaction (vertical drag) -------------------------------------- */
    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        if (ev.press)
        {
            const int hit = knobAt(ev.pos.getX(), ev.pos.getY());
            if (hit < 0)
                return false;
            fDragging = hit;
            fLastY    = ev.pos.getY();
            editParameter(static_cast<uint32_t>(hit), true);
            return true;
        }

        if (fDragging >= 0)
        {
            editParameter(static_cast<uint32_t>(fDragging), false);
            fDragging = -1;
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fDragging < 0)
            return false;

        const double y     = ev.pos.getY();
        const double delta = (fLastY - y) / 180.0; // one panel-height of drag ≈ full range
        fLastY = y;

        const uint32_t idx = static_cast<uint32_t>(fDragging);
        float frac = valueToFrac(idx, fValues[idx]) + static_cast<float>(delta);
        frac = clampf(frac, 0.0f, 1.0f);

        const float value = fracToValue(idx, frac);
        fValues[idx] = value;
        setParameterValue(idx, value);
        repaint();
        return true;
    }

private:
    float fValues[kParamCount];
    int   fDragging = -1;
    double fLastY   = 0.0;

    /* -- geometry --------------------------------------------------------- */
    // Column i spans [i/N, (i+1)/N] of the width; the knob sits in its upper part with the
    // label + readout below.
    void knobGeometry(uint32_t i, float& cx, float& cy, float& r) const
    {
        const float w    = getWidth();
        const float h    = getHeight();
        const float colW = w / static_cast<float>(kParamCount);
        cx = colW * (i + 0.5f);
        cy = h * 0.42f;
        r  = std::fmin(colW, h) * 0.30f;
    }

    int knobAt(double px, double py) const
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            float cx, cy, r;
            knobGeometry(i, cx, cy, r);
            const double dx = px - cx, dy = py - cy;
            if (dx * dx + dy * dy <= (r * 1.4) * (r * 1.4))
                return static_cast<int>(i);
        }
        return -1;
    }

    void drawKnob(uint32_t i)
    {
        float cx, cy, r;
        knobGeometry(i, cx, cy, r);

        const float frac     = valueToFrac(i, fValues[i]);
        const float valAngle = kArcStart + frac * kArcSweep;

        // value-ring track (dim, full sweep)
        beginPath();
        arc(cx, cy, r + 6, kArcStart, kArcStart + kArcSweep, CW);
        strokeColor(52, 56, 63, 255);
        strokeWidth(4.0f);
        stroke();

        // value-ring fill (accent, up to current value)
        beginPath();
        arc(cx, cy, r + 6, kArcStart, valAngle, CW);
        strokeColor(79, 208, 224, 255);
        strokeWidth(4.0f);
        stroke();

        // knob body (bevelled radial gradient)
        Paint body = radialGradient(cx, cy - r * 0.3f, r * 0.2f, r, Color(64, 68, 76), Color(34, 37, 43));
        beginPath();
        circle(cx, cy, r);
        fillPaint(body);
        fill();

        // rim
        beginPath();
        circle(cx, cy, r);
        strokeColor(18, 20, 24, 255);
        strokeWidth(1.5f);
        stroke();

        // indicator line (center → value angle)
        const float ix = cx + std::cos(valAngle) * (r - 3);
        const float iy = cy + std::sin(valAngle) * (r - 3);
        beginPath();
        moveTo(cx + std::cos(valAngle) * (r * 0.35f), cy + std::sin(valAngle) * (r * 0.35f));
        lineTo(ix, iy);
        strokeColor(233, 236, 240, 255);
        strokeWidth(2.5f);
        stroke();

        // label
        fontFace("sans");
        fontSize(13.0f);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(176, 182, 190, 255);
        text(cx, cy + r + 12, kParams[i].name, nullptr);

        // value readout
        char buf[24];
        formatValue(i, fValues[i], buf, sizeof(buf));
        fontSize(12.0f);
        fillColor(226, 230, 236, 255);
        text(cx, cy + r + 28, buf, nullptr);
    }

    /* -- value <-> fraction (log law for the time knobs) ------------------ */
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    float valueToFrac(uint32_t i, float v) const
    {
        const ParamSpec& s = kParams[i];
        v = clampf(v, s.min, s.max);
        if (s.logarithmic)
            return std::log(v / s.min) / std::log(s.max / s.min);
        return (v - s.min) / (s.max - s.min);
    }

    float fracToValue(uint32_t i, float f) const
    {
        const ParamSpec& s = kParams[i];
        if (s.logarithmic)
            return s.min * std::pow(s.max / s.min, f);
        return s.min + f * (s.max - s.min);
    }

    static void formatValue(uint32_t i, float v, char* buf, size_t cap)
    {
        if (kParams[i].unit[0] == 's') // a time control
        {
            if (v < 1.0f)
                std::snprintf(buf, cap, "%.0f ms", v * 1000.0f);
            else
                std::snprintf(buf, cap, "%.2f s", v);
        }
        else
        {
            std::snprintf(buf, cap, "%.2f", v);
        }
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleSynthUI)
};

UI* createUI()
{
    return new SimpleSynthUI();
}

END_NAMESPACE_DISTRHO
