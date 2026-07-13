// Knob.hpp — a reusable labelled rotary knob for DPF NanoVG UIs.
//
// It is a self-contained NanoSubWidget: give it a bounding box, a range, a caption and a value
// formatter, and it draws itself (arc value-ring + bevelled body + indicator + label + live
// readout) and handles its own interaction. Interaction (vertical drag, scroll-wheel, double-
// click-to-default, optional log law) comes from DPF's KnobEventHandler — the same machinery
// DPF's own widgets use — so this file only owns the drawing.
//
// Usage:
//   auto k = std::make_unique<Knob>(this, this); // parent widget, KnobEventHandler::Callback*
//   k->setId(myParamIndex);                       // so the callback knows which control fired
//   k->setLabel("Attack");
//   k->setRange(0.001f, 10.0f); k->setDefault(0.01f); k->setUsingLogScale(true);
//   k->setFormatter([](float v){ ... return std::string; });
//   k->setValue(0.01f, false);
//   k->setSize(w, h); k->setAbsolutePos(x, y);
//
// The parent implements KnobEventHandler::Callback and routes knobValueChanged() to its host
// (e.g. setParameterValue) using widget->getId().

#ifndef SIMPLESYNTH_KNOB_HPP
#define SIMPLESYNTH_KNOB_HPP

#include "NanoVG.hpp"
#include "EventHandlers.hpp"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

START_NAMESPACE_DGL

class Knob : public NanoSubWidget,
             public KnobEventHandler
{
public:
    explicit Knob(Widget* parent, KnobEventHandler::Callback* callback)
        : NanoSubWidget(parent),
          KnobEventHandler(this)
    {
        fHasFont = loadSharedResources(); // DPF's bundled DejaVu Sans for this NanoVG context
        setCallback(callback);
        setOrientation(KnobEventHandler::Vertical); // drag up = increase
    }

    void setLabel(const std::string& label) { fLabel = label; }
    void setAccentColor(Color color)         { fAccent = color; }

    // Custom value readout (e.g. "0.35 s"). Defaults to two decimals.
    void setFormatter(std::function<std::string(float)> fn) { fFormat = std::move(fn); }

protected:
    void onNanoDisplay() override
    {
        const float w  = getWidth();
        const float h  = getHeight();
        const float cx = w * 0.5f;
        const float cy = h * 0.42f;                       // knob above, label/readout below
        const float r  = std::fmin(w, h * 0.74f) * 0.34f;

        const float frac     = getNormalizedValue();      // 0..1, respects the log law
        const float valAngle = kArcStart + frac * kArcSweep;

        // value-ring track (dim, full sweep)
        beginPath();
        arc(cx, cy, r + 6, kArcStart, kArcStart + kArcSweep, CW);
        strokeColor(Color(52, 56, 63));
        strokeWidth(4.0f);
        stroke();

        // value-ring fill (accent, up to current value)
        beginPath();
        arc(cx, cy, r + 6, kArcStart, valAngle, CW);
        strokeColor(fAccent);
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
        strokeColor(Color(18, 20, 24));
        strokeWidth(1.5f);
        stroke();

        // indicator line (center → value angle)
        const float ix = cx + std::cos(valAngle) * (r - 3);
        const float iy = cy + std::sin(valAngle) * (r - 3);
        beginPath();
        moveTo(cx + std::cos(valAngle) * (r * 0.35f), cy + std::sin(valAngle) * (r * 0.35f));
        lineTo(ix, iy);
        strokeColor(Color(233, 236, 240));
        strokeWidth(2.5f);
        stroke();

        // label + value readout (skip if the shared font failed to load)
        if (fHasFont)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fontSize(13.0f);
            fillColor(Color(176, 182, 190));
            text(cx, cy + r + 10, fLabel.c_str(), nullptr);

            const std::string readout = fFormat ? fFormat(getValue()) : defaultFormat(getValue());
            fontSize(12.0f);
            fillColor(Color(226, 230, 236));
            text(cx, cy + r + 26, readout.c_str(), nullptr);
        }
    }

    // Hand interaction to the KnobEventHandler.
    bool onMouse(const MouseEvent& ev)   override { return mouseEvent(ev); }
    bool onMotion(const MotionEvent& ev) override { return motionEvent(ev); }
    bool onScroll(const ScrollEvent& ev) override { return scrollEvent(ev); }

private:
    std::string                       fLabel;
    std::function<std::string(float)> fFormat;
    Color                             fAccent { 79, 208, 224 };
    bool                              fHasFont = false;

    static constexpr float kArcStart = 0.75f * 3.14159265358979f; // 135°
    static constexpr float kArcSweep = 1.50f * 3.14159265358979f; // 270° sweep, gap at bottom

    static std::string defaultFormat(float v)
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        return buf;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Knob)
};

END_NAMESPACE_DGL

#endif // SIMPLESYNTH_KNOB_HPP
