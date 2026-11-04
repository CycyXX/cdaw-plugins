#include "DistPanels.h"
#include "../LookAndFeel/Palette.h"

namespace cdaw::DistUI
{
    using P = DistParameters::ID;
    using Voicing = DistEngine::Voicing;

    namespace Colours
    {
        juce::Colour heat (float t)
        {
            static const juce::Colour stops[] { juce::Colour (0xff3a0a06), juce::Colour (0xffb3241a), juce::Colour (0xffff5a1f),
                                                juce::Colour (0xffffa53a), juce::Colour (0xfffff3d0) };
            constexpr int numStops = (int) std::size (stops);
            const float p = juce::jlimit (0.0f, 1.0f, t) * (float) (numStops - 1);
            const int i = juce::jmin ((int) p, numStops - 2);
            return stops[i].interpolatedWith (stops[i + 1], p - (float) i);
        }
    }

    juce::String taglineOf (Voicing v)
    {
        switch (v)
        {
            case Voicing::boost:   return "glassy clean boost";
            case Voicing::crunch:  return "British crunch";
            case Voicing::lead:    return "singing sustain";
            case Voicing::modern:  return "tight high gain";
            case Voicing::fuzz:    return "starved and woolly";
            case Voicing::bass:    return "clean lows, grinding highs";
            case Voicing::numVoicings:
            default:               return {};
        }
    }

    //==============================================================================
    Panel::Panel (EffectModel& m, DistEngine& e, const juce::String& t) : model (m), engine (e), title (t)
    {
        setOpaque (true);
    }

    void Panel::paint (juce::Graphics& g)
    {
        g.fillAll (Palette::panelBackground);
        EffectUI::drawPanel (g, getLocalBounds().toFloat(), {});
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (title, titleRow().toFloat(), juce::Justification::centredLeft);
    }

    void Panel::refresh()
    {
        for (auto* k : knobs)
            k->refresh();
    }

    ParamKnob* Panel::addKnob (int id, const juce::String& label, juce::Colour colour, float skewMidPoint)
    {
        auto* k = knobs.add (new ParamKnob (model.parameter (id), label, colour, DistParameters::get (id).defaultValue, skewMidPoint));
        addAndMakeVisible (k);
        return k;
    }

    void Panel::layoutRow (juce::Rectangle<int> row, std::initializer_list<ParamKnob*> list)
    {
        const int w = row.getWidth() / juce::jmax (1, (int) list.size());
        for (auto* k : list)
            k->setBounds (row.removeFromLeft (w).reduced (2, 0));
    }

    DistEngine::Settings Panel::settings() const
    {
        return DistParameters::toSettings ([this] (int id) { return model.value (id); });
    }

    void Panel::drawPlot (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (EffectUI::plotBackground);
        g.fillRoundedRectangle (r, 3.0f);
    }

    void Panel::beginDrag (std::initializer_list<int> ids, const juce::String& undoName)
    {
        model.beginUndoTransaction (undoName);
        dragging.assign (ids.begin(), ids.end());
        for (int id : dragging)
            model.parameter (id)->beginGesture();
    }

    void Panel::dragTo (int id, float value)
    {
        const auto& spec = DistParameters::get (id);
        model.parameter (id)->setValue (juce::jlimit (spec.minimum, spec.maximum, value));
    }

    void Panel::endDrag()
    {
        for (int id : dragging)
            model.parameter (id)->endGesture();
        dragging.clear();
    }

    //==============================================================================
    VoicingStrip::VoicingStrip (EffectModel& m) : model (m)
    {
        setOpaque (true);
        setTooltip ("The circuit: each voicing has its own stages, curve and EQ");
    }

    juce::Rectangle<float> VoicingStrip::chip (int index) const
    {
        auto r = getLocalBounds().toFloat().reduced (8.0f, 4.0f);
        const float w = (r.getWidth() - 5.0f * 6.0f) / (float) DistEngine::numVoicings;
        return { r.getX() + (float) index * (w + 6.0f), r.getY(), w, r.getHeight() };
    }

    int VoicingStrip::chipAt (juce::Point<float> p) const
    {
        for (int i = 0; i < DistEngine::numVoicings; ++i)
            if (chip (i).contains (p))
                return i;
        return -1;
    }

    void VoicingStrip::refresh()
    {
        const int now = juce::roundToInt (model.value (P::voicing));
        if (now != shown)
        {
            shown = now;
            repaint();
        }
    }

    void VoicingStrip::mouseDown (const juce::MouseEvent& e)
    {
        const int i = chipAt (e.position);
        if (i >= 0 && i != juce::roundToInt (model.value (P::voicing)))
            model.setValue (P::voicing, (float) i, "Voicing: " + DistEngine::getVoicingNames()[i]);
        refresh();
    }

