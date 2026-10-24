#include "PluginEditor.h"

#include <complex>

namespace
{
    const juce::Colour kBg      { 0xff2f6f92 };
    const juce::Colour kTitle   { 0xff3d80a8 };
    const juce::Colour kScope   { 0xff29a6d6 };
    const juce::Colour kBand    { 0xff7b7bc0 };
    const juce::Colour kDarkRow { 0xff3c3c3c };
    const juce::Colour kInk     { 0xff12303e };
    const juce::Colour kWhite   { 0xffffffff };
    const juce::Colour kKeyW    { 0xffe6e8ee };
    const juce::Colour kKeyB    { 0xff15171c };
    const juce::Colour kKeyIn   { 0xff9db8dd };
    const juce::Colour kOrange  { 0xffff9a3c };
    const juce::Colour kEq      { 0xffffc15e };

    struct PeakCoef { double b0, b1, b2, a1, a2; };

    PeakCoef makePeak (double sr, double f, double q, double gainDb)
    {
        f = juce::jlimit (20.0, sr * 0.45, f);

        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0    = 1.0 + alpha / A;

        PeakCoef c;
        c.b0 = (1.0 + alpha * A) / a0;
        c.b1 = (-2.0 * std::cos (w0)) / a0;
        c.b2 = (1.0 - alpha * A) / a0;
        c.a1 = c.b1;
        c.a2 = (1.0 - alpha / A) / a0;
        return c;
    }

    double peakMag (const PeakCoef& c, double w)
    {
        const std::complex<double> z1 = std::polar (1.0, -w);
        const std::complex<double> z2 = z1 * z1;

        const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
        const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;

        return std::abs (num / den);
    }

    PeakCoef makeHighPass (double sr, double f, double q)
    {
        f = juce::jlimit (10.0, sr * 0.45, f);

        const double w0    = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double cw    = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0    = 1.0 + alpha;

        PeakCoef c;
        c.b0 = (1.0 + cw) * 0.5 / a0;
        c.b1 = -(1.0 + cw) / a0;
        c.b2 = c.b0;
        c.a1 = -2.0 * cw / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    PeakCoef makeLowPass (double sr, double f, double q)
    {
        f = juce::jlimit (10.0, sr * 0.45, f);

        const double w0    = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double cw    = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0    = 1.0 + alpha;

        PeakCoef c;
        c.b0 = (1.0 - cw) * 0.5 / a0;
        c.b1 = (1.0 - cw) / a0;
        c.b2 = c.b0;
        c.a1 = -2.0 * cw / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }
}

//==============================================================================
void HyperareaAudioProcessorEditor::LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                           float sliderPos, float startAngle, float endAngle,
                                                           juce::Slider& slider)
{
    auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float d = juce::jmin (area.getWidth(), area.getHeight()) - 2.0f;
    const auto circle = juce::Rectangle<float> (d, d).withCentre (area.getCentre());

    g.setColour (kWhite);
    g.fillEllipse (circle);

    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const float radius = d * 0.5f - 5.0f;

    juce::Path arc;
    arc.addCentredArc (circle.getCentreX(), circle.getCentreY(), radius, radius, 0.0f,
                       startAngle, angle, true);

    g.setColour (kInk);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    const float px = circle.getCentreX() + radius * std::sin (angle);
    const float py = circle.getCentreY() - radius * std::cos (angle);
    g.drawLine (circle.getCentreX(), circle.getCentreY(), px, py, 2.5f);

    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText (slider.getTextFromValue (slider.getValue()),
                circle.withTrimmedTop (d * 0.58f).toNearestInt(),
                juce::Justification::centredTop, false);
}

