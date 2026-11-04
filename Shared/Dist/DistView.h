#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DistPanels.h"
#include "DistPresets.h"

namespace cdaw
{
    /** C-Dist's editor, shared by both builds:
        - header: the preset browser (step, pick by voicing, save your own) and the input,
          output and heat meters
        - the six voicings, each chip drawing its own curve
        - the Forge: the curve the drive bends the wave with, the real samples glowing on it, the
          input under it and the output beside it, one sample followed through; the drive knobs
        - Embers: the input and output spectra, what the drive added rising as flames, and the
          harmonics a note gets right now
        - the circuit: gate, tight, the stages as valves glowing as hard as they clip; level and blend
        - the gate's history with its threshold to drag, and the tone curve with its nodes. */
    class DistView : public juce::Component,
                     private juce::Timer
    {
    public:
        DistView (EffectModel&, DistEngine&);
        ~DistView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        /** One display frame (the timer calls it; tests and snapshots too). */
        void tick();

        void applyPreset (const DistPresets::Preset&);
        juce::String getPresetName() const;

        static constexpr int defaultWidth = 1180, defaultHeight = 800, minimumWidth = 1080, minimumHeight = 740;

    private:
        void timerCallback() override                   { tick(); }
        void updatePresetButton();
        void stepPreset (int delta);
        void showPresetMenu();
        void savePreset();

        EffectModel& model;
        DistEngine& engine;

        juce::TextButton previousButton { "<" }, nextButton { ">" }, presetButton, saveButton { "Save" };
        std::unique_ptr<DistUI::VoicingStrip> voicings;
        std::unique_ptr<DistUI::ForgePanel> forge;
        std::unique_ptr<DistUI::EmbersPanel> embers;
        std::unique_ptr<DistUI::CircuitPanel> circuit;
        std::unique_ptr<DistUI::GatePanel> gate;
        std::unique_ptr<DistUI::TonePanel> tone;

        std::vector<DistPresets::Preset> presets;
        unsigned frame = 0;
        float inputLevel[2] {}, outputLevel[2] {}, heat = 0.0f;
        juce::Rectangle<int> headerArea, meterArea;
    };
}
