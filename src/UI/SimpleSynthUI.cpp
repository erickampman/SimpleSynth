// SimpleSynthUI.cpp — UI side: a row of reusable Knob widgets (Gain + ADSR) bound to the
// plugin parameters. The knob visuals + interaction live in the reusable Knob component
// (Knob.hpp); this file just creates one per parameter, lays them out, and shuttles values
// between the knobs and the host. Adding a control later is: make a Knob, configure it from
// the shared params table, place it.

#include "DistrhoUI.hpp"

#include <cstdio>
#include <memory>

#include "Knob.hpp"
#include "SimpleSynthParams.h"

START_NAMESPACE_DISTRHO

using namespace simplesynth;
using DGL_NAMESPACE::Knob;

class SimpleSynthUI : public UI,
                      public Knob::Callback
{
public:
    SimpleSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            const ParamSpec& s = kParams[i];
            auto knob = std::make_unique<Knob>(this, this);
            knob->setId(i);
            knob->setLabel(s.name);
            knob->configure(s.min, s.max, s.logarithmic, s.def);
            knob->setRealValue(s.def, false);
            const bool isTime = s.unit[0] == 's';
            knob->setFormatter([isTime](float v) { return format(v, isTime); });
            fKnobs[i] = std::move(knob);
        }
        layoutKnobs();
    }

protected:
    /* -- host -> UI ------------------------------------------------------- */
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount)
        {
            fKnobs[index]->setRealValue(value, false);
            fKnobs[index]->repaint();
        }
    }

    /* -- knob -> host (Knob::Callback, real units) ------------------------ */
    void knobDragStarted(Knob* knob) override  { editParameter(knob->getId(), true); }
    void knobDragFinished(Knob* knob) override { editParameter(knob->getId(), false); }
    void knobValueChanged(Knob* knob, float realValue) override
    {
        setParameterValue(knob->getId(), realValue);
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        // Panel background; the knobs draw themselves as child widgets.
        beginPath();
        rect(0, 0, getWidth(), getHeight());
        fillColor(24, 26, 31);
        fill();
    }

    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        layoutKnobs();
    }

private:
    std::unique_ptr<Knob> fKnobs[kParamCount];

    // Even columns across the full width; each knob fills its column and centres itself.
    void layoutKnobs()
    {
        const uint colW = getWidth() / kParamCount;
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            fKnobs[i]->setSize(colW, getHeight());
            fKnobs[i]->setAbsolutePos(static_cast<int>(i * colW), 0);
        }
    }

    static std::string format(float v, bool isTime)
    {
        char buf[24];
        if (isTime)
        {
            if (v < 1.0f)
                std::snprintf(buf, sizeof(buf), "%.0f ms", v * 1000.0f);
            else
                std::snprintf(buf, sizeof(buf), "%.2f s", v);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "%.2f", v);
        }
        return buf;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleSynthUI)
};

UI* createUI()
{
    return new SimpleSynthUI();
}

END_NAMESPACE_DISTRHO