//==============================================================================
HyperareaAudioProcessorEditor::HyperareaAudioProcessorEditor (HyperareaAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lnf);

    const auto setupCombo = [this] (juce::ComboBox& c, juce::Label& l, const juce::String& name,
                                    const juce::String& id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (id)))
            c.addItemList (choice->choices, 1);

        c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff4a4a4a));
        c.setColour (juce::ComboBox::textColourId, kWhite);
        c.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x33ffffff));
        c.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xccffffff));
        addAndMakeVisible (c);

        l.setText (name, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, juce::Colour (0xffd8d8d8));
        l.setFont (juce::Font (10.0f, juce::Font::bold));
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };

    setupCombo (modeBox,   modeLabel,   "MODE",  "mode");
    setupCombo (rootBox,   rootLabel,   "ROOT",  "root");
    setupCombo (scaleBox,  scaleLabel,  "SCALE", "scale");
    setupCombo (octaveBox, octaveLabel, "OCT",   "octave");
    setupCombo (chordBox,  chordLabel,  "CHORD", "chord");

    modeAtt   = std::make_unique<ComboAtt> (processorRef.apvts, "mode",   modeBox);
    rootAtt   = std::make_unique<ComboAtt> (processorRef.apvts, "root",   rootBox);
    scaleAtt  = std::make_unique<ComboAtt> (processorRef.apvts, "scale",  scaleBox);
    octaveAtt = std::make_unique<ComboAtt> (processorRef.apvts, "octave", octaveBox);
    chordAtt  = std::make_unique<ComboAtt> (processorRef.apvts, "chord",  chordBox);

    const auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, kWhite);
        l.setFont (juce::Font (9.0f, juce::Font::bold));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupKnob (shiftSlider, shiftLabel, "SHIFT");
    setupKnob (fmtSlider,   fmtLabel,   "FORMANT");
    setupKnob (sharpSlider, sharpLabel, "SHARP");
    setupKnob (attackSlider, attackLabel, "ATTACK");
    setupKnob (sustainSlider, sustainLabel, "SUSTAIN");
    setupKnob (driveSlider, driveLabel, "DRIVE");
    setupKnob (mixSlider,   mixLabel,   "MIX");
    setupKnob (outSlider,   outLabel,   "OUTPUT");

    shiftAtt = std::make_unique<SliderAtt> (processorRef.apvts, "shift",  shiftSlider);
    fmtAtt   = std::make_unique<SliderAtt> (processorRef.apvts, "fmt",    fmtSlider);
    sharpAtt = std::make_unique<SliderAtt> (processorRef.apvts, "sharp",  sharpSlider);
    attackAtt  = std::make_unique<SliderAtt> (processorRef.apvts, "attack",  attackSlider);
    sustainAtt = std::make_unique<SliderAtt> (processorRef.apvts, "sustain", sustainSlider);
    driveAtt = std::make_unique<SliderAtt> (processorRef.apvts, "drive",  driveSlider);
    mixAtt   = std::make_unique<SliderAtt> (processorRef.apvts, "mix",    mixSlider);
    outAtt   = std::make_unique<SliderAtt> (processorRef.apvts, "output", outSlider);

    setResizable (true, true);
    setResizeLimits (640, 416, 1200, 780);
    setSize (800, 520);

    startTimerHz (24);
}

HyperareaAudioProcessorEditor::~HyperareaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void HyperareaAudioProcessorEditor::timerCallback()
{
    repaint (scopeBounds.toNearestInt());
}

//==============================================================================
void HyperareaAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    const float sx = (float) b.getWidth()  / 800.0f;
    const float sy = (float) b.getHeight() / 520.0f;

    const auto R = [sx, sy] (int x, int y, int w, int h)
    {
        return juce::Rectangle<int> ((int) (x * sx), (int) (y * sy),
                                     (int) (w * sx), (int) (h * sy));
    };

    scopeBounds = R (16, 44, 768, 252).toFloat();
    keysBounds  = R (0, 437, 800, 83).toFloat();

    //--- knob row -----------------------------------------------------------
    auto knobBand = R (16, 300, 768, 96);

    juce::Slider* knobs[8]  { &shiftSlider, &fmtSlider, &sharpSlider, &attackSlider, &sustainSlider,
                              &driveSlider, &mixSlider, &outSlider };

    juce::Label*  labels[8] { &shiftLabel,  &fmtLabel,  &sharpLabel,  &attackLabel,  &sustainLabel,
                              &driveLabel,  &mixLabel,  &outLabel  };

    const int cell = knobBand.getWidth() / 8;

    for (int i = 0; i < 8; ++i)
    {
        auto area = knobBand.removeFromLeft (cell);

        labels[i]->setBounds (area.removeFromTop ((int) (14 * sy)));
        knobs[i]->setBounds (area.reduced ((int) (6 * sx), 0));
    }

    //--- combo row ----------------------------------------------------------
    auto row = R (0, 400, 800, 36);
    const int ry = row.getY() + (int) (5 * sy);
    const int rh = (int) (24 * sy);

    const auto place = [&] (juce::Label& l, juce::ComboBox& c, int lx, int lw, int cx, int cw)
    {
        l.setBounds (R (lx, 402, lw, 30));
        c.setBounds (R (cx, 0, cw, 0).withY (ry).withHeight (rh));
    };

    place (modeLabel,   modeBox,   22,  36,  56, 112);
    place (rootLabel,   rootBox,  186,  40, 218,  62);
    place (scaleLabel,  scaleBox, 300,  44, 332, 118);
    place (octaveLabel, octaveBox, 468, 30, 492,  54);
    place (chordLabel,  chordBox, 556,  46, 604, 100);
}

