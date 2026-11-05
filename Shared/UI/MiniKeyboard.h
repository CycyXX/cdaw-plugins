#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

/** The small piano in the synths' footers: where its keys are, and playing them with the mouse. */
namespace cdaw::MiniKeyboard
{
    inline constexpr int lowestKey = 21, highestKey = 108;

    inline bool isBlack (int key)
    {
        const int n = key % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    inline int numWhiteKeys()
    {
        int whites = 0;
        for (int k = lowestKey; k <= highestKey; ++k)
            if (! isBlack (k))
                ++whites;
        return whites;
    }

    inline float whiteWidth (juce::Rectangle<float> keys)
    {
        return keys.getWidth() / (float) numWhiteKeys();
    }

    /** The middle of a key. */
    inline float keyX (int key, juce::Rectangle<float> keys)
    {
        int index = 0;
        for (int k = lowestKey; k < key; ++k)
            if (! isBlack (k))
                ++index;

        const float w = whiteWidth (keys);
        return keys.getX() + (isBlack (key) ? (float) index * w : ((float) index + 0.5f) * w);
    }

    inline juce::Rectangle<float> whiteKey (int key, juce::Rectangle<float> keys)
    {
        const float w = whiteWidth (keys);
        return { keyX (key, keys) - w * 0.5f, keys.getY(), w - 1.0f, keys.getHeight() };
    }

    inline juce::Rectangle<float> blackKey (int key, juce::Rectangle<float> keys)
    {
        const float w = whiteWidth (keys);
        return { keyX (key, keys) - w * 0.32f, keys.getY(), w * 0.64f, keys.getHeight() * 0.6f };
    }

    /** The key at a point, or -1 off the keyboard. The gaps drawn between white keys count as keys. */
    inline int keyAt (juce::Point<float> at, juce::Rectangle<float> keys)
    {
        if (! keys.contains (at))
            return -1;

        const int whites = numWhiteKeys();
        int index = juce::jlimit (0, whites - 1, (int) ((at.x - keys.getX()) / whiteWidth (keys)));

        int white = lowestKey;
        for (int k = lowestKey; k <= highestKey; ++k)
            if (! isBlack (k) && index-- == 0)
            {
                white = k;
                break;
            }

        // Black keys sit on top, and only ever next to the white key under the point.
        for (const int k : { white - 1, white + 1 })
            if (k >= lowestKey && k <= highestKey && isBlack (k) && blackKey (k, keys).contains (at))
                return k;

        return white;
    }

    /** Message thread. Tells an engine which key the mouse is holding: the engine picks it up at its
        next block, so a key held for less than a block would never sound. A quick click or a trackpad
        tap is therefore held for a moment, finished off by tick(). */
    class MousePlayer
    {
    public:
        explicit MousePlayer (std::atomic<int>& engineKey) : wanted (engineKey) {}
        ~MousePlayer()      { wanted = -1; }

        /** A key to hold, or -1 for none. Holding the same key again does nothing. */
        void press (int key)
        {
            releasing = false;

            if (key == wanted.load())
                return;

            pressedAt = juce::Time::getMillisecondCounterHiRes();
            wanted = key;
        }

        void release()
        {
            releasing = wanted.load() >= 0;
            tick();
        }

        void tick()
        {
            if (releasing && juce::Time::getMillisecondCounterHiRes() - pressedAt >= minimumHoldMs)
            {
                releasing = false;
                wanted = -1;
            }
        }

        static constexpr double minimumHoldMs = 100.0;

    private:
        std::atomic<int>& wanted;
        double pressedAt = 0.0;
        bool releasing = false;

        JUCE_DECLARE_NON_COPYABLE (MousePlayer)
    };
}