    void VoicingStrip::mouseMove (const juce::MouseEvent& e)
    {
        const int i = chipAt (e.position);
        if (i != hover)
        {
            hover = i;
            setMouseCursor (i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void VoicingStrip::mouseExit (const juce::MouseEvent&)
    {
        hover = -1;
        repaint();
    }

    void VoicingStrip::paint (juce::Graphics& g)
    {
        g.fillAll (Palette::panelBackground);
        const int current = juce::roundToInt (model.value (P::voicing));

        for (int i = 0; i < DistEngine::numVoicings; ++i)
        {
            auto r = chip (i);
            const bool on = i == current;
            if (on)
            {
                g.setGradientFill (juce::ColourGradient::vertical (Colours::ember.withAlpha (0.32f), r.getY(), Colours::hot.withAlpha (0.12f), r.getBottom()));
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (Colours::ember);
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.5f);
            }
            else
            {
                g.setColour (i == hover ? Palette::panelRaised.brighter (0.08f) : Palette::panelRaised);
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (Palette::panelOutline);
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            }

            // The voicing's own curve, in miniature.
            auto glyph = r.removeFromLeft (r.getHeight() * 1.1f).reduced (8.0f, 8.0f);
            DistEngine::Settings s;
            s.voicing = (Voicing) i;
            s.drive = 0.6f;
            const auto& p = DistEngine::profile (s.voicing);
            const float span = 4.0f / std::pow (DistEngine::stageGain (s), (float) p.stages);     // each voicing's knee, whatever its gain
            juce::Path curve;
            for (int k = 0; k <= 40; ++k)
            {
                const float x = span * (-1.0f + 2.0f * (float) k / 40.0f);
                const float y = DistEngine::transferCurve (s, x, span);
                const float px = glyph.getX() + glyph.getWidth() * (float) k / 40.0f;
                const float py = glyph.getCentreY() - juce::jlimit (-1.2f, 1.2f, y) / 1.2f * glyph.getHeight() * 0.5f;
                if (k == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
            }
            g.setColour (Palette::panelOutline.withAlpha (0.6f));
            g.drawHorizontalLine ((int) glyph.getCentreY(), glyph.getX(), glyph.getRight());
            if (on)
            {
                g.setColour (Colours::ember.withAlpha (0.3f));
                g.strokePath (curve, juce::PathStrokeType (4.0f));
            }
            g.setColour (on ? Colours::amber : Palette::textDim);
            g.strokePath (curve, juce::PathStrokeType (1.5f));

            auto text = r.reduced (2.0f, 6.0f);
            g.setColour (on ? Palette::text : Palette::text.withAlpha (0.8f));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (juce::String (DistEngine::getVoicingNames()[i]).toUpperCase(), text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::bottomLeft);
            g.setColour (on ? Colours::amber : Palette::textDim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (taglineOf ((Voicing) i), text, juce::Justification::topLeft, true);
        }
    }

    //==============================================================================
    ForgePanel::ForgePanel (EffectModel& m, DistEngine& e) : Panel (m, e, "THE FORGE")
    {
        addKnob (P::input, "Input", Palette::textDim);
        addKnob (P::drive, "Drive", Colours::ember);
        addKnob (P::tight, "Tight", Colours::cool, 140.0f);
        addKnob (P::character, "Character", Colours::amber);
        lastRead = engine.pairWrite.load();
    }

    void ForgePanel::resized()
    {
        auto r = content();
        layoutRow (r.removeFromBottom (knobHeight), { knobs[0], knobs[1], knobs[2], knobs[3] });
        display = r.withTrimmedBottom (4);
    }

    void ForgePanel::refresh()
    {
        Panel::refresh();
        ++frame;

        const bool nowBass = juce::roundToInt (model.value (P::voicing)) == (int) Voicing::bass;
        if (nowBass != bass)
        {
            bass = nowBass;
            knobs[2]->setLabel (bass ? "Crossover" : "Tight");
        }

        // New (in, out) pairs since the last frame: into the heat grid, which cools as it goes.
        const int write = engine.pairWrite.load();
        int count = (write - lastRead + DistEngine::pairLength) % DistEngine::pairLength;
        lastRead = write;
        for (auto& v : grid)
            v *= 0.8f;

        float peak = 0.0f;
        for (int k = 0; k < count; ++k)
        {
            const int i = (write - count + k + DistEngine::pairLength) % DistEngine::pairLength;
            const float in = engine.pairIn[(size_t) i].load(), out = engine.pairOut[(size_t) i].load();
            peak = juce::jmax (peak, std::abs (in));
            const float shown = std::copysign (std::sqrt (juce::jmin (1.0f, std::abs (in) / range)), in);     // the square-root scale the plot uses
            const int gx = (int) ((shown * 0.5f + 0.5f) * gridSize), gy = (int) ((0.5f - out / 2.3f) * gridSize);
            if (gx >= 0 && gx < gridSize && gy >= 0 && gy < gridSize)
                grid[(size_t) (gy * gridSize + gx)] += 1.0f;
        }

        if (count > 0)
        {
            // The view follows the playing: quickly out, slowly back in.
            const float target = juce::jlimit (0.01f, 2.0f, peak * 1.25f);
            range += (target - range) * (target > range ? 0.3f : 0.02f);
        }

        float largest = 1.0e-6f;
        for (auto v : grid)
            largest = juce::jmax (largest, v);
        {
            juce::Image::BitmapData pixels (heatImage, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < gridSize; ++y)
                for (int x = 0; x < gridSize; ++x)
                {
                    const float v = std::sqrt (grid[(size_t) (y * gridSize + x)] / largest);
                    pixels.setPixelColour (x, y, v < 0.02f ? juce::Colours::transparentBlack : Colours::heat (0.3f + 0.7f * v).withAlpha (juce::jmin (1.0f, v * 1.4f)));
                }
        }

        // The waves: a stretch starting where the input rises through zero, so it holds still.
        int start = (write - window - 1 + DistEngine::pairLength) % DistEngine::pairLength;
        for (int back = 0; back < 1024; ++back)
        {
            const int i = (write - window - back + 2 * DistEngine::pairLength) % DistEngine::pairLength;
            const int previous = (i - 1 + DistEngine::pairLength) % DistEngine::pairLength;
            if (engine.pairIn[(size_t) previous].load() < 0.0f && engine.pairIn[(size_t) i].load() >= 0.0f)
            {
                start = i;
                break;
            }
        }
        for (int k = 0; k < window; ++k)
        {
            const int i = (start + k) % DistEngine::pairLength;
            waveIn[(size_t) k] = engine.pairIn[(size_t) i].load();
            waveOut[(size_t) k] = engine.pairOut[(size_t) i].load();
        }

        repaint (display);
    }

    void ForgePanel::paint (juce::Graphics& g)
    {
        Panel::paint (g);
        const auto s = settings();
        const auto& p = DistEngine::profile (s.voicing);
        auto area = display.toFloat();
        drawPlot (g, area);

        auto r = area.reduced (10.0f, 10.0f);
        constexpr float strip = 58.0f;
        const float side = juce::jmin (r.getHeight() - strip - 6.0f, r.getWidth() * 0.5f);
        const juce::Rectangle<float> square (r.getX(), r.getY(), side, side);
        const juce::Rectangle<float> inStrip (square.getX(), square.getBottom() + 6.0f, side, strip);
        const juce::Rectangle<float> outStrip (square.getRight() + 10.0f, square.getY(), r.getRight() - square.getRight() - 10.0f, side);
        const juce::Rectangle<float> info (outStrip.getX(), inStrip.getY(), outStrip.getWidth(), strip);

        // The input on a square-root scale: with tens of dB of gain the knee is a sliver of the
        // range on a straight scale; this gives it, and the quiet parts of the wave, room.
        auto xFor = [&] (float x) { return square.getCentreX() + std::copysign (std::sqrt (juce::jmin (1.0f, std::abs (x) / range)), x) * side * 0.5f; };
        auto yFor = [&] (float y) { return square.getCentreY() - juce::jlimit (-1.15f, 1.15f, y) / 1.15f * side * 0.5f; };

        // Axes and a frame.
        g.setColour (Palette::panelOutline.withAlpha (0.5f));
        g.drawRect (square, 1.0f);
        g.drawLine (square.getX(), square.getCentreY(), square.getRight(), square.getCentreY(), 1.0f);
        g.drawLine (square.getCentreX(), square.getY(), square.getCentreX(), square.getBottom(), 1.0f);
        g.setFont (juce::FontOptions (9.0f));
        g.setColour (Palette::textDisabled);
        g.drawText ("in to " + juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (range))) + " dBFS (" + juce::String::fromUTF8 ("\xe2\x88\x9a") + " scale)",
                    square.reduced (4.0f).removeFromBottom (11.0f), juce::Justification::centredRight);
        g.drawText ("out", square.reduced (4.0f).removeFromTop (11.0f), juce::Justification::centredLeft);

        // The curve, hot where it flattens (where it clips).
        const float level = juce::jmax (0.002f, engine.inputLevel.load());
        const float tiny = range * 0.002f;
        const float slope0 = juce::jmax (1.0e-6f, (DistEngine::transferCurve (s, tiny, level) - DistEngine::transferCurve (s, -tiny, level)) / (2.0f * tiny));

        // The line it would be without clipping (curved by the scale).
        {
            juce::Path line;
            for (int k = 0; k <= 80; ++k)
            {
                const float t = -1.0f + 2.0f * (float) k / 80.0f, x = std::copysign (t * t, t) * range;
                if (k == 0) line.startNewSubPath (xFor (x), yFor (x * slope0));
                else        line.lineTo (xFor (x), yFor (x * slope0));
            }
            const float dashes[] { 3.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (line, line, dashes, 2);
            g.saveState();
            g.reduceClipRegion (square.toNearestInt());
            g.setColour (Palette::textDisabled);
            g.fillPath (line);
            g.restoreState();
        }

        g.setOpacity (1.0f);
        g.drawImage (heatImage, square, juce::RectanglePlacement::stretchToFit);

        constexpr int points = 160;
        juce::Point<float> previous;
        float previousY = 0.0f;
        juce::Path whole;
        std::vector<std::pair<juce::Line<float>, float>> segments;
        float previousX = 0.0f;
        for (int k = 0; k <= points; ++k)
        {
            const float t = -1.0f + 2.0f * (float) k / points, x = std::copysign (t * t, t) * range;
            const float y = DistEngine::transferCurve (s, x, level);
            const juce::Point<float> at { xFor (x), yFor (y) };
            if (k == 0)
            {
                whole.startNewSubPath (at);
            }
            else
            {
                whole.lineTo (at);
                const float slope = (y - previousY) / juce::jmax (1.0e-9f, x - previousX);
                segments.push_back ({ { previous, at }, juce::jlimit (0.0f, 1.0f, 1.0f - slope / slope0) });
            }
            previous = at;
            previousX = x;
            previousY = y;
        }
        g.setColour (Colours::ember.withAlpha (0.18f));
        g.strokePath (whole, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto& [line, saturated] : segments)
        {
            g.setColour (Colours::heat (0.45f + 0.55f * saturated));
            g.drawLine (line, 2.2f);
        }

        // The input under the curve (time running down), the output beside it (time running right).
        drawPlot (g, inStrip);
        drawPlot (g, outStrip.withTrimmedLeft (-2.0f));
        juce::Path in, out;
        for (int k = 0; k < window; ++k)
        {
            const float t = (float) k / (window - 1);
            const juce::Point<float> a { juce::jlimit (inStrip.getX(), inStrip.getRight(), xFor (waveIn[(size_t) k])), inStrip.getY() + t * inStrip.getHeight() };
            const juce::Point<float> b { outStrip.getX() + t * outStrip.getWidth(), yFor (waveOut[(size_t) k]) };
            if (k == 0) { in.startNewSubPath (a); out.startNewSubPath (b); }
            else        { in.lineTo (a); out.lineTo (b); }
        }
        g.setColour (Colours::cool.withAlpha (0.85f));
        g.strokePath (in, juce::PathStrokeType (1.4f));
        g.setColour (Colours::ember.withAlpha (0.25f));
        g.strokePath (out, juce::PathStrokeType (4.0f));
        g.setColour (Colours::amber);
        g.strokePath (out, juce::PathStrokeType (1.5f));

        g.setColour (Palette::panelOutline.withAlpha (0.6f));
        g.drawHorizontalLine ((int) square.getCentreY(), outStrip.getX(), outStrip.getRight());
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.setColour (Palette::textDisabled);
        g.drawText ("IN", inStrip.reduced (4.0f, 2.0f), juce::Justification::topLeft);
        g.drawText ("OUT", outStrip.reduced (6.0f, 3.0f), juce::Justification::topRight);

        // One sample followed through: from the input, onto the curve, out to the output.
        const int k = (int) ((frame % 150) / 150.0f * (window - 1));
        const juce::Point<float> fromIn { juce::jlimit (inStrip.getX(), inStrip.getRight(), xFor (waveIn[(size_t) k])), inStrip.getY() + (float) k / (window - 1) * inStrip.getHeight() };
        const juce::Point<float> onCurve { xFor (waveIn[(size_t) k]), yFor (waveOut[(size_t) k]) };
        const juce::Point<float> toOut { outStrip.getX() + (float) k / (window - 1) * outStrip.getWidth(), yFor (waveOut[(size_t) k]) };
        {
            juce::Path guide;
            guide.startNewSubPath (fromIn);
            guide.lineTo (onCurve);
            guide.lineTo (toOut);
            const float dashes[] { 2.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (guide, guide, dashes, 2);
            g.setColour (Colours::whiteHot.withAlpha (0.5f));
            g.fillPath (guide);
        }
        for (auto [point, colour] : { std::pair (fromIn, Colours::cool), std::pair (onCurve, Colours::whiteHot), std::pair (toOut, Colours::amber) })
        {
            EffectUI::drawGlow (g, point, 10.0f, colour, 0.8f);
            g.setColour (Palette::text);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (point));
        }

        // What it's doing, in numbers.
        const float gainDb = p.minGainDb + s.drive * (p.maxGainDb - p.minGainDb);
        const float uneven = (p.asymmetry + s.character * p.characterBias) * 100.0f;
        auto text = info.reduced (8.0f, 4.0f);
        const float rowH = text.getHeight() / 3.0f;
        auto line = [&] (const juce::String& name, const juce::String& value, juce::Colour colour)
        {
            auto row = text.removeFromTop (rowH);
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.setColour (Palette::textDim);
            g.drawText (name, row.removeFromLeft (110.0f), juce::Justification::centredLeft);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.setColour (colour);
            g.drawText (value, row, juce::Justification::centredLeft);
        };
        line (juce::String (p.name).toUpperCase() + "  " + juce::String (p.stages) + (p.stages == 1 ? " STAGE" : " STAGES"),
              "+" + juce::String (juce::roundToInt (gainDb)) + " dB gain", Colours::ember);
        line ("UNEVEN / BLOOM", juce::String (juce::roundToInt (uneven)) + " %  /  " + juce::String (juce::roundToInt (engine.biasShift.load() * p.sag * 100.0f / juce::jmax (0.01f, p.sag))) + " %",
              Colours::amber);
        line ("LEVEL MATCH", juce::String (juce::Decibels::gainToDecibels (engine.compensation.load()), 1) + " dB", Palette::text);
    }

    //==============================================================================
    EmbersPanel::EmbersPanel (EffectModel& m, DistEngine& e) : Panel (m, e, "EMBERS"), input (e.inputFifo), output (e.outputFifo) {}

    EmbersPanel::~EmbersPanel()
    {
        engine.inputFifo->active = false;
        engine.outputFifo->active = false;
    }

    void EmbersPanel::visibilityChanged()
    {
        engine.inputFifo->active = isShowing();
        engine.outputFifo->active = isShowing();
    }

    void EmbersPanel::resized()
    {
        plot = content().toFloat().reduced (6.0f, 6.0f).withTrimmedBottom (12.0f);
        columnHz.resize ((size_t) juce::jmax (1, (int) plot.getWidth()));
        for (size_t i = 0; i < columnHz.size(); ++i)
            columnHz[i] = axis.toHz (plot.getX() + (float) i + 0.5f, plot);
    }

    void EmbersPanel::refresh()
    {
        ++frame;
        if (isShowing())
        {
            engine.inputFifo->active = true;
            engine.outputFifo->active = true;
        }
        input.update (columnHz, engine.getSampleRate());
        output.update (columnHz, engine.getSampleRate());
        repaint();
    }

    void EmbersPanel::paint (juce::Graphics& g)
    {
        Panel::paint (g);
        drawPlot (g, content().toFloat());
        EffectUI::drawFrequencyGrid (g, plot, axis, true);

        auto yFor = [this] (float db) { return juce::jmap (juce::jlimit (-90.0f, -6.0f, db), -90.0f, -6.0f, plot.getBottom(), plot.getY()); };
        const auto columns = juce::jmin (input.levels.size(), output.levels.size());

        if (columns > 0)
        {
            // What the distortion added: flames rising from the input up to the output, flickering.
            const float t = (float) frame / 60.0f;
            for (size_t i = 0; i < columns; ++i)
            {
                const float added = output.levels[i] - input.levels[i];
                if (added < 2.0f || output.levels[i] < -88.0f)
                    continue;

                const float x = plot.getX() + (float) i;
                const float flicker = 0.5f + 0.25f * (std::sin ((float) i * 0.37f + t * 7.1f) + std::sin ((float) i * 0.11f - t * 4.3f));
                const float top = yFor (output.levels[i] + 3.0f * flicker), bottom = yFor (input.levels[i]);
                const float strength = juce::jlimit (0.0f, 1.0f, added / 30.0f);
                g.setGradientFill (juce::ColourGradient::vertical (Colours::heat (0.55f + 0.45f * strength).withAlpha (0.85f), top,
                                                                   Colours::hot.withAlpha (0.05f), juce::jmax (top + 1.0f, bottom)));
                g.fillRect (x, top, 1.0f, juce::jmax (1.0f, bottom - top));
            }

            juce::Path in, out;
            for (size_t i = 0; i < columns; ++i)
            {
                const float x = plot.getX() + (float) i;
                if (i == 0) { in.startNewSubPath (x, yFor (input.levels[i])); out.startNewSubPath (x, yFor (output.levels[i])); }
                else        { in.lineTo (x, yFor (input.levels[i])); out.lineTo (x, yFor (output.levels[i])); }
            }
            g.setColour (Colours::amber.withAlpha (0.9f));
            g.strokePath (out, juce::PathStrokeType (1.3f));
            g.setColour (Colours::cool.withAlpha (0.9f));
            g.strokePath (in, juce::PathStrokeType (1.3f));
        }

        // The harmonics a note gets at the level playing now.
        const auto s = settings();
        const float level = juce::jmax (0.02f, engine.inputLevel.load());
        const auto harmonics = DistEngine::harmonicsOf (s, level);
        auto inset = plot.removeFromRight (juce::jmin (180.0f, plot.getWidth() * 0.4f)).removeFromTop (juce::jmin (88.0f, plot.getHeight() * 0.6f)).reduced (4.0f);
        g.setColour (EffectUI::plotBackground.withAlpha (0.85f));
        g.fillRoundedRectangle (inset, 4.0f);
        g.setColour (Palette::panelOutline);
        g.drawRoundedRectangle (inset, 4.0f, 1.0f);
        auto bars = inset.reduced (8.0f, 6.0f);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.setColour (Palette::textDim);
        g.drawText ("HARMONICS NOW", bars.removeFromTop (11.0f), juce::Justification::centredLeft);
        bars.removeFromTop (3.0f);
        constexpr int shown = 12;
        const float w = bars.getWidth() / shown;
        for (int h = 0; h < shown; ++h)
        {
            const float v = juce::jlimit (0.0f, 1.0f, (harmonics[(size_t) h] + 60.0f) / 60.0f);
            auto bar = juce::Rectangle<float> (bars.getX() + w * (float) h + 1.0f, bars.getBottom() - v * bars.getHeight(), w - 2.0f, v * bars.getHeight());
            g.setColour (h == 0 ? Colours::cool : Colours::heat (0.4f + 0.6f * v).withAlpha (h % 2 == 1 ? 1.0f : 0.8f));
            g.fillRect (bar);
        }

        g.setFont (juce::FontOptions (9.5f));
        g.setColour (Colours::cool);
        g.drawText ("in", content().toFloat().reduced (10.0f, 4.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
        g.setColour (Colours::amber);
        g.drawText ("      out (what the drive added glows)", content().toFloat().reduced (10.0f, 4.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
    }

    //==============================================================================
    CircuitPanel::CircuitPanel (EffectModel& m, DistEngine& e) : Panel (m, e, "THE CIRCUIT")
    {
        addKnob (P::level, "Level", Palette::text);
        addKnob (P::blend, "Blend", Colours::amber);
    }

    void CircuitPanel::resized()
    {
        auto r = content();
        auto row = r.removeFromBottom (knobHeight);
        layoutRow (row.removeFromRight (juce::jmin (180, row.getWidth() / 2)), { knobs[0], knobs[1] });
        display = r.withTrimmedBottom (4);
    }

    void CircuitPanel::refresh()
    {
        Panel::refresh();
        flow += 0.4 + 40.0 * juce::jmin (0.5f, engine.inputLevel.load());
        repaint();
    }

    void CircuitPanel::paint (juce::Graphics& g)
    {
        Panel::paint (g);
        const auto s = settings();
        const auto& p = DistEngine::profile (s.voicing);
        const bool bass = s.voicing == Voicing::bass;
        auto area = display.toFloat();
        drawPlot (g, area);

        struct Node { juce::String label, detail; int stage; juce::Colour colour; float glow; };
        std::vector<Node> nodes;
        const bool open = engine.gateOpen.load();
        const float gateGain = engine.gateGain.load();
        nodes.push_back ({ "IN", {}, -1, Colours::cool, juce::jmin (1.0f, engine.inputLevel.load() * 4.0f) });
        nodes.push_back ({ "GATE", ! s.gateOn ? "off" : (open ? "open" : "shut"), -1, s.gateOn ? (open ? Colours::gate : Palette::textDisabled) : Palette::panelOutline,
                           s.gateOn ? gateGain : 0.0f });
        nodes.push_back ({ bass ? "SPLIT" : "TIGHT", model.parameter (P::tight)->getCurrentText(), -1, Colours::cool, 0.0f });
        nodes.push_back ({ "VOICE", juce::String (p.name), -1, Colours::amber, 0.0f });
        for (int st = 0; st < p.stages; ++st)
            nodes.push_back ({ juce::String (st + 1), juce::String (juce::roundToInt (engine.stageHeat[(size_t) st].load() * 100.0f)) + " %", st, Colours::ember,
                               engine.stageHeat[(size_t) st].load() });
        if (bass)
            nodes.push_back ({ "SUM", {}, -1, Colours::cool, 0.0f });
        nodes.push_back ({ "TONE", {}, -1, Colours::tone, 0.0f });
        nodes.push_back ({ "BLEND", model.parameter (P::blend)->getCurrentText(), -1, Colours::amber, 0.0f });
        nodes.push_back ({ "OUT", {}, -1, Palette::text, 0.0f });

        auto r = area.reduced (38.0f, 10.0f);
        const float y = bass ? r.getY() + r.getHeight() * 0.38f : r.getCentreY();
        const float step = r.getWidth() / (float) (nodes.size() - 1);
        auto xOf = [&] (size_t i) { return r.getX() + step * (float) i; };
        const float level = juce::jmin (1.0f, engine.inputLevel.load() * 4.0f);

        // Wires with the signal flowing along them.
        auto wire = [&] (juce::Point<float> a, juce::Point<float> b, juce::Colour colour)
        {
            g.setColour (Palette::panelOutline);
            g.drawLine ({ a, b }, 2.0f);
            const float length = a.getDistanceFrom (b);
            for (float d = std::fmod ((float) flow, 14.0f); d < length; d += 14.0f)
            {
                const auto at = a + (b - a) * (d / length);
                g.setColour (colour.withAlpha (0.15f + 0.75f * level));
                g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (at));
            }
        };

        for (size_t i = 0; i + 1 < nodes.size(); ++i)
            wire ({ xOf (i), y }, { xOf (i + 1), y }, nodes[i].stage >= 0 || nodes[i + 1].stage >= 0 ? Colours::ember : Colours::cool);

        if (bass)
        {
            // The clean lows, around the stages.
            const size_t split = 2, sum = 4 + (size_t) p.stages;
            const float low = r.getY() + r.getHeight() * 0.86f;
            wire ({ xOf (split), y }, { xOf (split), low }, Colours::cool);
            wire ({ xOf (split), low }, { xOf (sum), low }, Colours::cool);
            wire ({ xOf (sum), low }, { xOf (sum), y }, Colours::cool);
            g.setFont (juce::FontOptions (9.5f));
            g.setColour (Colours::cool);
            g.drawText ("clean lows under " + model.parameter (P::tight)->getCurrentText(),
                        juce::Rectangle<float> (xOf (split), low - 14.0f, xOf (sum) - xOf (split), 12.0f), juce::Justification::centred);
        }

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto& n = nodes[i];
            const juce::Point<float> c { xOf (i), y };
            if (n.stage >= 0)
            {
                // A valve: glowing as hard as the stage clips.
                const auto colour = Colours::heat (0.3f + 0.7f * n.glow);
                EffectUI::drawGlow (g, c, 34.0f, colour, 0.25f + 0.75f * n.glow);
                auto bulb = juce::Rectangle<float> (34.0f, 34.0f).withCentre (c);
                g.setColour (EffectUI::plotBackground);
                g.fillEllipse (bulb);
                g.setColour (colour);
                g.drawEllipse (bulb, 1.6f);
                juce::Path filament;
                filament.startNewSubPath (c.x - 8.0f, c.y + 5.0f);
                filament.cubicTo (c.x - 4.0f, c.y - 9.0f, c.x + 4.0f, c.y - 9.0f, c.x + 8.0f, c.y + 5.0f);
                g.setColour (colour.withAlpha (0.4f + 0.6f * n.glow));
                g.strokePath (filament, juce::PathStrokeType (1.8f));
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.setColour (Palette::text);
                g.drawText (n.label, bulb.translated (0.0f, 4.0f), juce::Justification::centred);
                g.setFont (juce::FontOptions (9.5f));
                g.setColour (colour);
                g.drawText (n.detail, juce::Rectangle<float> (60.0f, 12.0f).withCentre (c.translated (0.0f, 27.0f)), juce::Justification::centred);
            }
            else
            {
                auto box = juce::Rectangle<float> (juce::jmin (58.0f, step - 8.0f), 26.0f).withCentre (c);
                if (n.glow > 0.0f)
                    EffectUI::drawGlow (g, c, 26.0f, n.colour, n.glow * 0.6f);
                g.setColour (Palette::panelRaised);
                g.fillRoundedRectangle (box, 5.0f);
                g.setColour (n.colour);
                g.drawRoundedRectangle (box, 5.0f, 1.3f);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.setColour (Palette::text);
                g.drawText (n.label, box, juce::Justification::centred);
                if (n.detail.isNotEmpty())
                {
                    g.setFont (juce::FontOptions (9.0f));
                    g.setColour (n.colour);
                    g.drawText (n.detail, juce::Rectangle<float> (step, 12.0f).withCentre (c.translated (0.0f, 22.0f)), juce::Justification::centred, true);
                }
            }
        }
    }

    //==============================================================================
    GatePanel::GatePanel (EffectModel& m, DistEngine& e)
        : Panel (m, e, "NOISE GATE"), onButton (m, P::gateOn, "On", Colours::gate, "Noise gate on / off")
    {
        addAndMakeVisible (onButton);
        addKnob (P::gateThreshold, "Threshold", Colours::gate);
        addKnob (P::gateRelease, "Release", Colours::gate, 0.15f);
    }

    void GatePanel::resized()
    {
        onButton.setBounds (titleRow().removeFromRight (44));
        auto r = content();
        auto knobArea = r.removeFromRight (2 * 64);
        knobs[0]->setBounds (knobArea.removeFromLeft (64).withSizeKeepingCentre (64, juce::jmin (knobHeight, knobArea.getHeight())));
        knobs[1]->setBounds (knobArea.withSizeKeepingCentre (64, juce::jmin (knobHeight, knobArea.getHeight())));
        display = r.withTrimmedRight (6).withTrimmedBottom (2);
    }

    void GatePanel::refresh()
    {
        Panel::refresh();
        onButton.refresh();
        repaint (display);
    }

    float GatePanel::yFor (float db) const
    {
        auto p = display.toFloat().reduced (6.0f, 6.0f);
        return juce::jmap (juce::jlimit (-96.0f, 0.0f, db), -96.0f, 0.0f, p.getBottom(), p.getY());
    }

    float GatePanel::dbAt (float y) const
    {
        auto p = display.toFloat().reduced (6.0f, 6.0f);
        return juce::jmap (y, p.getBottom(), p.getY(), -96.0f, 0.0f);
    }

    void GatePanel::mouseMove (const juce::MouseEvent& e)
    {
        const bool near = display.contains (e.getPosition()) && std::abs (e.position.y - yFor (model.value (P::gateThreshold))) < 6.0f;
        if (near != overLine)
        {
            overLine = near;
            setMouseCursor (near ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            repaint (display);
        }
    }

    void GatePanel::mouseDown (const juce::MouseEvent& e)
    {
        if (display.contains (e.getPosition()))
        {
            beginDrag ({ P::gateThreshold }, "Gate threshold");
            dragTo (P::gateThreshold, dbAt (e.position.y));
        }
    }

    void GatePanel::mouseDrag (const juce::MouseEvent& e)
    {
        if (! dragging.empty())
            dragTo (P::gateThreshold, dbAt (e.position.y));
    }

    void GatePanel::mouseUp (const juce::MouseEvent&)
    {
        endDrag();
    }

    void GatePanel::paint (juce::Graphics& g)
    {
        Panel::paint (g);
        const auto s = settings();
        auto area = display.toFloat();
        drawPlot (g, area);
        auto p = area.reduced (6.0f, 6.0f);

        const int write = engine.gateWrite.load();
        constexpr int n = DistEngine::gateHistoryLength;
        juce::Path level;
        level.startNewSubPath (p.getX(), p.getBottom());
        for (int k = 0; k < n; ++k)
        {
            const int i = (write + k) % n;
            const float x = p.getX() + p.getWidth() * (float) k / (n - 1);
            level.lineTo (x, yFor (engine.gateLevelDb[(size_t) i].load()));

            const float gain = s.gateOn ? engine.gateGainHistory[(size_t) i].load() : 1.0f;
            if (s.gateOn && gain > 0.01f)
            {
                g.setColour (Colours::gate.withAlpha (0.12f * gain));
                g.fillRect (x, p.getY(), p.getWidth() / (n - 1) + 0.5f, p.getHeight());
            }
        }
        level.lineTo (p.getRight(), p.getBottom());
        level.closeSubPath();
        g.setGradientFill (juce::ColourGradient::vertical (Colours::cool.withAlpha (0.55f), p.getY(), Colours::cool.withAlpha (0.08f), p.getBottom()));
        g.fillPath (level);

        // Open above the threshold, shut 6 dB under it.
        const float open = yFor (s.gateThresholdDb), shut = yFor (s.gateThresholdDb - 6.0f);
        g.setColour (s.gateOn ? Colours::gate : Palette::textDisabled);
        g.drawLine (p.getX(), open, p.getRight(), open, overLine || ! dragging.empty() ? 2.5f : 1.5f);
        juce::Path closing;
        closing.startNewSubPath (p.getX(), shut);
        closing.lineTo (p.getRight(), shut);
        const float dashes[] { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (closing, closing, dashes, 2);
        g.setColour ((s.gateOn ? Colours::gate : Palette::textDisabled).withAlpha (0.6f));
        g.fillPath (closing);

        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.setColour (! s.gateOn ? Palette::textDisabled : (engine.gateOpen.load() ? Colours::gate : Palette::textDim));
        g.drawText (! s.gateOn ? "OFF" : (engine.gateOpen.load() ? "OPEN" : "SHUT"), p.reduced (4.0f, 2.0f), juce::Justification::topRight);
        g.setFont (juce::FontOptions (9.0f));
        g.setColour (Palette::textDim);
        g.drawText (model.parameter (P::gateThreshold)->getCurrentText(), juce::Rectangle<float> (p.getX() + 4.0f, open - 13.0f, 80.0f, 12.0f), juce::Justification::centredLeft);
    }

    //==============================================================================
    TonePanel::TonePanel (EffectModel& m, DistEngine& e) : Panel (m, e, "TONE")
    {
        addKnob (P::bass, "Bass", Colours::tone);
        addKnob (P::mid, "Mid", Colours::tone);
        addKnob (P::midFreq, "Mid Freq", Colours::tone, 900.0f);
        addKnob (P::treble, "Treble", Colours::tone);
        addKnob (P::smooth, "Smooth", Colours::cool, 7000.0f);
    }

    void TonePanel::resized()
    {
        auto r = content();
        layoutRow (r.removeFromBottom (knobHeight), { knobs[0], knobs[1], knobs[2], knobs[3], knobs[4] });
        display = r.withTrimmedBottom (4);
    }

    void TonePanel::refresh()
    {
        Panel::refresh();
        repaint (display);
    }

    juce::Rectangle<float> TonePanel::plotArea() const
    {
        return display.toFloat().reduced (6.0f, 6.0f).withTrimmedBottom (10.0f);
    }

    float TonePanel::yFor (double db) const
    {
        auto p = plotArea();
        return juce::jmap (juce::jlimit (-26.0f, 20.0f, (float) db), -24.0f, 18.0f, p.getBottom(), p.getY());
    }

    juce::Point<float> TonePanel::nodeAt (int node) const
    {
        const auto s = settings();
        auto p = plotArea();
        const double rate = engine.getSampleRate(), reference = DistEngine::getResponseDb (s, 1000.0, rate);
        auto onCurve = [&] (double hz) { return juce::Point<float> (axis.toX (hz, p), yFor (DistEngine::getResponseDb (s, hz, rate) - reference)); };
        switch (node)
        {
            case tightNode:  return onCurve (s.tight);
            case bassNode:   return { axis.toX (120.0, p), yFor (s.bassDb) };
            case midNode:    return { axis.toX (s.midHz, p), yFor (s.midDb) };
            case trebleNode: return { axis.toX (3200.0, p), yFor (s.trebleDb) };
            case smoothNode:
            default:         return s.smooth >= 19900.0f ? juce::Point<float> (p.getRight() - 4.0f, yFor (0.0)) : onCurve (s.smooth);
        }
    }

    int TonePanel::nodeNear (juce::Point<float> at) const
    {
        int best = -1;
        float distance = 12.0f;
        for (int i = 0; i < numNodes; ++i)
            if (const auto d = at.getDistanceFrom (nodeAt (i)); d < distance)
            {
                distance = d;
                best = i;
            }
        return best;
    }

    void TonePanel::mouseMove (const juce::MouseEvent& e)
    {
        const int near = nodeNear (e.position);
        if (near != hover)
        {
            hover = near;
            setMouseCursor (near < 0 ? juce::MouseCursor::NormalCursor
                                     : near == midNode ? juce::MouseCursor::UpDownLeftRightResizeCursor
                                     : (near == tightNode || near == smoothNode) ? juce::MouseCursor::LeftRightResizeCursor
                                                                                 : juce::MouseCursor::UpDownResizeCursor);
            repaint (display);
        }
    }

    void TonePanel::mouseDown (const juce::MouseEvent& e)
    {
        held = nodeNear (e.position);
        switch (held)
        {
            case tightNode:  beginDrag ({ P::tight }, "Tight"); break;
            case bassNode:   beginDrag ({ P::bass }, "Bass"); break;
            case midNode:    beginDrag ({ P::mid, P::midFreq }, "Mid"); break;
            case trebleNode: beginDrag ({ P::treble }, "Treble"); break;
            case smoothNode: beginDrag ({ P::smooth }, "Smooth"); break;
            default:         break;
        }
    }

    void TonePanel::mouseDrag (const juce::MouseEvent& e)
    {
        if (held < 0)
            return;

        auto p = plotArea();
        const float hz = (float) axis.toHz (e.position.x, p);
        const float db = juce::jmap (e.position.y, p.getBottom(), p.getY(), -24.0f, 18.0f);
        switch (held)
        {
            case tightNode:  dragTo (P::tight, hz); break;
            case bassNode:   dragTo (P::bass, db); break;
            case midNode:    dragTo (P::mid, db); dragTo (P::midFreq, hz); break;
            case trebleNode: dragTo (P::treble, db); break;
            case smoothNode: dragTo (P::smooth, hz); break;
            default:         break;
        }
        repaint (display);
    }

    void TonePanel::mouseUp (const juce::MouseEvent&)
    {
        endDrag();
        held = -1;
    }

    void TonePanel::paint (juce::Graphics& g)
    {
        Panel::paint (g);
        const auto s = settings();
        const double rate = engine.getSampleRate();
        drawPlot (g, display.toFloat());
        auto p = plotArea();
        EffectUI::drawFrequencyGrid (g, p, axis, true);
        g.setColour (Palette::panelOutline.withAlpha (0.8f));
        g.drawHorizontalLine ((int) yFor (0.0), p.getX(), p.getRight());

        auto curve = [&] (const DistEngine::Settings& settings)
        {
            const double reference = DistEngine::getResponseDb (settings, 1000.0, rate);
            juce::Path path;
            for (float x = p.getX(); x <= p.getRight(); x += 2.0f)
            {
                const float y = yFor (DistEngine::getResponseDb (settings, axis.toHz (x, p), rate) - reference);
                if (x == p.getX()) path.startNewSubPath (x, y); else path.lineTo (x, y);
            }
            return path;
        };

        // The voicing alone (flat controls), then everything.
        auto voicingOnly = s;
        voicingOnly.bassDb = voicingOnly.midDb = voicingOnly.trebleDb = 0.0f;
        voicingOnly.smooth = 20000.0f;
        g.setColour (Colours::amber.withAlpha (0.35f));
        g.strokePath (curve (voicingOnly), juce::PathStrokeType (1.0f));

        auto full = curve (s);
        juce::Path fill (full);
        fill.lineTo (p.getRight(), p.getBottom());
        fill.lineTo (p.getX(), p.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient::vertical (Colours::tone.withAlpha (0.22f), p.getY(), Colours::tone.withAlpha (0.02f), p.getBottom()));
        g.fillPath (fill);
        g.setColour (Colours::tone);
        g.strokePath (full, juce::PathStrokeType (1.8f));

        static const char* letters[] { "T", "B", "M", "T", "S" };
        const juce::Colour colours[] { Colours::cool, Colours::tone, Colours::tone, Colours::tone, Colours::cool };
        for (int i = 0; i < numNodes; ++i)
            EffectUI::drawHandle (g, nodeAt (i), colours[i], hover == i || held == i, i == trebleNode ? "H" : letters[i]);

        g.setFont (juce::FontOptions (9.5f));
        g.setColour (Palette::textDim);
        g.drawText ("the voicing and the controls, 1 kHz at 0 dB", p.reduced (4.0f, 2.0f).removeFromTop (11.0f), juce::Justification::centredRight);
    }
}
