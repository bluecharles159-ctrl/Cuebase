#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>

#include "Data.h"

class QAudioOutput;
class QMediaPlayer;
class QVideoSink;

// One playable piece of the timeline: a program, a break bumper or an ad.
struct Segment {
    enum Type { Program, Break, Ad } type = Program;
    int part = 0;                // Break: 0 = break in, 1 = break out
    QString programId;           // the program this segment belongs to
    QString title, kind, file, text, color;
    int ms = 0;
};

// The playout engine: turns the program planner into a timeline and plays it.
class Playout : public QObject {
    Q_OBJECT
public:
    enum State { Stopped, Playing, Paused };

    Playout(Library *lib, QObject *parent = nullptr);

    State state() const { return m_state; }
    bool isLive() const { return m_state != Stopped; }
    const QList<Segment> &segments() const { return m_segs; }
    int index() const { return m_state == Stopped ? -1 : m_index; }
    const Segment *current() const;
    qint64 segmentElapsed() const { return m_segElapsed; }
    qint64 totalElapsed() const { return m_total; }
    int currentProgramIndex() const;          // index in Library::programs, -1 when off air
    qint64 finishedAt(const QString &programId) const { return m_finished.value(programId, 0); }
    bool canBreak() const;                    // a break/ad follows the current program
    qint64 breakCountdownMs() const;          // -1 when no break is coming
    QVideoSink *videoSink() const { return m_sink; }
    QAudioOutput *audio() const { return m_audio; }

public slots:
    void start(int segment = 0);
    void stop();
    void togglePause();
    void skip();
    void breakNow();
    void jumpTo(int segment);
    void playProgram(const QString &programId);

signals:
    void segmentsChanged();
    void segmentChanged();
    void stateChanged();
    void ticked();
    void programFinished(const QString &programId);

private slots:
    void rebuild();
    void onTick();

private:
    void enter();
    void advance();
    void finishAll();
    void markFinished(const QString &programId);
    void startMedia(const Segment &s);

    Library *m_lib;
    QList<Segment> m_segs;
    State m_state = Stopped;
    int m_index = -1;
    qint64 m_segElapsed = 0, m_total = 0, m_lastTick = 0;
    QHash<QString, qint64> m_finished;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QVideoSink *m_sink;
};
