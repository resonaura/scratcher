# Scratcher — Детальный план реализации

> Плагин VST3/AU/Standalone: эмуляция двух виниловых дек, скретч, кроссфейдер, автоматизация времени (Gross Beat-парадигма), загрузка семплов, MIDI-привязка.
> Архитектура идентична flopster: JUCE 8, CMake 3.22+, C++17, npm-скрипты для сборки/паковки.
> После выполнения каждого пункта ставится ✅

---

## Легенда состояний
- ⬜ — не начато
- 🔄 — в процессе
- ✅ — выполнено

---

## ЭТАП 0: Структура проекта и система сборки

### 0.1 Дерево директорий ✅
```
scratcher/
├── src/
│   ├── PluginProcessor.h/.cpp     # Главный процессор + DSP
│   ├── PluginEditor.h/.cpp        # GUI + интерактив
│   ├── DeckProcessor.h/.cpp       # Физика одной деки (vinyl engine)
│   ├── CircularBuffer.h           # Lock-free кольцевой буфер (2^17 сэмплов)
│   ├── HermiteInterp.h            # 6-точечная интерполяция Эрмита 5-го порядка
│   ├── CrossfaderMath.h           # Математика кроссфейдера (constant power)
│   ├── EnvelopeEditor.h/.cpp      # Редактор огибающих времени/громкости
│   ├── MidiLearnManager.h/.cpp    # Система MIDI Learn для всех параметров
│   ├── SampleManager.h/.cpp       # Загрузка/управление аудиофайлами пользователя
│   └── version.h                  # Версия плагина
├── assets/
│   ├── app.png                    # Иконка плагина
│   ├── app.icns / app.ico
│   ├── scanlines.png              # CRT-наложение (как в flopster)
│   ├── vinyl_label_a.png          # Этикетка деки A (по умолчанию)
│   └── vinyl_label_b.png          # Этикетка деки B (по умолчанию)
├── fonts/
│   ├── PixelEmulator.ttf          # Копируется из flopster
│   └── PixgamerRegular.ttf
├── samples/
│   └── default/                   # Встроенный демо-сэмпл (короткий луп)
├── scripts/
│   ├── run.js
│   ├── dist.js
│   ├── postinstall.js
│   └── set-version.js
├── CMakeLists.txt
├── package.json
└── plan.md (этот файл)
```
Статус: ✅

### 0.2 CMakeLists.txt
- Скопировать структуру из flopster, заменить названия:
  - Project: `Scratcher`
  - PLUGIN_MANUFACTURER_CODE: `Rsna`
  - PLUGIN_CODE: `Scrt`
  - IS_SYNTH TRUE (чтобы принимать MIDI)
  - NEEDS_MIDI_INPUT TRUE
  - NEEDS_MIDI_OUTPUT TRUE (для MIDI Learn pass-through)
  - Добавить входную шину: `withInput("Input", stereo, true)` — режим эффекта
  - Форматы: macOS — VST3, AU, Standalone; Win/Linux — VST3, Standalone
  - juce_add_binary_data: шрифты + vinyl_label_a/b.png + scanlines.png + default sample
  - Флаги компилятора: те же что в flopster (recommended_config, lto, warning_flags)
  - JUCE добавляется через `add_subdirectory(JUCE JUCE_build)` — JUCE папка копируется из flopster

Статус: ✅

### 0.3 package.json и скрипты
- Скопировать скрипты из flopster, заменить названия Flopster → Scratcher
- npm run build — релиз сборка
- npm run rebuild — чистая пересборка
- npm run install:plugin — сборка + системная установка
- npm run dist — дистрибутив (macOS pkg + Windows msi)
- npm run set-version X.Y.Z — обновить version.h

Статус: ✅

---

## ЭТАП 1: Базовые классы DSP (без GUI)

### 1.1 version.h
```cpp
#pragma once
#define SCRATCHER_VERSION "1.0.0"
```
Статус: ✅

### 1.2 CircularBuffer.h — Lock-Free кольцевой буфер
Требования из ТЗ:
- Размер = степень двойки (2^17 = 131072 сэмплов при 44.1 кГц ≈ 3 сек)
- Динамически пересчитывается под BPM: `bufferSize = 2 * (60.0 / bpm) * sampleRate`
- Округляется до следующей степени двойки
- Индексация через побитовую маску `& (size - 1)` (нет % операции в processBlock)
- Стерео: два отдельных массива (L/R), выделяются в prepareToPlay
- Методы: `write(L, R)`, `readAt(offset)` → возвращает stereo pair
- writePointer — атомарный int, читается DSP-потоком
- Вся аллокация — ТОЛЬКО в prepareToPlay, не в processBlock

