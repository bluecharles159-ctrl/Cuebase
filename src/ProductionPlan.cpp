#include "ProductionPlan.h"
#include <QJsonArray>
#include <QSet>
#include <QUuid>
#include <cmath>
#include <limits>
#include <utility>

namespace {
bool reject(QString *error,const QString &message){if(error)*error=message;return false;}
bool integer(double value){return std::isfinite(value)&&std::floor(value)==value;}
bool validRect(QRectF r){return std::isfinite(r.x())&&std::isfinite(r.y())&&std::isfinite(r.width())&&std::isfinite(r.height())&&qAbs(r.x())<=1000000&&r.y()>=ProductionPlan::cardTop&&r.bottom()<=1000000&&r.width()>=100&&r.height()>=70&&r.width()<=10000&&r.height()<=10000;}
QString endpointKey(const PlanEndpoint &e){return e.id+":"+QString::number(e.port);}
QString linkKey(const PlanLink &link){const auto a=endpointKey(link.from),b=endpointKey(link.to);return a<b?a+"|"+b:b+"|"+a;}
}

qint64 ProductionPlan::durationMs()const{qint64 result=0;for(const auto &entry:entries){if(entry.durationMs<1||entry.durationMs>std::numeric_limits<qint64>::max()-result)return -1;result+=entry.durationMs;}return result;}
qint64 ProductionPlan::startMs(const QString &id)const{qint64 result=0;for(const auto &entry:entries){if(entry.id==id)return result;if(entry.durationMs<1||entry.durationMs>std::numeric_limits<qint64>::max()-result)return -1;result+=entry.durationMs;}return -1;}
int ProductionPlan::entryAt(qint64 ms)const{if(ms<0)return -1;for(int i=0;i<entries.size();++i){if(entries[i].durationMs<1)return -1;if(ms<entries[i].durationMs)return i;ms-=entries[i].durationMs;}return -1;}
bool ProductionPlan::moveEntry(const QString &id,int destination){for(int i=0;i<entries.size();++i)if(entries[i].id==id){const int target=qBound(0,destination,int(entries.size())-1);if(i==target)return false;entries.move(i,target);return true;}return false;}
void ProductionPlan::removeLinksFor(const QString &id){links.removeIf([&](const PlanLink &l){return l.from.id==id||l.to.id==id;});}
bool ProductionPlan::removeEntry(const QString &id){for(int i=0;i<entries.size();++i)if(entries[i].id==id){entries.removeAt(i);removeLinksFor(id);return true;}return false;}
bool ProductionPlan::removeCard(const QString &id){for(int i=0;i<cards.size();++i)if(cards[i].id==id){if(cards[i].locked)return false;cards.removeAt(i);removeLinksFor(id);return true;}return false;}
bool ProductionPlan::setCardRect(const QString &id,QRectF rect){rect=rect.normalized();rect.setSize(QSizeF(qBound(100.,rect.width(),10000.),qBound(70.,rect.height(),10000.)));if(rect.top()<cardTop)rect.moveTop(cardTop);if(!validRect(rect))return false;for(auto &card:cards)if(card.id==id){if(card.locked||card.rect==rect)return false;card.rect=rect;return true;}return false;}
bool ProductionPlan::validEndpoint(const PlanEndpoint &e)const{if(e.port<0||e.port>3)return false;for(const auto &card:cards)if(card.id==e.id)return true;for(const auto &entry:entries)if(entry.id==e.id)return e.port==3;return false;}
bool ProductionPlan::addLink(PlanLink link){if(!validEndpoint(link.from)||!validEndpoint(link.to)||link.from.id==link.to.id)return false;if(link.style!="Solid"&&link.style!="Dashed"&&link.style!="Dotted")return false;const auto key=linkKey(link);for(const auto &existing:links)if(linkKey(existing)==key)return false;if(link.id.isEmpty())link.id=QUuid::createUuid().toString(QUuid::WithoutBraces);for(const auto &entry:entries)if(entry.id==link.id)return false;for(const auto &card:cards)if(card.id==link.id)return false;for(const auto &existing:links)if(existing.id==link.id)return false;links.append(link);return true;}