//==============================================================================
void HyperareaAudioProcessorEditor::drawScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    processorRef.readScope (snapshot);

    if (snapshot.valid)
    {
        float m = -90.0f;
        for (int k = 0; k < snapshot.numBins; ++k)
            m = juce::jmax (m, snapshot.db[(size_t) k]);

        peakDb = juce::jmax (m, peakDb - 0.8f);
    }

    g.setColour (kScope);
    g.fillRect (r);

    if (! snapshot.valid || snapshot.numBins < 4)
        return;

    auto inner = r.reduced (3.0f, 5.0f);

    const double nyq   = 24000.0;
    const int    nb    = snapshot.numBins;
    const double binHz = nyq / (double) (nb - 1);

    const int W = juce::jmax (1, (int) inner.getWidth());

    if ((int) dispDb.size() < W)
    {
        dispDb.assign ((size_t) W, -90.0f);
        tmpDb .assign ((size_t) W, -90.0f);
    }

    constexpr float floorDb = 66.0f;

    //--- per pixel: interpolate the bins (no stair steps) -------------------
    for (int px = 0; px < W; ++px)
    {
        const float f = 20.0f * std::pow (20000.0f / 20.0f, ((float) px + 0.5f) / (float) W);
        const float bin = juce::jlimit (1.0f, (float) (nb - 2), (float) (f / binHz));

        const int   i0 = (int) bin;
        const float fr = bin - (float) i0;

        const float v = snapshot.db[(size_t) i0] * (1.0f - fr)
                      + snapshot.db[(size_t) (i0 + 1)] * fr;

        // smooth release so the curve glides instead of flickering
        dispDb[(size_t) px] = juce::jmax (v, dispDb[(size_t) px] - 1.4f);
    }

    //--- spatial smoothing (two 3-tap passes) -------------------------------
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int px = 0; px < W; ++px)
        {
            const int a = juce::jmax (0, px - 1);
            const int b = juce::jmin (W - 1, px + 1);

            tmpDb[(size_t) px] = 0.25f * dispDb[(size_t) a]
                               + 0.50f * dispDb[(size_t) px]
                               + 0.25f * dispDb[(size_t) b];
        }

        std::copy (tmpDb.begin(), tmpDb.begin() + W, dispDb.begin());
    }

    //--- draw ---------------------------------------------------------------
    juce::Path path;
    bool started = false;

    for (int px = 0; px < W; ++px)
    {
        const float rel = juce::jlimit (0.0f, floorDb, peakDb - dispDb[(size_t) px]);
        const float y   = inner.getY() + inner.getHeight() * (rel / floorDb);
        const float x   = inner.getX() + (float) px;

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else           path.lineTo (x, y);
    }

    if (! started)
        return;

    juce::Path fill (path);
    fill.lineTo (inner.getRight(), inner.getBottom());
    fill.lineTo (inner.getX(),     inner.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (kWhite.withAlpha (0.42f), 0.0f, inner.getY(),
                                             kWhite.withAlpha (0.06f), 0.0f, inner.getBottom(), false));
    g.fillPath (fill);

    g.setColour (kWhite);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