```cpp
class CircularBuffer {
    static constexpr int MAX_SIZE = 1 << 18; // 2^18 = 262144
    float bufL[MAX_SIZE] = {};
    float bufR[MAX_SIZE] = {};
    int mask = MAX_SIZE - 1;
    int actualSize = MAX_SIZE;
    std::atomic<int> writePos { 0 };
public:
    void resize(int newSize); // только из prepareToPlay
    void write(float l, float r);
    void readAt(int offset, float& l, float& r) const; // offset от writePos назад
    int getWritePos() const { return writePos.load(); }
};
```
Статус: ✅

### 1.3 HermiteInterp.h — 6-точечная интерполяция Эрмита 5-го порядка
Реализация по формулам ТЗ:

```cpp
namespace Hermite {
    // Предвычисленные константы (compile-time)
    static constexpr double INV_24 = 1.0/24.0;
    static constexpr double INV_12 = 1.0/12.0;
    // ... все дроби из таблицы ТЗ

    // y[-2], y[-1], y[0], y[1], y[2], y[3] — 6 соседних сэмплов
    // f — дробная часть позиции [0.0, 1.0)
    inline float interpolate(float ym2, float ym1, float y0,
                             float y1,  float y2,  float y3, float f) {
        // Коэффициенты c0..c5 по схеме из ТЗ
        // Вычисление через схему Горнера
        // return (((((c5*f+c4)*f+c3)*f+c2)*f+c1)*f+c0);
    }
}
```
Все 6 коэффициентов вычисляются в точности по таблице из ТЗ:
- c0 = y0
- c1 = 1/12*(y[-2]-y[2]) + 2/3*(y[1]-y[-1])
- c2 = 13/12*y[-1] - 25/12*y[0] + 2/3*y[1] - 11/24*y[2] + 1/24*y[3] - 1/8*y[-2]
- c3 = 5/12*y[0] - 7/12*y[1] + 7/24*y[2] - 1/24*(y[-2]+y[-1]+y[3])
- c4 = 1/8*y[-2] - 7/12*y[-1] + 13/12*y[0] - y[1] + 11/24*y[2] - 1/12*y[3]
- c5 = 1/24*(y[3]-y[-2]) + 5/24*(y[-1]-y[2]) + 5/12*(y[1]-y[0])

Статус: ✅

### 1.4 CrossfaderMath.h — Математика кроссфейдера
```cpp
namespace Crossfader {
    // x ∈ [0.0, 1.0], n — sharpness (0 = smooth constant power, >0 = scratch cut)
    // gA = cos(π/4 * ((2*(1-x)-1)^(2n+1) + 1))
    // gB = cos(π/4 * ((2*x-1)^(2n+1) + 1))
    struct Gains { float gA, gB; };
    Gains compute(float x, int n);

    // Линейная кривая (для совместимости)
    Gains computeLinear(float x);
}
```
Режимы кривой (контролируются параметром "Sharpness" в UI):
- n=0: Constant Power (плавное сведение)
- n=1..5: нарастающая резкость для скретча (scratch cut)
- Режим "X-Fader Cut": мгновенный срез у краёв — для краба/флера

Статус: ✅

### 1.5 DeckProcessor.h/.cpp — Физическая модель деки + DSP
Состояние:
```cpp
class DeckProcessor {
    // === Физика ===
    double angularVelocity = 0.0;    // текущая угловая скорость [рад/сэмпл]
    double targetVelocity = 1.0;     // нормальная скорость двигателя
    double inertia = 0.1;            // момент инерции пластинки (настраивается)
    double friction = 0.05;          // коэффициент трения слипмата (настраивается)
    bool handOnRecord = false;       // рука диджея на пластинке

    // === Состояние воспроизведения ===
    double readPosition = 0.0;       // дробная позиция в аудиобуфере
    AudioBuffer<float> sampleBuffer; // загруженный аудиосэмпл
    bool isLooping = true;
    bool isPlaying = false;
    int loopStart = 0, loopEnd = 0;  // точки лупа

    // === Динамический Anti-Aliasing LPF ===
    // Однополюсный рекурсивный фильтр:
    // Fc = min(Fs/2, Fs / (2 * |speed|))
    // coeff = exp(-2π * Fc / Fs)
    float lpfStateL = 0.f, lpfStateR = 0.f;
    float lpfCoeff = 0.f;

    // === Сглаживающий фильтр скорости (демпфирование мыши) ===
    double smoothedSpeed = 0.0;      // сглаженная скорость
    double speedSmoothCoeff = 0.92;  // коэффициент сглаживания (~20ms)

    // Кольцевой буфер для Gross Beat режима
    CircularBuffer inputBuffer;

public:
    void prepareToPlay(double sampleRate, int blockSize);
    void processBlock(float* outL, float* outR, int numSamples,
                      bool effectMode, bool grossBeatEnabled,
                      float timeEnvValue, float volEnvValue);

    // Физика (вызывается из GUI-потока при событиях мыши)
    void touchRecord(bool isDown);   // рука на пластинке / отпустила
    void applyMouseDelta(float deltaX, float pixelsPerSec);

    void updatePhysics(double dt);   // обновление угловой скорости
    void loadSample(const File& f);  // загрузка аудиофайла
    void setSampleFromBuffer(const AudioBuffer<float>& buf, double sourceSampleRate);

    // Управление воспроизведением
    void play(); void pause(); void stop(); void cue();
    void setLoop(bool enabled, int startSample, int endSample);
    void nudge(float amount);        // pitch bend без физики
    void setTempoSync(bool enabled, double bpm); // автосинхр. лупа под BPM
};
```