bool ProductionPlan::validate(QString *error)const{
    if(entries.size()>10000||cards.size()>10000||links.size()>50000)return reject(error,"This planning board exceeds the supported item count.");
    QSet<QString> ids;
    auto claim=[&](const QString &id){if(id.isEmpty()||id.size()>128||ids.contains(id)||id.contains('|')||id.contains(':'))return false;ids.insert(id);return true;};
    for(const auto &entry:entries){if(!claim(entry.id)||entry.sourceId.isEmpty()||entry.sourceId.size()>128)return reject(error,"A plan item has an invalid or duplicate identifier.");if(entry.kind!="Program"&&entry.kind!="Ad"&&entry.kind!="Element")return reject(error,"A plan item has an unsupported source type.");if(entry.durationMs<1||entry.durationMs>2147483647)return reject(error,"Every plan item needs a finite, positive duration.");}
    for(const auto &card:cards){if(!claim(card.id)||!validRect(card.rect)||!card.background.isValid()||card.html.size()>1048576||card.image.size()>32768)return reject(error,"A note card has invalid geometry or content.");}
    QSet<QString> connections;
    for(const auto &link:links){const auto key=linkKey(link);if(!claim(link.id)||!validEndpoint(link.from)||!validEndpoint(link.to)||link.from.id==link.to.id||connections.contains(key))return reject(error,"A board connection has a missing, duplicate or invalid endpoint.");if(link.style!="Solid"&&link.style!="Dashed"&&link.style!="Dotted")return reject(error,"A connection has an unsupported line style.");connections.insert(key);}
    if(durationMs()<0)return reject(error,"The plan duration is invalid.");if(error)error->clear();return true;
}
QJsonObject ProductionPlan::toJson()const{
    QJsonArray es,cs,ls;
    for(const auto &e:entries)es.append(QJsonObject{{"id",e.id},{"kind",e.kind},{"source",e.sourceId},{"durationMs",double(e.durationMs)}});
    for(const auto &c:cards)cs.append(QJsonObject{{"id",c.id},{"html",c.html},{"image",c.image},{"x",c.rect.x()},{"y",c.rect.y()},{"width",c.rect.width()},{"height",c.rect.height()},{"background",c.background.name(QColor::HexArgb)},{"locked",c.locked}});
    for(const auto &l:links)ls.append(QJsonObject{{"id",l.id},{"from",l.from.id},{"fromPort",l.from.port},{"to",l.to.id},{"toPort",l.to.port},{"style",l.style}});
    return {{"version",1},{"entries",es},{"cards",cs},{"links",ls}};
}
bool ProductionPlan::fromJson(const QJsonObject &json,ProductionPlan *result,QString *error){
    if(!result)return reject(error,"No planning document was supplied.");
    if(json["version"].toDouble()!=1||!json["entries"].isArray()||!json["cards"].isArray()||!json["links"].isArray())return reject(error,"This planning board has an unsupported format.");
    if(json["entries"].toArray().size()>10000||json["cards"].toArray().size()>10000||json["links"].toArray().size()>50000)return reject(error,"This planning board exceeds the supported item count.");
    ProductionPlan candidate;
    for(const auto value:json["entries"].toArray()){if(!value.isObject())return reject(error,"Invalid plan item.");const auto o=value.toObject();const auto duration=o["durationMs"].toDouble(-1);if(!integer(duration)||duration<1||duration>2147483647)return reject(error,"Invalid plan duration.");candidate.entries.append({o["id"].toString(),o["kind"].toString(),o["source"].toString(),qint64(duration)});}
    for(const auto value:json["cards"].toArray()){if(!value.isObject())return reject(error,"Invalid note card.");const auto o=value.toObject();for(const auto &field:{"x","y","width","height"})if(!o[field].isDouble())return reject(error,"Missing note card geometry.");PlanCard c;c.id=o["id"].toString();c.html=o["html"].toString();c.image=o["image"].toString();c.rect={o["x"].toDouble(),o["y"].toDouble(),o["width"].toDouble(),o["height"].toDouble()};c.background=QColor(o["background"].toString());c.locked=o["locked"].toBool();candidate.cards.append(c);}
    for(const auto value:json["links"].toArray()){if(!value.isObject())return reject(error,"Invalid connection.");const auto o=value.toObject();const auto a=o["fromPort"].toDouble(-1),b=o["toPort"].toDouble(-1);if(!integer(a)||!integer(b)||a<0||a>3||b<0||b>3)return reject(error,"Invalid connection port.");candidate.links.append({o["id"].toString(),{o["from"].toString(),int(a)},{o["to"].toString(),int(b)},o["style"].toString()});}
    if(!candidate.validate(error))return false;*result=std::move(candidate);return true;
}
