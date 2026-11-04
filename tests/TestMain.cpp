#include "TestSupport.h"
#include <iostream>

// Runs every test, or the one named by the first argument.
struct ConsoleRunner : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

int main (int argc, char* argv[])
{
    int failures = 0, passes = 0, results = 0;

    {
        juce::ScopedJuceInitialiser_GUI juceInit;

        ConsoleRunner runner;
        runner.setAssertOnFailure (false);

        const auto which = argc > 1 ? juce::String (argv[1]) : juce::String ("Plugins");
        juce::Array<juce::UnitTest*> named;
        for (auto* test : juce::UnitTest::getAllTests())
            if (test->getName() == which)
                named.add (test);

        if (named.isEmpty())
            runner.runTestsInCategory (which);
        else
            runner.runTests (named);

        results = runner.getNumResults();
        for (int i = 0; i < results; ++i)
        {
            failures += runner.getResult (i)->failures;
            passes += runner.getResult (i)->passes;
        }
    }

    std::cout << std::endl << "Plugin tests: " << passes << " passed, " << failures << " failed" << std::endl;
    return failures == 0 && results > 0 ? 0 : 1;
}