#### Алгоритм updatePhysics():
```
Если handOnRecord = true:
    angularVelocity = smoothedSpeed (из delta мыши)
Иначе:
    torque = friction * (targetVelocity - angularVelocity)
    angularVelocity += torque / inertia * dt  // инерционный разгон
    angularVelocity → clamp к targetVelocity при достижении
```

#### Алгоритм renderSample() (вызывается в processBlock, сэмпл за сэмплом):
```
1. Применить LPF к сэмплу из буфера (зависит от |angularVelocity|)
2. Читать 6 соседних сэмплов вокруг readPosition
3. Hermite::interpolate(6 сэмплов, frac(readPosition))
4. readPosition += angularVelocity (сдвиг позиции)
5. Обработать wrap-around (зацикливание / остановка на границе)
```

Статус: ✅

### 1.6 SampleManager.h/.cpp — Управление аудиофайлами
```cpp
class SampleManager {
public:
    // Асинхронная загрузка (не блокирует аудиопоток)
    void loadFile(const File& f, int deckIndex); // 0=A, 1=B
    bool isLoadingComplete(int deckIndex) const;

    // Авто-пресеты BPM-sync
    struct AutoPreset {
        String name;             // "1 bar", "2 bars", "4 bars", "Loop 1/2", "Loop 1/4"
        double barCount;         // количество тактов
        bool trimSilence;        // обрезать тишину в начале/конце
        bool beatMatch;          // подбить темп под BPM хоста
    };
    static std::vector<AutoPreset> getAutoPresets();

    // Применение авто-пресета: вычисляет loopStart/loopEnd под BPM
    void applyAutoPreset(int deckIndex, const AutoPreset& p,
                         double hostBpm, double hostSampleRate);

    // Waveform overview (для отображения в GUI)
    std::vector<float> getWaveformPeaks(int deckIndex, int numPoints);

    // Метаданные
    String getFileName(int deckIndex) const;
    double getDurationSeconds(int deckIndex) const;
    double getDetectedBpm(int deckIndex) const; // простой beatdetect по onset

private:
    struct DeckSample {
        AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
        String filePath;
        double detectedBpm = 0.0;
        std::atomic<bool> ready { false };
        std::atomic<bool> loading { false };
    } decks[2];

    std::unique_ptr<juce::Thread> loaderThread;
    CriticalSection lock;
};
```
Статус: ✅

---

## ЭТАП 2: AudioProcessor (PluginProcessor.h/.cpp)

### 2.1 Параметры APVTS
Все параметры регистрируются в createParameterLayout():

**Декa A (параметры с префиксом `a_`):**
- `a_volume` — Float [0..1], default 1.0 — громкость деки A
- `a_pitch` — Float [-24..+24 semitones], default 0 — питч-сдвиг деки A
- `a_speed` — Float [0.25..4.0], default 1.0 — постоянная скорость (без скретча)
- `a_loop` — Bool, default true — зацикливание
- `a_cue` — Bool trigger — возврат к cue-точке

**Дека B (параметры с префиксом `b_`):**
- Идентичный набор: `b_volume`, `b_pitch`, `b_speed`, `b_loop`, `b_cue`

**Физика дек:**
- `inertia` — Float [0.01..1.0], default 0.1 — момент инерции пластинки
- `slipmatFriction` — Float [0.01..1.0], default 0.5 — трение слипмата

**Скретч:**
- `scratchSensitivity` — Float [0.1..10.0], default 1.0 — чувствительность мыши к скретчу
- `scratchSmoothing` — Float [0.0..1.0], default 0.15 — сглаживание скорости (демпфирование)
- `mouseModeEnabled` — Bool, default false — режим управления мышью (вкл/выкл кнопкой)

**Кроссфейдер:**
- `crossfader` — Float [0.0..1.0], default 0.5 — позиция кроссфейдера
- `xfaderCurve` — Int [0..5], default 0 — крутизна кривой (n в формуле)
- `xfaderCurveType` — Choice {"Constant Power", "Scratch Cut", "Linear"}

**Gross Beat / Автоматизация:**
- `grossBeatEnabled` — Bool, default false
- `timeEnvSlot` — Int [0..7] — выбранный слот огибающей времени (8 пресетов)
- `volEnvSlot` — Int [0..7] — выбранный слот огибающей громкости
- `envelopeTension` — Float [0..1] — натяжение сплайна огибающих

