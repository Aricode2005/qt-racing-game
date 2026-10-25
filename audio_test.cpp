#include <QCoreApplication>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QDebug>

int main(int argc, char *argv[]) {
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    qDebug() << "Default audio output:" << QMediaDevices::defaultAudioOutput().description();
    return 0;
}
