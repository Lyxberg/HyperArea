#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    bool scaleHas (int scale, int semitone)
    {
        static const int masks[8][12] =
        {
            { 1,1,1,1,1,1,1,1,1,1,1,1 },   // Chromatic
            { 1,0,1,0,1,1,0,1,0,1,0,1 },   // Major
            { 1,0,1,1,0,1,0,1,1,0,1,0 },   // Minor
            { 1,0,0,1,0,1,0,1,0,0,1,0 },   // Minor pentatonic
            { 1,0,1,0,1,0,0,1,0,1,0,0 },   // Major pentatonic
            { 1,0,1,1,0,1,0,1,0,1,0,1 },   // Dorian
            { 1,1,0,1,0,1,0,1,1,0,1,0 },   // Phrygian
            { 1,0,1,1,0,1,0,1,1,0,0,1 }    // Harmonic minor
        };

        return masks[juce::jlimit (0, 7, scale)][juce::jlimit (0, 11, semitone)] != 0;
    }

    float wrapPi (float x)
    {
        while (x >  juce::MathConstants<float>::pi) x -= juce::MathConstants<float>::twoPi;
        while (x < -juce::MathConstants<float>::pi) x += juce::MathConstants<float>::twoPi;
        return x;
    }
}

//==============================================================================
HyperareaAudioProcessor::HyperareaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
HyperareaAudioProcessor::createParameterLayout()
{
    using namespace juce;
    using Range = NormalisableRange<float>;

    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto fmt = [] (int decimals, const String& suffix)
    {
        return [decimals, suffix] (float v, int) { return String (v, decimals) + suffix; };
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
        StringArray { "Spectral Snap", "Snap to Scale", "Transpose" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Root",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
        StringArray { "Chromatic", "Major", "Minor", "Minor Pentatonic",
                      "Major Pentatonic", "Dorian", "Phrygian", "Harmonic Minor", "Custom" }, 3));

    for (int d = 0; d < 12; ++d)
    {
        const bool on = d == 0 || d == 3 || d == 5 || d == 7 || d == 10;   // minor pentatonic

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "deg" + String (d), 1 },
                       "Degree " + String (d), on));
    }

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "octave", 1 }, "Octave",
        StringArray { "-2", "-1", "0", "+1", "+2" }, 2));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "chord", 1 }, "Chord",
        StringArray { "Single", "Power", "Minor", "Major", "Minor 7", "Major 7", "Sus4" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "shift", 1 }, "Shift",
        Range (-24.0f, 24.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " st"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vowel", 1 }, "Vowel",
        Range (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fmt", 1 }, "Formant",
        Range (0.0f, 100.0f, 0.1f), 70.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sharp", 1 }, "Sharp",
        Range (0.0f, 100.0f, 0.1f), 30.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Glide",
        Range (0.0f, 300.0f, 1.0f), 15.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " ms"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive",
        Range (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        Range (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
        Range (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (1, " dB"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
        Range (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain",
        Range (-100.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " %"))));

    //--- 4 band EQ: high-pass, peak, peak, low-pass -------------------------
    layout.add (std::make_unique<AudioParameterFloat>
    (
        ParameterID { "eq1f", 1 }, "EQ 1 High Pass",
        NormalisableRange<float> (20.0f, 2000.0f, 0.0f, 0.35f),
        20.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " Hz"))
    ));

    static const float eqPeakFreqs[2] { 550.0f, 2500.0f };

    for (int i = 0; i < 2; ++i)
    {
        layout.add (std::make_unique<AudioParameterFloat>
        (
            ParameterID { "eq" + String (i + 2) + "f", 1 },
            "EQ " + String (i + 2) + " Freq",
            NormalisableRange<float> (30.0f, 18000.0f, 0.0f, 0.28f),
            eqPeakFreqs[i],
            AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " Hz"))
        ));

        layout.add (std::make_unique<AudioParameterFloat>
        (
            ParameterID { "eq" + String (i + 2) + "g", 1 },
            "EQ " + String (i + 2) + " Gain",
            Range (-18.0f, 18.0f, 0.1f),
            0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction (fmt (1, " dB"))
        ));
    }

    layout.add (std::make_unique<AudioParameterFloat>
    (
        ParameterID { "eq4f", 1 }, "EQ 4 Low Pass",
        NormalisableRange<float> (200.0f, 20000.0f, 0.0f, 0.35f),
        20000.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (fmt (0, " Hz"))
    ));

    return layout;
}

