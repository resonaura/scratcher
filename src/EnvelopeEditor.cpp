#include "EnvelopeEditor.h"
#include <algorithm>
#include <cmath>

//==============================================================================
// EnvelopePattern
//==============================================================================
float EnvelopePattern::evaluate(float phase) const noexcept
{
    if (points.empty()) return 0.f;
    if (points.size() == 1) return points[0].y;
    phase = std::clamp(phase, 0.f, 1.f);

    // Find surrounding points
    int i1 = 0;
    for (int i = 0; i < (int)points.size() - 1; ++i)
    {
        if (phase <= points[i + 1].x) { i1 = i; break; }
        i1 = (int)points.size() - 2;
    }

    const auto& p0 = points[i1];
    const auto& p1 = points[i1 + 1];
    float t = (p1.x - p0.x) < 1e-6f ? 0.f
              : (phase - p0.x) / (p1.x - p0.x);

    // Catmull-Rom / tension spline
    // Get neighbouring points for tangent computation
    const auto& pm = (i1 > 0) ? points[i1 - 1] : p0;
    const auto& p2 = (i1 + 2 < (int)points.size()) ? points[i1 + 2] : p1;

    float tension = (p0.tension + p1.tension) * 0.5f;
    float scale   = 1.0f - tension;

    float m0 = scale * (p1.y - pm.y);
    float m1 = scale * (p2.y - p0.y);

    // Hermite basis
    float t2 = t * t, t3 = t2 * t;
    float h00 =  2*t3 - 3*t2 + 1;
    float h10 =    t3 - 2*t2 + t;
    float h01 = -2*t3 + 3*t2;
    float h11 =    t3 -   t2;

    return std::clamp(h00 * p0.y + h10 * m0 + h01 * p1.y + h11 * m1, 0.f, 1.f);
}

float EnvelopePattern::evaluateDerivative(float phase) const noexcept
{
    const float eps = 0.001f;
    return (evaluate(phase + eps) - evaluate(phase - eps)) / (2.f * eps);
}

void EnvelopePattern::addPoint(float x, float y, float tension)
{
    if ((int)points.size() >= MAX_POINTS) return;
    points.push_back({ std::clamp(x, 0.f, 1.f),
                       std::clamp(y, 0.f, 1.f),
                       std::clamp(tension, -1.f, 1.f) });
    sortPoints();
}

void EnvelopePattern::removePoint(int index)
{
    if (index >= 0 && index < (int)points.size() && points.size() > 2)
        points.erase(points.begin() + index);
}

void EnvelopePattern::sortPoints()
{
    std::sort(points.begin(), points.end(),
              [](const EnvelopePoint& a, const EnvelopePoint& b){ return a.x < b.x; });
}

void EnvelopePattern::saveToXml(juce::XmlElement& parent, const juce::String& tag) const
{
    auto* node = parent.createNewChildElement(tag);
    node->setAttribute("name", name);
    for (auto& p : points)
    {
        auto* e = node->createNewChildElement("P");
        e->setAttribute("x", p.x);
        e->setAttribute("y", p.y);
        e->setAttribute("t", p.tension);
    }
}

void EnvelopePattern::loadFromXml(const juce::XmlElement& parent, const juce::String& tag)
{
    auto* node = parent.getChildByName(tag);
    if (!node) return;
    name = node->getStringAttribute("name");
    points.clear();
    for (auto* e : node->getChildIterator())
    {
        EnvelopePoint p;
        p.x       = static_cast<float>(e->getDoubleAttribute("x", 0.0));
        p.y       = static_cast<float>(e->getDoubleAttribute("y", 0.0));
        p.tension = static_cast<float>(e->getDoubleAttribute("t", 0.0));
        points.push_back(p);
    }
    if (points.empty()) setToFlat();
}