//==============================================================================
void HyperareaAudioProcessorEditor::drawKeyboard (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int target   = processorRef.getTargetMidi();
    const int detected = processorRef.getDetectedMidi();
    const bool voiced  = processorRef.isVoiced();

    const int firstMidi = 24;
    const int numOctaves = 3;
    const float whiteW = r.getWidth() / (float) (7 * numOctaves);
    const float blackW = whiteW * 0.62f;

    static const int whiteSemis[7] { 0, 2, 4, 5, 7, 9, 11 };
    static const int blackSemis[5] { 1, 3, 6, 8, 10 };
    static const float blackPos[5] { 0.7f, 1.7f, 3.7f, 4.7f, 5.7f };

    g.setColour (juce::Colour (0xff12222c));
    g.fillRect (r);

    int whiteIndex = 0;

    for (int o = 0; o < numOctaves; ++o)
    {
        for (int i = 0; i < 7; ++i)
        {
            const int midi = firstMidi + o * 12 + whiteSemis[i];
            auto key = juce::Rectangle<float> (r.getX() + whiteIndex * whiteW, r.getY(),
                                               whiteW - 1.0f, r.getHeight());

            juce::Colour fill (kKeyW);

            if (keyInScale (midi))
                fill = kKeyIn;

            if (midi == target)
                fill = kOrange;

            g.setColour (fill);
            g.fillRect (key);

            if (midi == detected && voiced)
            {
                g.setColour (juce::Colour (0xff23d18b));
                g.drawRect (key, 2.0f);
            }

            ++whiteIndex;
        }
    }

    for (int o = 0; o < numOctaves; ++o)
    {
        for (int i = 0; i < 5; ++i)
        {
            const int midi = firstMidi + o * 12 + blackSemis[i];
            const float x = r.getX() + (o * 7 + blackPos[i]) * whiteW - blackW * 0.5f;

            auto key = juce::Rectangle<float> (x, r.getY(), blackW, r.getHeight() * 0.6f);

            juce::Colour fill (kKeyB);

            if (keyInScale (midi))
                fill = juce::Colour (0xff2b4f74);

            if (midi == target)
                fill = kOrange.darker (0.2f);

            g.setColour (fill);
            g.fillRect (key);

            if (midi == detected && voiced)
            {
                g.setColour (juce::Colour (0xff23d18b));
                g.drawRect (key, 2.0f);
            }
        }
    }
}

//==============================================================================
bool HyperareaAudioProcessorEditor::keyInScale (int midi) const
{
    const int root  = (int) processorRef.apvts.getRawParameterValue ("root")->load();
    const int scale = (int) processorRef.apvts.getRawParameterValue ("scale")->load();
    const int deg   = ((midi - root) % 12 + 12) % 12;

    if (scale == 8)
        return processorRef.apvts.getRawParameterValue ("deg" + juce::String (deg))->load() > 0.5f;

    return HyperareaAudioProcessor::isInScale (midi, root, scale);
}

int HyperareaAudioProcessorEditor::keyAt (juce::Point<float> pos) const
{
    auto r = keysBounds;

    if (! r.contains (pos))
        return -1;

    const int numOctaves = 3;
    const float whiteW = r.getWidth() / (float) (7 * numOctaves);
    const float blackW = whiteW * 0.62f;

    static const int whiteSemis[7] { 0, 2, 4, 5, 7, 9, 11 };
    static const int blackSemis[5] { 1, 3, 6, 8, 10 };
    static const float blackPos[5] { 0.7f, 1.7f, 3.7f, 4.7f, 5.7f };

    // black keys first (they sit on top)
    if (pos.y < r.getY() + r.getHeight() * 0.6f)
    {
        for (int o = 0; o < numOctaves; ++o)
        {
            for (int i = 0; i < 5; ++i)
            {
                const float x = r.getX() + (o * 7 + blackPos[i]) * whiteW - blackW * 0.5f;

                if (pos.x >= x && pos.x < x + blackW)
                    return 24 + o * 12 + blackSemis[i];
            }
        }
    }

    const int idx = juce::jlimit (0, 7 * numOctaves - 1,
                                  (int) ((pos.x - r.getX()) / whiteW));

    return 24 + (idx / 7) * 12 + whiteSemis[idx % 7];
}

void HyperareaAudioProcessorEditor::toggleDegree (int midi)
{
    const int root = (int) processorRef.apvts.getRawParameterValue ("root")->load();
    const int deg  = ((midi - root) % 12 + 12) % 12;

    auto* param = processorRef.apvts.getParameter ("deg" + juce::String (deg));
    if (param == nullptr)
        return;

    const bool on = param->getValue() > 0.5f;

    int enabled = 0;
    for (int d = 0; d < 12; ++d)
        if (processorRef.apvts.getRawParameterValue ("deg" + juce::String (d))->load() > 0.5f)
            ++enabled;

    if (on && enabled <= 1)
        return;                         // never leave the scale empty

    // editing the keyboard switches the scale selector to Custom
    if (auto* s = processorRef.apvts.getParameter ("scale"))
        s->setValueNotifyingHost (s->convertTo0to1 (8.0f));

    param->setValueNotifyingHost (on ? 0.0f : 1.0f);
}

void HyperareaAudioProcessorEditor::setRootNote (int midi)
{
    if (auto* r = processorRef.apvts.getParameter ("root"))
        r->setValueNotifyingHost (r->convertTo0to1 ((float) (((midi % 12) + 12) % 12)));
}

