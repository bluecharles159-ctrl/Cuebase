#pragma once
#include "ElementDesign.h"
struct Element;

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
CuePlan compileCueBoard(const ElementDesign &source,const QJsonArray &board,const QJsonObject &settings={},int depth=0);
CuePlan compileAnimationCue(const ElementDesign &source,const QJsonObject &cue);
QJsonObject newCueBoardEntry(const QJsonObject &cue);
QString cueTimingLabel(const QJsonObject &cue);

QJsonObject bakeCueBoard(const Element &element);
bool canEditCueBoard(const Element &before,const Element &after,qint64 start,qint64 now,bool live,QString *reason=nullptr);

Element cueBoardContext(const Element &root,const QStringList &path);
QJsonObject cueBoardSection(const Element &root,const QStringList &path);
bool setCueBoardSection(Element &root,const QStringList &path,const QJsonObject &section);
bool setCueBoardContext(Element &root,const QStringList &path,const Element &context);
