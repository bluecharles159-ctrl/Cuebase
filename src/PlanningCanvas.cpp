#include "PlanningCanvas.h"
#include "UiControls.h"
#include <QColorDialog>
#include <QCache>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFontDialog>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainterPath>
#include <QPointer>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <cmath>

namespace {
QString newPlanId(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
QString timeLabel(qint64 ms){return QString("%1:%2").arg(ms/60000).arg(ms/1000%60,2,10,QChar('0'));}
QString portKey(const PlanEndpoint &e){return e.id+":"+QString::number(e.port);}
QPointF direction(int port){return port==0?QPointF(-1,0):port==1?QPointF(0,-1):port==2?QPointF(1,0):QPointF(0,1);}
QPainterPath connection(QPointF a,int ap,QPointF b,int bp){const double reach=qBound(35.,QLineF(a,b).length()*.45,220.);QPainterPath p(a);p.cubicTo(a+direction(ap)*reach,b+direction(bp)*reach,b);return p;}
constexpr auto sourceMime="application/x-cuebase-plan-source";
}

struct PlanningCanvas::Impl {
    PlanningCanvas *q;
    ProductionPlan doc,before;
    QVector<QRectF> timeline;
    QRectF band,plus,rubber;
    QString selected,hover,dragId,editingId;
    QPointF press,last;
    QRectF original;
    PlanEndpoint origin;
    int edges=0,phase=0;
    enum Gesture {None,Create,Move,Resize,Link,Entry,Pan} gesture=None;
    qint64 playhead=0;
    double timelineScale=1;
    QPoint scrollStart,mouseStart;
    QGraphicsProxyWidget *proxy=nullptr;
    QPointer<QTextEdit> editor;
    QCache<QString,QPixmap> images{32768}; // KiB: keep note-image storage below 32 MiB.
    QTimer dashTimer;
    explicit Impl(PlanningCanvas *owner):q(owner){dashTimer.setInterval(100);QObject::connect(&dashTimer,&QTimer::timeout,q,[this]{for(const auto &c:doc.cards)if(c.html.isEmpty()&&c.image.isEmpty()){phase=(phase+1)%16;q->viewport()->update();break;}});dashTimer.start();}
    PlanCard *card(const QString &id){for(auto &c:doc.cards)if(c.id==id)return &c;return nullptr;}
    QString cardAt(QPointF p)const{for(auto it=doc.cards.crbegin();it!=doc.cards.crend();++it)if(it->rect.adjusted(-7,-7,7,7).contains(p))return it->id;return {};}
    int entryAt(QPointF p)const{for(int i=0;i<timeline.size();++i)if(timeline[i].contains(p))return i;return -1;}
    QPointF port(const PlanEndpoint &e)const{for(const auto &c:doc.cards)if(c.id==e.id){const auto r=c.rect;return e.port==0?QPointF(r.left(),r.center().y()):e.port==1?QPointF(r.center().x(),r.top()):e.port==2?QPointF(r.right(),r.center().y()):QPointF(r.center().x(),r.bottom());}for(int i=0;i<doc.entries.size();++i)if(doc.entries[i].id==e.id)return {timeline[i].center().x(),timeline[i].bottom()};return {};}
    PlanEndpoint hitPort(QPointF p)const{for(const auto &c:doc.cards)for(int n=0;n<4;++n){PlanEndpoint e{c.id,n};if(QLineF(p,port(e)).length()<9)return e;}for(int i=0;i<doc.entries.size();++i){PlanEndpoint e{doc.entries[i].id,3};if(QLineF(p,port(e)).length()<9)return e;}return {};}
    void geometry(){timeline.clear();double x=20;for(const auto &e:doc.entries){const double width=qBound(120.,double(e.durationMs)*.006*timelineScale,700.);timeline.append(QRectF(x,38,width-4,78));x+=width;}band={20,34,qMax(400.,x-20+44),86};plus={band.right()-34,64,26,26};QRectF bounds=band.united(QRectF(0,0,1000,650));for(const auto &c:doc.cards)bounds=bounds.united(c.rect.adjusted(-30,-30,80,80));q->setSceneRect(bounds.adjusted(-30,-20,60,50));q->viewport()->update();}
    double xAt(qint64 ms)const{qint64 start=0;for(int i=0;i<doc.entries.size();++i){const auto duration=doc.entries[i].durationMs;if(ms<start+duration)return timeline[i].left()+timeline[i].width()*qBound(0.,double(ms-start)/duration,1.);start+=duration;}return timeline.isEmpty()?20:timeline.last().right();}
    void publish(){geometry();emit q->documentEdited(doc);}
    void edit(const QString &id){q->finishEditing();auto *c=card(id);if(!c)return;selected=id;editingId=id;auto *box=new QWidget;box->setObjectName("planNoteEditor");box->setAutoFillBackground(true);auto *layout=new QVBoxLayout(box);layout->setContentsMargins(5,5,5,5);layout->setSpacing(3);auto *bar=new QHBoxLayout;bar->setSpacing(2);layout->addLayout(bar);editor=new QTextEdit;editor->setObjectName("planNoteText");editor->setAcceptRichText(true);editor->setFrameShape(QFrame::NoFrame);editor->setHtml(c->html);layout->addWidget(editor);
        auto button=[&](QString text,QString tip){auto *b=new QToolButton;b->setText(text);b->setToolTip(tip);b->setFixedSize(22,22);bar->addWidget(b);return b;};
        auto *bold=button("B","Bold");bold->setCheckable(true);QObject::connect(bold,&QToolButton::clicked,q,[this](bool on){if(editor){editor->setFontWeight(on?QFont::Bold:QFont::Normal);editor->setFocus();}});
        auto *italic=button("I","Italic");italic->setCheckable(true);QObject::connect(italic,&QToolButton::clicked,q,[this](bool on){if(editor){editor->setFontItalic(on);editor->setFocus();}});
        auto *underline=button("U","Underline");underline->setCheckable(true);QObject::connect(underline,&QToolButton::clicked,q,[this](bool on){if(editor){editor->setFontUnderline(on);editor->setFocus();}});
        auto *color=button("A","Text colour");QObject::connect(color,&QToolButton::clicked,q,[this]{if(!editor)return;const auto chosen=QColorDialog::getColor(editor->textColor(),q,"Text colour");if(chosen.isValid()&&editor){editor->setTextColor(chosen);editor->setFocus();}});
        auto *font=button("…","Font and size");QObject::connect(font,&QToolButton::clicked,q,[this]{if(!editor)return;bool ok=false;const auto chosen=QFontDialog::getFont(&ok,editor->currentFont(),q,"Note typography");if(ok&&editor){editor->setCurrentFont(chosen);editor->setFocus();}});
        bar->addStretch();auto *done=button("","Finish editing · Ctrl+Enter");done->setIcon(uiIcon("apply"));QObject::connect(done,&QToolButton::clicked,q,[this]{QTimer::singleShot(0,q,[this]{q->finishEditing();});});
        proxy=q->scene()->addWidget(box);proxy->setZValue(100);proxy->setGeometry(c->rect.adjusted(3,3,-3,-3));editor->setFocus();q->viewport()->update();
    }
    void menu(const QString &id,QPoint global){q->finishEditing();auto *c=card(id);if(!c)return;QMenu menu(q);auto *text=menu.addAction(uiIcon("text"),"Edit text");auto *picture=menu.addAction(uiIcon("image"),"Choose image…");auto *background=menu.addAction("Background colour…");auto *lock=menu.addAction(uiIcon(c->locked?"unlock":"lock"),c->locked?"Unlock card":"Lock position");menu.addSeparator();auto *clearImage=menu.addAction("Remove image");clearImage->setEnabled(!c->image.isEmpty());auto *remove=menu.addAction(uiIcon("delete"),"Delete card");remove->setEnabled(!c->locked);auto *chosen=menu.exec(global);if(!chosen)return;if(chosen==text){edit(id);return;}c=card(id);if(!c)return;
        if(chosen==picture){const auto path=QFileDialog::getOpenFileName(q,"Note image",{},"Images (*.png *.jpg *.jpeg *.webp *.bmp)");if(path.isEmpty())return;c->image=path;}
        if(chosen==background){const auto value=QColorDialog::getColor(c->background,q,"Card background",QColorDialog::ShowAlphaChannel);if(!value.isValid())return;c->background=value;}
        if(chosen==lock)c->locked=!c->locked;if(chosen==clearImage)c->image.clear();if(chosen==remove)doc.removeCard(id);publish();
    }
};

PlanningCanvas::PlanningCanvas(QWidget *parent):QGraphicsView(parent),d(std::make_unique<Impl>(this)){
    setObjectName("planningCanvas");setScene(new QGraphicsScene(this));setFrameShape(QFrame::NoFrame);setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);setMouseTracking(true);setAcceptDrops(true);setFocusPolicy(Qt::StrongFocus);setTransformationAnchor(AnchorUnderMouse);setAlignment(Qt::AlignLeft|Qt::AlignTop);d->geometry();
}
PlanningCanvas::~PlanningCanvas()=default;
void PlanningCanvas::setDocument(const ProductionPlan &doc){if(d->proxy){delete d->proxy;d->proxy=nullptr;d->editor=nullptr;d->editingId.clear();}d->doc=doc;d->gesture=Impl::None;d->geometry();}
void PlanningCanvas::setPlayhead(qint64 ms){d->playhead=ms;viewport()->update();}
void PlanningCanvas::setTimelineScale(double value){d->timelineScale=qBound(.2,value,4.);d->geometry();}
void PlanningCanvas::finishEditing(){if(!d->editor)return;const auto id=d->editingId;const auto html=d->editor->toPlainText().trimmed().isEmpty()?QString():d->editor->toHtml();delete d->proxy;d->proxy=nullptr;d->editor=nullptr;d->editingId.clear();if(auto *card=d->card(id);card&&card->html!=html){card->html=html;d->publish();}viewport()->update();}

