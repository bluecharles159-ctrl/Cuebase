#include "ParticlePathEditor.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <utility>

ParticlePathEditor::ParticlePathEditor(QWidget *canvas,const QJsonArray &initial,
                                       std::function<QTransform()> projection,
                                       std::function<void(QJsonArray)> save)
    :QWidget(canvas),project(std::move(projection)),commit(std::move(save)),knots(ParticlePath::read(initial)){
    setAttribute(Qt::WA_DeleteOnClose);setObjectName("particlePathEditor");
    setFocusPolicy(Qt::StrongFocus);setCursor(Qt::CrossCursor);
    setGeometry(canvas->rect());canvas->installEventFilter(this);
    auto *bar=new QHBoxLayout(this);bar->setAlignment(Qt::AlignRight|Qt::AlignTop);
    auto *redraw=new QPushButton("Redraw",this),*done=new QPushButton("Done",this),*cancel=new QPushButton("Cancel",this);
    done->setObjectName("saveParticlePath");
    redraw->setToolTip("Draw a replacement path. Cancel restores the saved path.");
    for(auto *button:{redraw,done,cancel})bar->addWidget(button);
    connect(redraw,&QPushButton::clicked,this,[this]{draw=true;stroke.clear();active=-1;setFocus();update();});
    connect(done,&QPushButton::clicked,this,[this]{finish();});
    connect(cancel,&QPushButton::clicked,this,&QWidget::close);
    draw=knots.size()<2;show();raise();setFocus();
}
bool ParticlePathEditor::eventFilter(QObject *obj,QEvent *event){
    if(obj==parentWidget() && event->type()==QEvent::Resize)setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(obj,event);
}
void ParticlePathEditor::paintEvent(QPaintEvent *){
    const auto transform=project();if(!transform.isInvertible())return;
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    const auto points=stroke.isEmpty()?knots:ParticlePath::smooth(stroke);
    painter.setPen(QPen(palette().highlight().color(),2));painter.setBrush(Qt::NoBrush);
    painter.drawPath(transform.map(ParticlePath::curve(points)));
    if(draw || !stroke.isEmpty())return;
    for(int i=0;i<points.size();++i){
        const auto &k=points[i];const auto anchor=transform.map(k.point);
        if(active==i){
            painter.setPen(QPen(palette().highlight().color(),1));painter.setBrush(palette().base());
            for(auto control:{k.incoming,k.outgoing}){
                const auto p=transform.map(control);painter.drawLine(anchor,p);painter.drawEllipse(p,3,3);
            }
        }
        painter.setPen(QPen(Qt::black,1));painter.setBrush(active==i?palette().highlight():palette().light());
        painter.drawEllipse(anchor,4,4);
    }
}
void ParticlePathEditor::addSample(QPointF screen,bool last){
    bool ok=false;const auto transform=project();const auto inverse=transform.inverted(&ok);if(!ok)return;
    const auto point=inverse.map(screen);
    if(!std::isfinite(point.x()) || !std::isfinite(point.y()) || qAbs(point.x())>8 || qAbs(point.y())>8)return;
    if(!stroke.isEmpty() && QLineF(transform.map(stroke.last()),screen).length()<(last?1.:12.))return;
    if(stroke.size()<128)stroke.append(point);else if(last)stroke.last()=point;
    update();
}
void ParticlePathEditor::mousePressEvent(QMouseEvent *event){
    if(event->button()==Qt::RightButton){close();return;}
    if(event->button()!=Qt::LeftButton)return;
    setFocus();
    if(draw){stroke.clear();addSample(event->position(),true);return;}
    const auto transform=project();if(!transform.isInvertible())return;
    const auto hit=[&](QPointF p){return QLineF(transform.map(p),event->position()).length()<=7;};
    if(active>=0 && hit(knots[active].incoming)){handle=2;return;}
    if(active>=0 && hit(knots[active].outgoing)){handle=3;return;}
    active=-1;handle=0;
    for(int i=0;i<knots.size();++i)if(hit(knots[i].point)){active=i;handle=1;break;}
    update();
}
void ParticlePathEditor::mouseMoveEvent(QMouseEvent *event){
    if(!(event->buttons()&Qt::LeftButton))return;
    if(draw){addSample(event->position(),false);return;}
    if(active<0 || !handle)return;
    bool ok=false;const auto point=project().inverted(&ok).map(event->position());
    if(!ok || !std::isfinite(point.x()) || !std::isfinite(point.y()) || qAbs(point.x())>8 || qAbs(point.y())>8)return;
    auto &k=knots[active];
    if(handle==1){const auto delta=point-k.point;k.incoming+=delta;k.outgoing+=delta;k.point=point;}
    else if(handle==2)k.incoming=point;
    else k.outgoing=point;
    update();
}
void ParticlePathEditor::mouseReleaseEvent(QMouseEvent *event){
    if(event->button()!=Qt::LeftButton)return;
    if(draw){
        addSample(event->position(),true);
        if(stroke.size()>=2){knots=ParticlePath::smooth(stroke);active=0;draw=false;}
        stroke.clear();
    }
    handle=0;update();
}
void ParticlePathEditor::keyPressEvent(QKeyEvent *event){
    if(event->key()==Qt::Key_Escape)close();
    else if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter)finish();
    else if(event->key()==Qt::Key_Delete && active>=0 && knots.size()>2){knots.removeAt(active);active=-1;update();}
    event->accept();
}
void ParticlePathEditor::finish(){
    if(knots.size()<2 || draw)return;
    commit(ParticlePath::write(knots));close();
}
