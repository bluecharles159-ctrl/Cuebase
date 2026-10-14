#include "ProductionPlan.h"
#include <QtTest>
#include <QJsonArray>

class PlanningTests:public QObject {
    Q_OBJECT
private slots:
    void mixedSequence(){
        ProductionPlan plan;plan.entries={{"show","Program","program-1",60000},{"spot","Ad","ad-1",15000},{"graphic","Element","element-1",5000}};
        QVERIFY(plan.validate());QCOMPARE(plan.durationMs(),80000);QCOMPARE(plan.startMs("graphic"),75000);
        QCOMPARE(plan.entryAt(-1),-1);QCOMPARE(plan.entryAt(0),0);QCOMPARE(plan.entryAt(60000),1);QCOMPARE(plan.entryAt(79999),2);QCOMPARE(plan.entryAt(80000),-1);
        QVERIFY(plan.moveEntry("graphic",0));QCOMPARE(plan.startMs("show"),5000);QCOMPARE(plan.entries.first().id,QString("graphic"));QVERIFY(!plan.moveEntry("missing",1));
    }
    void sharedPortsAndRemoval(){
        ProductionPlan plan;plan.entries={{"show","Program","program-1",60000}};
        for(const auto &id:{"a","b","c"}){PlanCard card;card.id=id;plan.cards.append(card);}
        QVERIFY(plan.addLink({"ab",{"a",2},{"b",0},"Solid"}));
        QVERIFY(plan.addLink({"ac",{"a",2},{"c",0},"Dashed"}));
        QVERIFY(plan.addLink({"cb",{"c",2},{"b",0},"Dotted"}));
        QVERIFY(!plan.addLink({"duplicate",{"b",0},{"a",2},"Solid"}));
        QVERIFY(!plan.addLink({"self",{"a",0},{"a",3},"Solid"}));
        QVERIFY(!plan.addLink({"missing",{"a",0},{"absent",3},"Solid"}));
        QVERIFY(!plan.addLink({"wrong-port",{"a",0},{"show",0},"Solid"}));
        QVERIFY(plan.addLink({"timeline",{"a",1},{"show",3},"Solid"}));
        QVERIFY(plan.validate());QVERIFY(plan.removeEntry("show"));QCOMPARE(plan.links.size(),3);
        QVERIFY(plan.removeCard("b"));QCOMPARE(plan.links.size(),1);QCOMPARE(plan.links.first().id,QString("ac"));
    }
    void cardGeometryAndLock(){
        ProductionPlan plan;PlanCard card;card.id="note";plan.cards.append(card);
        QVERIFY(plan.setCardRect("note",QRectF(40,20,20,20)));QCOMPARE(plan.cards.first().rect,QRectF(40,160,100,70));
        plan.cards.first().locked=true;QVERIFY(!plan.setCardRect("note",QRectF(80,300,200,100)));QVERIFY(!plan.removeCard("note"));
        plan.cards.first().locked=false;QVERIFY(!plan.setCardRect("note",QRectF(1e20,300,200,100)));QVERIFY(plan.validate());
    }
    void roundTripAndTransactionalRead(){
        ProductionPlan plan;plan.entries={{"show","Program","source",1000}};PlanCard card;card.id="card";card.html="<p><b>Next segment</b></p>";card.image="images/reference.png";card.background=QColor("#80334455");plan.cards.append(card);QVERIFY(plan.addLink({"link",{"card",1},{"show",3},"Dashed"}));
        const auto json=plan.toJson();ProductionPlan restored;QString error;QVERIFY2(ProductionPlan::fromJson(json,&restored,&error),qPrintable(error));QCOMPARE(restored.toJson(),json);
        auto bad=json;auto entries=bad["entries"].toArray();auto entry=entries[0].toObject();entry["durationMs"]=-1;entries[0]=entry;bad["entries"]=entries;QVERIFY(!ProductionPlan::fromJson(bad,&restored,&error));QVERIFY(!error.isEmpty());QCOMPARE(restored.toJson(),json);
        bad=json;auto cards=bad["cards"].toArray();auto invalidCard=cards[0].toObject();invalidCard["y"]=0;cards[0]=invalidCard;bad["cards"]=cards;QVERIFY(!ProductionPlan::fromJson(bad,&restored));QCOMPARE(restored.toJson(),json);
        bad=json;bad["version"]=2;QVERIFY(!ProductionPlan::fromJson(bad,&restored));
    }
};
QTEST_APPLESS_MAIN(PlanningTests)
#include "PlanningTests.moc"
