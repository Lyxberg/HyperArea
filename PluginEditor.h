#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class HyperareaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit HyperareaAudioProcessorEditor (HyperareaAudioProcessor&);
    ~HyperareaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawScope (juce::Graphics&, juce::Rectangle<float>);
    void drawKeyboard (juce::Graphics&, juce::Rectangle<float>);
    void drawEq (juce::Graphics&, juce::Rectangle<float>);

    juce::Rectangle<float> scopeInner() const;
    float eqFreqToX (float f) const;
    float eqXToFreq (float x) const;
    float eqGainToY (float g) const;
    float eqYToGain (float y) const;
    juce::Point<float> eqPointPos (int band) const;
    float eqBandValue (int band, bool freq) const;
    void  setEqParam (int band, bool freq, float value);
    int   eqHitTest (juce::Point<float>) const;

    int eqDragBand = -1;
    int  keyAt (juce::Point<float>) const;
    bool keyInScale (int midi) const;
    void toggleDegree (int midi);
    void setRootNote (int midi);

    struct LnF final : juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override;
    };

    HyperareaAudioProcessor& processorRef;
    LnF lnf;

    juce::ComboBox modeBox, rootBox, scaleBox, octaveBox, chordBox;
    juce::Label    modeLabel, rootLabel, scaleLabel, octaveLabel, chordLabel;

    juce::Slider shiftSlider, fmtSlider, sharpSlider, driveSlider, mixSlider, outSlider;
    juce::Label  shiftLabel, fmtLabel, sharpLabel, driveLabel, mixLabel, outLabel;
    juce::Slider attackSlider, sustainSlider;
    juce::Label  attackLabel, sustainLabel;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> shiftAtt, fmtAtt, sharpAtt, driveAtt, mixAtt, outAtt;
    std::unique_ptr<SliderAtt> attackAtt, sustainAtt;
    std::unique_ptr<ComboAtt>  modeAtt, rootAtt, scaleAtt, octaveAtt, chordAtt;

    juce::Rectangle<float> scopeBounds, keysBounds;
    HyperareaAudioProcessor::ScopeSnapshot snapshot;
    float peakDb = -90.0f;
    std::vector<float> dispDb, tmpDb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperareaAudioProcessorEditor)
};
