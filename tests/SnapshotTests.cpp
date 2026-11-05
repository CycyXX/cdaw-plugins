#include "TestSupport.h"
#include "Shared/Chorus/ChorusView.h"
#include "Shared/CVerb/CVerbView.h"
#include "Shared/Compressor/CompressorView.h"
#include "Shared/Delay/DelayView.h"
#include "Shared/Dist/DistView.h"
#include "Shared/Esser/EsserView.h"
#include "Shared/Filter/FilterView.h"
#include "Shared/Lead/LeadView.h"
#include "Shared/LookAndFeel/CDAWLookAndFeel.h"
#include "Shared/Multiband/MultibandView.h"
#include "Shared/Phaser/PhaserView.h"
#include "Shared/Synth/SynthView.h"

namespace cdaw::test
{
    namespace
    {
        /** A parameter that just holds its value, for a view with no host. */
        template <typename Spec>
        struct HeldParameter : public KnobParameter
        {
            explicit HeldParameter (const Spec& s) : spec (s), value (s.defaultValue) {}

            float getValue() const override                 { return value; }
            void setValue (float v) override                { value = v; }
            void beginGesture() override                    {}
            void endGesture() override                      {}
            juce::Range<float> getRange() const override    { return spec.getRange().getRange(); }
            juce::String getText (float v) const override   { return spec.toText (v); }

            const Spec& spec;
            float value;
        };

        struct TestModel : public EffectModel
        {
            explicit TestModel (const ParameterSpecs& specs)
            {
                for (auto& spec : specs)
                    parameters.push_back (std::make_shared<HeldParameter<ParameterSpec>> (spec));
            }

            std::shared_ptr<KnobParameter> parameter (int id) override      { return parameters[(size_t) id]; }
            double getBpm() override                                        { return 120.0; }
            juce::String getText (const juce::String& key) override         { return texts[key]; }
            void setText (const juce::String& key, const juce::String& v) override { texts[key] = v; }
            void set (int id, float v)                                      { parameters[(size_t) id]->setValue (v); }

            std::vector<std::shared_ptr<KnobParameter>> parameters;
            std::map<juce::String, juce::String> texts;
        };

        struct VerbModel : public CVerbModel
        {
            VerbModel()
            {
                for (int i = 0; i < CVerbParameters::count; ++i)
                    parameters.push_back (std::make_shared<HeldParameter<CVerbParameters::Spec>> (CVerbParameters::get ((CVerbParameters::ID) i)));
            }

            std::shared_ptr<KnobParameter> parameter (CVerbParameters::ID id) override  { return parameters[(size_t) id]; }
            ReverbEngine& getEngine() override                                          { return engine; }

            std::vector<std::shared_ptr<KnobParameter>> parameters;
            ReverbEngine engine;
        };

        struct CompModel : public CompressorModel
        {
            CompModel()
            {
                for (int i = 0; i < CompressorParameters::count; ++i)
                    parameters.push_back (std::make_shared<HeldParameter<CompressorParameters::Spec>> (CompressorParameters::get ((CompressorParameters::ID) i)));
            }

            std::shared_ptr<KnobParameter> parameter (CompressorParameters::ID id) override { return parameters[(size_t) id]; }
            CompressorEngine& getEngine() override                                          { return engine; }

            std::vector<std::shared_ptr<KnobParameter>> parameters;
            CompressorEngine engine;
        };

        struct MultiModel : public MultibandModel
        {
            MultiModel()
            {
                for (int i = 0; i < MultibandParameters::count; ++i)
                    parameters.push_back (std::make_shared<HeldParameter<MultibandParameters::Spec>> (MultibandParameters::get (i)));
            }

            std::shared_ptr<KnobParameter> parameter (int index) override   { return parameters[(size_t) index]; }
            MultibandEngine& getEngine() override                           { return engine; }

            std::vector<std::shared_ptr<KnobParameter>> parameters;
            MultibandEngine engine;
        };

        void save (juce::Component& c, const juce::String& name)
        {
            auto chosen = juce::SystemStats::getEnvironmentVariable ("CDAW_SNAPSHOT_DIR", {});
            auto dir = chosen.isNotEmpty() ? juce::File (chosen) : getTestDir().getChildFile ("snapshots");
            dir.createDirectory();
            auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
            auto file = dir.getChildFile (name + ".png");
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat().writeImageToStream (image, out);
        }