void PlanningCanvas::drawBackground(QPainter *p,const QRectF &visible){
    p->fillRect(visible,palette().color(QPalette::Base));p->setPen(QPen(palette().color(QPalette::Mid),.5));
    const int step=24;for(int x=int(std::floor(visible.left()/step))*step;x<visible.right();x+=step)for(int y=qMax(160,int(std::floor(visible.top()/step))*step);y<visible.bottom();y+=step)p->drawPoint(QPointF(x,y));
    p->setBrush(palette().color(QPalette::Window));p->setPen(QPen(palette().color(QPalette::Mid),1));p->drawRoundedRect(d->band,9,9);
    qint64 start=0;const int active=d->doc.entryAt(d->playhead);
    for(int i=0;i<d->timeline.size();++i){const auto r=d->timeline[i];const auto &e=d->doc.entries[i];auto data=presentation?presentation(e):PlanItemPresentation{e.sourceId,e.kind,{},QColor("#329cff")};const auto color=data.color.isValid()?data.color:palette().color(QPalette::Highlight);
        p->setPen(QPen(i==active?palette().color(QPalette::Highlight):palette().color(QPalette::Mid),i==active?2:1));p->setBrush(palette().color(i==active?QPalette::AlternateBase:QPalette::Button));p->drawRoundedRect(r.adjusted(2,2,-2,-2),7,7);p->fillRect(QRectF(r.left()+4,r.top()+9,3,r.height()-18),color);
        if(!data.thumbnail.isNull()){p->save();QPainterPath clip;clip.addRoundedRect(QRectF(r.left()+12,r.top()+12,46,34),4,4);p->setClipPath(clip);p->drawPixmap(QRectF(r.left()+12,r.top()+12,46,34).toRect(),data.thumbnail);p->restore();}
        p->setPen(palette().color(QPalette::Text));const double indent=data.thumbnail.isNull()?12:66;p->drawText(QRectF(r.left()+indent,r.top()+10,r.width()-indent-10,25),Qt::AlignVCenter,p->fontMetrics().elidedText(data.title,Qt::ElideRight,int(r.width()-indent-10)));p->setPen(color);p->drawText(r.adjusted(12,40,-8,-5),Qt::AlignLeft|Qt::AlignVCenter,e.kind=="Ad"?"AD  ·  "+timeLabel(e.durationMs):e.kind+"  ·  "+timeLabel(e.durationMs));p->setPen(palette().color(QPalette::PlaceholderText));p->drawText(QPointF(r.left(),24),timeLabel(start));start+=e.durationMs;
    }
    p->setPen(palette().color(QPalette::Text));uiIcon("add").paint(p,d->plus.toRect());if(d->doc.entries.isEmpty())p->drawText(d->band.adjusted(14,0,-50,0),Qt::AlignCenter,"Drop a program, ad or screen element here");
    p->setPen(palette().color(QPalette::PlaceholderText));p->drawText(QPointF(22,148),"NOTES  ·  Drag empty space to create a card. Middle-drag to pan; Ctrl+wheel to zoom.");
    QSet<QString> connected;
    for(const auto &link:d->doc.links){connected.insert(portKey(link.from));connected.insert(portKey(link.to));QPen pen(palette().color(QPalette::Highlight),2);pen.setStyle(link.style=="Dotted"?Qt::DotLine:link.style=="Dashed"?Qt::DashLine:Qt::SolidLine);p->setPen(pen);p->setBrush(Qt::NoBrush);p->drawPath(connection(d->port(link.from),link.from.port,d->port(link.to),link.to.port));}
    for(const auto &card:d->doc.cards){if(!card.rect.intersects(visible)||card.id==d->editingId)continue;p->setBrush(card.background);const bool empty=card.html.isEmpty()&&card.image.isEmpty();QPen border(card.id==d->selected?palette().color(QPalette::Highlight):palette().color(QPalette::Mid),card.id==d->selected?1.8:1);if(empty){border.setStyle(Qt::DashLine);border.setDashOffset(d->phase);}p->setPen(border);p->drawRoundedRect(card.rect,10,10);
        p->save();QPainterPath clip;clip.addRoundedRect(card.rect.adjusted(1,1,-1,-1),9,9);p->setClipPath(clip);
        if(!card.image.isEmpty()){
            if(!d->images.contains(card.image)){
                QImageReader reader(card.image);auto size=reader.size();size.scale(1600,1600,Qt::KeepAspectRatio);
                if(size.isValid())reader.setScaledSize(size);reader.setAutoTransform(true);
                auto *loaded=new QPixmap(QPixmap::fromImage(reader.read()));
                const int cost=qMax(1,int(qint64(loaded->width())*loaded->height()*4/1024));
                d->images.insert(card.image,loaded,cost);
            }
            const auto *image=d->images.object(card.image);
            if(image&&!image->isNull()){
                QRectF crop(image->rect());const double ratio=card.rect.width()/card.rect.height();
                if(crop.width()/crop.height()>ratio)crop.setWidth(crop.height()*ratio);else crop.setHeight(crop.width()/ratio);
                crop.moveCenter(QRectF(image->rect()).center());p->drawPixmap(card.rect,*image,crop);
            }else{p->setPen(palette().color(QPalette::Text));p->drawText(card.rect,Qt::AlignCenter,"Image unavailable");}
        }
        if(!card.html.isEmpty()){QTextDocument text;text.setDefaultFont(font());text.setDefaultStyleSheet("body{color:"+palette().color(QPalette::Text).name()+";}");text.setHtml(card.html);text.setTextWidth(card.rect.width()-24);p->translate(card.rect.topLeft()+QPointF(12,32));text.drawContents(p,QRectF(0,0,card.rect.width()-24,card.rect.height()-42));}
        else if(empty){p->setPen(palette().color(QPalette::PlaceholderText));uiIcon("add").paint(p,QRectF(card.rect.center()-QPointF(10,18),QSizeF(20,20)).toRect());p->drawText(card.rect.adjusted(8,20,-8,-8),Qt::AlignCenter,"Type a note or add an image");}p->restore();
        if(card.id==d->hover||card.id==d->selected){p->setPen(Qt::NoPen);p->setBrush(QColor(0,0,0,110));p->drawRoundedRect(QRectF(card.rect.left()+1,card.rect.top()+1,card.rect.width()-2,25),8,8);p->setPen(Qt::white);p->drawText(card.rect.adjusted(10,3,-35,-card.rect.height()+24),Qt::AlignVCenter,card.locked?"Locked":"Drag to move");uiIcon("ellipsis").paint(p,QRectF(card.rect.right()-26,card.rect.top()+4,18,18).toRect());}
    }
    auto dot=[&](const PlanEndpoint &e,bool show){if(!show||(d->gesture==Impl::Link&&e==d->origin))return;p->setPen(QPen(palette().color(QPalette::Base),1.5));p->setBrush(palette().color(QPalette::Highlight));p->drawEllipse(d->port(e),4.5,4.5);};
    for(const auto &card:d->doc.cards)for(int n=0;n<4;++n){PlanEndpoint e{card.id,n};dot(e,card.id==d->hover||card.id==d->selected||connected.contains(portKey(e)));}
    for(const auto &entry:d->doc.entries){PlanEndpoint e{entry.id,3};dot(e,d->gesture==Impl::Link||connected.contains(portKey(e)));}
    if(d->gesture==Impl::Link){p->setPen(QPen(palette().color(QPalette::Highlight),2));p->setBrush(Qt::NoBrush);const auto target=d->hitPort(d->last);p->drawPath(connection(d->port(d->origin),d->origin.port,d->last,target.id.isEmpty()?1:target.port));p->setBrush(palette().color(QPalette::Highlight));p->drawEllipse(d->last,5,5);}
    if(d->gesture==Impl::Create){p->setPen(QPen(palette().color(QPalette::Highlight),1,Qt::DashLine));auto fill=palette().color(QPalette::Highlight);fill.setAlpha(35);p->setBrush(fill);p->drawRoundedRect(d->rubber,8,8);}
    const auto x=d->xAt(d->playhead);p->setPen(QPen(palette().color(QPalette::Highlight),2));p->drawLine(QPointF(x,29),QPointF(x,124));p->setBrush(palette().color(QPalette::Highlight));p->drawPolygon(QPolygonF{{x-4,27},{x+4,27},{x,33}});
}