//==============================================================================
// Built-in presets
//==============================================================================
EnvelopePattern EnvelopePattern::makeNormal()
{
    EnvelopePattern p; p.name = "Normal"; p.setToFlat(0.f); return p;
}
EnvelopePattern EnvelopePattern::makeTapeStop()
{
    EnvelopePattern p; p.name = "Tape Stop";
    p.points = {{ 0.f, 0.f, 0.f }, { 0.5f, 0.5f, 0.f }, { 1.f, 1.f, 0.f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeReverse()
{
    EnvelopePattern p; p.name = "Reverse";
    p.points = {{ 0.f, 1.f, 0.f }, { 1.f, 0.f, 0.f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeStutter(int divisor)
{
    EnvelopePattern p; p.name = "Stutter 1/" + juce::String(divisor);
    p.points.clear();
    float step = 1.f / divisor;
    for (int i = 0; i < divisor; ++i)
    {
        p.points.push_back({ i * step,         0.f, 0.f });
        p.points.push_back({ i * step + step * 0.99f, 0.f, 0.f });
    }
    p.points.push_back({ 1.f, 0.f, 0.f });
    return p;
}
EnvelopePattern EnvelopePattern::makeScratch()
{
    EnvelopePattern p; p.name = "Vinyl Scratch";
    p.points = {{ 0.f, 0.5f, -0.5f }, { 0.25f, 0.f, -0.5f },
                { 0.5f, 0.5f, -0.5f }, { 0.75f, 1.f, -0.5f },
                { 1.f,  0.5f, -0.5f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeForwardFast()
{
    EnvelopePattern p; p.name = "Forward x2";
    p.points = {{ 0.f, 0.5f, 0.f }, { 1.f, 0.f, 0.f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeBabyScratch()
{
    EnvelopePattern p; p.name = "Baby Scratch";
    p.points.clear();
    for (int i = 0; i < 4; ++i)
    {
        p.points.push_back({ i * 0.25f,           0.25f, 0.f });
        p.points.push_back({ i * 0.25f + 0.125f,  0.75f, 0.f });
    }
    p.points.push_back({ 1.f, 0.25f, 0.f });
    return p;
}
EnvelopePattern EnvelopePattern::makeVolFull()
{
    EnvelopePattern p; p.name = "Full"; p.setToFlat(1.f); return p;
}
EnvelopePattern EnvelopePattern::makeVolGate(int divisor)
{
    EnvelopePattern p; p.name = "Gate 1/" + juce::String(divisor);
    float step = 1.f / divisor;
    for (int i = 0; i < divisor; ++i)
    {
        p.points.push_back({ i * step,           1.f, 0.f });
        p.points.push_back({ i * step + step * 0.5f, 0.f, 0.f });
        p.points.push_back({ i * step + step * 0.5f + 0.001f, 0.f, 0.f });
    }
    p.points.push_back({ 1.f, 1.f, 0.f });
    return p;
}
EnvelopePattern EnvelopePattern::makeVolTranceGate()
{
    EnvelopePattern p; p.name = "Trance Gate";
    p.points = {{ 0.f, 1.f, 0.f },   { 0.125f, 1.f, 0.f },
                { 0.126f, 0.f, 0.f }, { 0.25f, 0.f, 0.f },
                { 0.251f, 1.f, 0.f }, { 0.375f, 1.f, 0.f },
                { 0.376f, 0.f, 0.f }, { 0.5f,  0.f, 0.f },
                { 0.501f, 1.f, 0.f }, { 0.625f, 1.f, 0.f },
                { 0.626f, 0.f, 0.f }, { 0.875f, 0.f, 0.f },
                { 0.876f, 1.f, 0.f }, { 1.f,  1.f, 0.f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeVolFlare()
{
    EnvelopePattern p; p.name = "Flare Cut";
    p.points = {{ 0.f,  1.f, 0.f }, { 0.49f, 1.f, 0.f },
                { 0.5f, 0.f, 0.f }, { 0.51f, 0.f, 0.f },
                { 0.99f, 1.f, 0.f }, { 1.f, 1.f, 0.f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeVolCrab()
{
    EnvelopePattern p; p.name = "Crab";
    float step = 1.f / 16.f;
    bool on = true;
    for (int i = 0; i < 16; ++i)
    {
        p.points.push_back({ i * step,         on ? 1.f : 0.f, 0.f });
        p.points.push_back({ (i+1) * step - 0.001f, on ? 1.f : 0.f, 0.f });
        if (i % 4 != 1) on = !on;
    }
    p.points.push_back({ 1.f, 1.f, 0.f });
    return p;
}
EnvelopePattern EnvelopePattern::makeVolFadeIn()
{
    EnvelopePattern p; p.name = "Fade In";
    p.points = {{ 0.f, 0.f, -0.5f }, { 1.f, 1.f, -0.5f }};
    return p;
}
EnvelopePattern EnvelopePattern::makeVolFadeOut()
{
    EnvelopePattern p; p.name = "Fade Out";
    p.points = {{ 0.f, 1.f, -0.5f }, { 1.f, 0.f, -0.5f }};
    return p;
}

//==============================================================================
// EnvelopeEditorComponent
//==============================================================================
EnvelopeEditorComponent::EnvelopeEditorComponent()
{
    setOpaque(true);
}

void EnvelopeEditorComponent::setPattern(EnvelopePattern* p, bool isTime)
{
    pattern   = p;
    isTimeEnv = isTime;
    repaint();
}

void EnvelopeEditorComponent::setSafetyLineSamples(int totalWritten, int bufferSize)
{
    if (bufferSize > 0)
        safetyLineY = 1.f - (float)totalWritten / bufferSize;
    else
        safetyLineY = 0.f;
    repaint();
}

void EnvelopeEditorComponent::setHostPosition(float barPhase)
{
    hostPhase = barPhase;
    repaint();
}

juce::Point<float> EnvelopeEditorComponent::toScreen(float x, float y) const noexcept
{
    auto b = getLocalBounds().reduced(4).toFloat();
    return { b.getX() + x * b.getWidth(),
             b.getY() + (1.f - y) * b.getHeight() };
}

juce::Point<float> EnvelopeEditorComponent::fromScreen(float sx, float sy) const noexcept
{
    auto b = getLocalBounds().reduced(4).toFloat();
    return { std::clamp((sx - b.getX()) / b.getWidth(),  0.f, 1.f),
             std::clamp(1.f - (sy - b.getY()) / b.getHeight(), 0.f, 1.f) };
}

int EnvelopeEditorComponent::findNearestPoint(juce::Point<float> sp, float radius) const
{
    if (!pattern) return -1;
    float best = radius * radius;
    int   idx  = -1;
    for (int i = 0; i < (int)pattern->points.size(); ++i)
    {
        auto& p = pattern->points[i];
        auto  s = toScreen(p.x, p.y);
        float d = s.getDistanceSquaredFrom(sp);
        if (d < best) { best = d; idx = i; }
    }
    return idx;
}

void EnvelopeEditorComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(bgColour);

    // ── Grid ──────────────────────────────────────────────────────────────────
    g.setColour(gridColour);
    // Vertical lines at each beat (quarter note)
    for (int q = 1; q < 4; ++q)
    {
        float x = b.getX() + (float)q / 4.f * b.getWidth();
        g.drawLine(x, b.getY(), x, b.getBottom(), 1.f);
    }
    // Horizontal lines
    for (int row = 1; row < 4; ++row)
    {
        float y = b.getY() + (float)row / 4.f * b.getHeight();
        g.drawLine(b.getX(), y, b.getRight(), y, 1.f);
    }

    // ── Safety line (time envelope only) ──────────────────────────────────────
    if (isTimeEnv && safetyLineY < 1.f)
    {
        g.setColour(safeColour);
        float sy = b.getY() + safetyLineY * b.getHeight();
        g.drawLine(b.getX(), sy, b.getRight(), sy, 2.f);
        // Danger zone shading above safety line
        g.fillRect(juce::Rectangle<float>(b.getX(), b.getY(),
                                          b.getWidth(), sy - b.getY()));
    }

    // ── Envelope curve ────────────────────────────────────────────────────────
    if (pattern && pattern->points.size() >= 2)
    {
        juce::Path path;
        const int steps = 200;
        bool first = true;
        for (int i = 0; i <= steps; ++i)
        {
            float phase = (float)i / steps;
            float val   = pattern->evaluate(phase);
            auto  pt    = toScreen(phase, val);
            if (first) { path.startNewSubPath(pt); first = false; }
            else        path.lineTo(pt);
        }
        // Glow
        g.setColour(lineColour.withAlpha(0.3f));
        juce::PathStrokeType stroke3(3.f);
        g.strokePath(path, stroke3);
        g.setColour(lineColour);
        juce::PathStrokeType stroke1(1.5f);
        g.strokePath(path, stroke1);

        // Control points
        for (auto& p : pattern->points)
        {
            auto pt = toScreen(p.x, p.y);
            g.setColour(pointColour);
            g.fillEllipse(pt.x - 4, pt.y - 4, 8, 8);
            g.setColour(lineColour);
            g.drawEllipse(pt.x - 4, pt.y - 4, 8, 8, 1.5f);
        }
    }

    // ── Playhead ──────────────────────────────────────────────────────────────
    if (hostPhase >= 0.f && hostPhase <= 1.f)
    {
        float px = b.getX() + hostPhase * b.getWidth();
        g.setColour(juce::Colours::yellow.withAlpha(0.8f));
        g.drawLine(px, b.getY(), px, b.getBottom(), 1.f);
    }
}

void EnvelopeEditorComponent::resized() {}

void EnvelopeEditorComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!pattern) return;
    draggingPoint = findNearestPoint(e.position);

    if (editMode == EditMode::Draw && draggingPoint < 0)
    {
        auto np = fromScreen(e.position.x, e.position.y);
        pattern->addPoint(np.x, np.y);
        draggingPoint = findNearestPoint(e.position);
        if (onPatternChanged) onPatternChanged();
    }
    repaint();
}

void EnvelopeEditorComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!pattern || draggingPoint < 0) return;
    auto np = fromScreen(e.position.x, e.position.y);
    pattern->points[draggingPoint].x = np.x;
    pattern->points[draggingPoint].y = np.y;
    pattern->sortPoints();
    repaint();
    if (onPatternChanged) onPatternChanged();
}

void EnvelopeEditorComponent::mouseUp(const juce::MouseEvent&)
{
    draggingPoint = -1;
}

void EnvelopeEditorComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!pattern) return;
    int idx = findNearestPoint(e.position);
    if (idx >= 0) { pattern->removePoint(idx); repaint(); }
    if (onPatternChanged) onPatternChanged();
}
