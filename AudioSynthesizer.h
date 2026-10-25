#ifndef AUDIOSYNTHESIZER_H
#define AUDIOSYNTHESIZER_H

#include <QObject>
#include <QIODevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>

class AudioGenerator : public QIODevice {
    Q_OBJECT
public:
    explicit AudioGenerator(QObject *parent = nullptr);

    void start();
    void stop();

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override { Q_UNUSED(data); Q_UNUSED(len); return 0; }
    qint64 bytesAvailable() const override { return 4096 + QIODevice::bytesAvailable(); }

    void setEngineParameters(qreal rpm, bool throttle, bool braking, qreal speed);
    void triggerCrash();
    void setHorn(bool active);
    void setMusic(int track, bool playing, qreal volume);
    void getVisualizer(qreal *levels, int count);

private:
    qreal m_rpm = 900;
    bool m_throttle = false;
    bool m_braking = false;
    qreal m_speed = 0;
    bool m_horn = false;

    qreal m_phaseEngine = 0;
    qreal m_phaseHorn1 = 0;
    qreal m_phaseHorn2 = 0;
    qreal m_crashTimer = 0;

    // Procedural Synth Music Engine
    int m_musicTrack = 0;
    bool m_musicPlaying = true;
    qreal m_musicVolume = 0.50;
    qreal m_musicPhaseLead = 0;
    qreal m_musicPhaseBass = 0;
    qreal m_musicPhaseKick = 0;
    qint64 m_musicSampleCount = 0;
    qreal m_visualizer[16] = {0};
};

class AudioSynthesizer : public QObject {
    Q_OBJECT
public:
    explicit AudioSynthesizer(QObject *parent = nullptr);
    ~AudioSynthesizer();

    void initialize();
    void updateEngine(qreal rpm, bool throttle, bool braking, qreal speed);
    void playCrash();
    void setHorn(bool active);
    void updateMusic(int track, bool playing, qreal volume);
    void getVisualizer(qreal *levels, int count);

private:
    QAudioSink *m_audioSink = nullptr;
    AudioGenerator *m_generator = nullptr;
    bool m_initialized = false;
};

#endif // AUDIOSYNTHESIZER_H