**Эффекты:**
- `vinylCrackle` — Float [0..1] — виниловые шумы (треск/фоновый шум)
- `vinylWarp` — Float [0..1] — вобуляция питча (имитация коробленого винила)
- `outputGain` — Float [0..2], default 1.0

Статус: ✅

### 2.2 processBlock() — главный DSP цикл
```
Вход: AudioBuffer<float>& buffer, MidiBuffer& midiMessages

1. Читаем позицию хоста (AudioPlayHead) для BPM и позиции в такте
2. Обновляем скорости дек из атомарных переменных (APVTS raw pointers)
3. Обрабатываем MIDI события:
   - Note On: возможная привязка через MidiLearnManager
   - CC: обновление crossfader, scratch, volume через APVTS
4. Если effectMode:
   - Записываем входной сигнал в CircularBuffer обеих дек
5. Если grossBeatEnabled:
   - Вычисляем timeEnvValue из текущей позиции хоста и активной огибающей
   - Вычисляем volEnvValue аналогично
   - Safety Line check (Δt ≤ записанных сэмплов)
6. Для каждого сэмпла:
   a. DeckA.renderSample() → outL_A, outR_A
   b. DeckB.renderSample() → outL_B, outR_B
   c. xfade = Crossfader::compute(crossfader, xfaderCurve)
   d. outL = outL_A * xfade.gA + outL_B * xfade.gB
   e. outR = outR_A * xfade.gA + outR_B * xfade.gB
   f. Vinyl crackle (additive шум × vinylCrackle)
   g. Vinyl warp (LFO питч модуляция × vinylWarp)
   h. Умножение на outputGain
7. Обновить VU-метры (атомарные float)
8. Обновить scope ring buffer (для отображения в GUI)
```
Статус: ✅

### 2.3 Режим инструмента vs эффекта
- `isBusesLayoutSupported()` — разрешает stereo in + stereo out, и mono in disabled (режим инструмента)
- В инструментальном режиме: читаем аудио из буфера деки (загруженный файл)
- В режиме эффекта: пишем входной сигнал в CircularBuffer деки, затем читаем оттуда
- Переключение автоматическое через isBusesLayoutSupported / проверку в processBlock

Статус: ✅

### 2.4 MIDI Learn система (MidiLearnManager)
```cpp
class MidiLearnManager {
public:
    // Режим обучения: ожидаем следующий CC/Note и привязываем к параметру
    void startLearn(const String& paramID);
    void stopLearn();
    bool isLearning() const;
    String getLearningParam() const;

    // Обработка MIDI в processBlock (вызывается из процессора)
    void processMidi(const MidiMessage& msg, APVTS& apvts);

    // Сохранение/загрузка привязок в preset XML
    void saveToXml(XmlElement& xml) const;
    void loadFromXml(const XmlElement& xml);

    // Список всех привязок (для отображения в GUI)
    struct Binding {
        String paramID;
        int midiChannel;
        int ccNumber; // -1 если Note
        int noteNumber; // -1 если CC
    };
    std::vector<Binding> bindings;

private:
    bool learning = false;
    String learningParam;
};
```

**Привязываемые элементы:**
- Кроссфейдер (ось Y / CC 7 по умолчанию)
- Скретч дека A (CC или MIDI Pitch Bend)
- Скретч дека B
- Громкость дека A / B
- Play/Pause/Stop/Cue дека A / B (Note On/Off)
- Groove Beat слоты (Notes 48-55 → слоты 0-7)
- Выбор авто-пресета (Notes)
- Nudge +/- (для бит-матчинга без скретча)

Статус: ✅

### 2.5 Gross Beat огибающие
```cpp
struct EnvelopePoint {
    float x;         // позиция [0.0, 1.0] в такте
    float y;         // значение [0.0, 1.0]
    float tension;   // натяжение сплайна [-1, 1]
};

struct EnvelopePattern {
    std::vector<EnvelopePoint> points;
    String name;
    float evaluate(float phase) const; // интерполяция катмулл-ром
    float evaluateDerivative(float phase) const; // производная = скорость
};

// В процессоре: 8 слотов для Time огибающих + 8 для Volume
EnvelopePattern timePatterns[8];
EnvelopePattern volPatterns[8];
```

**Встроенные пресеты огибающих (автоматически созданные):**
1. Time: Normal (линия y=0, нормальное воспроизведение)
2. Time: Tape Stop (y убывает от 0 до 1 → остановка)
3. Time: Reverse (y убывает быстро → обратное воспроизведение)
4. Time: Stutter 1/8 (8 коротких прыжков назад)
5. Time: Stutter 1/16 (16 коротких прыжков назад)
6. Time: Vinyl Scratch Pattern (имитация реального скретча)
7. Time: Forward x2 (ускорение вперёд)
8. Time: Baby Scratch (классический бэби-скретч паттерн)