void PlanningCanvas::mousePressEvent(QMouseEvent *event){
    if(event->button()==Qt::MiddleButton){d->gesture=Impl::Pan;d->mouseStart=event->pos();d->scrollStart={horizontalScrollBar()->value(),verticalScrollBar()->value()};setCursor(Qt::ClosedHandCursor);event->accept();return;}
    if(event->button()!=Qt::LeftButton){QGraphicsView::mousePressEvent(event);return;}
    if(d->proxy&&itemAt(event->pos())){QGraphicsView::mousePressEvent(event);return;}finishEditing();setFocus();d->press=d->last=mapToScene(event->pos());d->before=d->doc;
    if(d->plus.contains(d->press)){emit addRequested(event->globalPosition().toPoint());return;}
    const auto port=d->hitPort(d->press);if(!port.id.isEmpty()){d->origin=port;d->gesture=Impl::Link;viewport()->update();return;}
    const int entry=d->entryAt(d->press);if(entry>=0){d->gesture=Impl::Entry;d->dragId=d->doc.entries[entry].id;return;}
    d->selected=d->cardAt(d->press);if(auto *card=d->card(d->selected)){d->dragId=card->id;if(card->html.isEmpty()&&card->image.isEmpty()&&QRectF(card->rect.center()-QPointF(16,24),QSizeF(32,32)).contains(d->press)){d->menu(card->id,event->globalPosition().toPoint());return;}if(d->press.x()>card->rect.right()-30&&d->press.y()<card->rect.top()+27){d->menu(card->id,event->globalPosition().toPoint());return;}if(card->locked)return;d->original=card->rect;d->edges=(qAbs(d->press.x()-card->rect.left())<7?1:0)|(qAbs(d->press.x()-card->rect.right())<7?2:0)|(qAbs(d->press.y()-card->rect.top())<7?4:0)|(qAbs(d->press.y()-card->rect.bottom())<7?8:0);d->gesture=d->edges?Impl::Resize:Impl::Move;setCursor(d->edges?Qt::SizeAllCursor:Qt::ClosedHandCursor);}
    else if(d->press.y()>=ProductionPlan::cardTop){d->gesture=Impl::Create;d->rubber=QRectF(d->press,d->press);}viewport()->update();event->accept();
}
void PlanningCanvas::mouseMoveEvent(QMouseEvent *event){
    const auto point=mapToScene(event->pos());d->last=point;
    if(d->gesture==Impl::Pan){const auto delta=event->pos()-d->mouseStart;horizontalScrollBar()->setValue(d->scrollStart.x()-delta.x());verticalScrollBar()->setValue(d->scrollStart.y()-delta.y());return;}
    if(d->gesture==Impl::Create)d->rubber=QRectF(d->press,QPointF(point.x(),qMax(ProductionPlan::cardTop,point.y()))).normalized();
    if(d->gesture==Impl::Move)d->doc.setCardRect(d->dragId,d->original.translated(point-d->press));
    if(d->gesture==Impl::Resize){auto r=d->original;const auto delta=point-d->press;if(d->edges&1)r.setLeft(qMin(r.right()-100,r.left()+delta.x()));if(d->edges&2)r.setRight(qMax(r.left()+100,r.right()+delta.x()));if(d->edges&4)r.setTop(qBound(ProductionPlan::cardTop,r.top()+delta.y(),r.bottom()-70));if(d->edges&8)r.setBottom(qMax(r.top()+70,r.bottom()+delta.y()));d->doc.setCardRect(d->dragId,r);}
    d->hover=d->cardAt(point);
    if(d->gesture==Impl::None){auto cursor=Qt::ArrowCursor;if(!d->hitPort(point).id.isEmpty())cursor=Qt::CrossCursor;else if(auto *c=d->card(d->hover);c&&!c->locked){const bool x=qAbs(point.x()-c->rect.left())<7||qAbs(point.x()-c->rect.right())<7,y=qAbs(point.y()-c->rect.top())<7||qAbs(point.y()-c->rect.bottom())<7;cursor=x&&y?Qt::SizeFDiagCursor:x?Qt::SizeHorCursor:y?Qt::SizeVerCursor:Qt::OpenHandCursor;}setCursor(cursor);QGraphicsView::mouseMoveEvent(event);}viewport()->update();
}
void PlanningCanvas::mouseReleaseEvent(QMouseEvent *event){
    const auto mode=d->gesture;d->gesture=Impl::None;setCursor(Qt::ArrowCursor);const auto point=mapToScene(event->pos());
    if(mode==Impl::Create){auto r=d->rubber;if(r.width()<12||r.height()<12)return;r.setWidth(qMax(220.,r.width()));r.setHeight(qMax(110.,r.height()));PlanCard card;card.id=newPlanId();card.rect=r;card.background=palette().color(QPalette::AlternateBase);d->doc.cards.append(card);const auto id=card.id;d->publish();d->edit(id);}
    else if(mode==Impl::Move||mode==Impl::Resize){if(d->doc.toJson()!=d->before.toJson())d->publish();}
    else if(mode==Impl::Link){auto target=d->hitPort(point);if(!target.id.isEmpty()&&d->doc.addLink({{},d->origin,target,"Solid"}))d->publish();}
    else if(mode==Impl::Entry){const int at=d->entryAt(point);if(QLineF(point,d->press).length()>8){if(d->doc.moveEntry(d->dragId,at<0?d->doc.entries.size()-1:at))d->publish();}else if(at>=0){const auto &e=d->doc.entries[at];emit seekRequested(d->doc.startMs(e.id)+qint64(qBound(0.,(point.x()-d->timeline[at].left())/d->timeline[at].width(),1.)*qMax<qint64>(0,e.durationMs-1)));}}
    else if(mode==Impl::None)QGraphicsView::mouseReleaseEvent(event);viewport()->update();
}
void PlanningCanvas::mouseDoubleClickEvent(QMouseEvent *event){if(d->proxy&&itemAt(event->pos())){QGraphicsView::mouseDoubleClickEvent(event);return;}const auto id=d->cardAt(mapToScene(event->pos()));if(!id.isEmpty()){d->gesture=Impl::None;d->edit(id);return;}QGraphicsView::mouseDoubleClickEvent(event);}
void PlanningCanvas::keyPressEvent(QKeyEvent *event){if(d->editor){if(event->key()==Qt::Key_Return&&(event->modifiers()&Qt::ControlModifier)){finishEditing();return;}QGraphicsView::keyPressEvent(event);return;}if(event->key()==Qt::Key_Escape&&d->gesture!=Impl::None){if(d->gesture!=Impl::Pan)d->doc=d->before;d->gesture=Impl::None;setCursor(Qt::ArrowCursor);d->geometry();return;}if(event->key()==Qt::Key_Delete&&!d->selected.isEmpty()){if(d->doc.removeCard(d->selected))d->publish();return;}if(!event->text().isEmpty()&&!event->modifiers()&&!d->selected.isEmpty()){d->edit(d->selected);if(d->editor)d->editor->insertPlainText(event->text());return;}QGraphicsView::keyPressEvent(event);}
void PlanningCanvas::contextMenuEvent(QContextMenuEvent *event){if(d->proxy&&itemAt(event->pos())){QGraphicsView::contextMenuEvent(event);return;}const auto point=mapToScene(event->pos());const auto port=d->hitPort(point);if(!port.id.isEmpty()){QMenu menu(this);for(const auto &style:{"Solid","Dashed","Dotted"}){auto *action=menu.addAction(style);connect(action,&QAction::triggered,this,[this,port,style]{for(auto &link:d->doc.links)if(link.from==port||link.to==port)link.style=style;d->publish();});}menu.addSeparator();auto *remove=menu.addAction("Disconnect all");connect(remove,&QAction::triggered,this,[this,port]{d->doc.links.removeIf([&](const PlanLink &l){return l.from==port||l.to==port;});d->publish();});menu.exec(event->globalPos());return;}const auto id=d->cardAt(point);if(!id.isEmpty()){d->menu(id,event->globalPos());return;}const auto entry=d->entryAt(point);if(entry>=0){emit entryMenuRequested(d->doc.entries[entry].id,event->globalPos());return;}QGraphicsView::contextMenuEvent(event);}
void PlanningCanvas::dragEnterEvent(QDragEnterEvent *event){if(event->mimeData()->hasFormat(sourceMime))event->acceptProposedAction();else event->ignore();}
void PlanningCanvas::dragMoveEvent(QDragMoveEvent *event){if(event->mimeData()->hasFormat(sourceMime)&&mapToScene(event->position().toPoint()).y()<130)event->acceptProposedAction();else event->ignore();}
void PlanningCanvas::dropEvent(QDropEvent *event){if(!event->mimeData()->hasFormat(sourceMime))return;const auto point=mapToScene(event->position().toPoint());if(point.y()>=130)return;const auto json=QJsonDocument::fromJson(event->mimeData()->data(sourceMime)).object();const int at=d->entryAt(point);emit sourceDropped(json["kind"].toString(),json["id"].toString(),at<0?d->doc.entries.size():at);event->acceptProposedAction();}
void PlanningCanvas::wheelEvent(QWheelEvent *event){if(event->modifiers()&Qt::ControlModifier){const double current=transform().m11(),target=qBound(.35,current*std::pow(1.0015,event->angleDelta().y()),2.5);scale(target/current,target/current);event->accept();return;}QGraphicsView::wheelEvent(event);}
