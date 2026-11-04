#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DistParameters.h"
#include "../UI/EffectWidgets.h"
#include "../UI/SpectrumAnalyser.h"

/** C-Dist's panels (in their own namespace, beside the synths' panels of the same names). */
namespace cdaw::DistUI
{
    /** C-Dist's colours: glowing valve and embers, the input cool beside them. */
    namespace Colours
    {
        inline const juce::Colour ember { 0xffff7a2f }, hot { 0xffff3b30 }, amber { 0xffffb347 }, whiteHot { 0xfffff1c1 },
                                  cool { 0xff4fa3ff }, gate { 0xff3ccf6e }, tone { 0xffffc857 };

        /** 0 (cold, dark red) .. 1 (white hot). */
        juce::Colour heat (float t);
    }

    /** A voicing's one-line description. */
    juce::String taglineOf (DistEngine::Voicing);

    //==============================================================================
    /** Base of C-Dist's panels: a dark titled panel with knobs bound to parameters. */
    class Panel : public juce::Component
    {
    public:
        Panel (EffectModel&, DistEngine&, const juce::String& title);

        void paint (juce::Graphics&) override;
        virtual void refresh();

        static constexpr int titleHeight = 28, knobHeight = 72;

    protected:
        ParamKnob* addKnob (int id, const juce::String& label, juce::Colour, float skewMidPoint = 0.0f);
        void layoutRow (juce::Rectangle<int> row, std::initializer_list<ParamKnob*>);
        juce::Rectangle<int> titleRow() const       { return getLocalBounds().removeFromTop (titleHeight).reduced (8, 4); }
        juce::Rectangle<int> content() const        { return getLocalBounds().withTrimmedTop (titleHeight).reduced (8, 4); }
        DistEngine::Settings settings() const;
        static void drawPlot (juce::Graphics&, juce::Rectangle<float>);

        /** Drags a parameter as one undoable gesture. */
        void beginDrag (std::initializer_list<int> ids, const juce::String& undoName);
        void dragTo (int id, float value);
        void endDrag();

        EffectModel& model;
        DistEngine& engine;
        juce::String title;
        juce::OwnedArray<ParamKnob> knobs;
        std::vector<int> dragging;
    };

    //==============================================================================
    /** The six voicings as chips, each drawing its own curve; click one to use it. */
    class VoicingStrip : public juce::Component,
                         public juce::SettableTooltipClient
    {
    public:
        VoicingStrip (EffectModel&);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void refresh();

    private:
        juce::Rectangle<float> chip (int index) const;
        int chipAt (juce::Point<float>) const;

        EffectModel& model;
        int shown = -1, hover = -1;
    };

    /** The headline: the curve the drive bends the wave with, coloured by how hard it saturates,
        and the real samples glowing on it where the wave spends its time. The input wave runs
        under it and the output beside it; a marker follows one sample through. */
    class ForgePanel : public Panel
    {
    public:
        ForgePanel (EffectModel&, DistEngine&);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh() override;

    private:
        static constexpr int gridSize = 128, window = 768;

        juce::Rectangle<int> display;
        juce::Image heatImage { juce::Image::ARGB, gridSize, gridSize, true };
        std::vector<float> grid = std::vector<float> ((size_t) (gridSize * gridSize), 0.0f);
        std::vector<float> waveIn = std::vector<float> ((size_t) window, 0.0f), waveOut = std::vector<float> ((size_t) window, 0.0f);
        int lastRead = 0;
        float range = 0.25f;            // the input shown: -range..range
        unsigned frame = 0;
        bool bass = false;
    };

    /** Input spectrum (cool) and output (hot): what the distortion added rises above the input as
        flickering flames; an inset shows the harmonics a note at this level gets. */
    class EmbersPanel : public Panel
    {
    public:
        EmbersPanel (EffectModel&, DistEngine&);
        ~EmbersPanel() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh() override;
        void visibilityChanged() override;

    private:
        SpectrumAnalyser input, output;
        std::vector<double> columnHz;
        EffectUI::FrequencyAxis axis;
        juce::Rectangle<float> plot;
        unsigned frame = 0;
    };

    /** The circuit as it runs: gate, tight (or the Bass split), each gain stage as a valve glowing
        as hard as it clips, tone and blend, with the signal flowing along the wires. */
    class CircuitPanel : public Panel
    {
    public:
        CircuitPanel (EffectModel&, DistEngine&);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh() override;

    private:
        juce::Rectangle<int> display;
        double flow = 0.0;
    };

    /** The gate: the input's level scrolling by, shaded green while open; drag the threshold. */
    class GatePanel : public Panel
    {
    public:
        GatePanel (EffectModel&, DistEngine&);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

    private:
        float yFor (float db) const;
        float dbAt (float y) const;

        ParamToggle onButton;
        juce::Rectangle<int> display;
        bool overLine = false;
    };

    /** The tone: the whole chain's curve (tight, the voicing, the controls), the voicing alone
        faintly; drag the nodes (tight, bass, mid, treble, smooth). */
    class TonePanel : public Panel
    {
    public:
        TonePanel (EffectModel&, DistEngine&);

        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

    private:
        enum Node { tightNode = 0, bassNode, midNode, trebleNode, smoothNode, numNodes };
        juce::Point<float> nodeAt (int node) const;
        int nodeNear (juce::Point<float>) const;
        juce::Rectangle<float> plotArea() const;
        float yFor (double db) const;

        juce::Rectangle<int> display;
        EffectUI::FrequencyAxis axis;
        int hover = -1, held = -1;
    };
}