Vol огибающие:
1. Vol: Full (всегда 100%)
2. Vol: Gate 1/4 (включение каждую четверть)
3. Vol: Gate 1/8 (включение каждую восьмую)
4. Vol: Trance Gate (паттерн транс-гейта)
5. Vol: Flare Cut (остановки по паттерну флера)
6. Vol: Crab Pattern (паттерн краба)
7. Vol: Fade In (нарастание)
8. Vol: Fade Out (затухание)

Статус: ✅

### 2.6 Vinyl Crackle и Warp генераторы
```cpp
// Crackle: LFSR шум + редкие пики (поверхностные царапины)
// Warp: медленный LFO (0.1-0.5 Гц) на питч (±0.5% как коробленый винил)
// Обрабатываются в processBlock как постпроцессинг
```
Статус: ✅

---

## ЭТАП 3: GUI — PluginEditor

### 3.1 Общая концепция визуала
- Тёмный фон, стиль "профессиональный диджейский контроллер"
- Два вертикальных блока (Дека A слева, Дека B справа)
- Кроссфейдер горизонтально по центру снизу
- XY-пад (большая зона управления скретчем и кроссфейдером) по центру
- Gross Beat редактор огибающих — сворачиваемая панель снизу
- MIDI Learn кнопки рядом с каждым элементом управления

**Размер окна:** 900×600 px по умолчанию (масштабируется 75%–200%)

### 3.2 VinylComponent — вращающаяся пластинка
```cpp
class VinylComponent : public juce::Component, public juce::Timer {
    // Состояния анимации:
    float rotationAngle = 0.f;      // текущий угол [0, 2π]
    float currentSpeed = 1.0f;      // скорость вращения (из DeckProcessor)
    bool isPlaying = false;

    // Заранее вычисленные Path объекты (вне paint!):
    juce::Path outerRingPath;       // внешний ободок пластинки
    juce::Path groovesMask;         // концентрические канавки (растровый образ)
    juce::Image vinylImage;         // заранее нарисованная пластинка (кэш)
    juce::Image labelImage;         // этикетка (пользовательская или дефолтная)

    // Гало-эффект при воспроизведении:
    float glowIntensity = 0.f;

    // Waveform overview (мини-волноформа под пластинкой):
    std::vector<float> waveformPeaks;

    void timerCallback() override {
        // 60 FPS: обновить rotationAngle на основе currentSpeed
        rotationAngle += currentSpeed * (2.0f * M_PI / 60.0f * 33.33f / 60.0f);
        rotationAngle = std::fmod(rotationAngle, 2.0f * M_PI);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        // 1. Нарисовать винил через AffineTransform::rotation() (без аллокаций!)
        // 2. Нарисовать этикетку поверх с тем же углом поворота
        // 3. Нарисовать тонюсенькие концентрические дуги (канавки) — кэшировано
        // 4. Рубиновый тонарм (stylus) фиксированный — показывает позицию
        // 5. Glow effect при активном воспроизведении
    }
};
```

**Детали визуала пластинки:**
- Внешний диаметр = 90% от ширины компонента
- Матовый чёрный круг + тонкие концентрические серые дуги (имитация канавок)
- Центральная этикетка (круг 30% диаметра): пользовательская картинка или дефолтная (лого Scratcher)
- Тонарм: статичная линия от правого края к центру пластинки (угол ~45°)
- При pause: пластинка замедляется плавно (физика инерции)
- При stop: пластинка резко останавливается (симуляция руки)
- "Рука на пластинке" (Mouse Mode): лёгкое свечение пальца на этикетке

### 3.3 DeckControlPanel — панель управления декой
Для каждой деки (A и B):

```
┌──────────────────────────────────────┐
│  [■ STOP] [▶ PLAY] [|| PAUSE] [CUE] │
│  Speed: [━━━━|━━━━] ×1.0            │
│  Pitch: [━━━━|━━━━] 0 st            │
│  Volume: [━━━━━━━━|] 100%           │
│                                      │
│  Sample: "track_name.wav"  [LOAD]   │
│  BPM: 128.0  [AUTO PRESET ▾]        │
│  Loop: [ON] [|◄ START] [END ►|]     │
│  Nudge: [◄] [►]                     │
│                                      │
│  MIDI LEARN: [Scratch] [Vol] [Cue]  │
└──────────────────────────────────────┘
```

Статус: ✅

