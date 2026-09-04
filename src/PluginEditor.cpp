#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>
#include <algorithm>

//==============================================================================
// Fonts helper
//==============================================================================
static juce::Font makePixelFont(float size)
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PixelEmulator_ttf, BinaryData::PixelEmulator_ttfSize);
    if (tf) return juce::Font(juce::FontOptions().withTypeface(tf).withHeight(size));
    return juce::Font(juce::FontOptions().withHeight(size));
}

static juce::Font makeUiFont(float size)
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PixgamerRegular_ttf, BinaryData::PixgamerRegular_ttfSize);
    if (tf) return juce::Font(juce::FontOptions().withTypeface(tf).withHeight(size));
    return juce::Font(juce::FontOptions().withHeight(size));
}

//==============================================================================
// ScratcherLAF
//==============================================================================
ScratcherLAF::ScratcherLAF()
{
    pixelFont = makePixelFont(12.f);
    uiFont    = makeUiFont(11.f);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff000000));
}

juce::Font ScratcherLAF::getTextButtonFont(juce::TextButton&, int h)
{
    return makeUiFont(std::min(11.f, (float)h * 0.55f));
}

juce::Font ScratcherLAF::getLabelFont(juce::Label&)
{
    return makeUiFont(10.f);
}

void ScratcherLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                        const juce::Colour& /*bg*/,
                                        bool isOver, bool isDown)
{
    auto b  = btn.getLocalBounds().toFloat().reduced(0.5f);
    bool on = btn.getToggleState();
    auto col = on  ? theme.accent.withAlpha(0.25f) :
               isDown ? theme.accent.withAlpha(0.15f) :
               isOver  ? theme.groove.brighter(0.3f) : theme.groove;
    g.setColour(col);
    g.fillRoundedRectangle(b, 3.f);
    g.setColour(on ? theme.accent : (isOver ? theme.textDim : theme.bgPanel));
    g.drawRoundedRectangle(b, 3.f, 1.f);
}

void ScratcherLAF::drawButtonText(juce::Graphics& g, juce::TextButton& btn,
                                  bool isOver, bool /*isDown*/)
{
    auto col = btn.getToggleState() ? theme.accent :
               isOver ? theme.textBright : theme.textDim;
    g.setColour(col);
    g.setFont(makeUiFont(std::min(11.f, (float)btn.getHeight() * 0.55f)));
    g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred);
}

void ScratcherLAF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                    float sliderPos, float minPos, float maxPos,
                                    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused(minPos, maxPos, style);
    auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h);
    // Track
    g.setColour(theme.bgDark);
    g.fillRoundedRectangle(b, 3.f);
    // Fill
    float fillW = sliderPos - b.getX();
    g.setColour(theme.accent.withAlpha(0.7f));
    g.fillRoundedRectangle(b.getX(), b.getY(), fillW, b.getHeight(), 3.f);
    // Border
    g.setColour(theme.groove);
    g.drawRoundedRectangle(b, 3.f, 1.f);
    // Value text
    g.setColour(theme.textBright);
    g.setFont(makeUiFont(std::min(10.f, b.getHeight() * 0.7f)));
    g.drawText(slider.getTextFromValue(slider.getValue()),
               b.toNearestInt(), juce::Justification::centred);
}

void ScratcherLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                    float sliderPos, float startAngle, float endAngle,
                                    juce::Slider& /*slider*/)
{
    auto b      = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h);
    auto centre = b.getCentre();
    float r     = std::min(b.getWidth(), b.getHeight()) * 0.42f;
    float angle = startAngle + sliderPos * (endAngle - startAngle);
    float thick = std::max(2.f, r * 0.15f);

    // Background arc
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, r, r, 0.f, startAngle, endAngle, true);
    juce::PathStrokeType stroke(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(theme.bgDark);
    g.strokePath(bgArc, stroke);

    // Value arc
    juce::Path valArc;
    valArc.addCentredArc(centre.x, centre.y, r, r, 0.f, startAngle, angle, true);
    g.setColour(theme.accent);
    g.strokePath(valArc, stroke);

    // Centre dot
    g.setColour(theme.textBright);
    g.fillEllipse(centre.x - 2.f, centre.y - 2.f, 4.f, 4.f);
}

void ScratcherLAF::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                int, int, int, int, juce::ComboBox& box)
{
    auto b = box.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(theme.bgPanel);
    g.fillRoundedRectangle(b, 3.f);
    g.setColour(theme.groove);
    g.drawRoundedRectangle(b, 3.f, 1.f);
    // Arrow
    float ax = w - 14.f, ay = h * 0.5f - 3.f;
    juce::Path arrow;
    arrow.addTriangle(ax, ay, ax + 8.f, ay, ax + 4.f, ay + 6.f);
    g.setColour(theme.textDim);
    g.fillPath(arrow);
}

//==============================================================================
// ScratcherVuMeter
//==============================================================================
void ScratcherVuMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff000000));

    const int NUM_SEGS = 24;
    float segH = b.getHeight() / NUM_SEGS;

    for (int i = 0; i < NUM_SEGS; ++i)
    {
        float normI = (float)(NUM_SEGS - 1 - i) / NUM_SEGS;
        float y = b.getY() + i * segH;
        juce::Colour col = (normI > 0.8f) ? juce::Colours::red :
                           (normI > 0.6f) ? juce::Colours::yellow : barColour;
        bool lit = level >= normI;
        g.setColour(lit ? col : col.withAlpha(0.1f));
        g.fillRect(b.getX() + 1, y + 1, b.getWidth() - 2, segH - 2);
        if (lit)
        {
            g.setColour(col.withAlpha(0.4f));
            g.fillRect(b.getX() + 1, y + 1, b.getWidth() - 2, segH - 2);
        }
    }
    g.setColour(juce::Colour(0x20ffffff));
    g.drawRect(b, 1.f);
}

//==============================================================================
// WaveformOverview
//==============================================================================
void WaveformOverview::setWaveformPeaks(const std::vector<float>& p)
{
    peaks = p;
    repaint();
}

void WaveformOverview::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff000000));

    if (peaks.empty()) {
        g.setColour(juce::Colour(0xff0d0d0d));
        g.drawText("No sample loaded", b.toNearestInt(), juce::Justification::centred);
        return;
    }

    // Loop zone
    float ls = loopStart * b.getWidth();
    float le = loopEnd   * b.getWidth();
    g.setColour(waveColour.withAlpha(0.12f));
    g.fillRect(b.getX() + ls, b.getY(), le - ls, b.getHeight());

    // Trim region overlay — dim outside trim, mark boundaries
    float ts = trimStart * b.getWidth();
    float te = trimEnd   * b.getWidth();
    if (ts > 0.f)
    {
        g.setColour(juce::Colour(0x88000000));
        g.fillRect(b.getX(), b.getY(), ts, b.getHeight());
    }
    if (te < b.getWidth())
    {
        g.setColour(juce::Colour(0x88000000));
        g.fillRect(b.getX() + te, b.getY(), b.getWidth() - te, b.getHeight());
    }
    // Trim start handle (blue) — always visible, thicker when dragging
    {
        bool active = (draggingHandle == 1);
        g.setColour(active ? juce::Colour(0xff3772ff) : juce::Colour(0xff3772ff).withAlpha(0.55f));
        float lx = b.getX() + ts;
        g.drawLine(lx, b.getY(), lx, b.getBottom(), active ? 3.f : 2.f);
        // Grip diamond
        float cy = b.getCentreY();
        juce::Path diamond;
        diamond.addTriangle(lx - 5.f, cy, lx, cy - 7.f, lx + 5.f, cy);
        diamond.addTriangle(lx - 5.f, cy, lx, cy + 7.f, lx + 5.f, cy);
        g.fillPath(diamond);
    }
    // Trim end handle (yellow) — always visible
    {
        bool active = (draggingHandle == 2);
        g.setColour(active ? juce::Colour(0xfffdca40) : juce::Colour(0xfffdca40).withAlpha(0.55f));
        float lx = b.getX() + te;
        g.drawLine(lx, b.getY(), lx, b.getBottom(), active ? 3.f : 2.f);
        float cy = b.getCentreY();
        juce::Path diamond;
        diamond.addTriangle(lx - 5.f, cy, lx, cy - 7.f, lx + 5.f, cy);
        diamond.addTriangle(lx - 5.f, cy, lx, cy + 7.f, lx + 5.f, cy);
        g.fillPath(diamond);
    }

    // Waveform
    float midY = b.getCentreY();
    float scaleH = b.getHeight() * 0.45f;
    g.setColour(waveColour.withAlpha(0.8f));
    for (int i = 0; i < (int)peaks.size(); ++i)
    {
        float x  = b.getX() + (float)i / peaks.size() * b.getWidth();
        float hw = peaks[(size_t)i] * scaleH;
        g.fillRect(x, midY - hw, 1.5f, hw * 2.f);
    }

    // Playhead
    float ph = b.getX() + playheadNorm * b.getWidth();
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawLine(ph, b.getY(), ph, b.getBottom(), 1.5f);

    g.setColour(juce::Colour(0x30ffffff));
    g.drawRect(b, 1.f);
}

float WaveformOverview::handleAt(float x) const
{
    float w = (float)getWidth();
    float ts = trimStart * w;
    float te = trimEnd   * w;
    if (std::abs(x - ts) < 8.f) return 1;
    if (std::abs(x - te) < 8.f) return 2;
    return 0;
}

void WaveformOverview::mouseMove(const juce::MouseEvent& e)
{
    int h = (int)handleAt((float)e.x);
    if (h == 1 || h == 2)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void WaveformOverview::mouseDown(const juce::MouseEvent& e)
{
    draggingHandle = (int)handleAt((float)e.x);
    if (draggingHandle == 0 && onSeek)
    {
        float norm = std::clamp(e.position.x / getWidth(), 0.f, 1.f);
        onSeek(norm);
    }
}

void WaveformOverview::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingHandle == 0) return;
    float norm = std::clamp(e.position.x / (float)getWidth(), 0.f, 1.f);
    if (draggingHandle == 1)
    {
        trimStart = std::min(norm, trimEnd - 0.01f);
        if (onTrimChanged) onTrimChanged(trimStart, trimEnd);
    }
    else if (draggingHandle == 2)
    {
        trimEnd = std::max(norm, trimStart + 0.01f);
        if (onTrimChanged) onTrimChanged(trimStart, trimEnd);
    }
    repaint();
}