//==============================================================================
juce::String HyperareaAudioProcessor::noteName (int midi)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = (midi % 12 + 12) % 12;
    const int o = midi / 12 - 1;
    return juce::String (names[n]) + juce::String (o);
}

bool HyperareaAudioProcessor::scaleMask (int scale, int semitone) { return scaleHas (scale, semitone); }

bool HyperareaAudioProcessor::isInScale (int midi, int root, int scale)
{
    return scaleHas (scale, ((midi - root) % 12 + 12) % 12);
}

//==============================================================================
void HyperareaAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    window.resize ((size_t) fftSize);

    for (int n = 0; n < fftSize; ++n)
        window[(size_t) n] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                                     * (double) n / (double) fftSize));

    for (auto& c : chans)
    {
        c.inRing.assign ((size_t) fftSize, 0.0f);
        c.ola.assign ((size_t) fftSize, 0.0f);
        c.hopOut.assign ((size_t) hopSize, 0.0f);
        c.dryDelay.assign ((size_t) fftSize, 0.0f);
        c.fftBuf.assign ((size_t) fftSize * 2, 0.0f);
        c.residual.assign ((size_t) fftSize * 2, 0.0f);
        c.lastPhase.assign ((size_t) halfBins + 1, 0.0f);
        c.trueFreq.assign ((size_t) halfBins + 1, 0.0f);

        for (auto& op : c.outPhase)
            op.assign ((size_t) halfBins + 1, 0.0f);

        c.specPhase.assign ((size_t) halfBins + 1, 0.0f);
        c.inPos = c.outIndex = c.dryPos = 0;
    }

    magAcc.assign ((size_t) halfBins + 1, 0.0f);
    outPhaseSum.assign ((size_t) halfBins + 1, 0.0f);
    anaRing.assign (anaSize, 0.0f);
    decBuf.assign (anaWin, 0.0f);
    corrBuf.assign ((size_t) (anaWin / 2 + 2), 0.0f);

    curRatio = 1.0f;
    blockCounter = 0;
    candidateMidi = -1;
    candidateCount = 0;

    setLatencySamples (latency);
}

bool HyperareaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void HyperareaAudioProcessor::analysePitch()
{
    for (int n = 0; n < anaWin; ++n)
    {
        float acc = 0.0f;

        for (int j = 0; j < anaDec; ++j)
            acc += anaRing[(size_t) ((blockCounter + n * anaDec + j) & (anaSize - 1))];

        decBuf[(size_t) n] = acc / (float) anaDec;
    }

    const double srDec = currentSampleRate / (double) anaDec;
    const int minLag = juce::jmax (2, (int) (srDec / 1200.0));
    const int maxLag = juce::jmin (anaWin / 2 - 1, (int) (srDec / 35.0));

    auto& corr = corrBuf;

    float bestV = 0.0f;
    int   bestLag = 0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0, e1 = 0.0, e2 = 0.0;

        for (int n = 0; n + lag < anaWin; ++n)
        {
            const float a = decBuf[(size_t) n];
            const float b = decBuf[(size_t) (n + lag)];
            sum += (double) a * b;
            e1  += (double) a * a;
            e2  += (double) b * b;
        }

        const float v = (float) (sum / std::sqrt (e1 * e2 + 1.0e-12));
        corr[(size_t) lag] = v;

        if (v > bestV) { bestV = v; bestLag = lag; }
    }

    if (bestV < 0.55f || bestLag == 0)
    {
        voiced.store (false);
        return;
    }

    int chosen = bestLag;

    for (int lag = minLag + 1; lag < maxLag; ++lag)
    {
        if (corr[(size_t) lag] >= 0.85f * bestV
            && corr[(size_t) lag] >= corr[(size_t) (lag - 1)]
            && corr[(size_t) lag] >= corr[(size_t) (lag + 1)])
        {
            chosen = lag;
            break;
        }
    }

    const float a = corr[(size_t) juce::jmax (1, chosen - 1)];
    const float b = corr[(size_t) chosen];
    const float c = corr[(size_t) juce::jmin (maxLag, chosen + 1)];
    const float den = a - 2.0f * b + c;
    const float delta = std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f;

    const double f0 = srDec / (double) (chosen + juce::jlimit (-0.5f, 0.5f, delta));

    if (f0 < 20.0 || f0 > 1500.0)
    {
        voiced.store (false);
        return;
    }

    const int midi = juce::jlimit (12, 108,
                                   (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0)));

    if (midi == candidateMidi)
    {
        if (candidateCount < 3)
            ++candidateCount;
    }
    else
    {
        candidateMidi = midi;
        candidateCount = 0;
    }

    if (candidateCount >= 1)
    {
        detectedMidi.store (midi);
        voiced.store (true);
    }
}

