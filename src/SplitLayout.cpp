#include "SplitLayout.h"
#include <QJsonObject>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

QStringList SplitLayout::layouts(){return {"Single","Side by side","Stacked","Host and guests","Four-way"};}
QStringList SplitLayout::transitions(){return {"Dissolve","Slide left","Slide up","Wipe","Iris"};}
int SplitLayout::paneCount(const QString &layout){
    return layout=="Four-way"?4:layout=="Host and guests"?3:layout=="Single"?1:2;
}
QString SplitLayout::validate(const QJsonArray &shots){
    QVector<QPair<qint64,qint64>> ranges;
    for(const auto entry:shots){
        const auto shot=entry.toObject();const qint64 start=shot.value("start").toInteger(),end=shot.value("end").toInteger();
        const auto layout=shot.value("layout").toString();
        if(start<0 || end<=start)return "End time must follow start time.";
        if(!layouts().contains(layout)||!transitions().contains(shot.value("transition").toString()))return "Select a layout and transition.";
        const auto sources=shot.value("sources").toArray();
        if(sources.size()!=paneCount(layout))return "Select a source for each pane.";
        for(const auto s:sources)if(s.toString().isEmpty())return "Select a source for each pane.";
        ranges.append({start,end});
    }
    std::sort(ranges.begin(),ranges.end());
    for(qsizetype i=1;i<ranges.size();++i)if(ranges[i].first<ranges[i-1].second)return "Two layouts overlap. Adjust their start or end time.";
    return {};
}

namespace {
QVector<QRectF> panes(const QString &layout){
    if(layout=="Side by side")return {{0,0,.5,1},{.5,0,.5,1}};
    if(layout=="Stacked")return {{0,0,1,.5},{0,.5,1,.5}};
    if(layout=="Four-way")return {{0,0,.5,.5},{.5,0,.5,.5},{0,.5,.5,.5},{.5,.5,.5,.5}};
    if(layout=="Host and guests")return {{0,0,.6,1},{.6,0,.4,.5},{.6,.5,.4,.5}};
    return {{0,0,1,1}};
}
QImage picture(const QJsonObject &shot,QSize size,const SplitLayout::Source &get){
    QImage frame(size,QImage::Format_RGB32);frame.fill(Qt::black);
    QPainter p(&frame);p.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto cells=panes(shot.value("layout").toString());const auto sources=shot.value("sources").toArray();
    for(int i=0;i<cells.size();++i){
        const auto c=cells[i];
        const int left=qRound(c.left()*size.width()),top=qRound(c.top()*size.height());
        const QRect target(left,top,qRound(c.right()*size.width())-left,qRound(c.bottom()*size.height())-top);
        if(target.isEmpty())continue;
        const auto image=get(shot.isEmpty()?QStringLiteral("program"):sources.at(i).toString(),target.size());
        if(image.isNull())continue;
        QSizeF crop=image.size();crop.scale(target.size(),Qt::KeepAspectRatioByExpanding);
        const double scale=image.width()/crop.width();
        QSizeF box=QSizeF(target.size())*scale;
        p.drawImage(target,image,QRectF((image.width()-box.width())/2,(image.height()-box.height())/2,box.width(),box.height()));
    }
    return frame;
}
}

bool SplitLayout::draw(QPainter &p,const QRect &rect,const QJsonArray &shots,qint64 time,const Source &source){
    QJsonObject active,previous;bool adjacentNext=false;
    for(auto item:shots){auto s=item.toObject();
        if(s.value("start").toInteger()<=time && time<s.value("end").toInteger()){active=s;break;}}
    if(active.isEmpty() || rect.isEmpty())return false;
    const qint64 start=active.value("start").toInteger(),end=active.value("end").toInteger();
    for(auto item:shots){const auto s=item.toObject();
        if(s.value("end").toInteger()==start)previous=s;
        if(s.value("start").toInteger()==end)adjacentNext=true;
    }
    const qint64 fade=qMin((end-start)/2,qBound(qint64(0),active.value("fade").toInteger(350),qint64(3000)));
    const bool leaving=!adjacentNext && fade>0 && end-time<fade;
    const bool entering=fade>0 && time-start<fade;
    const auto frame=picture(active,rect.size(),source);
    if(!entering && !leaving){p.drawImage(rect,frame);return true;}
    const auto other=picture(leaving?QJsonObject():previous,rect.size(),source);
    const double t=leaving?double(end-time)/fade:double(time-start)/fade;
    const double ease=t*t*(3.-2.*t);
    const auto transition=active.value("transition").toString();
    p.save();p.setClipRect(rect,Qt::IntersectClip);p.drawImage(rect,other);
    if(transition=="Slide left")p.translate((1-ease)*rect.width(),0);
    else if(transition=="Slide up")p.translate(0,(1-ease)*rect.height());
    else if(transition=="Wipe")p.setClipRect(QRectF(rect.x(),rect.y(),rect.width()*ease,rect.height()),Qt::IntersectClip);
    else if(transition=="Iris"){
        const double radius=std::hypot(rect.width(),rect.height())*.5*ease;
        QPainterPath path;path.addEllipse(QRectF(rect).center(),radius,radius);p.setClipPath(path,Qt::IntersectClip);
    }else p.setOpacity(p.opacity()*ease);
    p.drawImage(rect,frame);p.restore();return true;
}
