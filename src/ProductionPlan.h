#pragma once

#include <QColor>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVector>

// Planning is deliberately independent of Library and Playout. References are
// resolved by the board's private preview, never by the live broadcast clock.
struct PlanEntry {
    QString id, kind, sourceId;
    qint64 durationMs=10000;
};
struct PlanCard {
    QString id, html, image;
    QRectF rect{0,180,240,140};
    QColor background{"#1d2b36"};
    bool locked=false;
};
struct PlanEndpoint {
    QString id;
    int port=0; // card: left/top/right/bottom; sequence entry: bottom (3)
    bool operator==(const PlanEndpoint &)const=default;
};
struct PlanLink {
    QString id;
    PlanEndpoint from,to;
    QString style="Solid";
};

class ProductionPlan {
public:
    static constexpr double cardTop=160;
    QVector<PlanEntry> entries;
    QVector<PlanCard> cards;
    QVector<PlanLink> links;

    qint64 durationMs()const;
    qint64 startMs(const QString &id)const;
    int entryAt(qint64 ms)const;
    bool moveEntry(const QString &id,int destination);
    bool removeEntry(const QString &id);
    bool removeCard(const QString &id);
    bool addLink(PlanLink link);
    bool setCardRect(const QString &id,QRectF rect);
    bool validEndpoint(const PlanEndpoint &endpoint)const;
    bool validate(QString *error=nullptr)const;
    QJsonObject toJson()const;
    // Transactional: malformed input never changes the caller's document.
    static bool fromJson(const QJsonObject &json,ProductionPlan *result,QString *error=nullptr);
private:
    void removeLinksFor(const QString &id);
};