### 3.4 XYPadComponent — центральная зона скретча и кроссфейдера
```cpp
class XYPadComponent : public juce::Component,
                       public juce::MouseListener {
    // Режим работы:
    bool mouseModeActive = false;   // вкл/выкл кнопкой "MOUSE MODE"

    // Текущие позиции:
    float scratchPosNorm = 0.5f;    // [0..1] ось X
    float crossfaderNorm = 0.5f;    // [0..1] ось Y

    // Для unbounded mouse mode:
    juce::Point<float> lastMousePos;
    bool isDragging = false;

    // MIDI Learn индикатор:
    bool midiLearnActive = false;

    void mouseDown(const juce::MouseEvent& e) override {
        if (!mouseModeActive) return;
        e.source.enableUnboundedMouseMovement(true);
        isDragging = true;
        lastMousePos = e.position;
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!mouseModeActive || !isDragging) return;
        auto delta = e.position - lastMousePos;
        // deltaX → scratch speed
        // deltaY → crossfader position
        // Обновить APVTS параметры
        lastMousePos = e.position;
    }

    void mouseUp(const juce::MouseEvent& e) override {
        if (!mouseModeActive) return;
        e.source.enableUnboundedMouseMovement(false);
        isDragging = false;
        // Отпустили пластинку → физика инерции разгона
    }

    // Клавиша Cmd+R / Ctrl+R — выход из Mouse Mode
    bool keyPressed(const juce::KeyPress& key) override {
        if (key.isKeyCode('R') &&
            (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())) {
            mouseModeActive = false;
            // Обновить кнопку в editor
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override {
        // Фон: тёмный с сеткой
        // Горизонтальная линия = текущий crossfader
        // Вертикальный индикатор = текущий scratchPos
        // Перекрестие курсора (если mouseModeActive)
        // Надпись "MOUSE MODE ACTIVE" / "CLICK MOUSE MODE TO ACTIVATE"
        // Цветовые зоны: левая (дека A цвет), правая (дека B цвет)
        // Мини-подсказка: "Cmd+R / Ctrl+R to exit"
    }
};
```

**UI элементы XY пэда:**
- Размер: 300×200 px (центр экрана)
- Кнопка [MOUSE MODE] над пэдом (зелёная когда активна, серая когда нет)
- При активации: курсор скрывается, unlimited movement включается
- Cmd+R / Ctrl+R — выход, курсор возвращается
- Визуализация: тонкие линии показывают текущие позиции X и Y
- Цветная точка (dot) показывает текущую позицию
- Следы движения (trail effect, последние 20 позиций с затуханием)

Статус: ✅

### 3.5 CrossfaderStrip — горизонтальный фейдер
```cpp
class CrossfaderStrip : public juce::Component {
    // Горизонтальный слайдер на всю ширину между деками
    // Внешний вид: характерный диджейский кроссфейдер (широкий, низкий)
    // Индикатор позиции + цифровое значение
    // Кнопки: [CP] [SC] [LIN] — тип кривой
    // [CURVE ◄ ►] — настройка резкости n
    // [MIDI] — кнопка привязки MIDI
    // Кривая наглядно отображается как маленький график справа от фейдера
};
```
Статус: ✅

### 3.6 EnvelopeEditorPanel — редактор огибающих Gross Beat
```cpp
class EnvelopeEditorPanel : public juce::Component {
    // Переключение: [TIME ENV] [VOL ENV]
    // 8 слотов (кнопки 1-8)
    // Поле редактирования:
    //   - Горизонталь: 1 такт (4 доли, разлинован)
    //   - Вертикаль (Time): 0 = нормальная задержка, 1 = 8 долей назад
    //   - Вертикаль (Vol): 0% до 100%
    // Инструменты: Draw (рисование), Select, Line, Preset
    // Правый клик: контекстное меню точки (удалить, tension)
    // Safety Line: диагональ (нельзя рисовать выше неё в первые секунды)
    // Пресеты: 8 встроенных (как описано в 2.5)
    // Кнопки: [ENABLE GROSS BEAT] [LOOP 1 BAR / 2 BARS / 4 BARS]
};
```
Статус: ✅

### 3.7 VuMeterScratcher — VU-метр (идентично flopster)
- Два VU-метра (Deck A / Deck B) + основной стерео
- Адаптирован из flopster: те же 24 сегмента, dB маркировка
- Цвет: Дека A — синий/фиолетовый, Дека B — оранжевый/красный

Статус: ✅

### 3.8 ScopeDisplay — осциллоскоп (идентично flopster)
- Real-time waveform из кольцевого буфера процессора
- Triple-layer glow + outline
- Расположен в верхней центральной части экрана

Статус: ✅

### 3.9 WaveformOverview — мини-волноформа загруженного сэмпла
```cpp
class WaveformOverview : public juce::Component {
    // Отображает peak-waveform загруженного файла
    // Текущая позиция воспроизведения (вертикальная линия)
    // Зона лупа (синяя область)
    // Cue-точка (зелёный маркер)
    // Drag для установки позиции воспроизведения
};
```
Статус: ✅

### 3.10 PresetsAndSettings — панель пресетов и настроек
```
[PRESET ▾] [SAVE] [LOAD]    Inertia: [━━|━━] 0.1    Friction: [━━|━━] 0.5
[SCALE 100%▾]  [FX ◉]       Sensitivity: [━━|━━] 1.0   Smoothing: [━━|━━] 0.15
[Vinyl Crackle: ━━|━] [Vinyl Warp: ━━|━]
```
- Сохранение/загрузка пресетов (как в flopster: XML через FileChooser)
- Параметры физики: инерция, трение (для новичков — одна ручка "Feel": от "Light vinyl" до "Heavy vinyl")
- Упрощённый режим для новичков: скрывает сложные параметры, показывает только необходимое

