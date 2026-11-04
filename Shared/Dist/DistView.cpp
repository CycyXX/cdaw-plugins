#include "DistView.h"
#include "../LookAndFeel/Palette.h"

namespace cdaw
{
    using P = DistParameters::ID;
    using namespace DistUI;

    namespace
    {
        constexpr int headerHeight = 64, stripHeight = 62;
    }

    DistView::DistView (EffectModel& m, DistEngine& e) : model (m), engine (e)
    {
        setOpaque (true);

        for (auto* b : { &previousButton, &nextButton })
            addAndMakeVisible (b);
        previousButton.setTooltip ("Previous preset");
        nextButton.setTooltip ("Next preset");
        previousButton.onClick = [this] { stepPreset (-1); };
        nextButton.onClick = [this] { stepPreset (1); };
        presetButton.setTooltip ("Choose a preset");
        presetButton.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (presetButton);
        saveButton.setTooltip ("Save these settings as a preset of your own");
        saveButton.onClick = [this] { savePreset(); };
        addAndMakeVisible (saveButton);

        voicings = std::make_unique<VoicingStrip> (model);
        forge = std::make_unique<ForgePanel> (model, engine);
        embers = std::make_unique<EmbersPanel> (model, engine);
        circuit = std::make_unique<CircuitPanel> (model, engine);
        gate = std::make_unique<GatePanel> (model, engine);
        tone = std::make_unique<TonePanel> (model, engine);
        for (juce::Component* c : std::initializer_list<juce::Component*> { voicings.get(), forge.get(), embers.get(), circuit.get(), gate.get(), tone.get() })
            addAndMakeVisible (c);

        presets = DistPresets::everything();
        updatePresetButton();
        setSize (defaultWidth, defaultHeight);
        startTimerHz (60);
    }

    DistView::~DistView()
    {
        stopTimer();
    }

    void DistView::tick()
    {
        voicings->refresh();
        for (Panel* p : std::initializer_list<Panel*> { forge.get(), embers.get(), circuit.get(), gate.get(), tone.get() })
            p->refresh();

        if (frame % 15 == 0)
            updatePresetButton();
        ++frame;

        for (int ch = 0; ch < 2; ++ch)
        {
            const float in = juce::Decibels::gainToDecibels (engine.inputPeak[ch].take(), -100.0f);
            const float out = juce::Decibels::gainToDecibels (engine.outputPeak[ch].take(), -100.0f);
            inputLevel[ch] = in >= inputLevel[ch] ? in : juce::jmax (in, inputLevel[ch] - 0.8f);
            outputLevel[ch] = out >= outputLevel[ch] ? out : juce::jmax (out, outputLevel[ch] - 0.8f);
        }
        float hottest = 0.0f;
        for (auto& h : engine.stageHeat)
            hottest = juce::jmax (hottest, h.load());
        heat += (hottest - heat) * 0.2f;
        repaint (meterArea);
    }

    //==============================================================================
    juce::String DistView::getPresetName() const
    {
        auto name = model.getText ("presetName");
        return name.isNotEmpty() ? name : DistPresets::factory()[0].name;      // the defaults are the Singing Lead
    }

    void DistView::updatePresetButton()
    {
        const auto name = getPresetName();
        bool edited = false;

        for (auto& p : presets)
            if (p.name == name)
            {
                const auto values = DistPresets::valuesOf (p);
                for (int i = 0; i < P::count && ! edited; ++i)
                {
                    const auto& spec = DistParameters::get (i);
                    edited = std::abs (model.value (i) - values[(size_t) i]) > (spec.maximum - spec.minimum) * 0.0005f;
                }

                const auto text = p.category + " / " + name + (edited ? " *" : "");
                if (presetButton.getButtonText() != text)
                    presetButton.setButtonText (text);
                return;
            }

        presetButton.setButtonText (name);
    }

    void DistView::applyPreset (const DistPresets::Preset& preset)
    {
        model.beginUndoTransaction ("Preset: " + preset.name);
        const auto values = DistPresets::valuesOf (preset);

        for (int i = 0; i < P::count; ++i)
        {
            auto p = model.parameter (i);
            if (std::abs (p->getValue() - values[(size_t) i]) > 1.0e-6f)
            {
                p->beginGesture();
                p->setValue (values[(size_t) i]);
                p->endGesture();
            }
        }

        model.setText ("presetName", preset.name);
        updatePresetButton();
    }

