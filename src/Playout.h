#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

#include "Data.h"
#include "ElementDesign.h"
#include "CueSequence.h"
class DesignAudio;
class Replay;
class ContinuityOutput;

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
    int offsetMs = 0;            // where in the media file this piece starts
    bool spot = false;           // a standalone ad spot (can be dragged on the timeline)
    Transition fx;               // how it moves in and out
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
    qint64 nextProgramCountdownMs() const;    // time until the next program starts (-1 when there is none)
    qint64 programEndCountdownMs() const;     // time until the playing program ends (-1 when not in a program)
    qint64 remainingMs() const;               // timeline left to play (from the playhead, or the cue point)
    int groupFirst(int segment) const;        // first / last segment of the program a segment belongs to
    int groupLast(int segment) const;
    // true when segment i+1 simply carries on in the same media file (a cut with nothing placed in between)
    bool continues(int i) const;
    qint64 breakCountdownMs() const;          // -1 when no break is coming
    QVideoSink *videoSink() const { return m_sink; }
    QAudioOutput *audio() const { return m_audio; }
    QMediaPlayer *primaryPlayer() const { return m_player; }

    // ---- timeline geometry (shared by the timeline, the viewport scrubber and the status bar) ----
    static int blockWidth(const Segment &s);            // how wide a segment is drawn, in px
    qint64 segmentStartMs(int i) const { return i >= 0 && i < m_starts.size() ? m_starts[i] : 0; }
    qint64 totalMs() const { return m_totalMs; }
    int segmentAtMs(qint64 ms, qint64 *offsetInSegment = nullptr) const;
    qint64 timelinePosMs() const;             // where the broadcast is on the timeline (0 when off air)
    qint64 playheadMs() const { return isLive() ? timelinePosMs() : m_cueMs; }  // the live position, or the cue when off air
    qint64 groupEndMs(const QString &programId) const;    // end of a program and its break/ad
    qint64 programStartMs(const QString &programId) const;
    // the same timeline as a 0..1 strip where every segment is as wide as it is on the timeline
    double msToUnit(qint64 ms) const;
    qint64 unitToMs(double unit) const;
    double unitAt(int segment, bool end) const;

    void applyAudio();                        // re-applies master + per-timeline volume and mute
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    Replay *replay();
    ContinuityOutput *continuity();
    void setReplayMix(double value){m_replayMix=qBound(0.,value,1.);applyAudio();}
    void finishReplay(){if(m_finishAfterReplay){m_finishAfterReplay=false;finishAll();}}
    struct GraphicPlayback {bool visible=false,controlled=false; qint64 time=0;QString cue;bool advancing=true;QString design;int boardStep=-1;
        Element resolved(const Element &source)const{Element e=source;if(!design.isEmpty())e.design=design;return e;}
    };
    GraphicPlayback graphicPlayback(const Element &element,qint64 timelineTime,qint64 placementStart=-1,qint64 offset=0) const;
    void triggerElementAnimation(const QString &elementId,const QString &cueId);
    void advanceElementCue(const QString &elementId);

public slots:
    void start(int segment = 0, qint64 inSegmentMs = 0);
    void stop();
    void togglePause();
    void skip();
    void breakNow();
    void jumpTo(int segment, qint64 inSegmentMs = 0);
    void seekTo(qint64 timelineMs);           // moves the broadcast; off air it only sets the cue point
    void startFromCue();                      // Start Broadcast: goes live from the cue point
    void playProgram(const QString &programId);
    void setVolume(double v);
    void setMuted(bool m);

signals:
    void segmentsChanged();
    void segmentChanged();
    void stateChanged();
    void ticked();
    void programFinished(const QString &programId);
    void audioSourceCreated(QMediaPlayer *player,QAudioOutput *output);

private slots:
    void rebuild();
    void onTick();

private:
    struct Sfx {
        QMediaPlayer *player = nullptr;
        QAudioOutput *out = nullptr;
    };

    void enter(qint64 inSegmentMs = 0);
    void advance();
    void finishAll();
    void markFinished(const QString &programId);
    void startMedia(const Segment &s, qint64 inSegmentMs);
    void syncSounds();
    void syncDesignAudio();
    DesignAudio *m_designAudio = nullptr;
    QHash<QString,QPair<QString,ElementDesign>> m_audioDesigns;
    QHash<QString,QPair<QString,qint64>> m_manualCues;
    mutable QHash<QString,QPair<QString,QJsonArray>> m_cueCache;
    mutable QHash<QString,QPair<QString,CuePlan>> m_boardCache;
    void resetSounds();
    void playSound(const Placement &pl, qint64 inMs);
    void layerAudio(const Placement &pl, Sfx &s) const;
    void stopSound(const QString &id);

    ContinuityOutput *m_continuity=nullptr;
    Library *m_lib;
    QList<Segment> m_segs;
    QVector<qint64> m_starts;
    QVector<double> m_unitStarts;   // cumulative width weights, size = segments + 1
    qint64 m_totalMs = 0;
    State m_state = Stopped;
    int m_index = -1;
    qint64 m_segElapsed = 0, m_total = 0, m_lastTick = 0;
    QHash<QString, qint64> m_finished;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QVideoSink *m_sink;
    qint64 m_pendingSeek = -1;
    qint64 m_cueMs = 0;
    QHash<QString, Sfx> m_sfx;
    QSet<QString> m_soundsOn;
    double m_volume = 0.8;
    bool m_muted = false;
    double m_replayMix=0;
    Replay *m_replay=nullptr;
    bool m_finishAfterReplay=false;
};