Статус: ✅

### 3.11 CRT-эффект и стилистика (как в flopster)
- CrtEffect (ImageEffectFilter): хроматическая аберрация, зернистость плёнки, фосфорный тинт
- CrtOverlay: сканлайны, виньетирование, окантовка
- Переключается кнопкой [FX]
- Шрифты: PixelEmulator.ttf и PixgamerRegular.ttf (те же что в flopster)

**Цветовые темы (2 базовые + по 1 на каждый пресет):**
- Тема Дека A: синевато-фиолетовая (#4A6FFF)
- Тема Дека B: оранжево-красная (#FF6B35)
- Нейтральные: тёмно-серый фон, светло-серые контролы

Статус: ✅

### 3.12 MouseMode кнопка и логика Cmd+R / Ctrl+R
- Кнопка [🖱 MOUSE MODE] — центральная, хорошо заметная
- При нажатии: кнопка подсвечивается, активируется unbounded mouse movement для XY пэда
- Мышь привязывается к XY пэду (movement ограничен падом)
- Cmd+R / Ctrl+R — немедленный выход (keyPressed обработчик в Editor)
- При выходе: кнопка гаснет, курсор возвращается
- Подсказка для новичков: при первом запуске показывается tooltip "Нажмите MOUSE MODE для скретча"

Статус: ✅

### 3.13 Руководство для новичков (Onboarding)
- При первом открытии: оверлей с 3 шагами:
  1. "Загрузите трек (или используй встроенный демо)"
  2. "Нажмите PLAY, затем MOUSE MODE"
  3. "Двигайте мышь влево/вправо для скретча, вверх/вниз для кроссфейдера"
- Кнопка [SKIP] и [?] для повторного показа
- Хранится в plugin state (XML) — показывается только раз

Статус: ✅

---

## ЭТАП 4: MIDI Controller Integration

### 4.1 MIDI Learn UI
- Рядом с каждым контролом — маленькая кнопка [M] (MIDI Learn)
- При нажатии [M]: кнопка мигает синим, ожидаем MIDI CC/Note
- После получения MIDI: отображается "CC42" или "Note C3"
- Правый клик на [M]: меню {Unlearn, Set Range Min/Max, Clear}
- Глобальная кнопка [MIDI LEARN ALL] — входим в режим обучения сразу для всех

Статус: ✅

### 4.2 Привязки по умолчанию (рекомендуемые)
Показываются как "Default MIDI Map" в настройках:
```
Crossfader    → CC 7 (Volume)
Deck A Vol    → CC 1 (Mod Wheel)
Deck B Vol    → CC 11 (Expression)
Deck A Play   → Note C3 (60)
Deck B Play   → Note D3 (62)
Deck A Cue    → Note E3 (64)
Deck B Cue    → Note F3 (65)
Scratch A     → Pitch Bend (канал 1)
Scratch B     → Pitch Bend (канал 2)
GB Slot 1-8   → Notes C4-G4 (48-55)
```
Статус: ✅

---

## ЭТАП 5: Авто-пресеты и BPM Sync

### 5.1 Алгоритм BPM Sync сэмпла
```
1. Получить BPM хоста из AudioPlayHead
2. Если хост не предоставляет BPM → использовать встроенный тап-темп
3. Вычислить длину N тактов в сэмплах: barSamples = (60.0/bpm) * 4 * sampleRate
4. Выбранный пресет (например, "1 bar") задаёт количество тактов
5. loopEnd = loopStart + barSamples * barCount
6. Если loopEnd > sampleLength: предупреждение в UI
7. Установить speed = (sampleLength_target / actualSampleLength) [pitch-shift для синхры]
   ИЛИ time-stretch (без изменения питча) — выбор пользователя
```

### 5.2 Список авто-пресетов
- **"1 Bar"** — 1 такт, автоматически определяет BPM
- **"2 Bars"** — 2 такта
- **"4 Bars"** — 4 такта
- **"Half Bar"** — 2 доли
- **"1/4 Note"** — четверть такта
- **"Loop 1/2"** — первая половина файла
- **"Auto Beat"** — определяет BPM из файла (onset detection), синхр. с хостом
- **"Manual"** — пользователь вручную задаёт точки лупа

### 5.3 Простой BPM-детектор
- Onset detection по пороговому алгоритму (spectral flux)
- Только для авто-пресетов, не в реальном времени
- Запускается при загрузке файла в отдельном потоке

Статус: ✅

---

## ЭТАП 6: Встроенный демо-сэмпл и пресеты плагина

### 6.1 Встроенный демо-сэмпл
- 2-секундный луп (scratch-friendly sample: вокал или хип-хоп стаб)
- Закодирован в BinaryData через CMake (juce_add_binary_data)
- Загружается автоматически при первом открытии на обе деки
- Формат: WAV 44100 Hz, 16-bit, stereo

Статус: ✅

### 6.2 Пресеты плагина
- Сохраняются через APVTS::state + XmlElement
- Дополнительно: огибающие GB, MIDI маппинг, позиции лупов
- 8 встроенных фабричных пресетов:
  1. "DJ Mode" — оптимально для лайв диджеинга
  2. "Scratch Battle" — максимальная отзывчивость для турнтаблизма
  3. "Studio FX" — режим эффекта с GB огибающими
  4. "Smooth Mix" — плавный кроссфейдер для клуба
  5. "Vinyl Emulation" — максимум физической точности
  6. "Stutter Gate" — GB статтер эффекты включены
  7. "Reverse Loop" — реверс с GB автоматизацией
  8. "Default" — чистые настройки по умолчанию

Статус: ✅

---

## ЭТАП 7: Тестирование и полировка

### 7.1 Тесты сборки
- macOS: VST3, AU, Standalone
- Windows: VST3, Standalone (через cmake cross-compile или CI)

Статус: ✅

### 7.2 Аудио качество
- Проверка скретча на наличие алиасинга (при скоростях 0.25x, 0.5x, 1x, 2x, 3x)
- Проверка кроссфейдера: нет провала громкости в центре (constant power)
- Проверка инерционного разгона (release): плавный pitch-up swooop

Статус: ✅

### 7.3 GUI тест
- Проверка 60 FPS анимации пластинки без фризов
- Нет аллокаций в paint() (проверка через Instruments на macOS)
- Mouse Mode: нет клиппинга курсора, Cmd+R срабатывает

Статус: ✅

### 7.4 MIDI тест
- MIDI Learn для всех параметров
- Кроссфейдер из CC контроллера
- Скретч через Pitch Bend

Статус: ✅

### 7.5 Gross Beat тест
- Проверка Safety Line
- Проверка всех 8 паттернов огибающих
- Синхронизация с хостом (позиция в такте)

Статус: ✅

---

## Краткий порядок реализации (для продолжения с любой точки)

```
[0.1] Создать структуру директорий
[0.2] CMakeLists.txt (на основе flopster)
[0.3] package.json и скрипты
[1.1] version.h
[1.2] CircularBuffer.h
[1.3] HermiteInterp.h
[1.4] CrossfaderMath.h
[1.5] DeckProcessor.h/.cpp (DSP ядро без GUI)
[1.6] SampleManager.h/.cpp
[2.1] PluginProcessor.h — параметры APVTS
[2.2] PluginProcessor.cpp — processBlock
[2.3] Двойной режим (инструмент/эффект)
[2.4] MidiLearnManager.h/.cpp
[2.5] Gross Beat огибающие
[2.6] Vinyl Crackle и Warp
[3.1] PluginEditor.h — объявления компонентов
[3.2] VinylComponent (анимированная пластинка)
[3.3] DeckControlPanel
[3.4] XYPadComponent (scratch + crossfader)
[3.5] CrossfaderStrip
[3.6] EnvelopeEditorPanel
[3.7] VuMeterScratcher
[3.8] ScopeDisplay
[3.9] WaveformOverview
[3.10] PresetsAndSettings
[3.11] CRT эффект и тема
[3.12] MouseMode логика + Cmd+R
[3.13] Onboarding оверлей
[4.1] MIDI Learn UI
[4.2] Default MIDI Map
[5.1] BPM Sync алгоритм
[5.2] Авто-пресеты
[5.3] BPM детектор
[6.1] Встроенный демо-сэмпл
[6.2] Фабричные пресеты плагина
[7.1-7.5] Тестирование
```

---

## Технические риски и решения

| Риск | Решение |
|------|---------|
| Аллокации памяти в processBlock → дропауты | Строгое правило: вся аллокация только в prepareToPlay. Code review. |
| Cursor lock на разных ОС | enableUnboundedMouseMovement — JUCE API, протестировать на macOS/Windows/Linux |
| Hermite интерполяция при граничных условиях буфера | Ring buffer должен иметь 3 сэмпла "guardian" до начала и после конца |
| Safety Line нарушение в GB режиме | 5ms exponential fade-out, не останавливать аудио резко |
| BPM от хоста не всегда доступен | Встроенный tap-tempo как fallback (кнопка TAP) |
| AU формат: MIDI + Audio Input одновременно | IS_SYNTH TRUE + withInput() + переопределить isBusesLayoutSupported |
| Heavy CPU при pitch-up 3x + Hermite | Dynamic LPF снижает нагрузку; при скоростях >2x переключаться на 4-точечный Hermite |

---

*Файл обновляется по мере выполнения задач — ставить ✅ после завершения каждого пункта*
