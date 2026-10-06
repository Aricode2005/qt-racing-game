#include "AudioSynthesizer.h"
#include <QMediaDevices>
#include <QtMath>
#include <QRandomGenerator>

AudioGenerator::AudioGenerator(QObject *parent)
    : QIODevice(parent)
{
}

void AudioGenerator::start() {
    open(QIODevice::ReadOnly);
}

void AudioGenerator::stop() {
    close();
}

void AudioGenerator::setEngineParameters(qreal rpm, bool throttle, bool braking, qreal speed) {
    m_rpm = rpm;
    m_throttle = throttle;
    m_braking = braking;
    m_speed = speed;
}

void AudioGenerator::triggerCrash() {
    m_crashTimer = 0.45; // 450ms crash sound
}

void AudioGenerator::setHorn(bool active) {
    m_horn = active;
}

void AudioGenerator::setMusic(int track, bool playing, qreal volume) {
    m_musicTrack = track;
    m_musicPlaying = playing;
    m_musicVolume = qBound(0.0, volume, 1.0);
}

void AudioGenerator::getVisualizer(qreal *levels, int count) {
    if (!levels) return;
    for (int i = 0; i < count && i < 16; ++i) {
        levels[i] = m_visualizer[i];
    }
}

qint64 AudioGenerator::readData(char *data, qint64 maxlen) {
    // Generate 16-bit PCM Stereo @ 44100 Hz
    const int sampleRate = 44100;
    int samplesToGenerate = maxlen / 4; // 2 channels * 2 bytes
    qint16 *out = reinterpret_cast<qint16*>(data);

    qreal engineFreq = (m_rpm / 60.0) * 2.0; // 4-cylinder firing rate
    qreal engineVol = m_throttle ? 0.35 : 0.18;

    const int stepSamples = 5292; // ~125 BPM 16th-note step

    // Melodic pitch tables for 4 distinct synthwave tracks
    static const qreal leadNotesTrack0[8] = { 293.66, 349.23, 440.00, 523.25, 440.00, 349.23, 391.99, 440.00 }; // D minor
    static const qreal leadNotesTrack1[8] = { 440.00, 523.25, 659.25, 587.33, 523.25, 440.00, 392.00, 440.00 }; // A minor Outrun
    static const qreal leadNotesTrack2[8] = { 349.23, 440.00, 523.25, 587.33, 698.46, 587.33, 523.25, 440.00 }; // F major Sunset
    static const qreal leadNotesTrack3[8] = { 329.63, 392.00, 493.88, 587.33, 659.25, 587.33, 493.88, 392.00 }; // E minor Overdrive

    static const qreal bassNotesTrack0[4] = { 73.42, 87.31, 98.00, 110.00 };
    static const qreal bassNotesTrack1[4] = { 55.00, 65.41, 73.42, 82.41 };
    static const qreal bassNotesTrack2[4] = { 87.31, 116.54, 130.81, 73.42 };
    static const qreal bassNotesTrack3[4] = { 82.41, 98.00, 110.00, 123.47 };

    qreal curBassEnergy = 0;
    qreal curLeadEnergy = 0;
    qreal curHihatEnergy = 0;

    for (int i = 0; i < samplesToGenerate; ++i) {
        // 1. Engine sound synthesis (fundamental + harmonics + rumble)
        m_phaseEngine += (2.0 * M_PI * engineFreq) / sampleRate;
        if (m_phaseEngine > 2.0 * M_PI) m_phaseEngine -= 2.0 * M_PI;

        qreal sampleEngine = qSin(m_phaseEngine) * 0.6
                           + qSin(m_phaseEngine * 2.0) * 0.3
                           + qSin(m_phaseEngine * 3.0) * 0.15;

        // Subtle exhaust rumble
        qreal noise = ((QRandomGenerator::global()->generate() % 1000) / 1000.0 - 0.5) * 0.08;
        qreal totalSample = (sampleEngine + noise) * engineVol;

        // 2. Tire screech sound on heavy braking at speed
        if (m_braking && m_speed > 40) {
            qreal screechNoise = ((QRandomGenerator::global()->generate() % 1000) / 1000.0 - 0.5) * 0.25;
            totalSample += screechNoise;
        }

        // 3. Horn sound (400 Hz + 500 Hz dual-tone)
        if (m_horn) {
            m_phaseHorn1 += (2.0 * M_PI * 415.0) / sampleRate;
            m_phaseHorn2 += (2.0 * M_PI * 520.0) / sampleRate;
            if (m_phaseHorn1 > 2.0 * M_PI) m_phaseHorn1 -= 2.0 * M_PI;
            if (m_phaseHorn2 > 2.0 * M_PI) m_phaseHorn2 -= 2.0 * M_PI;
            totalSample += (qSin(m_phaseHorn1) + qSin(m_phaseHorn2)) * 0.22;
        }

        // 4. Crash impact sound
        if (m_crashTimer > 0) {
            qreal crashNoise = ((QRandomGenerator::global()->generate() % 1000) / 1000.0 - 0.5) * (m_crashTimer / 0.45) * 0.6;
            totalSample += crashNoise;
            m_crashTimer -= (1.0 / sampleRate);
        }

        // 5. Procedural 80s Synthwave Music Radio
        if (m_musicPlaying && m_musicVolume > 0.01) {
            int step = (m_musicSampleCount / stepSamples) % 32;
            qreal stepProgress = (m_musicSampleCount % stepSamples) / (qreal)stepSamples;

            int chordIdx = (step / 8) % 4;
            int melodyIdx = step % 8;

            int trackIdx = qBound(0, m_musicTrack, 3);
            qreal bassFreq = (trackIdx == 0) ? bassNotesTrack0[chordIdx] :
                            ((trackIdx == 1) ? bassNotesTrack1[chordIdx] :
                            ((trackIdx == 2) ? bassNotesTrack2[chordIdx] : bassNotesTrack3[chordIdx]));
            qreal leadFreq = (trackIdx == 0) ? leadNotesTrack0[melodyIdx] :
                            ((trackIdx == 1) ? leadNotesTrack1[melodyIdx] :
                            ((trackIdx == 2) ? leadNotesTrack2[melodyIdx] : leadNotesTrack3[melodyIdx]));

            // Pumping 16th-note sawtooth bassline
            m_musicPhaseBass += (2.0 * M_PI * bassFreq) / sampleRate;
            if (m_musicPhaseBass > 2.0 * M_PI) m_musicPhaseBass -= 2.0 * M_PI;
            qreal bassWave = (m_musicPhaseBass / M_PI) - 1.0;
            qreal bassEnv = qMax(0.0, 1.0 - stepProgress * 1.6);
            qreal bassSample = bassWave * bassEnv * 0.40;
            curBassEnergy += qAbs(bassSample);

            // Singing melody lead synth with chorus harmonic
            m_musicPhaseLead += (2.0 * M_PI * leadFreq) / sampleRate;
            if (m_musicPhaseLead > 2.0 * M_PI) m_musicPhaseLead -= 2.0 * M_PI;
            qreal leadWave = (qSin(m_musicPhaseLead) > 0 ? 0.35 : -0.35)
                           + qSin(m_musicPhaseLead * 2.003) * 0.22;
            qreal leadEnv = qMax(0.0, 1.0 - stepProgress * 0.85);
            qreal leadSample = leadWave * leadEnv * 0.32;
            curLeadEnergy += qAbs(leadSample);

            // Punchy kick drum on beats 0, 4, 8, 12...
            qreal kickSample = 0;
            if (step % 4 == 0) {
                qreal kickFreq = 42.0 + 95.0 * qMax(0.0, 1.0 - stepProgress * 3.5);
                m_musicPhaseKick += (2.0 * M_PI * kickFreq) / sampleRate;
                if (m_musicPhaseKick > 2.0 * M_PI) m_musicPhaseKick -= 2.0 * M_PI;
                kickSample = qSin(m_musicPhaseKick) * qMax(0.0, 1.0 - stepProgress * 2.2) * 0.50;
                curBassEnergy += qAbs(kickSample);
            }

            // Crisp 16th-note hi-hat tick
            qreal hihat = ((QRandomGenerator::global()->generate() % 1000) / 1000.0 - 0.5)
                        * qMax(0.0, 1.0 - stepProgress * 5.0) * 0.12;
            curHihatEnergy += qAbs(hihat);

            qreal musicSample = (bassSample + leadSample + kickSample + hihat) * m_musicVolume;
            totalSample += musicSample;
            m_musicSampleCount++;
        }

        // Clamp to 16-bit range
        totalSample = qBound(-1.0, totalSample, 1.0);
        qint16 val = static_cast<qint16>(totalSample * 32767);

        // Stereo out
        *out++ = val;
        *out++ = val;
    }

    // Update 16-band visualizer spectrum smoothly
    if (samplesToGenerate > 0) {
        qreal bLvl = qMin(1.0, (curBassEnergy / samplesToGenerate) * 4.2);
        qreal lLvl = qMin(1.0, (curLeadEnergy / samplesToGenerate) * 5.5);
        qreal hLvl = qMin(1.0, (curHihatEnergy / samplesToGenerate) * 8.0);

        for (int k = 0; k < 16; ++k) {
            qreal target = 0;
            if (k < 4) target = bLvl * (0.8 + 0.2 * (k % 2));
            else if (k < 11) target = lLvl * (0.75 + 0.25 * ((k * 3) % 4) / 4.0);
            else target = hLvl * (0.7 + 0.3 * (k % 3) / 3.0);

            if (!m_musicPlaying) target = 0;
            m_visualizer[k] = m_visualizer[k] * 0.65 + target * 0.35;
        }
    }

    return samplesToGenerate * 4;
}