void WaveformOverview::mouseUp(const juce::MouseEvent& /*e*/)
{
    draggingHandle = 0;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

//==============================================================================
// VinylComponent
//==============================================================================
VinylComponent::VinylComponent()
{
    startTimerHz(30);  // 30 fps — smooth enough, half the CPU cost vs 60fps
}

VinylComponent::~VinylComponent()
{
    stopTimer();
}

void VinylComponent::buildVinylImage()
{
    int sz = getWidth();
    if (sz < 8) return;
    vinylImage = juce::Image(juce::Image::ARGB, sz, sz, true);
    juce::Graphics ig(vinylImage);

    auto centre  = juce::Point<float>(sz * 0.5f, sz * 0.5f);
    float outerR = sz * 0.49f;

    // Main vinyl disc (dark)
    ig.setColour(juce::Colour(0xff111111));
    ig.fillEllipse(centre.x - outerR, centre.y - outerR, outerR * 2, outerR * 2);

    // Groove rings
    for (float r = outerR * 0.35f; r < outerR * 0.97f; r += 2.5f)
    {
        ig.setColour(juce::Colour(0xff1a1a1a));
        ig.drawEllipse(centre.x - r, centre.y - r, r * 2, r * 2, 1.0f);
    }

    // Outer rim
    ig.setColour(juce::Colour(0xff333333));
    ig.drawEllipse(centre.x - outerR, centre.y - outerR, outerR * 2, outerR * 2, 1.5f);

    // ── Centre label (ROTATES with the vinyl) — black disc + coloured border ──
    float labelR = outerR * 0.30f;
    // Black fill
    ig.setColour(juce::Colours::black);
    ig.fillEllipse(centre.x - labelR, centre.y - labelR, labelR * 2, labelR * 2);
    // Deck-colour border
    ig.setColour(deckColour.withAlpha(0.95f));
    ig.drawEllipse(centre.x - labelR, centre.y - labelR, labelR * 2, labelR * 2, 1.8f);
    // Text in deck colour
    ig.setColour(deckColour.brighter(0.3f));
    ig.setFont(makeUiFont(std::max(7.f, labelR * 0.38f)));
    ig.drawText("RSNRA",
                (int)(centre.x - labelR), (int)(centre.y - 9),
                (int)(labelR * 2), 18, juce::Justification::centred);

    // ── Rotation marker: vertical sticker ON the label, at its 12-o'clock edge ──
    {
        // Narrow width, tall height → clearly vertical, like a real sticker tab
        float sHalf  = outerR * 0.036f;   // half-width  (narrow)
        float tHalf  = outerR * 0.092f;   // half-height (tall)
        // Centre sits just inside the label top edge
        float sCX = centre.x;
        float sCY = centre.y - (labelR - tHalf * 0.55f);  // overlaps label edge slightly
        float sx = sCX - sHalf;
        float sy = sCY - tHalf;
        float sw = sHalf * 2.f;
        float sh = tHalf * 2.f;

        // Soft glow halos (drawn before fill so they appear behind)
        for (int layer = 4; layer >= 1; --layer)
        {
            float ex = layer * 1.8f;
            ig.setColour(deckColour.withAlpha(0.06f * (float)(5 - layer)));
            ig.fillRoundedRectangle(sx - ex, sy - ex, sw + ex * 2.f, sh + ex * 2.f, 2.5f);
        }
        // Black fill
        ig.setColour(juce::Colours::black);
        ig.fillRoundedRectangle(sx, sy, sw, sh, 2.f);
        // Bright deck-colour border
        ig.setColour(deckColour.withAlpha(0.97f));
        ig.drawRoundedRectangle(sx, sy, sw, sh, 2.f, 1.5f);
    }

    imageBuilt = true;
}

void VinylComponent::resized()
{
    imageBuilt = false;
}

void VinylComponent::mouseDown(const juce::MouseEvent& e)
{
    isDragging  = false;
    lastDragPos = e.position;
    e.source.enableUnboundedMouseMovement(true);
    if (onTouch) onTouch(true);
}

void VinylComponent::mouseDrag(const juce::MouseEvent& e)
{
    float dx = e.position.x - lastDragPos.x;
    lastDragPos = e.position;
    isDragging = true;
    if (onDrag) onDrag(dx, 500.f);
}

void VinylComponent::mouseUp(const juce::MouseEvent& e)
{
    e.source.enableUnboundedMouseMovement(false);
    isDragging = false;
    if (onTouch) onTouch(false);
}

void VinylComponent::timerCallback()
{
    // Primary: use playhead delta for accurate rotation
    float headDelta = pendingHeadDelta.exchange(0.f, std::memory_order_relaxed);
    constexpr float ROT_PER_FULL_SAMPLE = 10.0f * 2.f * 3.14159265f; // 10 rotations per full playthrough
    if (std::abs(headDelta) > 1e-7f)
    {
        rotationAngle += headDelta * ROT_PER_FULL_SAMPLE;
    }
    else if (isPlaying)
    {
        // Fallback: speed-based for smooth animation when head doesn't move (e.g., paused scratch)
        float speed = std::clamp(displaySpeed, -4.f, 4.f);
        constexpr float STEP_NORM = 2.f * 3.14159265f * 33.33f / 60.f / 30.f;
        rotationAngle += speed * STEP_NORM;
    }
    rotationAngle = std::fmod(rotationAngle, 2.f * 3.14159265f);
    if (rotationAngle < 0.f) rotationAngle += 2.f * 3.14159265f;

    // Glow
    float glowTarget = (handOnRecord ? 1.f : (isPlaying ? 0.3f : 0.f));
    glowIntensity += (glowTarget - glowIntensity) * 0.08f;

    repaint();
}

void VinylComponent::paint(juce::Graphics& g)
{
    if (!imageBuilt && getWidth() > 8) buildVinylImage();
    if (!imageBuilt) return;

    auto centre = getLocalBounds().getCentre().toFloat();
    float sz    = (float)std::min(getWidth(), getHeight());

    // Rotate pre-built vinyl image around its own centre, then place at component centre.
    float imgSz = (float)vinylImage.getWidth();
    juce::AffineTransform transform =
        juce::AffineTransform::translation(-imgSz * 0.5f, -imgSz * 0.5f)
        .followedBy(juce::AffineTransform::rotation(rotationAngle))
        .followedBy(juce::AffineTransform::translation(centre.x, centre.y));

    g.drawImageTransformed(vinylImage, transform);

    // Static spindle hole only (label is now part of the rotating vinyl image)
    {
        float holeR = sz * 0.02f;
        g.setColour(juce::Colours::black);
        g.fillEllipse(centre.x - holeR, centre.y - holeR, holeR * 2, holeR * 2);
    }

    // Tonearm (static, positioned relative to component size)
    float armStartX = sz * 0.92f;
    float armStartY = sz * 0.08f;
    float armEndX   = centre.x + sz * 0.28f;
    float armEndY   = centre.y - sz * 0.02f;
    g.setColour(juce::Colour(0xff888888));
    g.drawLine(armStartX, armStartY, armEndX, armEndY, 2.f);
    g.setColour(juce::Colour(0xffaaaaaa));
    g.fillEllipse(armStartX - 4.f, armStartY - 4.f, 8.f, 8.f);
    // Stylus
    g.setColour(juce::Colour(0xffcccccc));
    g.fillEllipse(armEndX - 3.f, armEndY - 3.f, 6.f, 6.f);

    // Glow overlay when playing/touching
    if (glowIntensity > 0.01f)
    {
        float glowR = sz * 0.49f * 1.05f;
        juce::ColourGradient glow(
            deckColour.withAlpha(glowIntensity * 0.5f), centre.x, centre.y,
            deckColour.withAlpha(0.f), centre.x + glowR, centre.y, true);
        g.setGradientFill(glow);
        g.fillEllipse(centre.x - glowR, centre.y - glowR, glowR * 2, glowR * 2);
    }

    // "Hand on record" indicator
    if (handOnRecord)
    {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(makeUiFont(10.f));
        g.drawText("SCRATCH", getLocalBounds(), juce::Justification::centred);
    }
}

//==============================================================================
// XYPadComponent — preset scratch patterns (dx per step, 20ms per step)
// Positive = forward, negative = back, 0 = momentary pause
const float XYPadComponent::PRESET_BABY[]    = {  80,  80,  80, -80, -80, -80 };
const float XYPadComponent::PRESET_FORWARD[] = {  60,  80, 100,  80,   0, -120, -120 };
const float XYPadComponent::PRESET_FLARE[]   = {  60,  60,   0, -60,  60,   0, -60,  60, 0, -60 };
const float XYPadComponent::PRESET_CRAB[]    = {  40,   0,  40,   0,  40,   0, -80, -80 };
const int   XYPadComponent::PRESET_LEN[]     = { 6, 7, 10, 8 };
const float* XYPadComponent::PRESETS[]       = { PRESET_BABY, PRESET_FORWARD, PRESET_FLARE, PRESET_CRAB };
const int   XYPadComponent::NUM_PRESETS      = 4;

//==============================================================================
XYPadComponent::XYPadComponent(ScratcherAudioProcessor& p, ScratcherLAF& /*l*/)
    : proc(p)
{
    setWantsKeyboardFocus(true);
    addKeyListener(this);
}

XYPadComponent::~XYPadComponent()
{
    stopTimer();
}

void XYPadComponent::setMouseModeActive(bool active)
{
    mouseModeActive = active;
    if (!active)
    {
        stopTimer();
        if (isDragging)
        {
            isDragging = false;
            proc.deckA.touchRecord(false);
            proc.deckB.touchRecord(false);
        }
    }
    if (onMouseModeChanged) onMouseModeChanged(active);
    repaint();
}

bool XYPadComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    bool cmdOrCtrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    if (cmdOrCtrl && key.isKeyCode('R'))
    {
        setMouseModeActive(false);
        return true;
    }
    return false;
}

void XYPadComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!mouseModeActive) return;
    stopTimer();  // cancel any running preset
    e.source.enableUnboundedMouseMovement(true);
    isDragging    = true;
    totalDragDist = 0.f;
    lastMousePos  = e.position;
    trailIdx      = 0;
    for (auto& pt : trail) pt = e.position;
    proc.deckA.touchRecord(true);
    proc.deckB.touchRecord(true);
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!mouseModeActive || !isDragging) return;

    auto delta = e.position - lastMousePos;
    lastMousePos = e.position;
    totalDragDist += std::abs(delta.x) + std::abs(delta.y);

    trail[trailIdx++ % TRAIL_LEN] = e.position;

    float dx = delta.x;
    float dy = delta.y;

    proc.deckA.applyMouseDelta(dx, 500.f);
    proc.deckB.applyMouseDelta(dx, 500.f);

    scratchVelocity = std::clamp(crossfaderNorm - dy / getHeight() * 2.f, 0.f, 1.f);
    if (auto* param = proc.apvts.getParameter("crossfader"))
        param->setValueNotifyingHost(scratchVelocity);

    if (onScratchMove) onScratchMove(dx, dy);
    repaint();
}

void XYPadComponent::mouseUp(const juce::MouseEvent& e)
{
    if (!mouseModeActive) return;
    e.source.enableUnboundedMouseMovement(false);
    isDragging = false;

    if (totalDragDist < 6.f)
    {
        // Pure click → trigger preset scratch
        triggerPresetScratch();
    }
    else
    {
        proc.deckA.touchRecord(false);
        proc.deckB.touchRecord(false);
    }
    repaint();
}

void XYPadComponent::triggerPresetScratch()
{
    presetStep = 0;
    // touchRecord stays true while preset plays — timerCallback releases it at end
    proc.deckA.touchRecord(true);
    proc.deckB.touchRecord(true);
    startTimer(20);
}

void XYPadComponent::timerCallback()
{
    int len = PRESET_LEN[presetType % NUM_PRESETS];
    if (presetStep >= len)
    {
        stopTimer();
        proc.deckA.touchRecord(false);
        proc.deckB.touchRecord(false);
        presetType = (presetType + 1) % NUM_PRESETS;
        repaint();
        return;
    }
    float dx = PRESETS[presetType % NUM_PRESETS][presetStep];
    proc.deckA.applyMouseDelta(dx, 500.f);
    proc.deckB.applyMouseDelta(dx, 500.f);
    ++presetStep;
}

void XYPadComponent::resized() {}

void XYPadComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(b, 6.f);

    // Grid
    g.setColour(juce::Colour(0xff0d0d0d));
    for (int i = 1; i < 4; ++i)
    {
        float x = b.getX() + (float)i / 4.f * b.getWidth();
        float y = b.getY() + (float)i / 4.f * b.getHeight();
        g.drawLine(x, b.getY(), x, b.getBottom(), 1.f);
        g.drawLine(b.getX(), y, b.getRight(), y, 1.f);
    }

    // Centre crosshair
    float cx = b.getCentreX(), cy = b.getCentreY();
    g.setColour(juce::Colour(0xff111111));
    g.drawLine(cx, b.getY(), cx, b.getBottom(), 1.5f);
    g.drawLine(b.getX(), cy, b.getRight(), cy, 1.5f);

    // Deck zones (A=left in blue, B=right in orange)
    g.setColour(juce::Colour(0xff3772ff).withAlpha(0.08f));
    g.fillRect(b.getX(), b.getY(), b.getWidth() * 0.5f, b.getHeight());
    g.setColour(juce::Colour(0xfffdca40).withAlpha(0.08f));
    g.fillRect(cx, b.getY(), b.getWidth() * 0.5f, b.getHeight());

    // Motion trail
    if (mouseModeActive && isDragging)
    {
        for (int i = 1; i < TRAIL_LEN; ++i)
        {
            int idx0 = (trailIdx - i    + TRAIL_LEN) % TRAIL_LEN;
            int idx1 = (trailIdx - i - 1 + TRAIL_LEN) % TRAIL_LEN;
            float alpha = (float)(TRAIL_LEN - i) / TRAIL_LEN * 0.6f;
            g.setColour(juce::Colour(0xff3772ff).withAlpha(alpha));
            auto& p0 = trail[idx0]; auto& p1 = trail[idx1];
            if (p0.getDistanceFrom(p1) < 100.f)
                g.drawLine(p0.x, p0.y, p1.x, p1.y, 2.f);
        }
    }

    // Crossfader indicator (horizontal line)
    float fy = b.getY() + (1.f - crossfaderNorm) * b.getHeight();
    g.setColour(juce::Colour(0xffe6e8e6).withAlpha(0.7f));
    g.drawLine(b.getX(), fy, b.getRight(), fy, 1.5f);

    // Scratch position indicator (vertical line)
    float fx = b.getX() + scratchPosNorm * b.getWidth();
    g.setColour(juce::Colour(0xff3772ff).withAlpha(0.7f));
    g.drawLine(fx, b.getY(), fx, b.getBottom(), 1.5f);

    // Status overlay
    if (mouseModeActive)
    {
        g.setColour(juce::Colour(0xff3772ff).withAlpha(0.9f));
        g.setFont(makeUiFont(10.f));
        g.drawText("MOUSE MODE ACTIVE  |  Cmd+R / Ctrl+R to exit",
                   b.removeFromBottom(18).toNearestInt(),
                   juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff6a6c6a).withAlpha(0.9f));
        g.setFont(makeUiFont(10.f));
        g.drawText("Click MOUSE MODE to scratch",
                   b.removeFromBottom(18).toNearestInt(),
                   juce::Justification::centred);
    }

    // Border
    g.setColour(mouseModeActive ? juce::Colour(0xff3772ff).withAlpha(0.6f)
                                : juce::Colour(0xff0d0d0d));
    g.drawRoundedRectangle(b, 6.f, 1.5f);
}

//==============================================================================
// CrossfaderStrip
//==============================================================================
CrossfaderStrip::CrossfaderStrip(ScratcherAudioProcessor& p, ScratcherLAF& l)
    : proc(p), laf(l)
{
    addAndMakeVisible(xfaderSlider);
    xfaderSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    xfaderSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    xfaderSlider.setRange(0.0, 1.0);
    xfaderSlider.setLookAndFeel(&laf);
    xfaderSlider.addListener(this);
    xfaderAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "crossfader", xfaderSlider);

    for (auto* btn : { &btnCP, &btnSC, &btnLin })
    {
        addAndMakeVisible(*btn);
        btn->setClickingTogglesState(true);
        btn->setRadioGroupId(1);
        btn->setLookAndFeel(&laf);
    }
    btnCP.setButtonText("CP"); btnCP.setToggleState(true, juce::dontSendNotification);
    btnSC.setButtonText("SC");
    btnLin.setButtonText("LIN");

    for (auto* btn : { &btnCurveDown, &btnCurveUp, &btnMidi })
    {
        addAndMakeVisible(*btn);
        btn->setLookAndFeel(&laf);
    }
    btnCurveDown.setButtonText("<");
    btnCurveUp.setButtonText(">");
    btnMidi.setButtonText("MIDI");
    btnMidi.addListener(this);
    btnCurveDown.addListener(this);
    btnCurveUp.addListener(this);

    addAndMakeVisible(lblCurveValue);
    lblCurveValue.setText("n=0", juce::dontSendNotification);
    lblCurveValue.setJustificationType(juce::Justification::centred);
    lblCurveValue.setLookAndFeel(&laf);
}

void CrossfaderStrip::resized()
{
    auto b = getLocalBounds();
    int  h = b.getHeight();

    // Layout: [CP][SC][LIN]  [<][n=0][>]  [MIDI]  ──fader──
    int btnW = 32, btnH = std::min(h, 22);
    int y    = (h - btnH) / 2;

    btnCP.setBounds(2, y, btnW, btnH);
    btnSC.setBounds(btnW + 4, y, btnW, btnH);
    btnLin.setBounds(btnW * 2 + 6, y, btnW, btnH);

    btnCurveDown.setBounds(btnW * 3 + 12, y, 18, btnH);
    lblCurveValue.setBounds(btnW * 3 + 32, y, 36, btnH);
    btnCurveUp.setBounds(btnW * 3 + 70, y, 18, btnH);

    btnMidi.setBounds(btnW * 3 + 94, y, 40, btnH);

    int sliderX = btnW * 3 + 140;
    xfaderSlider.setBounds(sliderX, 0, b.getWidth() - sliderX - 4, h);
}

void CrossfaderStrip::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff080808));
    g.fillRoundedRectangle(b, 4.f);
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawRoundedRectangle(b, 4.f, 1.f);
    // Label
    g.setColour(juce::Colour(0xff6a6c6a));
    g.setFont(makeUiFont(9.f));
    g.drawText("CROSSFADER", juce::Rectangle<int>(0, 0, 80, 14),
               juce::Justification::centred);
}

void CrossfaderStrip::sliderValueChanged(juce::Slider*) {}

void CrossfaderStrip::buttonClicked(juce::Button* b)
{
    if (b == &btnMidi)
    {
        if (proc.midiLearn.isLearning())
        {
            proc.midiLearn.cancelLearn();
            btnMidi.setButtonText("MIDI");
        }
        else
        {
            proc.midiLearn.startLearn("crossfader");
            btnMidi.setButtonText("...");
        }
    }
    if (b == &btnCurveDown || b == &btnCurveUp)
    {
        if (auto* param = proc.apvts.getParameter("xfaderCurve"))
        {
            float cur = param->getValue() * 6.f;
            int idx = (int)std::round(cur);
            if (b == &btnCurveDown) idx = std::max(0, idx - 1);
            else                    idx = std::min(6, idx + 1);
            param->setValueNotifyingHost(idx / 6.f);
            lblCurveValue.setText("n=" + juce::String(idx), juce::dontSendNotification);
        }
    }
}

//==============================================================================
// DeckPanel
//==============================================================================
DeckPanel::DeckPanel(ScratcherAudioProcessor& p, ScratcherLAF& l,
                     DeckProcessor& d, SampleManager::DeckIndex idx,
                     juce::Colour colour)
    : proc(p), laf(l), deck(d), deckIdx(idx), deckColour(colour)
{
    vinyl.setDeckColour(colour);
    waveform.setColour(colour);
    vu.setColour(colour);

    addAndMakeVisible(vinyl);
    addAndMakeVisible(waveform);
    addAndMakeVisible(vu);

    // Mouse drag on vinyl = scratch
    vinyl.onTouch = [&d](bool isDown) { d.touchRecord(isDown); };
    vinyl.onDrag  = [&d](float dx, float pps) { d.applyMouseDelta(dx, pps); };

    auto setupBtn = [&](juce::TextButton& btn, const juce::String& text) {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.addListener(this);
        btn.setLookAndFeel(&laf);
    };

    setupBtn(btnPlay,   "PLAY");
    setupBtn(btnPause,  "PAUSE");
    setupBtn(btnStop,   "STOP");
    setupBtn(btnCue,    "CUE");
    setupBtn(btnHold,   "HOLD"); btnHold.setClickingTogglesState(true);
    setupBtn(btnLoad,   "LOAD");
    setupBtn(btnLoop,   "LOOP");
    setupBtn(btnNudgeL, "<<");
    setupBtn(btnNudgeR, ">>");
    setupBtn(btnMidiVol,    "M");
    setupBtn(btnMidiScratch,"M");
    setupBtn(btnMidiCue,    "M");
    setupBtn(btnSync, "SYNC");

    for (auto* lb : { &btnL1, &btnL2, &btnL4 })
    {
        addAndMakeVisible(*lb);
        lb->addListener(this);
        lb->setLookAndFeel(&laf);
        lb->setClickingTogglesState(true);
        lb->setRadioGroupId(30 + (deckIdx == SampleManager::DeckA ? 0 : 1));
    }
    btnL1.setButtonText("1 BAR");
    btnL2.setButtonText("2 BAR");
    btnL4.setButtonText("FULL");

    for (auto* l2 : { &lblSampleName, &lblBpm, &lblDuration })
    {
        addAndMakeVisible(*l2);
        l2->setLookAndFeel(&laf);
        l2->setJustificationType(juce::Justification::centredLeft);
    }
    lblSampleName.setText("No sample", juce::dontSendNotification);
    lblBpm.setText("BPM: --", juce::dontSendNotification);
    lblDuration.setText("0:00", juce::dontSendNotification);

    for (auto* l2 : { &lblVolume, &lblPitch, &lblSpeed })
    {
        addAndMakeVisible(*l2);
        l2->setLookAndFeel(&laf);
        l2->setJustificationType(juce::Justification::centredLeft);
    }
    lblVolume.setText("VOL", juce::dontSendNotification);
    lblPitch.setText("PITCH", juce::dontSendNotification);
    lblSpeed.setText("SPEED", juce::dontSendNotification);

    for (auto* sl : { &slVolume, &slPitch, &slSpeed })
    {
        addAndMakeVisible(*sl);
        sl->setSliderStyle(juce::Slider::LinearHorizontal);
        sl->setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
        sl->setLookAndFeel(&laf);
    }

    juce::String prefix = (deckIdx == SampleManager::DeckA) ? "a_" : "b_";
    attachVol   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, prefix + "volume", slVolume);
    attachPitch = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, prefix + "pitch",  slPitch);
    attachSpeed = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, prefix + "speed",  slSpeed);

    addAndMakeVisible(cmbAutoPreset);
    cmbAutoPreset.setLookAndFeel(&laf);
    cmbAutoPreset.addItem("Manual", 1);
    auto presets = SampleManager::getAutoPresets();
    for (int i = 0; i < (int)presets.size(); ++i)
        cmbAutoPreset.addItem(presets[(size_t)i].name, i + 2);
    cmbAutoPreset.setSelectedId(1, juce::dontSendNotification);  // Manual = full sample

    // Wire trim handles on waveform — drag directly to set APVTS parameters
    {
        juce::String pfix = (deckIdx == SampleManager::DeckA) ? "a_" : "b_";
        waveform.onTrimChanged = [this, pfix](float ts, float te)
        {
            if (auto* sp = proc.apvts.getParameter(pfix + "trimStart"))
                sp->setValueNotifyingHost(sp->convertTo0to1(ts));
            if (auto* ep = proc.apvts.getParameter(pfix + "trimEnd"))
                ep->setValueNotifyingHost(ep->convertTo0to1(te));
        };
    }

    // Disable PLAY until a real sample is loaded
    btnPlay.setEnabled(deck.hasSampleLoaded());

    startTimerHz(20);  // 20fps for deck panel updates is plenty
}

DeckPanel::~DeckPanel()
{
    stopTimer();
    btnPlay.removeListener(this);
    btnPause.removeListener(this);
    btnStop.removeListener(this);
    btnCue.removeListener(this);
    btnHold.removeListener(this);
    btnLoad.removeListener(this);
    btnLoop.removeListener(this);
    btnNudgeL.removeListener(this);
    btnNudgeR.removeListener(this);
    btnSync.removeListener(this);
}