    void DistView::stepPreset (int delta)
    {
        presets = DistPresets::everything();
        if (presets.empty())
            return;

        int current = 0;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].name == getPresetName())
                current = i;

        const int n = (int) presets.size();
        applyPreset (presets[(size_t) (((current + delta) % n + n) % n)]);
    }

    void DistView::showPresetMenu()
    {
        presets = DistPresets::everything();
        const auto current = getPresetName();
        juce::PopupMenu menu;

        juce::Component::SafePointer<DistView> safe (this);
        auto choose = [safe] (DistPresets::Preset p) { if (safe != nullptr) safe->applyPreset (p); };

        for (auto& voicing : DistEngine::getVoicingNames())
        {
            juce::PopupMenu sub;
            for (auto& p : presets)
                if (! p.isUser() && p.category == voicing)
                    sub.addItem (p.name, true, p.name == current, [choose, p] { choose (p); });
            menu.addSubMenu (voicing, sub);
        }

        juce::PopupMenu user;
        for (auto& p : presets)
            if (p.isUser())
                user.addItem (p.name, true, p.name == current, [choose, p] { choose (p); });
        if (user.getNumItems() == 0)
            user.addItem ("(none yet: use Save)", false, false, [] {});
        menu.addSubMenu ("User", user);

        menu.addSeparator();
        menu.addItem ("Save as preset...", [safe] { if (safe != nullptr) safe->savePreset(); });
        menu.addItem ("Show presets folder", [] { DistPresets::getUserFolder().createDirectory(); DistPresets::getUserFolder().revealToUser(); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton));
    }

    void DistView::savePreset()
    {
        auto* window = new juce::AlertWindow ("Save preset", "Name of the preset (saved in " + DistPresets::getUserFolder().getFullPathName() + "):",
                                              juce::MessageBoxIconType::NoIcon, this);
        window->addTextEditor ("name", getPresetName());
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<DistView> safe (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
        {
            if (result != 1 || safe == nullptr)
                return;

            const auto name = window->getTextEditorContents ("name").trim();
            auto& model = safe->model;
            const auto saved = DistPresets::saveUser (name, [&model] (int id) { return model.value (id); });
            if (saved.failed())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not saved", saved.getErrorMessage());
                return;
            }

            model.beginUndoTransaction ("Save preset");
            model.setText ("presetName", name);
            safe->presets = DistPresets::everything();
            safe->updatePresetButton();
        }), true);
    }

    //==============================================================================
    void DistView::paint (juce::Graphics& g)
    {
        g.fillAll (Palette::panelBackground);

        auto header = headerArea;
        g.setGradientFill (juce::ColourGradient::horizontal (Colours::hot.withAlpha (0.10f), 0.0f, Palette::windowBackground, (float) getWidth() * 0.5f));
        g.fillRect (header);
        g.setColour (Palette::panelOutline);
        g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
        g.drawText ("C-DIST", header.withTrimmedLeft (14).withWidth (130).withTrimmedBottom (14), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (10.0f));
        g.setColour (Colours::amber);
        g.drawText ("drive circuits after modern amps", header.withTrimmedLeft (14).withWidth (200).withTrimmedTop (30), juce::Justification::centredLeft);

        // In, out and heat.
        auto m = meterArea.toFloat();
        const float barW = 9.0f, gap = 3.0f, groupGap = 12.0f;
        auto bar = [&] (float x, float value, juce::Colour top, juce::Colour bottom)
        {
            auto r = juce::Rectangle<float> (x, m.getY() + 12.0f, barW, m.getHeight() - 12.0f);
            g.setColour (EffectUI::plotBackground);
            g.fillRect (r);
            auto fill = r.withTop (r.getBottom() - juce::jlimit (0.0f, 1.0f, value) * r.getHeight());
            g.setGradientFill (juce::ColourGradient::vertical (top, r.getY(), bottom, r.getBottom()));
            g.fillRect (fill);
        };
        auto label = [&] (float x, float w, const char* text)
        {
            g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
            g.setColour (Palette::textDim);
            g.drawText (text, juce::Rectangle<float> (x, m.getY(), w, 11.0f), juce::Justification::centred);
        };
        float x = m.getX();
        label (x, 2.0f * barW + gap, "IN");
        for (int ch = 0; ch < 2; ++ch, x += barW + gap)
            bar (x, (inputLevel[ch] + 48.0f) / 48.0f, Palette::meterRed, Colours::cool);
        x += groupGap - gap;
        label (x, 2.0f * barW + gap, "OUT");
        for (int ch = 0; ch < 2; ++ch, x += barW + gap)
            bar (x, (outputLevel[ch] + 48.0f) / 48.0f, Palette::meterRed, Palette::meterGreen);
        x += groupGap - gap;
        label (x - 12.0f, barW + 24.0f, "HEAT");
        bar (x, heat, Colours::whiteHot, Colours::hot);
    }

    void DistView::resized()
    {
        auto r = getLocalBounds();
        headerArea = r.removeFromTop (headerHeight);

        auto h = headerArea.reduced (8, 6);
        meterArea = h.removeFromRight (100).reduced (0, 2);
        h.removeFromLeft (220);
        auto presetArea = h.withSizeKeepingCentre (juce::jmin (430, h.getWidth() - 20), 28);
        previousButton.setBounds (presetArea.removeFromLeft (28));
        presetArea.removeFromLeft (4);
        saveButton.setBounds (presetArea.removeFromRight (56));
        presetArea.removeFromRight (4);
        nextButton.setBounds (presetArea.removeFromRight (28));
        presetArea.removeFromRight (4);
        presetButton.setBounds (presetArea);

        voicings->setBounds (r.removeFromTop (stripHeight));

        auto body = r.reduced (8, 6);
        const int gap = 6;
        auto left = body.removeFromLeft (body.getWidth() * 55 / 100);
        body.removeFromLeft (gap);
        forge->setBounds (left.removeFromTop (left.getHeight() * 62 / 100));
        left.removeFromTop (gap);
        circuit->setBounds (left);

        const int height = body.getHeight();
        embers->setBounds (body.removeFromTop (height * 38 / 100));
        body.removeFromTop (gap);
        gate->setBounds (body.removeFromTop (height * 25 / 100));
        body.removeFromTop (gap);
        tone->setBounds (body);
    }
}