        /** Pinkish noise, for the spectra. */
        void feedNoise (AnalyserFifo& fifo, int seed, std::function<void()> afterEachBlock)
        {
            fifo.active = true;
            juce::Random rng (seed);
            juce::AudioBuffer<float> noise (1, 4096);
            float b0 = 0, b1 = 0, b2 = 0;
            for (int block = 0; block < 12; ++block)
            {
                for (int i = 0; i < noise.getNumSamples(); ++i)
                {
                    auto white = rng.nextFloat() * 2.0f - 1.0f;
                    b0 = 0.99765f * b0 + white * 0.0990460f;
                    b1 = 0.96300f * b1 + white * 0.2965164f;
                    b2 = 0.57000f * b2 + white * 1.0526913f;
                    noise.setSample (0, i, (b0 + b1 + b2 + white * 0.1848f) * 0.08f);
                }
                fifo.push (noise);
                afterEachBlock();
            }
        }
    }

    /** The screenshots in docs/images, of every view playing. Run with CDAW_SNAPSHOTS=1 and
        CDAW_SNAPSHOT_DIR=docs/images. Without them it only checks each view opens at its size. */
    struct SnapshotTests : public juce::UnitTest
    {
        SnapshotTests() : juce::UnitTest ("Plugin views", "Plugins") {}