void DeckPanel::resized()
{
    auto b  = getLocalBounds().reduced(2);
    int  w  = b.getWidth();
    int  h  = b.getHeight();

    // Vinyl platter: square in top portion
    int vinylSz = std::min(w, (int)(h * 0.45f));
    vinyl.setBounds(b.getX() + (w - vinylSz) / 2, b.getY(), vinylSz, vinylSz);

    int y = b.getY() + vinylSz + 2;
    int rem = h - vinylSz - 2;

    // VU meter: right strip
    vu.setBounds(b.getRight() - 12, b.getY(), 12, vinylSz);

    // Waveform overview
    waveform.setBounds(b.getX(), y, w, std::min(rem / 5, 30));
    y += waveform.getHeight() + 2;
    rem -= waveform.getHeight() + 2;

    // Sample info
    lblSampleName.setBounds(b.getX(), y, w - 50, 14);
    btnLoad.setBounds(b.getRight() - 48, y, 48, 14);
    y += 16; rem -= 16;

    lblBpm.setBounds(b.getX(), y, w / 2, 14);
    lblDuration.setBounds(b.getX() + w / 2, y, w / 2, 14);
    y += 16; rem -= 16;

    // Auto-preset combo
    cmbAutoPreset.setBounds(b.getX(), y, w, 18);
    y += 20; rem -= 20;

    // Sliders
    int slLabelW = 40, slH = 16;
    lblVolume.setBounds(b.getX(), y, slLabelW, slH);
    slVolume.setBounds(b.getX() + slLabelW, y, w - slLabelW - 16, slH);
    btnMidiVol.setBounds(b.getRight() - 14, y, 14, slH);
    y += slH + 2; rem -= slH + 2;

    lblPitch.setBounds(b.getX(), y, slLabelW, slH);
    slPitch.setBounds(b.getX() + slLabelW, y, w - slLabelW - 16, slH);
    y += slH + 2; rem -= slH + 2;

    lblSpeed.setBounds(b.getX(), y, slLabelW, slH);
    slSpeed.setBounds(b.getX() + slLabelW, y, w - slLabelW - 38, slH);
    btnMidiScratch.setBounds(b.getRight() - 36, y, 36, slH);
    y += slH + 2; rem -= slH + 2;

    // Transport buttons
    int btnW = (w - 7) / 8, btnH = 18;
    btnPlay.setBounds(b.getX(),              y, btnW, btnH);
    btnPause.setBounds(b.getX() + btnW + 1,  y, btnW, btnH);
    btnStop.setBounds(b.getX() + btnW*2 + 2, y, btnW, btnH);
    btnCue.setBounds(b.getX() + btnW*3 + 3,  y, btnW, btnH);
    btnHold.setBounds(b.getX() + btnW*4 + 4, y, btnW, btnH);
    btnMidiCue.setVisible(false);
    btnLoop.setBounds(b.getX() + btnW*5 + 5, y, btnW, btnH);
    btnSync.setBounds(b.getX() + btnW*6 + 6, y, btnW, btnH);
    btnNudgeL.setVisible(false);
    btnNudgeR.setVisible(false);
    y += btnH + 2;

    // Loop length buttons
    int lw = w / 3 - 1;
    btnL1.setBounds(b.getX(),           y, lw, 16);
    btnL2.setBounds(b.getX() + lw + 1,  y, lw, 16);
    btnL4.setBounds(b.getX() + lw * 2 + 2, y, w - lw * 2 - 2, 16);
}

void DeckPanel::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff080808));
    g.fillRoundedRectangle(b, 6.f);

    if (fileDragOver)
    {
        g.setColour(deckColour.withAlpha(0.18f));
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(deckColour.withAlpha(0.9f));
        g.drawRoundedRectangle(b.reduced(2.f), 6.f, 2.5f);
        g.setFont(makeUiFont(13.f));
        g.drawText("DROP AUDIO", b.toNearestInt(), juce::Justification::centred);
    }
    else
    {
        g.setColour(deckColour.withAlpha(0.4f));
        g.drawRoundedRectangle(b, 6.f, 2.f);
    }

    // Deck label (A or B)
    g.setColour(deckColour.withAlpha(0.8f));
    g.setFont(makePixelFont(20.f));
    juce::String letter = (deckIdx == SampleManager::DeckA) ? "A" : "B";
    g.drawText("DECK " + letter, b.toNearestInt(), juce::Justification::topLeft, false);
}

bool DeckPanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".aif"
            || ext == ".ogg" || ext == ".flac" || ext == ".m4a")
            return true;
    }
    return false;
}

void DeckPanel::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    fileDragOver = false;
    for (auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            proc.sampleMgr.loadFile(file, deckIdx, deck, deck.getSampleRate());
            break;
        }
    }
    repaint();
}

void DeckPanel::timerCallback()
{
    // Wheel release: countdown ticks → re-engage motor
    if (wheelReleaseCountdown > 0)
    {
        if (--wheelReleaseCountdown == 0)
            deck.touchRecord(false);
    }

    // Update VU meter
    vu.setLevel(deck.getVuLevel());
    vu.repaint();

    // Update vinyl speed — animates in Playing, Hold, and during wheel scratch
    vinyl.setSpeed(deck.getCurrentSpeed());
    auto ps = deck.getPlayState();
    bool active = (ps == DeckProcessor::PlayState::Playing
                || ps == DeckProcessor::PlayState::Hold
                || wheelReleaseCountdown > 0);
    vinyl.setPlaying(active);

    // Update waveform playhead
    float curHead = deck.getPlayheadNorm();
    waveform.setPlayheadNorm(curHead);

    // Feed playhead delta to vinyl for accurate rotation
    float headDelta = curHead - prevPlayheadNorm;
    // Handle loop wrap-around
    if (headDelta >  0.5f) headDelta -= 1.0f;
    if (headDelta < -0.5f) headDelta += 1.0f;
    vinyl.addPlayheadDelta(headDelta);
    prevPlayheadNorm = curHead;

    // Update trim visualisation on waveform
    waveform.setTrimBounds(deck.getTrimStartNorm(), deck.getTrimEndNorm());

    // Keep btnPlay in sync with whether a real sample is loaded
    bool hasReal = deck.hasSampleLoaded();
    if (btnPlay.isEnabled() != hasReal)
        btnPlay.setEnabled(hasReal);

    // Update BPM label
    double bpm = deck.getDetectedBpm();
    lblBpm.setText("BPM: " + (bpm > 0 ? juce::String(bpm, 1) : "--"),
                   juce::dontSendNotification);
}

void DeckPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // Each wheel tick scratches the deck.
    // deltaY: typically ±0.1 per notch; scale to pixels so 500 px/s = 1× speed.
    // Timer runs at 20 Hz (50 ms), so 6 ticks = 300 ms release timeout.
    float dx = wheel.deltaY * 400.f;
    deck.touchRecord(true);
    deck.applyMouseDelta(dx, 500.f);
    wheelReleaseCountdown = 6;
}

void DeckPanel::buttonClicked(juce::Button* b)
{
    if (b == &btnPlay)   { deck.playTape(); btnHold.setToggleState(false, juce::dontSendNotification); }
    if (b == &btnPause)  { deck.pause();   btnHold.setToggleState(false, juce::dontSendNotification); }
    if (b == &btnStop)   { deck.stopTape();btnHold.setToggleState(false, juce::dontSendNotification); }
    if (b == &btnCue)
    {
        auto state = deck.getPlayState();
        if (state == DeckProcessor::PlayState::Playing)
        {
            // Return to cue point and pause
            deck.cue();
        }
        else
        {
            // Set new cue point at current position and stay paused
            deck.setCuePoint(deck.getPlayheadSample());
            deck.pause();
        }
        btnHold.setToggleState(false, juce::dontSendNotification);
    }
    if (b == &btnHold)
    {
        if (btnHold.getToggleState()) deck.hold();
        else                          deck.pause();
    }
    if (b == &btnLoad)   { openFileBrowser(); }
    if (b == &btnLoop)   {
        bool on = btnLoop.getToggleState();
        btnLoop.setToggleState(!on, juce::dontSendNotification);
        deck.setLoop(!on);
    }
    if (b == &btnNudgeL) { deck.nudge(-0.5f); }
    if (b == &btnNudgeR) { deck.nudge(+0.5f); }

    // Loop length buttons — 1 bar, 2 bar, full sample
    if (b == &btnL1 || b == &btnL2 || b == &btnL4)
    {
        if (b == &btnL4)
        {
            // FULL = entire sample within trim region
            int trimEnd   = deck.getTrimEnd();
            int trimStart = deck.getTrimStart();
            deck.setLoop(true, trimStart, trimEnd <= 0 ? deck.getTotalSamples() : trimEnd);
        }
        else
        {
            double deckBpm = deck.getDetectedBpm();
            double hostBpm = proc.hostBpm.load(std::memory_order_relaxed);
            double bpm     = (deckBpm > 20.0) ? deckBpm : (hostBpm > 20.0 ? hostBpm : 0.0);
            double numBars = (b == &btnL1) ? 1.0 : 2.0;
            deck.setLoopLengthByBars(numBars, bpm);
        }
    }

    // MIDI learn buttons
    auto startLearn = [this](const juce::String& param, juce::TextButton& btn) {
        if (!proc.midiLearn.isLearning()) {
            proc.midiLearn.startLearn(param);
            btn.setButtonText("...");
        }
    };
    juce::String prefix = (deckIdx == SampleManager::DeckA) ? "a_" : "b_";
    if (b == &btnMidiVol)     startLearn(prefix + "volume", btnMidiVol);
    if (b == &btnMidiCue)     startLearn(prefix + "speed", btnMidiCue);

    if (b == &btnMidiScratch)
    {
        bool isA = (deckIdx == SampleManager::DeckA);
        auto& learningFlag = isA ? proc.midiScratchLearningA : proc.midiScratchLearningB;
        auto& ccAtom       = isA ? proc.midiScratchCCA       : proc.midiScratchCCB;

        if (ccAtom.load() >= 0)
        {
            // Unbind
            ccAtom.store(-1);
            btnMidiScratch.setButtonText("SCR MIDI");
        }
        else
        {
            learningFlag.store(true);
            btnMidiScratch.setButtonText("LEARN...");
        }
    }
    else if (b == &btnSync)
    {
        bool isA = (deckIdx == SampleManager::DeckA);
        DeckProcessor& otherDeck = isA ? proc.deckB : proc.deckA;
        double thisBpm  = deck.getDetectedBpm();
        double otherBpm = otherDeck.getDetectedBpm();
        juce::String paramId = isA ? "a_speed" : "b_speed";
        float newSpeed = 1.0f;
        if (thisBpm > 0.0 && otherBpm > 0.0)
            newSpeed = (float)(otherBpm / thisBpm);
        else
            newSpeed = otherDeck.getCurrentSpeed();
        newSpeed = std::clamp(newSpeed, 0.25f, 4.0f);
        if (auto* p = proc.apvts.getParameter(paramId))
            p->setValueNotifyingHost(p->convertTo0to1(newSpeed));
    }
}

void DeckPanel::openFileBrowser()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load sample for Deck " + juce::String(deckIdx == SampleManager::DeckA ? "A" : "B"),
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.ogg;*.flac");

    chooser->launchAsync(juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto files = fc.getResults();
            if (files.isEmpty()) return;
            proc.sampleMgr.loadFile(files[0], deckIdx, deck,
                                    proc.getSampleRate());
            lblSampleName.setText(files[0].getFileNameWithoutExtension(),
                                  juce::dontSendNotification);
        });
}

void DeckPanel::updateMidiScratchButton()
{
    bool isA = (deckIdx == SampleManager::DeckA);
    auto& ccAtom = isA ? proc.midiScratchCCA : proc.midiScratchCCB;
    int cc = ccAtom.load();
    btnMidiScratch.setButtonText(cc >= 0 ? "CC#" + juce::String(cc) : "SCR MIDI");
}

void DeckPanel::onSampleLoaded()
{
    // Update waveform overview
    waveform.setWaveformPeaks(deck.getWaveformPeaks());
    btnPlay.setEnabled(true);
    deck.play();

    // Apply current auto-preset
    int presetId = cmbAutoPreset.getSelectedId();
    if (presetId >= 2)
    {
        auto presets = SampleManager::getAutoPresets();
        int idx = presetId - 2;
        if (idx < (int)presets.size())
        {
            double bpm = proc.hostBpm.load(std::memory_order_relaxed);
            proc.sampleMgr.applyAutoPreset(deckIdx, presets[(size_t)idx], bpm, deck);
        }
    }

    repaint();
}

//==============================================================================
// ScopeDisplay
//==============================================================================
void ScopeDisplay::updateScope(ScratcherAudioProcessor& proc)
{
    int newPos = proc.scopeWritePos.load(std::memory_order_acquire);
    int numNew = newPos - lastScopePos;
    if (numNew <= 0) return;

    int n = (int)scopeSamples.size();
    if (numNew > n) { numNew = n; }

    for (int i = 0; i < numNew; ++i)
    {
        int idx = (lastScopePos + i) & (ScratcherAudioProcessor::SCOPE_SIZE - 1);
        scopeSamples[(size_t)(i % n)] = proc.scopeBuffer[idx].load(std::memory_order_relaxed);
    }
    lastScopePos = newPos;
}

void ScopeDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff000000));

    int n = (int)scopeSamples.size();
    float midY = b.getCentreY();
    float scaleH = b.getHeight() * 0.45f;

    juce::Path path;
    bool first = true;
    for (int i = 0; i < n; ++i)
    {
        float x = b.getX() + (float)i / n * b.getWidth();
        float y = midY - scopeSamples[(size_t)i] * scaleH;
        if (first) { path.startNewSubPath(x, y); first = false; }
        else path.lineTo(x, y);
    }

    // Glow
    juce::PathStrokeType stroke3(3.f);
    g.setColour(juce::Colour(0xff3772ff).withAlpha(0.2f));
    g.strokePath(path, stroke3);

    juce::PathStrokeType stroke1(1.5f);
    g.setColour(juce::Colour(0xff3772ff).withAlpha(0.8f));
    g.strokePath(path, stroke1);

    // Centre line
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawLine(b.getX(), midY, b.getRight(), midY, 1.f);

    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawRect(b, 1.f);
}

//==============================================================================
// SlicePad
//==============================================================================
juce::String SlicePad::noteToName(int note)
{
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return juce::String(names[note % 12]) + juce::String(note / 12 - 1);
}

void SlicePad::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.f);
    juce::Colour bg = isLearning ? juce::Colour(0xfffdca40).withAlpha(0.5f)
                    : isActive   ? juce::Colour(0xff3772ff).withAlpha(0.35f)
                                 : juce::Colour(0xff1a1a1a);
    g.setColour(bg);
    g.fillRoundedRectangle(b, 3.f);

    juce::Colour border = isLearning ? juce::Colour(0xfffdca40)
                        : isActive   ? juce::Colour(0xff3772ff)
                                     : juce::Colour(0xff2a2a2a);
    g.setColour(border);
    g.drawRoundedRectangle(b, 3.f, 1.f);

    // Slice start marker (thin vertical line at normStart position)
    float markerX = b.getX() + normStart * b.getWidth();
    g.setColour(juce::Colour(0xff3772ff).withAlpha(0.6f));
    g.drawLine(markerX, b.getY() + 2.f, markerX, b.getBottom() - 2.f, 1.f);

    // Index number (top-left)
    g.setColour(juce::Colour(0xff6a6c6a));
    g.setFont(8.f);
    g.drawText(juce::String(idx + 1), b.toNearestInt().reduced(1), juce::Justification::topLeft);

    // Note name (centre)
    g.setColour(isLearning ? juce::Colour(0xfffdca40) : juce::Colour(0xffe6e8e6));
    g.setFont(9.f);
    g.drawText(isLearning ? "?" : noteToName(midiNote),
               b.toNearestInt(), juce::Justification::centred);

    // "L" learn indicator (bottom-right, small)
    g.setColour(juce::Colour(0xff2a2a2a));
    g.setFont(7.f);
    g.drawText("L", b.toNearestInt().reduced(1), juce::Justification::bottomRight);
}

void SlicePad::mouseDown(const juce::MouseEvent& e)
{
    // Bottom-right 10×10 = learn button area
    if (e.x >= getWidth() - 10 && e.y >= getHeight() - 10)
    {
        if (onLearnNote) onLearnNote();
        return;
    }
    if (onTrigger) onTrigger();
}

void SlicePad::mouseUp(const juce::MouseEvent& /*e*/) {}

//==============================================================================
// SlicerPanel
//==============================================================================
SlicerPanel::SlicerPanel(ScratcherAudioProcessor& p, ScratcherLAF& /*l*/) : proc(p)
{
    for (int i = 0; i < 8; ++i)
    {
        padsA[i].setIndex(i);
        padsA[i].setMidiNote(36 + i);
        addAndMakeVisible(padsA[i]);

        padsA[i].onTrigger = [this, i] {
            const auto si = (size_t)i;
            const int totalA = proc.deckA.getTotalSamples();
            if (totalA > 0)
            {
                int startPos = proc.sliceStartsA[si].load(std::memory_order_relaxed);
                int endPos   = (i < 7) ? proc.sliceStartsA[si + 1].load(std::memory_order_relaxed)
                                       : totalA;
                proc.deckA.setSliceAttackMs(proc.sliceAttackMsA[si].load(std::memory_order_relaxed));
                proc.deckA.setSliceDecayMs (proc.sliceDecayMsA[si].load(std::memory_order_relaxed));
                proc.deckA.setLoop(true, startPos, endPos);
                proc.deckA.seekToSample(startPos);
                proc.slicerActiveNote.store(proc.sliceNotesA[si].load(std::memory_order_relaxed),
                                            std::memory_order_relaxed);
            }
        };
        padsA[i].onLearnNote = [this, i] {
            proc.sliceLearningDeck.store(0, std::memory_order_relaxed);
            proc.sliceLearningIndex.store(i, std::memory_order_relaxed);
            padsA[i].setLearning(true);
        };

        padsB[i].setIndex(i);
        padsB[i].setMidiNote(48 + i);
        addAndMakeVisible(padsB[i]);

        padsB[i].onTrigger = [this, i] {
            const auto si = (size_t)i;
            const int totalB = proc.deckB.getTotalSamples();
            if (totalB > 0)
            {
                int startPos = proc.sliceStartsB[si].load(std::memory_order_relaxed);
                int endPos   = (i < 7) ? proc.sliceStartsB[si + 1].load(std::memory_order_relaxed)
                                       : totalB;
                proc.deckB.setSliceAttackMs(proc.sliceAttackMsB[si].load(std::memory_order_relaxed));
                proc.deckB.setSliceDecayMs (proc.sliceDecayMsB[si].load(std::memory_order_relaxed));
                proc.deckB.setLoop(true, startPos, endPos);
                proc.deckB.seekToSample(startPos);
            }
        };
        padsB[i].onLearnNote = [this, i] {
            proc.sliceLearningDeck.store(1, std::memory_order_relaxed);
            proc.sliceLearningIndex.store(i, std::memory_order_relaxed);
            padsB[i].setLearning(true);
        };
    }

    startTimerHz(30);
}

SlicerPanel::~SlicerPanel() { stopTimer(); }

void SlicerPanel::resized()
{
    auto b = getLocalBounds().reduced(2);
    int rowH = b.getHeight() / 2 - 1;
    auto rowA = b.removeFromTop(rowH);
    b.removeFromTop(2);
    auto rowB = b;

    int padW = rowA.getWidth() / 8;
    for (int i = 0; i < 8; ++i)
    {
        padsA[i].setBounds(rowA.removeFromLeft(padW).reduced(1, 0));
        padsB[i].setBounds(rowB.removeFromLeft(padW).reduced(1, 0));
    }
}

void SlicerPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff080808));
    g.fillAll();
    // Deck labels
    auto b = getLocalBounds().reduced(2);
    int rowH = b.getHeight() / 2 - 1;
    g.setColour(juce::Colour(0xff3772ff).withAlpha(0.8f));
    g.setFont(7.f);
    g.drawText("A", 0, 0, 12, rowH, juce::Justification::centred);
    g.setColour(juce::Colour(0xfffdca40).withAlpha(0.8f));
    g.drawText("B", 0, rowH + 2, 12, b.getHeight() - rowH - 2, juce::Justification::centred);
}

void SlicerPanel::timerCallback()
{
    const int activeNote   = proc.slicerActiveNote.load(std::memory_order_relaxed);
    const int learnDeck    = proc.sliceLearningDeck.load(std::memory_order_relaxed);
    const int learnIdx     = proc.sliceLearningIndex.load(std::memory_order_relaxed);
    const int totalA       = proc.deckA.getTotalSamples();
    const int totalB       = proc.deckB.getTotalSamples();

    for (int i = 0; i < 8; ++i)
    {
        const auto si = (size_t)i;
        int noteA = proc.sliceNotesA[si].load(std::memory_order_relaxed);
        int noteB = proc.sliceNotesB[si].load(std::memory_order_relaxed);

        padsA[i].setActive(activeNote == noteA);
        padsB[i].setActive(activeNote == noteB);

        padsA[i].setLearning(learnDeck == 0 && learnIdx == i);
        padsB[i].setLearning(learnDeck == 1 && learnIdx == i);

        padsA[i].setMidiNote(noteA);
        padsB[i].setMidiNote(noteB);

        if (totalA > 0)
            padsA[i].setNormStart((float)proc.sliceStartsA[si].load() / (float)totalA);
        if (totalB > 0)
            padsB[i].setNormStart((float)proc.sliceStartsB[si].load() / (float)totalB);
    }
}

void SlicerPanel::refreshPads()
{
    timerCallback(); // just force a full refresh
}

//==============================================================================
// SlicerWaveformView
//==============================================================================
SlicerWaveformView::SlicerWaveformView(ScratcherAudioProcessor& p, ScratcherLAF& /*l*/)
    : proc(p)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(20);
}

SlicerWaveformView::~SlicerWaveformView() { stopTimer(); }
void SlicerWaveformView::resized() {}
void SlicerWaveformView::refreshWaveforms() { repaint(); }
void SlicerWaveformView::timerCallback() { repaint(); }

float SlicerWaveformView::normToX(float norm) const
{
    // Map normalised sample position (0..1) to pixel X, applying zoom/scroll
    float visibleRange = 1.f / zoomLevel;
    float localNorm    = (norm - scrollOffset) / visibleRange;
    return localNorm * (float)getWidth();
}

float SlicerWaveformView::xToNorm(float x) const
{
    float visibleRange = 1.f / zoomLevel;
    float localNorm    = x / (float)getWidth();
    return scrollOffset + localNorm * visibleRange;
}

