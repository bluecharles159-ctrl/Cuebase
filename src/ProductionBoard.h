#pragma once
#include "ProductionPlan.h"
#include <QDialog>
#include <memory>
class Library;
class Playout;
class ProductionBoard:public QDialog {
    Q_OBJECT
public:
    explicit ProductionBoard(Library *library,QWidget *parent=nullptr);
    ~ProductionBoard()override;
    const ProductionPlan &document()const;
    bool applyDocument(const ProductionPlan &plan,QString *error=nullptr);
    bool appendSource(const QString &kind,const QString &sourceId,int before=-1);
    bool savePlan(QString *error=nullptr);
    Library *previewLibrary()const;
    Playout *previewPlayout()const;
    void previewAt(qint64 milliseconds,bool play=false);
    void undoPlan();
    void redoPlan();
    void setBroadcastPlayout(Playout *broadcast);
    void done(int result)override;
protected:
    void paintEvent(QPaintEvent *)override;
    void showEvent(QShowEvent *)override;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