        void runTest() override
        {
            CDAWLookAndFeel laf;
            juce::LookAndFeel::setDefaultLookAndFeel (&laf);

            beginTest ("Views open and paint");

            {
                using P = DelayParameters::ID;
                TestModel model (DelayParameters::all());
                model.set (P::division, 8.0f);
                model.set (P::feedback, 0.55f);
                model.set (P::mode, 1.0f);
                model.set (P::lowCut, 180.0f);
                model.set (P::highCut, 4500.0f);
                model.set (P::drive, 0.35f);
                model.set (P::wobble, 0.3f);
                model.set (P::duck, 0.4f);
                DelayEngine engine;
                engine.prepare (48000.0, 512);
                DelayView view (model, engine);
                view.setSize (DelayView::defaultWidth, DelayView::defaultHeight);

                // Hits every 0.75 s, the last one 375 ms ago: on the echo grid, so echoes light up.
                for (int i = 0; i < 400; ++i)
                {
                    const float t = std::fmod ((float) i / 60.0f + 0.475f, 0.75f);
                    const float level = t < 0.12f ? 0.8f * (1.0f - t / 0.12f) + 0.0005f : 0.0005f;
                    view.tick (level, level * 0.85f, level * 0.5f, level * 0.4f, level > 0.1f ? 8.0f : 1.0f);
                }
                finish (view, "c-delay");
            }

            {
                using P = ChorusParameters::ID;
                TestModel model (ChorusParameters::all());
                model.set (P::voices, 3.0f);
                model.set (P::feedback, 0.25f);
                ChorusEngine engine;
                engine.prepare (48000.0, 512);
                ChorusView view (model, engine);
                view.setSize (ChorusView::defaultWidth, ChorusView::defaultHeight);

                const double phases[] { 0.12, 0.47, 0.81, 0.3 };
                for (int v = 0; v < ChorusEngine::maxVoices; ++v)
                    engine.voicePhase[(size_t) v].store (phases[v]);
                for (int i = 0; i < 30; ++i)
                    view.tick (0.4f, 0.3f);
                finish (view, "c-chorus");
            }

            {
                using P = PhaserParameters::ID;
                TestModel model (PhaserParameters::all());
                model.set (P::stages, 8.0f);
                model.set (P::rate, 0.5f);
                model.set (P::feedback, 0.6f);
                model.set (P::spread, 0.35f);
                PhaserEngine engine;
                engine.prepare (48000.0, 512);
                PhaserView view (model, engine);
                view.setSize (PhaserView::defaultWidth, PhaserView::defaultHeight);

                // Four seconds of sweep at 0.5 Hz, with sound coming and going.
                for (int i = 0; i < 260; ++i)
                {
                    engine.lfoPhase.store (std::fmod (i * 0.5 / 60.0, 1.0));
                    view.tick (0.3f, (i / 50) % 3 == 2 ? 0.01f : 0.3f);
                }
                finish (view, "c-phaser");
            }

            {
                using P = FilterParameters::ID;
                TestModel model (FilterParameters::all());
                model.set (P::cutoff, 900.0f);
                model.set (P::resonance, 0.55f);
                model.set (P::slope, 1.0f);
                model.set (P::lfoAmount, 0.35f);
                model.set (P::lfoStereo, 0.4f);
                model.set (P::envAmount, 0.25f);
                FilterEngine engine;
                engine.prepare (48000.0, 512);
                FilterView view (model, engine);
                view.setSize (FilterView::defaultWidth, FilterView::defaultHeight);

                // A 1 Hz LFO with hits pushing the envelope up now and then.
                const auto s = FilterParameters::toSettings ([&model] (int id) { return model.value (id); });
                for (int i = 0; i < 260; ++i)
                {
                    const double phase = i / 60.0;
                    const float env = (i % 45) < 8 ? 1.0f - (float) (i % 45) / 8.0f : 0.0f;
                    auto at = [&] (double p) { return (float) FilterEngine::modulatedCutoff (s, FilterEngine::lfoValue (s.lfoShape, p), env, 48000.0); };
                    view.tick (0.3f, 0.25f, at (phase), at (phase + 0.2), env);
                }
                feedNoise (*engine.outputFifo, 9, [&view] { view.updateSpectrum(); });
                finish (view, "c-filter");
            }

            {
                using P = EsserParameters::ID;
                TestModel model (EsserParameters::all());
                model.set (P::threshold, -30.0f);
                model.set (P::range, 10.0f);
                EsserEngine engine;
                engine.prepare (48000.0, 512);
                EsserView view (model, engine);
                view.setSize (EsserView::defaultWidth, EsserView::defaultHeight);

                // Speech-like: the band's level wandering below the threshold, with an "s" now and then.
                const auto s = EsserParameters::toSettings ([&model] (int id) { return model.value (id); });
                for (int i = 0; i < 900; ++i)
                {
                    const int t = i % 110;
                    const float levelDb = t < 22 ? -18.0f + 4.0f * std::sin ((float) t * 0.4f) : -44.0f + 8.0f * std::sin ((float) i * 0.13f);
                    view.tick (juce::Decibels::decibelsToGain (levelDb), s.thresholdDb, EsserEngine::getReductionDb (s, levelDb, s.thresholdDb));
                }
                finish (view, "c-esser");
            }

            {
                using P = CompressorParameters::ID;
                CompModel model;
                model.parameter (P::threshold)->setValue (-22.0f);
                model.parameter (P::ratio)->setValue (4.0f);
                model.engine.prepare (48000.0);
                CompressorView view (model);
                view.setSize (CompressorView::defaultWidth, CompressorView::defaultHeight);

                // Drum-like hits: fast rise, decay, compressed by up to ~9 dB.
                juce::Random rng (3);
                for (int i = 0; i < 400; ++i)
                {
                    auto phase = (i % 30) / 30.0f;
                    auto level = juce::Decibels::decibelsToGain (-4.0f - 40.0f * phase + rng.nextFloat() * 2.0f);
                    auto gr = juce::jmax (0.0f, (juce::Decibels::gainToDecibels (level) + 22.0f) * 0.75f);
                    auto out = level * juce::Decibels::decibelsToGain (-gr);
                    view.tick (level, level * 0.9f, out, out * 0.9f, level, gr);
                }
                finish (view, "c-comp");
            }

            {
                using namespace MultibandParameters;
                MultiModel model;
                for (int b = 0; b < 3; ++b)
                    model.parameter (bandParam (b, bandOn))->setValue (1.0f);
                model.parameter (bandParam (0, bandThreshold))->setValue (-24.0f);
                model.parameter (bandParam (2, bandRatio))->setValue (6.0f);
                model.engine.prepare (48000.0, 512);
                MultibandView view (model);
                view.setSize (MultibandView::defaultWidth, MultibandView::defaultHeight);

                MultibandView::Levels levels {};
                for (int i = 0; i < 120; ++i)
                {
                    const float in[4] { 0.5f, 0.3f, 0.2f, 0.08f }, gr[4] { 7.5f, 3.2f, 1.4f, 0.0f };
                    for (int b = 0; b < 4; ++b)
                    {
                        levels.bandIn[b] = in[b];
                        levels.reduction[b] = gr[b];
                        levels.bandOut[b] = in[b] * juce::Decibels::decibelsToGain (-gr[b]);
                    }
                    levels.in[0] = levels.in[1] = 0.7f;
                    levels.out[0] = levels.out[1] = 0.45f;
                    view.tick (levels);
                }
                feedNoise (*model.engine.postFifo, 11, [&view] { view.updateSpectrum(); });
                finish (view, "c-multicomp");
            }

            {
                VerbModel model;
                model.parameter (CVerbParameters::preLowCut)->setValue (180.0f);
                model.engine.prepare (48000.0, 512, model.getSettings());
                CVerbView view (model);
                view.setSize (CVerbView::defaultWidth, CVerbView::defaultHeight);

                // Hits every 1.2 s with the reverb ringing on after each.
                for (int i = 0; i < 360; ++i)
                {
                    auto t = (float) (i % 72) / 60.0f;
                    auto dry = t < 0.15f ? 0.6f * (1.0f - t / 0.15f) + 0.01f : 0.0f;
                    auto wet = 0.3f * std::exp (-t * 2.4f) * (1.0f - std::exp (-t * 40.0f)) + 0.002f;
                    view.tick (dry, wet);
                }
                feedNoise (*model.engine.wetFifo, 5, [&view] { view.updateSpectrum(); });
                finish (view, "c-verb");
            }

            {
                using P = DistParameters::ID;
                TestModel model (DistParameters::all());
                model.set (P::gateOn, 1.0f);
                model.set (P::gateThreshold, -58.0f);
                model.set (P::mid, 2.0f);
                DistEngine engine;
                engine.prepare (48000.0, 512);
                DistView view (model, engine);
                view.setSize (DistView::defaultWidth, DistView::defaultHeight);

                // Plucked notes with a little hum between them, for the gate to shut on.
                juce::Random random (5);
                const auto s = DistParameters::toSettings ([&model] (int id) { return model.value (id); });
                engine.inputFifo->active = engine.outputFifo->active = true;
                juce::AudioBuffer<float> block (2, 512);
                const int stop = (int) (2.45 * 48000.0);
                for (int start = 0, n = 0; start + 512 <= stop; start += 512, ++n)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        const double now = (start + i) / 48000.0;
                        const double t = std::fmod (now, 0.5);
                        double x = 0.0;
                        for (int h = 1; h <= 12; ++h)
                            x += std::sin (juce::MathConstants<double>::twoPi * 110.0 * h * t) / h * std::exp (-t * (2.0 + h * 0.8));
                        const auto v = (float) (0.13 * x) * (std::fmod (now, 1.0) < 0.62 ? 1.0f : 0.0f) + 0.0004f * (random.nextFloat() - 0.5f);
                        block.setSample (0, i, v);
                        block.setSample (1, i, v);
                    }
                    engine.process (block, 0, 512, s);
                    if (n % 2 == 0)
                        view.tick();
                }
                finish (view, "c-dist");
            }