void SlicerWaveformView::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff000000));

    const float totalH  = b.getHeight();
    const float rowH    = (totalH - ENV_STRIP_H * 2.f) * 0.5f;  // waveform row height
    const float visW    = b.getWidth();

    juce::Colour colA = juce::Colour(0xff3772ff);
    juce::Colour colB = juce::Colour(0xfffdca40);

    for (int deck = 0; deck < 2; ++deck)
    {
        auto& dp       = (deck == 0) ? proc.deckA : proc.deckB;
        auto& starts   = (deck == 0) ? proc.sliceStartsA : proc.sliceStartsB;
        auto& notesArr = (deck == 0) ? proc.sliceNotesA   : proc.sliceNotesB;
        auto& atkArr   = (deck == 0) ? proc.sliceAttackMsA : proc.sliceAttackMsB;
        auto& dcyArr   = (deck == 0) ? proc.sliceDecayMsA  : proc.sliceDecayMsB;
        juce::Colour col = (deck == 0) ? colA : colB;
        int total = dp.getTotalSamples();

        float rowTop     = b.getY() + deck * (rowH + ENV_STRIP_H);
        float envTop     = rowTop + rowH;
        auto  rowRect    = juce::Rectangle<float>(b.getX(), rowTop,  visW, rowH);
        auto  envRect    = juce::Rectangle<float>(b.getX(), envTop, visW, ENV_STRIP_H);

        // ── Waveform background ───────────────────────────────────────────
        g.setColour(juce::Colour(0xff080808));
        g.fillRect(rowRect);
        g.setColour(col.withAlpha(0.7f));
        g.setFont(8.f);
        g.drawText("DECK " + juce::String(deck == 0 ? "A" : "B"),
                   rowRect.toNearestInt().removeFromLeft(40).removeFromTop(12),
                   juce::Justification::centredLeft);

        if (total > 0)
        {
            // Waveform (only visible region)
            const auto& peaks = dp.getWaveformPeaks();
            float midY   = rowRect.getCentreY();
            float scaleH = rowRect.getHeight() * 0.42f;
            g.setColour(col.withAlpha(0.6f));

            float visibleRange = 1.f / zoomLevel;
            int pStart = std::max(0, (int)(scrollOffset * peaks.size()) - 1);
            int pEnd   = std::min((int)peaks.size(), (int)((scrollOffset + visibleRange) * peaks.size()) + 2);

            for (int pi = pStart; pi < pEnd; ++pi)
            {
                float norm = (float)pi / peaks.size();
                float x    = normToX(norm);
                if (x < b.getX() || x > b.getRight()) continue;
                float hw = peaks[(size_t)pi] * scaleH;
                g.fillRect(x, midY - hw, 1.5f, hw * 2.f);
            }

            // Playhead
            float phNorm = dp.getPlayheadNorm();
            float phX    = normToX(phNorm);
            if (phX >= b.getX() && phX <= b.getRight())
            {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawLine(phX, rowRect.getY(), phX, rowRect.getBottom(), 1.5f);
            }

            // Slice markers
            int activeNote = proc.slicerActiveNote.load(std::memory_order_relaxed);
            for (int s = 0; s < 8; ++s)
            {
                const auto ss = (size_t)s;
                int startPos  = starts[ss].load(std::memory_order_relaxed);
                float norm    = (float)startPos / total;
                float sx      = normToX(norm);
                if (sx < b.getX() - 20.f || sx > b.getRight() + 20.f) continue;

                bool isActive   = (activeNote == notesArr[ss].load(std::memory_order_relaxed));
                bool isSelected = (selectedDeck == deck && selectedSlice == s);

                g.setColour(isSelected ? juce::Colours::white.withAlpha(0.9f)
                           : isActive  ? col.brighter(0.5f)
                                       : col.withAlpha(0.8f));
                g.drawLine(sx, rowRect.getY(), sx, rowRect.getBottom(),
                           (isActive || isSelected) ? 2.5f : 1.5f);

                g.setFont(7.f);
                g.drawText(juce::String(s + 1),
                           (int)sx + 1, (int)rowRect.getY() + 1, 10, 10,
                           juce::Justification::topLeft);
            }
        }
        else
        {
            g.setColour(juce::Colour(0xff1a1a1a));
            g.setFont(makeUiFont(10.f));
            g.drawText("No sample loaded", rowRect.toNearestInt(), juce::Justification::centred);
        }

        g.setColour(col.withAlpha(0.3f));
        g.drawRect(rowRect, 1.f);

        // ── Envelope strip ────────────────────────────────────────────────
        g.setColour(juce::Colour(0xff080808));
        g.fillRect(envRect);

        if (selectedDeck == deck && selectedSlice >= 0 && total > 0)
        {
            const auto ss = (size_t)selectedSlice;
            int atk = atkArr[ss].load(std::memory_order_relaxed);
            int dcy = dcyArr[ss].load(std::memory_order_relaxed);
            float halfW = envRect.getWidth() * 0.5f;

            // Attack bar (left half) — blue
            float atkFrac = std::min(1.f, atk / 500.f);
            g.setColour(juce::Colour(0xff3772ff).withAlpha(0.25f));
            g.fillRect(envRect.getX(), envRect.getY(), halfW * atkFrac, envRect.getHeight());
            g.setColour(juce::Colour(0xff3772ff));
            g.setFont(7.f);
            g.drawText("ATK " + juce::String(atk) + "ms",
                       (int)envRect.getX(), (int)envRect.getY(),
                       (int)halfW, (int)envRect.getHeight(),
                       juce::Justification::centred);

            // Decay bar (right half)
            float dcyFrac = std::min(1.f, dcy / 500.f);
            g.setColour(juce::Colour(0xfffdca40).withAlpha(0.25f));
            g.fillRect(envRect.getX() + halfW, envRect.getY(), halfW * dcyFrac, envRect.getHeight());
            g.setColour(juce::Colour(0xfffdca40));
            g.drawText("DCY " + juce::String(dcy) + "ms",
                       (int)(envRect.getX() + halfW), (int)envRect.getY(),
                       (int)halfW, (int)envRect.getHeight(),
                       juce::Justification::centred);

            // Divider
            g.setColour(col.withAlpha(0.4f));
            g.drawLine(envRect.getX() + halfW, envRect.getY(),
                       envRect.getX() + halfW, envRect.getBottom(), 1.f);
        }
        else
        {
            g.setColour(juce::Colour(0xff1a1a1a));
            g.setFont(7.f);
            g.drawText("click slice to edit ATK / DCY",
                       envRect.toNearestInt(), juce::Justification::centred);
        }

        g.setColour(col.withAlpha(0.2f));
        g.drawRect(envRect, 1.f);
    }

    // Zoom indicator (top-right corner)
    if (zoomLevel > 1.01f)
    {
        g.setColour(juce::Colour(0xffe6e8e6).withAlpha(0.7f));
        g.setFont(7.f);
        g.drawText("x" + juce::String(zoomLevel, 1),
                   (int)b.getRight() - 30, (int)b.getY() + 2, 28, 10,
                   juce::Justification::centredRight);
    }
}

int SlicerWaveformView::getSliceAtX(float x, int deck, float& outNorm) const
{
    auto& dp     = (deck == 0) ? proc.deckA : proc.deckB;
    auto& starts = (deck == 0) ? proc.sliceStartsA : proc.sliceStartsB;
    int total    = dp.getTotalSamples();
    if (total <= 0) return -1;

    // Threshold in normalised coords: 8px worth
    float threshNorm = 8.f / (float)getWidth() / zoomLevel;
    float minDist = threshNorm;
    int bestSlice = -1;
    for (int s = 0; s < 8; ++s)
    {
        float sliceNorm = (float)starts[(size_t)s].load() / total;
        float sliceX    = normToX(sliceNorm);
        float dist      = std::abs(sliceX - x) / (float)getWidth();
        if (dist < minDist) { minDist = dist; bestSlice = s; outNorm = sliceNorm; }
    }
    return bestSlice;
}

void SlicerWaveformView::mouseDown(const juce::MouseEvent& e)
{
    const float totalH = (float)getHeight();
    const float rowH   = (totalH - ENV_STRIP_H * 2.f) * 0.5f;

    // Determine which section was clicked
    int deck = -1;
    bool inEnvStrip = false;
    for (int d = 0; d < 2; ++d)
    {
        float rowTop = d * (rowH + ENV_STRIP_H);
        float envTop = rowTop + rowH;
        if (e.y >= rowTop && e.y < envTop)           { deck = d; inEnvStrip = false; break; }
        if (e.y >= envTop && e.y < envTop + ENV_STRIP_H) { deck = d; inEnvStrip = true;  break; }
    }
    if (deck < 0) return;

    if (inEnvStrip && selectedDeck == deck && selectedSlice >= 0)
    {
        // Start envelope drag
        float halfW = (float)getWidth() * 0.5f;
        dragEnvType   = (e.x < halfW) ? 1 : 2;  // 1=attack, 2=decay
        dragEnvStartX = (float)e.x;
        auto& atkArr  = (deck == 0) ? proc.sliceAttackMsA : proc.sliceAttackMsB;
        auto& dcyArr  = (deck == 0) ? proc.sliceDecayMsA  : proc.sliceDecayMsB;
        dragEnvStartMs = (dragEnvType == 1)
            ? atkArr[(size_t)selectedSlice].load(std::memory_order_relaxed)
            : dcyArr[(size_t)selectedSlice].load(std::memory_order_relaxed);
        return;
    }

    // Click on waveform — check for slice hit
    float norm = 0.f;
    int sliceIdx = getSliceAtX((float)e.x, deck, norm);
    if (sliceIdx >= 0)
    {
        // Select this slice (single click = select, start drag)
        selectedDeck  = deck;
        selectedSlice = sliceIdx;
        draggingDeck  = deck;
        draggingSlice = sliceIdx;
        dragStartX    = (float)e.x;
    }
    else
    {
        // Click in empty area — deselect
        selectedDeck  = -1;
        selectedSlice = -1;
    }
    dragEnvType = 0;
    repaint();
}

void SlicerWaveformView::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Double-click resets zoom
    juce::ignoreUnused(e);
    zoomLevel    = 1.f;
    scrollOffset = 0.f;
    repaint();
}

void SlicerWaveformView::mouseDrag(const juce::MouseEvent& e)
{
    // ── Envelope strip drag ───────────────────────────────────────────────
    if (dragEnvType > 0 && selectedDeck >= 0 && selectedSlice >= 0)
    {
        float dx    = (float)e.x - dragEnvStartX;
        int   delta = (int)(dx / (float)getWidth() * 500.f);
        int   newMs = std::clamp(dragEnvStartMs + delta, 0, 500);

        if (dragEnvType == 1)
        {
            auto& atkArr = (selectedDeck == 0) ? proc.sliceAttackMsA : proc.sliceAttackMsB;
            atkArr[(size_t)selectedSlice].store(newMs, std::memory_order_relaxed);
        }
        else
        {
            auto& dcyArr = (selectedDeck == 0) ? proc.sliceDecayMsA : proc.sliceDecayMsB;
            dcyArr[(size_t)selectedSlice].store(newMs, std::memory_order_relaxed);
        }
        repaint();
        return;
    }

    // ── Slice marker drag with push behaviour ─────────────────────────────
    if (draggingDeck < 0 || draggingSlice < 0) return;

    float dx = (float)e.x - dragStartX;
    dragStartX = (float)e.x;

    auto& dp     = (draggingDeck == 0) ? proc.deckA     : proc.deckB;
    auto& starts = (draggingDeck == 0) ? proc.sliceStartsA : proc.sliceStartsB;
    int total    = dp.getTotalSamples();
    if (total <= 0) return;

    // Convert pixel delta → sample delta using current zoom
    int delta = (int)((dx / (float)getWidth()) * total / zoomLevel);
    int i     = draggingSlice;

    const auto si = (size_t)i;
    int newPos = starts[si].load(std::memory_order_relaxed) + delta;
    newPos = std::clamp(newPos, 0, total - 1);

    // Push neighbours forward / backward when crossing them
    constexpr int MIN_GAP = 512;
    if (delta > 0)
    {
        // Moving right — push right neighbours forward
        for (int j = i + 1; j < 8; ++j)
        {
            const auto sj = (size_t)j;
            int prevPos = starts[sj - 1].load(std::memory_order_relaxed);
            int jPos    = starts[sj].load(std::memory_order_relaxed);
            if (jPos < prevPos + MIN_GAP)
                starts[sj].store(std::min(total - 1, prevPos + MIN_GAP), std::memory_order_relaxed);
            else break;
        }
    }
    else if (delta < 0)
    {
        // Moving left — push left neighbours backward
        for (int j = i - 1; j >= 0; --j)
        {
            const auto sj = (size_t)j;
            int nextPos = starts[sj + 1].load(std::memory_order_relaxed);
            int jPos    = starts[sj].load(std::memory_order_relaxed);
            if (jPos > nextPos - MIN_GAP)
                starts[sj].store(std::max(0, nextPos - MIN_GAP), std::memory_order_relaxed);
            else break;
        }
    }

    starts[si].store(newPos, std::memory_order_relaxed);
    repaint();
}

void SlicerWaveformView::mouseUp(const juce::MouseEvent&)
{
    draggingDeck  = -1;
    draggingSlice = -1;
    dragEnvType   = 0;
}

void SlicerWaveformView::mouseWheelMove(const juce::MouseEvent& e,
                                        const juce::MouseWheelDetails& w)
{
    // Ctrl+wheel = zoom, plain wheel = scroll
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        float oldZoom = zoomLevel;
        zoomLevel = std::clamp(zoomLevel * (1.f + w.deltaY * 0.25f), 1.f, 16.f);

        // Zoom around the mouse position
        float mouseNorm = xToNorm((float)e.x);
        float newRange  = 1.f / zoomLevel;
        scrollOffset    = std::clamp(mouseNorm - (float)e.x / (float)getWidth() * newRange,
                                     0.f, 1.f - newRange);
        juce::ignoreUnused(oldZoom);
    }
    else
    {
        float visibleRange = 1.f / zoomLevel;
        scrollOffset = std::clamp(scrollOffset - w.deltaY * visibleRange * 0.2f,
                                  0.f, 1.f - visibleRange);
    }
    repaint();
}

