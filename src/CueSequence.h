#pragma once
#include "ElementDesign.h"

struct CueStep {
    QString entryId, cueId, name, design;
    int iteration=0;
    qint64 start=0,end=1,holdMs=0;
    bool manualHold=false;
    int loopTo=-1;
    qint64 duration() const {return manualHold?-1:end-start+holdMs;}
};
struct CueSample {
    bool visible=false,advancing=false;
    qint64 frame=0;
    int step=-1;
};
struct CuePlan {
    QVector<CueStep> steps;
    QString error;
    qint64 duration() const;
    CueSample sample(qint64 elapsed,int firstStep=0) const;
    int next(int step) const;
};
CuePlan compileCueBoard(const ElementDesign &source,const QJsonArray &board);
CuePlan compileAnimationCue(const ElementDesign &source,const QJsonObject &cue);
QJsonObject newCueBoardEntry(const QJsonObject &cue);
QString cueTimingLabel(const QJsonObject &cue);
