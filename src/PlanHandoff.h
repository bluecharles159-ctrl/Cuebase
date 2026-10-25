#pragma once
#include "Data.h"
#include "ProductionPlan.h"
#include <QObject>
class Playout;

// An explicit, checked append transaction. Never starts or stops broadcast.
class PlanHandoff : public QObject {
    Q_OBJECT
public:
    PlanHandoff(Library *library, Playout *broadcast, QObject *parent=nullptr);
    bool validate(const ProductionPlan &plan, QString *error=nullptr) const;
    bool append(const ProductionPlan &plan, QString *error=nullptr);
    bool undo(QString *error=nullptr);
    bool canUndo() const;
signals:
    void changed();
private:
    Library *m_library;
    Playout *m_broadcast;
    QList<ProgramItem> m_before;
    bool m_hasUndo=false, m_writing=false;
};