//==============================================================================
// GrossBeatPanel
//==============================================================================
GrossBeatPanel::GrossBeatPanel(ScratcherAudioProcessor& p, ScratcherLAF& l)
    : proc(p), laf(l)
{
    addAndMakeVisible(btnEnable);
    btnEnable.setButtonText("GROSS BEAT");
    btnEnable.setClickingTogglesState(true);
    btnEnable.addListener(this);
    btnEnable.setLookAndFeel(&laf);

    addAndMakeVisible(btnTimeEnv);
    btnTimeEnv.setButtonText("TIME");
    btnTimeEnv.setClickingTogglesState(true);
    btnTimeEnv.setRadioGroupId(10);
    btnTimeEnv.setToggleState(true, juce::dontSendNotification);
    btnTimeEnv.addListener(this);
    btnTimeEnv.setLookAndFeel(&laf);

    addAndMakeVisible(btnVolEnv);
    btnVolEnv.setButtonText("VOL");
    btnVolEnv.setClickingTogglesState(true);
    btnVolEnv.setRadioGroupId(10);
    btnVolEnv.addListener(this);
    btnVolEnv.setLookAndFeel(&laf);

    for (int i = 0; i < 8; ++i)
    {
        addAndMakeVisible(slotBtns[i]);
        slotBtns[i].setButtonText(juce::String(i + 1));
        slotBtns[i].setClickingTogglesState(true);
        slotBtns[i].setRadioGroupId(11);
        slotBtns[i].addListener(this);
        slotBtns[i].setLookAndFeel(&laf);
    }
    slotBtns[0].setToggleState(true, juce::dontSendNotification);

    for (auto* b : { &btnLoop1, &btnLoop2, &btnLoop4 })
    {
        addAndMakeVisible(*b);
        b->addListener(this);
        b->setLookAndFeel(&laf);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(20);
    }
    btnLoop1.setButtonText("1 BAR");
    btnLoop2.setButtonText("2 BARS");
    btnLoop4.setButtonText("4 BARS");

    addAndMakeVisible(envEditor);
    envEditor.setPattern(&proc.timePatterns[0], true);
    envEditor.onPatternChanged = [this]{ repaint(); };
    // Panel is permanently hidden — no timer needed.
}

GrossBeatPanel::~GrossBeatPanel()
{
    // stopTimer() not needed — timer was never started
}

void GrossBeatPanel::resized()
{
    auto b = getLocalBounds().reduced(2);
    int h = b.getHeight();
    int y = b.getY();

    // Top bar
    btnEnable.setBounds(b.getX(), y, 90, 18);
    btnTimeEnv.setBounds(b.getX() + 96, y, 40, 18);
    btnVolEnv.setBounds(b.getX() + 138, y, 36, 18);

    // Slot buttons
    int slotW = 22;
    for (int i = 0; i < 8; ++i)
        slotBtns[i].setBounds(b.getX() + 180 + i * (slotW + 2), y, slotW, 18);

    // Loop buttons
    int loopX = b.getX() + 180 + 8 * (slotW + 2) + 8;
    btnLoop1.setBounds(loopX,      y, 50, 18);
    btnLoop2.setBounds(loopX + 52, y, 55, 18);
    btnLoop4.setBounds(loopX + 109,y, 55, 18);

    y += 22;
    envEditor.setBounds(b.getX(), y, b.getWidth(), h - 22);
}

void GrossBeatPanel::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff080808));
    g.fillRoundedRectangle(b, 4.f);
    g.setColour(juce::Colour(0xff0d0d0d));
    g.drawRoundedRectangle(b, 4.f, 1.f);
}

void GrossBeatPanel::timerCallback()
{
    // Update safety line and playhead in editor
    float phase = static_cast<float>(proc.hostBarPhase.load(std::memory_order_relaxed));
    envEditor.setHostPosition(phase);
}

void GrossBeatPanel::buttonClicked(juce::Button* b)
{
    if (b == &btnEnable)
    {
        proc.grossBeatEnabled.store(btnEnable.getToggleState());
        if (auto* param = proc.apvts.getParameter("grossBeatEnabled"))
            param->setValueNotifyingHost(btnEnable.getToggleState() ? 1.f : 0.f);
    }
    if (b == &btnTimeEnv) { showingTimeEnv = true;  updateEnvEditor(); }
    if (b == &btnVolEnv)  { showingTimeEnv = false; updateEnvEditor(); }

    for (int i = 0; i < 8; ++i)
    {
        if (b == &slotBtns[i])
        {
            if (showingTimeEnv) proc.activeTimeSlot.store(i);
            else               proc.activeVolSlot.store(i);
            updateEnvEditor();
        }
    }

    double bpm = proc.hostBpm.load(std::memory_order_relaxed);
    if (bpm <= 0.0) bpm = 120.0;
    if (b == &btnLoop1) { proc.deckA.setLoopLengthByBars(1.0, bpm); proc.deckB.setLoopLengthByBars(1.0, bpm); }
    if (b == &btnLoop2) { proc.deckA.setLoopLengthByBars(2.0, bpm); proc.deckB.setLoopLengthByBars(2.0, bpm); }
    if (b == &btnLoop4) { proc.deckA.setLoopLengthByBars(4.0, bpm); proc.deckB.setLoopLengthByBars(4.0, bpm); }
}

void GrossBeatPanel::updateEnvEditor()
{
    int slot = showingTimeEnv ? proc.activeTimeSlot.load() : proc.activeVolSlot.load();
    if (showingTimeEnv) envEditor.setPattern(&proc.timePatterns[(size_t)slot], true);
    else                envEditor.setPattern(&proc.volPatterns[(size_t)slot],  false);
    envEditor.repaint();
}

//==============================================================================
// OnboardingOverlay
//==============================================================================
const char* OnboardingOverlay::STEPS[] = {
    "Welcome to Scratcher!\nLoad a track via the LOAD button on either deck,\nor use the demo sample to get started right away.",
    "Press PLAY on either deck, then scroll\nthe mouse wheel over the deck to scratch!\nScroll up = forward, down = reverse.",
    "Use HOLD mode to stop the motor — only your hand moves the record.\nEnable SLICER to trigger transient-aligned sections via MIDI notes.\nBind any knob or slider to MIDI with the M buttons."
};

OnboardingOverlay::OnboardingOverlay()
{
    setOpaque(false);
    addAndMakeVisible(btnNext);  btnNext.setButtonText("Next");   btnNext.addListener(this);
    addAndMakeVisible(btnSkip);  btnSkip.setButtonText("Skip");   btnSkip.addListener(this);
}

void OnboardingOverlay::resized()
{
    auto b = getLocalBounds();
    btnNext.setBounds(b.getCentreX() + 10, b.getBottom() - 36, 80, 26);
    btnSkip.setBounds(b.getCentreX() - 90, b.getBottom() - 36, 80, 26);
}

void OnboardingOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xdd000000));

    auto b = getLocalBounds().toFloat().reduced(60.f, 100.f);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRoundedRectangle(b, 12.f);
    g.setColour(juce::Colour(0xff3772ff));
    g.drawRoundedRectangle(b, 12.f, 2.f);

    g.setColour(juce::Colour(0xff3772ff));
    g.setFont(makePixelFont(16.f));
    g.drawText("SCRATCHER  —  Quick Start", b.removeFromTop(32).toNearestInt(),
               juce::Justification::centred);

    g.setColour(juce::Colours::white);
    g.setFont(makeUiFont(13.f));
    g.drawFittedText(STEPS[step], b.reduced(20, 10).toNearestInt(),
                     juce::Justification::centred, 6);

    // Step indicator
    g.setColour(juce::Colour(0xff6a6c6a));
    g.setFont(makeUiFont(10.f));
    g.drawText("Step " + juce::String(step + 1) + " of " + juce::String(NUM_STEPS),
               getLocalBounds().removeFromBottom(40), juce::Justification::centred);
}

void OnboardingOverlay::buttonClicked(juce::Button* b)
{
    if (b == &btnNext)
    {
        step++;
        if (step >= NUM_STEPS) { if (onDismiss) onDismiss(); return; }
        btnNext.setButtonText(step == NUM_STEPS - 1 ? "Let's go!" : "Next");
        repaint();
    }
    else if (b == &btnSkip)
    {
        if (onDismiss) onDismiss();
    }
}

//==============================================================================
// Main Editor
//==============================================================================
ScratcherAudioProcessorEditor::ScratcherAudioProcessorEditor(ScratcherAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      deckPanelA(p, laf, p.deckA, SampleManager::DeckA, theme.deckA),
      deckPanelB(p, laf, p.deckB, SampleManager::DeckB, theme.deckB),
      crossfader(p, laf),
      xyPad(p, laf),
      grossBeat(p, laf),
      slicerPanel(p, laf),
      slicerWaveformView(p, laf)
{
    setLookAndFeel(&laf);
    addAndMakeVisible(deckPanelA);
    addAndMakeVisible(deckPanelB);
    addAndMakeVisible(crossfader);
    addAndMakeVisible(xyPad);
    addAndMakeVisible(scope);
    addAndMakeVisible(grossBeat);
    addAndMakeVisible(slicerPanel);
    slicerPanel.setVisible(false);
    addAndMakeVisible(slicerWaveformView);
    slicerWaveformView.setVisible(false);

    // Top bar controls
    auto setupTopBtn = [&](juce::TextButton& btn, const juce::String& text, bool toggle = false)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.addListener(this);
        btn.setLookAndFeel(&laf);
        if (toggle) btn.setClickingTogglesState(true);
    };

    setupTopBtn(btnSavePreset, "SAVE");
    setupTopBtn(btnLoadPreset, "LOAD PRESET");
    setupTopBtn(btnTap,        "TAP");
    setupTopBtn(btnSlicer,     "SLICER", true);
    setupTopBtn(btnMidiScratch, "MIDI SCR");
    setupTopBtn(btnHelp,       "?");

    addAndMakeVisible(cmbPreset);
    cmbPreset.setLookAndFeel(&laf);
    for (int i = 0; i < 8; ++i)
        cmbPreset.addItem(proc.getProgramName(i), i + 1);
    cmbPreset.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);

    addAndMakeVisible(cmbScale);
    cmbScale.setLookAndFeel(&laf);
    {
        int id = 1;
        for (auto s : { "75%", "100%", "125%", "150%", "175%", "200%" })
            cmbScale.addItem(s, id++);
    }
    cmbScale.setSelectedId(2, juce::dontSendNotification);
    cmbScale.onChange = [this] {
        const int scales[] = { 75, 100, 125, 150, 175, 200 };
        int idx = cmbScale.getSelectedId() - 1;
        if (idx >= 0 && idx < 6) applyScaling(scales[idx]);
    };

    // Physics knobs
    auto setupKnob = [&](juce::Slider& sl, juce::Label& lb, const juce::String& text,
                         const juce::String& /*param*/) {
        addAndMakeVisible(sl);
        sl.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 40, 12);
        sl.setLookAndFeel(&laf);
        addAndMakeVisible(lb);
        lb.setText(text, juce::dontSendNotification);
        lb.setLookAndFeel(&laf);
        lb.setJustificationType(juce::Justification::centred);
    };

    setupKnob(knobInertia,     lblInertia,     "INERTIA",    "inertia");
    setupKnob(knobFriction,    lblFriction,    "FRICTION",   "slipmatFriction");
    setupKnob(knobScratchSens, lblScratchSens, "SENSITIVITY","scratchSens");
    setupKnob(knobScratchSmooth,lblScratchSmooth,"SMOOTHING", "scratchSmooth");
    setupKnob(knobCrackle,     lblCrackle,     "CRACKLE",    "vinylCrackle");
    setupKnob(knobWarp,        lblWarp,        "WARP",       "vinylWarp");
    setupKnob(knobVinylNoise,  lblVinylNoise,  "HISS",       "vinylNoiseVol");
    setupKnob(knobOutputGain,  lblOutputGain,  "OUTPUT",     "outputGain");

    attachInertia       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "inertia",           knobInertia);
    attachFriction      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "slipmatFriction",   knobFriction);
    attachScratchSens   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scratchSens",       knobScratchSens);
    attachScratchSmooth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "scratchSmooth",     knobScratchSmooth);
    attachCrackle       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "vinylCrackle",      knobCrackle);
    attachWarp          = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "vinylWarp",         knobWarp);
    attachVinylNoise    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "vinylNoiseVol",     knobVinylNoise);
    attachOutputGain    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "outputGain",        knobOutputGain);

    // FX knobs
    setupKnob(knobRevSize,   lblRevSize,   "REV.SIZE",  "reverbSize");
    setupKnob(knobRevDamp,   lblRevDamp,   "REV.DAMP",  "reverbDamp");
    setupKnob(knobRevWet,    lblRevWet,    "REV.WET",   "reverbWet");
    setupKnob(knobDelTime,   lblDelTime,   "DEL.TIME",  "delayTime");
    setupKnob(knobDelFb,     lblDelFb,     "DEL.FB",    "delayFeedback");
    setupKnob(knobDelWet,    lblDelWet,    "DEL.WET",   "delayWet");
    setupKnob(knobAPanRate,  lblAPanRate,  "PAN.RATE",  "autopanRate");
    setupKnob(knobAPanDepth, lblAPanDepth, "PAN.DEPTH", "autopanDepth");

    attachRevSize   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "reverbSize",      knobRevSize);
    attachRevDamp   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "reverbDamp",      knobRevDamp);
    attachRevWet    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "reverbWet",       knobRevWet);
    attachDelTime   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "delayTime",       knobDelTime);
    attachDelFb     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "delayFeedback",   knobDelFb);
    attachDelWet    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "delayWet",        knobDelWet);
    attachAPanRate  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "autopanRate",     knobAPanRate);
    attachAPanDepth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "autopanDepth",    knobAPanDepth);

    // CRT / OpenGL effects disabled

    // Slice recompute callback
    proc.onSlicesRecomputed = [this](int /*deck*/) {
        juce::MessageManager::callAsync([this] { slicerPanel.refreshPads(); });
    };

    // MIDI learn callback
    proc.midiLearn.onLearnComplete = [this](const MidiLearnManager::Binding& b) {
        lblMidiStatus.setText("Learned: " + proc.midiLearn.getBindingLabel(b.paramID),
                              juce::dontSendNotification);
    };
    addAndMakeVisible(lblMidiStatus);
    lblMidiStatus.setLookAndFeel(&laf);
    lblMidiStatus.setJustificationType(juce::Justification::centredRight);

    setSize(BASE_WIDTH, BASE_HEIGHT);
    setResizable(false, false);
    addKeyListener(this);
    xyPad.addKeyListener(this);

    startTimerHz(30);
    checkOnboarding();
}