//==============================================================================
juce::Rectangle<float> HyperareaAudioProcessorEditor::scopeInner() const
{
    return scopeBounds.reduced (3.0f, 5.0f);
}

float HyperareaAudioProcessorEditor::eqFreqToX (float f) const
{
    auto in = scopeInner();
    const float u = std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f);
    return in.getX() + in.getWidth() * u;
}

float HyperareaAudioProcessorEditor::eqXToFreq (float x) const
{
    auto in = scopeInner();
    const float u = juce::jlimit (0.0f, 1.0f, (x - in.getX()) / juce::jmax (1.0f, in.getWidth()));
    return juce::jlimit (30.0f, 18000.0f, 20.0f * std::pow (1000.0f, u));
}

float HyperareaAudioProcessorEditor::eqGainToY (float g) const
{
    auto in = scopeInner();
    return in.getCentreY() - (juce::jlimit (-18.0f, 18.0f, g) / 18.0f) * in.getHeight() * 0.45f;
}

float HyperareaAudioProcessorEditor::eqYToGain (float y) const
{
    auto in = scopeInner();
    const float half = juce::jmax (1.0f, in.getHeight() * 0.45f);
    return juce::jlimit (-18.0f, 18.0f, (in.getCentreY() - y) / half * 18.0f);
}

float HyperareaAudioProcessorEditor::eqBandValue (int band, bool freq) const
{
    if (! freq && (band == 0 || band == 3))
        return 0.0f;                       // HP / LP have no gain

    const char* id = freq ? "f" : "g";
    return processorRef.apvts.getRawParameterValue ("eq" + juce::String (band + 1) + id)->load();
}

juce::Point<float> HyperareaAudioProcessorEditor::eqPointPos (int band) const
{
    return { eqFreqToX (eqBandValue (band, true)), eqGainToY (eqBandValue (band, false)) };
}

void HyperareaAudioProcessorEditor::setEqParam (int band, bool freq, float value)
{
    const juce::String id = "eq" + juce::String (band + 1) + (freq ? "f" : "g");

    if (auto* p = processorRef.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

int HyperareaAudioProcessorEditor::eqHitTest (juce::Point<float> pos) const
{
    int best = -1;
    float bestD = 20.0f;

    for (int i = 0; i < 4; ++i)
    {
        const float d = eqPointPos (i).getDistanceFrom (pos);

        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }

    return best;
}

//==============================================================================
void HyperareaAudioProcessorEditor::drawEq (juce::Graphics& g, juce::Rectangle<float>)
{
    auto in = scopeInner();

    const double sr = processorRef.getSampleRate() > 0.0 ? processorRef.getSampleRate() : 48000.0;

    PeakCoef coef[4];

    coef[0] = makeHighPass (sr, eqBandValue (0, true), 0.707);
    coef[1] = makePeak     (sr, eqBandValue (1, true), 0.9, eqBandValue (1, false));
    coef[2] = makePeak     (sr, eqBandValue (2, true), 0.9, eqBandValue (2, false));
    coef[3] = makeLowPass  (sr, eqBandValue (3, true), 0.707);

    //--- 0 dB reference ------------------------------------------------------
    const float y0 = eqGainToY (0.0f);

    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawHorizontalLine ((int) y0, in.getX(), in.getRight());

    //--- response curve ------------------------------------------------------
    juce::Path path;
    const int W = juce::jmax (1, (int) in.getWidth());

    for (int px = 0; px < W; ++px)
    {
        const float f = eqXToFreq (in.getX() + (float) px + 0.5f);
        const double w = 2.0 * juce::MathConstants<double>::pi * (double) f / sr;

        double mag = 1.0;

        for (int i = 0; i < 4; ++i)
            mag *= peakMag (coef[i], w);

        const float db = (float) (20.0 * std::log10 (juce::jmax (1.0e-6, mag)));
        const float y  = eqGainToY (db);
        const float x  = in.getX() + (float) px;

        if (px == 0) path.startNewSubPath (x, y);
        else         path.lineTo (x, y);
    }

    g.setColour (kEq.withAlpha (0.95f));
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    //--- handles -------------------------------------------------------------
    for (int i = 0; i < 4; ++i)
    {
        const auto p = eqPointPos (i);
        const bool active = (i == eqDragBand);
        const float rad = active ? 8.0f : 6.0f;

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (p.x - rad - 2.0f, p.y - rad - 2.0f, (rad + 2.0f) * 2.0f, (rad + 2.0f) * 2.0f);

        g.setColour (active ? kOrange : kEq);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);

        g.setColour (kInk.withAlpha (0.7f));
        g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f).toNearestInt(),
                    juce::Justification::centred, false);

        if (i == 0 || i == 3)
        {
            g.setFont (juce::Font (8.0f, juce::Font::bold));
            g.setColour (kWhite.withAlpha (0.95f));
            g.drawText (i == 0 ? "HP" : "LP",
                        juce::Rectangle<float> (p.x - 12.0f, p.y - rad - 14.0f, 24.0f, 10.0f).toNearestInt(),
                        juce::Justification::centred, false);
        }
    }
}