AudioSynthesizer::AudioSynthesizer(QObject *parent)
    : QObject(parent)
{
}

AudioSynthesizer::~AudioSynthesizer() {
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
    }
    if (m_generator) {
        m_generator->stop();
        delete m_generator;
    }
}

void AudioSynthesizer::initialize() {
    if (m_initialized) return;

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (defaultDevice.isNull()) {
        return;
    }

    m_audioSink = new QAudioSink(defaultDevice, format, this);
    m_generator = new AudioGenerator(this);
    m_generator->start();
    m_audioSink->start(m_generator);

    m_initialized = true;
}

void AudioSynthesizer::updateEngine(qreal rpm, bool throttle, bool braking, qreal speed) {
    if (m_generator) {
        m_generator->setEngineParameters(rpm, throttle, braking, speed);
    }
}

void AudioSynthesizer::playCrash() {
    if (m_generator) {
        m_generator->triggerCrash();
    }
}

void AudioSynthesizer::setHorn(bool active) {
    if (m_generator) {
        m_generator->setHorn(active);
    }
}

void AudioSynthesizer::updateMusic(int track, bool playing, qreal volume) {
    if (m_generator) {
        m_generator->setMusic(track, playing, volume);
    }
}

void AudioSynthesizer::getVisualizer(qreal *levels, int count) {
    if (m_generator) {
        m_generator->getVisualizer(levels, count);
    }
}