ScratcherAudioProcessorEditor::~ScratcherAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void ScratcherAudioProcessorEditor::checkOnboarding()
{
    if (proc.isFirstRun.load())
    {
        onboarding = std::make_unique<OnboardingOverlay>();
        onboarding->setSize(getWidth(), getHeight());
        addAndMakeVisible(*onboarding);
        onboarding->onDismiss = [this]
        {
            onboarding->setVisible(false);
            onboarding.reset();
            proc.isFirstRun.store(false);
        };
    }
}

void ScratcherAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff000000));

    // Subtle separator between physics row and FX row
    auto b = getLocalBounds().reduced(4);
    b.removeFromTop(28 + 4 + 56 + 2); // top bar + physics knobs
    auto fxRowY = b.getY();
    g.setColour(juce::Colour(0xff1e1e1e));
    g.drawLine(4.f, (float)fxRowY - 1, (float)(getWidth() - 4), (float)fxRowY - 1, 1.f);
}

void ScratcherAudioProcessorEditor::resized()
{
    layoutComponents();
    if (onboarding) onboarding->setBounds(getLocalBounds());
}

void ScratcherAudioProcessorEditor::layoutComponents()
{
    auto b  = getLocalBounds().reduced(4);
    int  W  = b.getWidth();

    // ── Top bar (30 px) ───────────────────────────────────────────────────────
    auto topBar = b.removeFromTop(28);
    cmbPreset.setBounds(topBar.removeFromLeft(120));
    topBar.removeFromLeft(4);
    btnSavePreset.setBounds(topBar.removeFromLeft(46));
    topBar.removeFromLeft(2);
    btnLoadPreset.setBounds(topBar.removeFromLeft(80));
    topBar.removeFromLeft(8);
    btnSlicer.setBounds(topBar.removeFromLeft(54));
    topBar.removeFromLeft(4);
    btnMidiScratch.setBounds(topBar.removeFromLeft(62));
    topBar.removeFromLeft(4);
    btnTap.setBounds(topBar.removeFromLeft(40));
    topBar.removeFromLeft(4);
    btnHelp.setBounds(topBar.removeFromLeft(22));
    topBar.removeFromLeft(4);
    cmbScale.setBounds(topBar.removeFromLeft(58));
    lblMidiStatus.setBounds(topBar);

    b.removeFromTop(4);

    // ── Physics knobs row (56 px) ─────────────────────────────────────────────
    auto knobRow = b.removeFromTop(56);
    int  kw = std::min(60, knobRow.getWidth() / 8);
    struct KnobPair { juce::Slider* sl; juce::Label* lb; };
    KnobPair knobPairs[] = {
        { &knobInertia,      &lblInertia      },
        { &knobFriction,     &lblFriction     },
        { &knobScratchSens,  &lblScratchSens  },
        { &knobScratchSmooth,&lblScratchSmooth},
        { &knobCrackle,      &lblCrackle      },
        { &knobWarp,         &lblWarp         },
        { &knobVinylNoise,   &lblVinylNoise   },
        { &knobOutputGain,   &lblOutputGain   }
    };
    for (auto& kp : knobPairs)
    {
        auto cell = knobRow.removeFromLeft(kw);
        kp.lb->setBounds(cell.removeFromBottom(12));
        kp.sl->setBounds(cell);
        knobRow.removeFromLeft(2);
    }

    b.removeFromTop(2);

    // ── FX knobs row (56 px) ──────────────────────────────────────────────────
    auto fxRow = b.removeFromTop(56);
    int  fkw = std::min(60, fxRow.getWidth() / 8);
    KnobPair fxPairs[] = {
        { &knobRevSize,   &lblRevSize   },
        { &knobRevDamp,   &lblRevDamp   },
        { &knobRevWet,    &lblRevWet    },
        { &knobDelTime,   &lblDelTime   },
        { &knobDelFb,     &lblDelFb     },
        { &knobDelWet,    &lblDelWet    },
        { &knobAPanRate,  &lblAPanRate  },
        { &knobAPanDepth, &lblAPanDepth },
    };
    for (auto& kp : fxPairs)
    {
        auto cell = fxRow.removeFromLeft(fkw);
        kp.lb->setBounds(cell.removeFromBottom(12));
        kp.sl->setBounds(cell);
        fxRow.removeFromLeft(2);
    }

    b.removeFromTop(4);

    // ── Crossfader strip (28 px) ──────────────────────────────────────────────
    crossfader.setBounds(b.removeFromBottom(28));
    b.removeFromBottom(2);

    // Gross Beat hidden (removed from UI)
    grossBeat.setVisible(false);

    // ── Slicer panel — full width, above crossfader ───────────────────────────
    xyPad.setBounds({});
    bool slicerOn = proc.slicerEnabled.load(std::memory_order_relaxed);
    slicerPanel.setVisible(slicerOn);
    if (slicerOn)
    {
        slicerPanel.setBounds(b.removeFromBottom(88));
        b.removeFromBottom(2);
    }

    // ── Main area: Deck A | Scope | Deck B ───────────────────────────────────
    int deckW = (int)(W * 0.36f);

    deckPanelA.setBounds(b.removeFromLeft(deckW));
    deckPanelB.setBounds(b.removeFromRight(deckW));

    bool slicerVisible = proc.slicerEnabled.load(std::memory_order_relaxed);
    scope.setVisible(!slicerVisible);
    slicerWaveformView.setVisible(slicerVisible);
    if (slicerVisible)
        slicerWaveformView.setBounds(b);
    else
        scope.setBounds(b);
}

void ScratcherAudioProcessorEditor::timerCallback()
{
    updateVuMeters();
    scope.updateScope(proc);
    scope.repaint();

    float vuL = proc.deckA.getVuLevel();
    float vuR = proc.deckB.getVuLevel();
    juce::ignoreUnused(vuL, vuR);

    // Update MIDI learn status
    if (proc.midiLearn.isLearning())
        lblMidiStatus.setText("Waiting for MIDI... (" + proc.midiLearn.getLearningParam() + ")",
                              juce::dontSendNotification);

}

void ScratcherAudioProcessorEditor::updateVuMeters() {}

bool ScratcherAudioProcessorEditor::keyPressed(const juce::KeyPress& /*key*/, juce::Component*)
{
    return false;
}

void ScratcherAudioProcessorEditor::buttonClicked(juce::Button* b)
{
    if (b == &btnSlicer)
    {
        proc.slicerEnabled.store(btnSlicer.getToggleState(), std::memory_order_relaxed);
        slicerPanel.setVisible(btnSlicer.getToggleState());
        resized();
    }
    else if (b == &btnMidiScratch)
    {
        // Global button: binds/unbinds BOTH decks simultaneously
        int ccA = proc.midiScratchCCA.load(std::memory_order_relaxed);
        int ccB = proc.midiScratchCCB.load(std::memory_order_relaxed);
        if (ccA >= 0 || ccB >= 0)
        {
            // Already bound → unbind both
            proc.midiScratchCCA.store(-1, std::memory_order_relaxed);
            proc.midiScratchCCB.store(-1, std::memory_order_relaxed);
            btnMidiScratch.setButtonText("MIDI SCR");
        }
        else
        {
            // Start learning for both decks at once
            proc.midiScratchLearningA.store(true, std::memory_order_relaxed);
            proc.midiScratchLearningB.store(true, std::memory_order_relaxed);
            btnMidiScratch.setButtonText("LEARN...");
        }
    }
    else if (b == &btnTap)
    {
        proc.tapTempo();
    }
    else if (b == &btnSavePreset)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Scratcher preset", juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory), "*.scratcher");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode
                           | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                if (files.isEmpty()) return;
                juce::MemoryBlock data;
                proc.getStateInformation(data);
                files[0].replaceWithData(data.getData(), data.getSize());
            });
    }
    else if (b == &btnLoadPreset)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Scratcher preset", juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory), "*.scratcher");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto files = fc.getResults();
                if (files.isEmpty()) return;
                juce::MemoryBlock data;
                if (files[0].loadFileAsData(data))
                    proc.setStateInformation(data.getData(), (int)data.getSize());
            });
    }
    else if (b == &btnHelp)
    {
        // Re-show onboarding
        proc.isFirstRun.store(true);
        checkOnboarding();
        if (onboarding) onboarding->setBounds(getLocalBounds());
    }
}

void ScratcherAudioProcessorEditor::updateMidiScratchButton()
{
    int ccA = proc.midiScratchCCA.load(std::memory_order_relaxed);
    int ccB = proc.midiScratchCCB.load(std::memory_order_relaxed);
    if (ccA >= 0 && ccA == ccB)
        btnMidiScratch.setButtonText("CC#" + juce::String(ccA));
    else if (ccA >= 0 || ccB >= 0)
        btnMidiScratch.setButtonText("MIXED");
    else
        btnMidiScratch.setButtonText("MIDI SCR");
}

void ScratcherAudioProcessorEditor::updateMidiScratchButtons()
{
    deckPanelA.updateMidiScratchButton();
    deckPanelB.updateMidiScratchButton();
}

void ScratcherAudioProcessorEditor::updateSlicerPads()
{
    slicerPanel.refreshPads();
}

void ScratcherAudioProcessorEditor::onSampleLoaded(SampleManager::DeckIndex d)
{
    if (d == SampleManager::DeckA) deckPanelA.onSampleLoaded();
    else                           deckPanelB.onSampleLoaded();
}

void ScratcherAudioProcessorEditor::applyScaling(int scale)
{
    scalePercent = scale;
    float s = scale / 100.f;
    setSize((int)(BASE_WIDTH * s), (int)(BASE_HEIGHT * s));
}