//==============================================================================
void HyperareaAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (scopeBounds.contains (e.position))
    {
        eqDragBand = eqHitTest (e.position);

        if (eqDragBand < 0)
        {
            // nothing hit: grab the band nearest in frequency
            float bestD = 1.0e9f;

            for (int i = 0; i < 4; ++i)
            {
                const float d = std::abs (eqPointPos (i).x - e.position.x);

                if (d < bestD) { bestD = d; eqDragBand = i; }
            }
        }

        repaint (scopeBounds.toNearestInt());
        return;
    }

    const int midi = keyAt (e.position);

    if (midi < 0)
        return;

    if (e.mods.isRightButtonDown() || e.mods.isAltDown())
        setRootNote (midi);
    else
        toggleDegree (midi);

    repaint();
}

void HyperareaAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (eqDragBand < 0)
        return;

    float f = eqXToFreq (e.position.x);

    if (eqDragBand == 0)      f = juce::jlimit (20.0f, 2000.0f, f);
    else if (eqDragBand == 3) f = juce::jlimit (200.0f, 20000.0f, f);

    setEqParam (eqDragBand, true, f);

    if (eqDragBand == 1 || eqDragBand == 2)
        setEqParam (eqDragBand, false, eqYToGain (e.position.y));

    repaint (scopeBounds.toNearestInt());
}

void HyperareaAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (eqDragBand >= 0)
    {
        eqDragBand = -1;
        repaint (scopeBounds.toNearestInt());
    }
}

void HyperareaAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! scopeBounds.contains (e.position))
        return;

    const int band = eqHitTest (e.position);

    if (band < 0)
        return;

    if (band == 0)      setEqParam (0, true, 20.0f);
    else if (band == 3) setEqParam (3, true, 20000.0f);
    else                setEqParam (band, false, 0.0f);

    repaint (scopeBounds.toNearestInt());
}

//==============================================================================
void HyperareaAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float sx = b.getWidth()  / 800.0f;
    const float sy = b.getHeight() / 520.0f;

    g.fillAll (kBg);

    //--- title bar ----------------------------------------------------------
    g.setColour (kTitle);
    g.fillRect (0.0f, 0.0f, b.getWidth(), 38.0f * sy);

    g.setColour (kInk);
    g.setFont (juce::Font (24.0f * sy, juce::Font::bold));
    g.drawText ("HYPERAREA", (int) (14 * sx), (int) (2 * sy), (int) (400 * sx), (int) (34 * sy),
                juce::Justification::centredLeft, false);

    {
        const int det = processorRef.getDetectedMidi();
        const int tgt = processorRef.getTargetMidi();

        g.setFont (juce::Font (11.0f * sy, juce::Font::bold));
        g.setColour (kInk.withAlpha (0.85f));
        g.drawText ("IN " + HyperareaAudioProcessor::noteName (det)
                      + "   OUT " + HyperareaAudioProcessor::noteName (tgt),
                    (int) (560 * sx), (int) (6 * sy), (int) (226 * sx), (int) (26 * sy),
                    juce::Justification::centredRight, false);
    }

    //--- knob band ----------------------------------------------------------
    g.setColour (kBand);
    g.fillRect (juce::Rectangle<int> ((int) (16 * sx), (int) (300 * sy),
                                      (int) (768 * sx), (int) (96 * sy)).toFloat());

    //--- combo row ----------------------------------------------------------
    g.setColour (kDarkRow);
    g.fillRect (juce::Rectangle<int> (0, (int) (400 * sy), b.getWidth(),
                                      (int) (36 * sy)).toFloat());

    drawScope (g, scopeBounds);
    drawEq (g, scopeBounds);
    drawKeyboard (g, keysBounds);
}