            {
                using P = SynthParameters::ID;
                TestModel model (SynthParameters::all());
                model.set (P::delayOn, 1.0f);
                model.set (P::macro2, 0.4f);
                SynthEngine engine;
                engine.prepare (48000.0, 512);
                SynthView view (model, engine);
                view.setSize (SynthView::defaultWidth, SynthView::defaultHeight);

                // A chord held for a while, one note already released.
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                const auto s = SynthParameters::toSettings ([&model] (int id) { return model.value (id); });
                for (int block = 0; block < 300; ++block)
                {
                    midi.clear();
                    if (block == 0)
                        for (int note : { 48, 55, 60, 64, 71 })
                            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (80 + note % 30)), 0);
                    if (block == 240)
                        midi.addEvent (juce::MidiMessage::noteOff (1, 71), 0);

                    engine.process (buffer, 0, 512, midi, s, 120.0);
                    view.getStereoField().add (buffer.getReadPointer (0), buffer.getReadPointer (1), 512);
                    engine.outputFifo->active = true;
                    if (block % 3 == 0)
                        view.tick();
                }

                for (auto [page, name] : { std::pair (0, "c-synth"), std::pair (1, "c-synth-mod"), std::pair (2, "c-synth-fx") })
                {
                    view.showPage (page);
                    for (int i = 0; i < 20; ++i)
                        view.tick();
                    finish (view, name);
                }

                clickKeyboard<VoiceField> (view, engine);
            }

            {
                using P = LeadParameters::ID;
                TestModel model (LeadParameters::all());
                for (auto [id, v] : std::initializer_list<std::pair<int, float>> {
                         { P::sync, 1.0f }, { P::macro3, 0.4f }, { P::macro1, 0.25f }, { P::vibratoDepth, 35.0f },
                         { P::voiceMode, 1.0f }, { P::glide, 0.12f }, { P::distOn, 1.0f }, { P::distDrive, 0.4f } })
                    model.set (id, v);
                LeadEngine engine;
                engine.prepare (48000.0, 512);
                LeadView view (model, engine);
                view.setSize (LeadView::defaultWidth, LeadView::defaultHeight);

                // A legato line gliding up and back down, a low key held under the last note, whose vibrato comes in.
                juce::AudioBuffer<float> buffer (2, 512);
                juce::MidiBuffer midi;
                const auto s = LeadParameters::toSettings ([&model] (int id) { return model.value (id); });
                const std::vector<std::tuple<int, int, bool>> events { { 0, 64, true }, { 60, 67, true }, { 90, 64, false }, { 140, 71, true },
                                                                       { 170, 67, false }, { 200, 60, true }, { 220, 69, true }, { 230, 71, false } };
                for (int block = 0; block < 400; ++block)
                {
                    midi.clear();
                    for (auto& [at, key, on] : events)
                        if (at == block)
                            midi.addEvent (on ? juce::MidiMessage::noteOn (1, key, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, key), 0);

                    engine.process (buffer, 0, 512, midi, s, 120.0);
                    view.getStereoField().add (buffer.getReadPointer (0), buffer.getReadPointer (1), 512);
                    engine.outputFifo->active = true;
                    if (block % 2 == 0)
                        view.tick();
                }

                for (auto [page, name] : { std::pair (0, "c-lead"), std::pair (1, "c-lead-mod"), std::pair (2, "c-lead-fx") })
                {
                    view.showPage (page);
                    for (int i = 0; i < (page == 0 ? 1 : 20); ++i)
                        view.tick();
                    finish (view, name);
                }

                clickKeyboard<LeadUI::VoiceField> (view, engine);
            }

            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        }

        /** A click on the footer's keyboard in a whole view: it reaches the keyboard, holds a key, and lets go. */
        template <typename Field, typename Engine>
        void clickKeyboard (juce::Component& view, Engine& engine)
        {
            Field* field = nullptr;
            for (auto* child : view.getChildren())
                if (auto* f = dynamic_cast<Field*> (child))
                    field = f;

            expect (field != nullptr, "the view has a keyboard");
            if (field == nullptr)
                return;

            const juce::Point<float> at ((float) field->getWidth() * 0.5f, (float) field->getHeight() - 8.0f);
            view.setVisible (true);     // a hidden component is never under the mouse
            expect (view.getComponentAt (view.getLocalPoint (field, at).toInt()) == field, "nothing covers the keyboard");

            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent click (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys::leftButtonModifier,
                                          1.0f, 0.0f, 0.0f, 0.0f, 0.0f, field, field, now, at, now, 1, false);

            field->mouseDown (click);
            expect (juce::isPositiveAndBelow (engine.mouseKey.load() - MiniKeyboard::lowestKey, MiniKeyboard::highestKey - MiniKeyboard::lowestKey + 1), "a key is held");

            field->mouseUp (click);
            juce::Thread::sleep ((int) MiniKeyboard::MousePlayer::minimumHoldMs + 20);
            field->refresh();
            expectEquals (engine.mouseKey.load(), -1, "and released");
        }

        void finish (juce::Component& view, const juce::String& name)
        {
            // Painting is what the test does either way; the file is only kept when asked for.
            auto image = view.createComponentSnapshot (view.getLocalBounds(), true, 1.0f);
            expect (image.isValid(), name + " paints");

            if (juce::SystemStats::getEnvironmentVariable ("CDAW_SNAPSHOTS", {}) == "1")
                save (view, name);
        }
    };

    static SnapshotTests snapshotTests;
}