//==============================================================================
void HyperareaAudioProcessor::renderFrame (int channel, const float* ratios, int numVoices)
{
    auto& c = chans[channel];

    float* buf = c.fftBuf.data();
    const int mask = fftSize - 1;

    for (int n = 0; n < fftSize; ++n)
        buf[n] = c.inRing[(size_t) ((c.inPos + n) & mask)] * window[(size_t) n];

    std::fill (buf + fftSize, buf + 2 * fftSize, 0.0f);

    fft.performRealOnlyForwardTransform (buf, true);

    const auto outputFrame = [&]()
    {
        if (channel == 0)
            captureScope (buf, halfBins);

        fft.performRealOnlyInverseTransform (buf);

        for (int n = 0; n < fftSize; ++n)
            c.ola[(size_t) n] += buf[n] * window[(size_t) n] * 0.5f;

        std::copy (c.ola.begin(), c.ola.begin() + hopSize, c.hopOut.begin());
        std::move (c.ola.begin() + hopSize, c.ola.end(), c.ola.begin());
        std::fill (c.ola.end() - hopSize, c.ola.end(), 0.0f);
    };

    //--- identity: nothing to shift ---------------------------------------
    if (! spectralSnap && numVoices == 1 && std::abs (ratios[0] - 1.0f) < 0.0005f)
    {
        outputFrame();
        return;
    }

    const double expct = juce::MathConstants<double>::twoPi * (double) hopSize / (double) fftSize;
    const double sr = currentSampleRate;

    //--- phase analysis (once per frame) -----------------------------------
    for (int k = 1; k < halfBins; ++k)
    {
        const float re = buf[2 * k], im = buf[2 * k + 1];
        const float mag = std::sqrt (re * re + im * im);
        const float ph  = std::atan2 (im, re);

        float d = ph - c.lastPhase[(size_t) k] - (float) (k * expct);
        d = wrapPi (d);
        c.lastPhase[(size_t) k] = ph;

        c.trueFreq[(size_t) k] = (float) (((double) k * expct + (double) d) * sr
                                          / (juce::MathConstants<double>::twoPi * (double) hopSize));
        c.residual[(size_t) (2 * k)]     = mag;
        c.residual[(size_t) (2 * k + 1)] = 0.0f;
    }

    //--- spectral snap: every partial lands on its own scale note ----------
    if (spectralSnap)
    {
        std::fill (magAcc.begin(), magAcc.end(), 0.0f);

        const double binHz = sr / (double) fftSize;
        double inE = 0.0;

        for (int k = 1; k < halfBins; ++k)
        {
            const float mag = c.residual[(size_t) (2 * k)];

            if (mag <= 1.0e-6f)
                continue;

            const double f = (double) k * binHz;

            if (f < 20.0)
                continue;

            inE += (double) mag;

            const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0));
            int best = midi;

            for (int d = 0; d <= 6; ++d)
            {
                if (inScaleMidi (midi + d)) { best = midi + d; break; }
                if (inScaleMidi (midi - d)) { best = midi - d; break; }
            }

            const int    tm = best + 12 * frameOctave + frameShift;
            const double tf = 440.0 * std::pow (2.0, (tm - 69) / 12.0);
            const double tb = tf / binHz;

            if (tb < 1.0 || tb >= (double) (halfBins - 1))
                continue;

            const int   t0 = (int) tb;
            const float fr = (float) (tb - (double) t0);

            magAcc[(size_t) t0]     += mag * (1.0f - fr);
            magAcc[(size_t) (t0 + 1)] += mag * fr;
        }

        double outE = 0.0;

        for (int t = 1; t < halfBins; ++t)
            outE += (double) magAcc[(size_t) t];

        const float g = outE > 1.0e-9 ? (float) (inE / outE) : 1.0f;

        std::fill (buf, buf + 2 * fftSize, 0.0f);

        for (int t = 1; t < halfBins; ++t)
        {
            if (magAcc[(size_t) t] <= 1.0e-7f)
                continue;

            c.specPhase[(size_t) t] += (float) (juce::MathConstants<double>::twoPi
                                               * (double) t * binHz * (double) hopSize / sr);

            const float m = magAcc[(size_t) t] * g;
            buf[2 * t]     = m * std::cos (c.specPhase[(size_t) t]);
            buf[2 * t + 1] = m * std::sin (c.specPhase[(size_t) t]);
        }

        outputFrame();
        return;
    }

    //--- remap every voice into the same spectrum --------------------------
    std::fill (buf, buf + 2 * fftSize, 0.0f);

    const float twoPiF = juce::MathConstants<float>::twoPi;
    const float voiceGain = 1.0f / std::sqrt ((float) juce::jmax (1, numVoices));

    for (int v = 0; v < numVoices; ++v)
    {
        const float ratio = ratios[v];
        auto& ph = c.outPhase[(size_t) v];

        for (int k = 1; k < halfBins; ++k)
        {
            const float src = (float) k / ratio;

            if (src < 1.0f || src >= (float) (halfBins - 1))
                continue;

            const int   i0 = (int) src;
            const float fr = src - (float) i0;

            const float mag = c.residual[(size_t) (2 * i0)] * (1.0f - fr)
                            + c.residual[(size_t) (2 * (i0 + 1))] * fr;

            const float f0 = c.trueFreq[(size_t) i0] * (1.0f - fr)
                           + c.trueFreq[(size_t) (i0 + 1)] * fr;

            ph[(size_t) k] += f0 * ratio * (float) hopSize / (float) sr * twoPiF;

            buf[2 * k]     += voiceGain * mag * std::cos (ph[(size_t) k]);
            buf[2 * k + 1] += voiceGain * mag * std::sin (ph[(size_t) k]);
        }
    }

    outputFrame();
}


