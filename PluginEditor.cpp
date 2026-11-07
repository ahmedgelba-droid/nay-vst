#include "PluginEditor.h"

//==============================================================================
void NayLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();

    juce::ColourGradient grad (juce::Colour (0xffffe34d), r.getX(), r.getY(),
                               juce::Colour (0xffe8a800), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 7.0f);

    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 7.0f, 1.5f);

    // small "window" at the top: grey = natural, blue = lowered by a quarter tone
    auto slot = juce::Rectangle<float> (r.getWidth() * 0.55f, 9.0f)
                    .withCentre ({ r.getCentreX(), r.getY() + 14.0f });
    g.setColour (on ? juce::Colour (0xff2f8bff) : juce::Colour (0xff8a8a8a));
    g.fillRoundedRectangle (slot, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (slot, 2.0f, 1.0f);

    if (highlighted || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.3f : 0.15f));
        g.fillRoundedRectangle (r, 7.0f);
    }

    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (b.getButtonText(), r.withTrimmedTop (r.getHeight() * 0.55f), juce::Justification::centred);
}

//==============================================================================
NayAudioProcessorEditor::NayAudioProcessorEditor (NayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (540, 400);

    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    for (int i = 0; i < 12; ++i)
    {
        keys[(size_t) i].setButtonText (names[i]);
        addAndMakeVisible (keys[(size_t) i]);
        keyAtt[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, "qt" + juce::String (i), keys[(size_t) i]);
    }

    struct KD { const char* id; const char* name; };
    static const KD kd[7] = { { "glide", "Glide" },        { "vibDepth", "Vib Depth" },
                              { "vibRate", "Vib Speed" },  { "tune", "Tune Amt" },
                              { "attack", "Attack" },      { "release", "Release" },
                              { "gain", "Gain" } };

    for (size_t k = 0; k < 7; ++k)
    {
        auto& kn = knobs[k];
        kn.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        kn.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
        kn.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        kn.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        kn.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffffd400));
        kn.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.6f));
        kn.slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible (kn.slider);

        kn.label.setText (kd[k].name, juce::dontSendNotification);
        kn.label.setJustificationType (juce::Justification::centred);
        kn.label.setColour (juce::Label::textColourId, juce::Colours::white);
        kn.label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        kn.label.attachToComponent (&kn.slider, false);

        kn.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kd[k].id, kn.slider);
    }

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Select the folder with your Nay samples", juce::File(), "");
        juce::Component::SafePointer<NayAudioProcessorEditor> safe (this);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;

                                  const auto dir = fc.getResult();
                                  if (dir.isDirectory())
                                  {
                                      safe->proc.loadSampleFolder (dir);
                                      safe->info.setText (safe->proc.getSampleInfo(), juce::dontSendNotification);
                                  }
                              });
    };

    info.setText (proc.getSampleInfo(), juce::dontSendNotification);
    info.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (info);
}

NayAudioProcessorEditor::~NayAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void NayAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight();

    // colourful background like the reference
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1fbf4a), 0.0f, H,
                                             juce::Colour (0xff8a2be2), W, 0.0f, false));
    g.fillAll();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xccff8a00), W * 0.1f, H * 0.1f,
                                             juce::Colours::transparentBlack, W * 0.5f, H * 0.5f, true));
    g.fillAll();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xaaff3aa0), W * 0.9f, H * 0.2f,
                                             juce::Colours::transparentBlack, W * 0.5f, H * 0.4f, true));
    g.fillAll();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xaa00d0ff), W * 0.4f, H * 0.95f,
                                             juce::Colours::transparentBlack, W * 0.4f, H * 0.55f, true));
    g.fillAll();

    // frame
    g.setColour (juce::Colour (0xffb01820));
    g.drawRect (b, 6.0f);
    g.setColour (juce::Colour (0xffff7a30).withAlpha (0.8f));
    g.drawRect (b.reduced (6.0f), 1.5f);

    // title
    const auto titleArea = juce::Rectangle<float> (0.0f, 8.0f, W, 62.0f);
    g.setFont (juce::Font (juce::FontOptions (54.0f, juce::Font::bold | juce::Font::italic)));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawText ("NAY", titleArea.translated (3.0f, 3.0f), juce::Justification::centred);
    g.setColour (juce::Colour (0xffe01020));
    g.drawText ("NAY", titleArea, juce::Justification::centred);
}

void NayAudioProcessorEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    const int bw = 50, bh = 58, step = 62;
    const int x0 = (w - (6 * step + bw)) / 2;
    const int topY = 78, botY = topY + bh + 10;

    // bottom row: 7 natural notes
    static const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int i = 0; i < 7; ++i)
        keys[(size_t) whitePc[i]].setBounds (x0 + i * step, botY, bw, bh);

    // top row: 5 sharps grouped 2 + 3, sitting between the naturals
    static const int blackPc[5]    = { 1, 3, 6, 8, 10 };
    static const int blackAfter[5] = { 0, 1, 3, 4, 5 };
    for (int j = 0; j < 5; ++j)
        keys[(size_t) blackPc[j]].setBounds (x0 + blackAfter[j] * step + step / 2, topY, bw, bh);

    const int knobY = botY + bh + 40;
    for (int k = 0; k < 7; ++k)
        knobs[(size_t) k].slider.setBounds (x0 + k * step - 4, knobY, bw + 8, 84);

    loadButton.setBounds (x0, h - 42, 150, 26);
    info.setBounds (x0 + 162, h - 42, w - x0 * 2 - 162, 26);
}
