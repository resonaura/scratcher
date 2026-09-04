#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>

//==============================================================================
// Envelope data structures and editor component for Gross Beat-style
// time and volume manipulation.
//==============================================================================

struct EnvelopePoint
{
    float x = 0.f;       // position in bar [0, 1]
    float y = 0.f;       // value [0, 1]
    float tension = 0.f; // curve tension [-1, 1]
};

//==============================================================================
class EnvelopePattern
{
public:
    static constexpr int MAX_POINTS = 64;

    juce::String name;
    std::vector<EnvelopePoint> points;

    EnvelopePattern() { setToFlat(); }

    void setToFlat(float value = 0.f)
    {
        points.clear();
        points.push_back({ 0.f, value, 0.f });
        points.push_back({ 1.f, value, 0.f });
    }

    // Evaluate at phase [0, 1] using Catmull-Rom spline with tension.
    float evaluate(float phase) const noexcept;

    // Derivative at phase (determines playback speed in time envelope).
    float evaluateDerivative(float phase) const noexcept;

    void addPoint(float x, float y, float tension = 0.f);
    void removePoint(int index);
    void sortPoints();

    void saveToXml(juce::XmlElement& parent, const juce::String& tag) const;
    void loadFromXml(const juce::XmlElement& parent, const juce::String& tag);

    // Factory: built-in presets
    static EnvelopePattern makeNormal();
    static EnvelopePattern makeTapeStop();
    static EnvelopePattern makeReverse();
    static EnvelopePattern makeStutter(int divisor); // 8 or 16
    static EnvelopePattern makeScratch();
    static EnvelopePattern makeForwardFast();
    static EnvelopePattern makeBabyScratch();

    static EnvelopePattern makeVolFull();
    static EnvelopePattern makeVolGate(int divisor);
    static EnvelopePattern makeVolTranceGate();
    static EnvelopePattern makeVolFlare();
    static EnvelopePattern makeVolCrab();
    static EnvelopePattern makeVolFadeIn();
    static EnvelopePattern makeVolFadeOut();
};

//==============================================================================
// EnvelopeEditorComponent — the visual editor inside the plugin GUI.
// Displays and allows editing of a single EnvelopePattern.
//==============================================================================
class EnvelopeEditorComponent : public juce::Component
{
public:
    enum class EditMode { Draw, Select, Line };

    std::function<void()> onPatternChanged; // called after user edits

    EnvelopeEditorComponent();

    void setPattern(EnvelopePattern* p, bool isTimeEnvelope);
    void setSafetyLineSamples(int totalWritten, int bufferSize);
    void setHostPosition(float barPhase); // 0..1, draws playhead cursor

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    EditMode getEditMode() const { return editMode; }
    void     setEditMode(EditMode m) { editMode = m; }

private:
    EnvelopePattern* pattern = nullptr;
    bool isTimeEnv = true;
    float safetyLineY = 1.f;  // normalised y of safety line
    float hostPhase   = 0.f;
    EditMode editMode = EditMode::Draw;

    int    draggingPoint = -1;
    juce::Point<float> dragStart;

    juce::Point<float> toScreen(float x, float y) const noexcept;
    juce::Point<float> fromScreen(float sx, float sy) const noexcept;
    int findNearestPoint(juce::Point<float> screenPos, float radius = 10.f) const;

    juce::Colour bgColour    { 0xff1a1a2e };
    juce::Colour gridColour  { 0xff2a2a4e };
    juce::Colour lineColour  { 0xff4a90ff };
    juce::Colour pointColour { 0xffffffff };
    juce::Colour safeColour  { 0x40ff4444 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeEditorComponent)
};