//==============================================================================
void HyperareaAudioProcessor::captureScope (const float* buf, int halfBins)
{
    ScopeSnapshot local;
    const float norm = 4.0f / (float) fftSize;      // 0 dB = full-scale sine

    local.numBins = halfBins + 1;

    for (int k = 0; k <= halfBins; ++k)
    {
        const float re = buf[2 * k], im = buf[2 * k + 1];
        const float mag = std::sqrt (re * re + im * im) * norm;

        local.db[(size_t) k] = juce::Decibels::gainToDecibels (mag, -90.0f);
    }

    local.valid = true;

    const juce::SpinLock::ScopedTryLockType sl (scopeLock);
    if (sl.isLocked())
        scope = local;
}

bool HyperareaAudioProcessor::readScope (ScopeSnapshot& dest) const
{
    const juce::SpinLock::ScopedTryLockType sl (scopeLock);

    if (! sl.isLocked() || ! scope.valid)
        return false;

    dest = scope;
    return true;
}

//==============================================================================
void HyperareaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin (2, buffer.getNumChannels());

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const int   mode   = (int) apvts.getRawParameterValue ("mode")->load();
    const int   root   = (int) apvts.getRawParameterValue ("root")->load();
    const int   scale  = (int) apvts.getRawParameterValue ("scale")->load();
    const int   octave = (int) apvts.getRawParameterValue ("octave")->load() - 2;
    const int   shift  = (int) apvts.getRawParameterValue ("shift")->load();
    const float glideMs = apvts.getRawParameterValue ("glide")->load();
    const float vowel   = apvts.getRawParameterValue ("vowel")->load() * 0.01f;
    const float fmtAmt  = apvts.getRawParameterValue ("fmt")->load() * 0.01f;
    const float sharp   = apvts.getRawParameterValue ("sharp")->load() * 0.01f;
    const float drive   = apvts.getRawParameterValue ("drive")->load() * 0.01f;
    const float mix     = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    const float outGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load());

    float* d0 = buffer.getWritePointer (0);
    float* d1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    //--- active scale -------------------------------------------------------
    curRoot = root;

    if (scale == 8)                     // Custom
    {
        for (int d = 0; d < 12; ++d)
            curMask[(size_t) d] = apvts.getRawParameterValue ("deg" + juce::String (d))->load() > 0.5f;
    }
    else
    {
        for (int d = 0; d < 12; ++d)
            curMask[(size_t) d] = scaleMask (scale, d);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = d1 != nullptr ? 0.5f * (d0[i] + d1[i]) : d0[i];
        anaRing[(size_t) blockCounter] = m;
        blockCounter = (blockCounter + 1) & (anaSize - 1);
    }

    analysePitch();

    //--- target ratio -------------------------------------------------------
    float targetRatio = 1.0f;
    int   outNote = detectedMidi.load();

    spectralSnap = (mode == 0);
    frameRoot   = root;
    frameScale  = scale;
    frameOctave = octave;
    frameShift  = shift;

    if (mode == 1)          // snap the fundamental to the scale
    {
        if (voiced.load())
        {
            int snapped = detectedMidi.load();

            for (int d = 0; d <= 6; ++d)
            {
                if (inScaleMidi (snapped + d)) { snapped += d; break; }
                if (inScaleMidi (snapped - d)) { snapped -= d; break; }
            }

            const int target = juce::jlimit (12, 108, snapped + 12 * octave + shift);
            outNote = target;
            targetRatio = std::pow (2.0f, (float) (target - detectedMidi.load()) / 12.0f);
        }
        else
        {
            targetRatio = curRatio;
            outNote = detectedMidi.load();
        }
    }
    else if (mode == 2)     // plain transposition
    {
        targetRatio = std::pow (2.0f, (float) shift / 12.0f);
        outNote = detectedMidi.load() + shift;
    }
    else                    // spectral snap: every partial maps on its own
    {
        targetRatio = 1.0f;
        outNote = detectedMidi.load();
    }

    targetRatio = juce::jlimit (0.25f, 4.0f, targetRatio);
    targetMidi.store (juce::jlimit (12, 108, outNote));
    //--- glide --------------------------------------------------------------
    const double dt = (double) numSamples / currentSampleRate;
    const float tau = juce::jmax (0.001f, glideMs * 0.001f);
    const float k   = 1.0f - std::exp (-(float) dt / tau);
    curRatio += (targetRatio - curRatio) * k;

    //--- formant bank -------------------------------------------------------
    static const float vowels[5][3] =
    {
        { 730.0f, 1090.0f, 2440.0f },   // A
        { 530.0f, 1840.0f, 2480.0f },   // E
        { 270.0f, 2290.0f, 3010.0f },   // I
        { 570.0f,  840.0f, 2410.0f },   // O
        { 300.0f,  870.0f, 2240.0f }    // U
    };

    const double qv = 1.5 + sharp * 18.0;

    if (std::abs (vowel - lastVowel) > 0.002f || std::abs ((float) qv - lastQ) > 0.05f)
    {
        lastVowel = vowel;
        lastQ = (float) qv;

        const float pos = vowel * 4.0f;
        const int   i0  = juce::jlimit (0, 3, (int) pos);
        const float fr  = pos - (float) i0;

        for (int b = 0; b < 3; ++b)
        {
            const double f = (double) (vowels[i0][b] * (1.0f - fr) + vowels[i0 + 1][b] * fr);

            for (auto& ch : chans)
                ch.fmt[b].setPeaking (currentSampleRate, f, qv, 14.0);
        }
    }

    //--- chord voices -------------------------------------------------------
    static const int chordSemis[7][4] =
    {
        { 0, -99, -99, -99 },   // Single
        { 0,   7,  12, -99 },   // Power
        { 0,   3,   7, -99 },   // Minor
        { 0,   4,   7, -99 },   // Major
        { 0,   3,   7,  10 },   // Minor 7
        { 0,   4,   7,  11 },   // Major 7
        { 0,   5,   7, -99 }    // Sus4
    };

    const int chord = juce::jlimit (0, 6, (int) apvts.getRawParameterValue ("chord")->load());

    float ratios[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    int   numVoices = 0;

    for (int i = 0; i < 4; ++i)
    {
        const int s = chordSemis[chord][i];

        if (s <= -99)
            break;

        ratios[numVoices++] = juce::jlimit (0.1f, 8.0f,
                                            curRatio * std::pow (2.0f, (float) s / 12.0f));
    }

    //--- 4 band EQ -----------------------------------------------------------
    bool eqActive = false;

    {
        const float hp = apvts.getRawParameterValue ("eq1f")->load();
        const float lp = apvts.getRawParameterValue ("eq4f")->load();

        if (hp > 23.0f || lp < 19000.0f)
            eqActive = true;

        for (auto& ch : chans)
        {
            ch.eq[0].setHighPass (currentSampleRate, hp, 0.707);
            ch.eq[3].setLowPass  (currentSampleRate, lp, 0.707);
        }
    }

    for (int b = 1; b <= 2; ++b)
    {
        const float ef = apvts.getRawParameterValue ("eq" + juce::String (b + 1) + "f")->load();
        const float eg = apvts.getRawParameterValue ("eq" + juce::String (b + 1) + "g")->load();

        if (std::abs (eg) > 0.05f)
            eqActive = true;

        for (auto& ch : chans)
            ch.eq[b].setPeaking (currentSampleRate, ef, 0.9, eg);
    }

    //--- transient shaper ----------------------------------------------------
    const float atkAmt = apvts.getRawParameterValue ("attack")->load() * 0.01f;
    const float susAmt = apvts.getRawParameterValue ("sustain")->load() * 0.01f;

    for (auto& ch : chans)
    {
        ch.envFast.setTimes (currentSampleRate, 0.5f, 35.0f);
        ch.envSlow.setTimes (currentSampleRate, 8.0f, 120.0f);
        ch.envTail.setTimes (currentSampleRate, 40.0f, 420.0f);
    }

    //--- run -----------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& s = chans[ch];
            float* d = buffer.getWritePointer (ch);
            const float x = d[i];

            const float dry = s.dryDelay[(size_t) s.dryPos];
            s.dryDelay[(size_t) s.dryPos] = x;
            if (++s.dryPos >= fftSize) s.dryPos = 0;

            s.inRing[(size_t) s.inPos] = x;
            s.inPos = (s.inPos + 1) & (fftSize - 1);

            float wet = s.hopOut[(size_t) s.outIndex];

            if (fmtAmt > 0.001f)
            {
                float fw = wet;
                fw = s.fmt[0].process (fw);
                fw = s.fmt[1].process (fw);
                fw = s.fmt[2].process (fw);
                wet += (fw - wet) * fmtAmt;
            }

            if (++s.outIndex >= hopSize)
            {
                s.outIndex = 0;
                renderFrame (ch, ratios, numVoices);
            }

            if (eqActive)
                for (int b = 0; b < 4; ++b)
                    wet = s.eq[b].process (wet);

            if (drive > 0.001f)
            {
                const float pre = 1.0f + drive * 8.0f;
                wet = std::tanh (wet * pre) / std::sqrt (pre);
            }

            // transient detection on the wet signal (what the user hears)
            s.envFast.process (wet);
            s.envSlow.process (wet);
            s.envTail.process (wet);

            const float slow  = s.envSlow.y;
            const float trans = juce::jlimit (0.0f, 1.0f,
                                              (s.envFast.y - slow) / (slow + 1.0e-4f) * 2.5f);
            const float tail  = juce::jlimit (0.0f, 1.0f,
                                              (slow - s.envTail.y) / (s.envTail.y + 1.0e-4f) * 2.0f);

            // transient shaping (wet only)
            const float shaperDb = atkAmt * 14.0f * trans + susAmt * 7.0f * tail;

            if (std::abs (shaperDb) > 0.02f)
                wet *= juce::Decibels::decibelsToGain (shaperDb);

            // positive attack keeps the original transient un-snapped
            const float lock   = 0.45f * juce::jmax (0.0f, atkAmt);
            const float mixEff = mix * (1.0f - lock * trans);

            d[i] = dry + (wet * outGain - dry) * mixEff;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* HyperareaAudioProcessor::createEditor()
{
    return new HyperareaAudioProcessorEditor (*this);
}

//==============================================================================
void HyperareaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HyperareaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperareaAudioProcessor();
}
