#include "SurfacePaint.h"
#include "BuildModel.h"
#include "UiControls.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRandomGenerator>
#include <QShortcut>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <functional>
#include <memory>

namespace {
QJsonObject layer(bool mask, int number) {
    return {{"target",mask ? "mask" : "material"},
            {"name",QString(mask ? "Mask %1" : "Paint %1").arg(number)}};
}

class PaintCanvas final : public QWidget {
public:
    QImage base, preview;
    QJsonArray layers, points;
    QJsonObject brush;
    int index=-1;
    std::function<void(const QJsonArray&)> changed;
    explicit PaintCanvas(QWidget *parent):QWidget(parent) {
        setMinimumSize(280,220);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        setCursor(Qt::CrossCursor);
        setObjectName("surfacePaintCanvas");
    }
    QJsonArray withStroke() const {
        auto draft=layers;
        if (points.isEmpty() || index<0 || index>=draft.size()) return draft;
        auto active=draft[index].toObject();
        auto strokes=active.value("strokes").toArray();
        auto stroke=brush;
        stroke["points"]=points;
        strokes.append(stroke);
        active["strokes"]=strokes;
        draft[index]=active;
        return draft;
    }
    void refresh() {
        preview=base;
        SurfacePaint::composite(preview,preview.rect(),withStroke());
        update();
    }
private:
    QRectF surfaceRect() const {
        const auto scaled=QSizeF(base.size()).scaled(QSizeF(width()-16,height()-16),Qt::KeepAspectRatio);
        return QRectF((width()-scaled.width())/2,(height()-scaled.height())/2,scaled.width(),scaled.height());
    }
    void sample(QPointF pos) {
        auto r=surfaceRect();
        QJsonArray xy{qBound(0.,(pos.x()-r.left())/r.width(),1.),
                      qBound(0.,(pos.y()-r.top())/r.height(),1.)};
        if (points.isEmpty() || points.last()!=xy) points.append(xy);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button()!=Qt::LeftButton || index<0 || !surfaceRect().contains(event->position())
            || !layers.at(index).toObject().value("visible").toBool(true)) return;
        brush["seed"]=int(QRandomGenerator::global()->bounded(0x7fffffff));
        sample(event->position()); refresh();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (points.isEmpty()) return;
        sample(event->position()); update();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button()!=Qt::LeftButton || points.isEmpty()) return;
        sample(event->position());
        const auto next=withStroke();
        points={};
        changed(next);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(),palette().window());
        const auto r=surfaceRect();
        p.setClipRect(r);
        p.fillRect(r,QColor(62,65,68));
        for (int y=int(r.top());y<r.bottom();y+=12)
            for (int x=int(r.left())+(((y-int(r.top()))/12)%2)*12;x<r.right();x+=24)
                p.fillRect(QRect(x,y,12,12),QColor(77,80,83));
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        if (points.isEmpty()) p.drawImage(r,preview);
        else {
            QImage current=base;
            SurfacePaint::composite(current,current.rect(),withStroke());
            p.drawImage(r,current);
        }
    }
};
}

void SurfacePaint::edit(QWidget *parent,BuildModel *model) {
    const auto *object=model->active();
    if (!object || !object->hasSurface() || object->locked) return;
    const auto id=object->id;
    const auto original=object->style.value("paint.layers").toArray();
    std::unique_ptr<QDialog> dialog(roundedModal(parent,"Paint surface"));
    dialog->setObjectName("surfacePaintDialog");
    dialog->resize(780,530);
    auto *root=new QVBoxLayout(dialog.get());
    root->addWidget(new QLabel("Paint surface  ·  "+object->name));
    auto *body=new QHBoxLayout;
    root->addLayout(body,1);
    auto *canvas=new PaintCanvas(dialog.get());
    body->addWidget(canvas,1);
    auto source=object->evaluated(model->t);
    source.style.remove("paint.layers");
    source.fitTextLayout();
    const auto texture=model->design.textureFor(source);
    const int w=qBound(1,qRound(source.w*kPxPerUnit),4096),h=qBound(1,qRound(source.h*kPxPerUnit),4096);
    canvas->base=texture.copy((texture.width()-w)/2,(texture.height()-h)/2,w,h);
    if (w>640 || h>640) canvas->base=canvas->base.scaled(640,640,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    auto *controls=new QVBoxLayout;
    body->addLayout(controls);
    controls->addWidget(new QLabel("Layers"));
    auto *list=new QListWidget;
    list->setObjectName("surfacePaintLayers");
    list->setMaximumWidth(240);
    controls->addWidget(list,1);
    auto *addRow=new QHBoxLayout;
    controls->addLayout(addRow);
    auto button=[](QHBoxLayout *row,const QString &label) {
        auto *b=new QPushButton(label); row->addWidget(b); return b;
    };
    auto *addPaint=button(addRow,"+ Paint"),*addMask=button(addRow,"+ Mask");
    addMask->setObjectName("surfaceAddMask");
    auto *actions=new QHBoxLayout;
    controls->addLayout(actions);
    auto *up=button(actions,"Raise"),*down=button(actions,"Lower"),*remove=button(actions,"Remove");
    auto *form=new QFormLayout;
    controls->addLayout(form);
    auto percent=[&](const QString &label,int minimum,int initial) {
        auto *input=new QSpinBox;input->setRange(minimum,100);input->setValue(initial);input->setSuffix("%");
        form->addRow(label,input);return input;
    };
    auto *opacity=percent("Layer opacity",0,100);
    auto *color=new QPushButton;form->addRow("Brush color",color);
    auto *size=percent("Brush size",1,8),*flow=percent("Flow",1,100),*jitter=percent("Jitter",0,0);
    auto *erase=new QCheckBox("Erase paint");controls->addWidget(erase);
    auto *footer=new QHBoxLayout;root->addLayout(footer);
    auto *undo=button(footer,"Undo"),*redo=button(footer,"Redo");footer->addStretch();
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);footer->addWidget(buttons);

    canvas->layers=original.isEmpty()?QJsonArray{layer(false,1)}:original;
    int selected=int(canvas->layers.size())-1;
    struct Snapshot { QJsonArray layers; int selection; };
    QVector<Snapshot> history{{canvas->layers,selected}};
    int step=0;
    QColor ink("#3d9aff");
    auto syncBrush=[&] {
        canvas->brush={{"color",ink.name(QColor::HexArgb)},{"size",size->value()/100.},
                       {"flow",flow->value()/100.},{"jitter",jitter->value()/100.},{"erase",erase->isChecked()}};
        color->setText(ink.name());
        QPixmap swatch(16,16);swatch.fill(ink);color->setIcon(QIcon(swatch));
    };
    auto updateSelection=[&] {
        selected=canvas->index=selected>=0 && selected<canvas->layers.size()?selected:-1;
        const auto current=canvas->layers.at(selected).toObject();
        const bool mask=current.value("target").toString()=="mask";
        QSignalBlocker block(opacity);
        opacity->setValue(qRound(100*current.value("opacity").toDouble(1.)));
        opacity->setEnabled(selected>=0);remove->setEnabled(selected>=0);
        up->setEnabled(selected>=0 && selected+1<canvas->layers.size());down->setEnabled(selected>0);
        color->setEnabled(selected>=0 && !mask);erase->setText(mask?"Restore mask":"Erase paint");
    };
    auto refresh=[&] {
        QSignalBlocker block(list);
        list->clear();
        for (int i=int(canvas->layers.size())-1;i>=0;--i) {
            const auto data=canvas->layers.at(i).toObject();
            auto *item=new QListWidgetItem(data.value("name").toString(),list);
            item->setFlags(item->flags()|Qt::ItemIsUserCheckable|Qt::ItemIsEditable);
            item->setCheckState(data.value("visible").toBool(true)?Qt::Checked:Qt::Unchecked);
            item->setData(Qt::UserRole,i);
            if (i==selected) list->setCurrentItem(item);
        }
        updateSelection();undo->setEnabled(step>0);redo->setEnabled(step+1<history.size());canvas->refresh();
    };
    auto change=[&](const QJsonArray &next,int index) {
        if (next==canvas->layers) return;
        history.resize(step+1);
        history.append({next,index});
        if (history.size()>64) history.removeFirst();
        step=int(history.size())-1;
        selected=index;canvas->layers=next;refresh();
    };
    canvas->changed=[&](const QJsonArray &next){change(next,selected);};
    auto travel=[&](int delta) {
        const int next=step+delta;
        if (next<0 || next>=history.size()) return;
        step=next;canvas->layers=history[step].layers;selected=history[step].selection;refresh();
    };
    QObject::connect(undo,&QPushButton::clicked,dialog.get(),[&]{travel(-1);});
    QObject::connect(redo,&QPushButton::clicked,dialog.get(),[&]{travel(1);});
    undo->setShortcut(QKeySequence::Undo);redo->setShortcut(QKeySequence::Redo);
    for (auto *b:{addPaint,addMask}) QObject::connect(b,&QPushButton::clicked,dialog.get(),[&,b] {
        auto next=canvas->layers;next.append(layer(b==addMask,int(next.size()+1)));change(next,int(next.size()-1));
    });
    for (auto *b:{up,down,remove}) QObject::connect(b,&QPushButton::clicked,dialog.get(),[&,b] {
        if (selected<0) return;
        auto next=canvas->layers;const auto entry=next.takeAt(selected);
        if (b==remove) {change(next,qMin(selected,int(next.size()-1)));return;}
        const int target=selected+(b==up?1:-1);next.insert(target,entry);change(next,target);
    });
    QObject::connect(list,&QListWidget::currentRowChanged,dialog.get(),[&](int row) {
        selected=row<0?-1:int(canvas->layers.size())-1-row;updateSelection();
    });
    QObject::connect(list,&QListWidget::itemChanged,dialog.get(),[&](QListWidgetItem *item) {
        auto next=canvas->layers;const int index=item->data(Qt::UserRole).toInt();
        auto entry=next.at(index).toObject();entry["visible"]=item->checkState()==Qt::Checked;
        entry["name"]=item->text().trimmed();next[index]=entry;change(next,index);
    });
    QObject::connect(opacity,&QSpinBox::valueChanged,dialog.get(),[&](int value) {
        if (selected<0) return;
        auto next=canvas->layers;auto entry=next.at(selected).toObject();entry["opacity"]=value/100.;
        next[selected]=entry;change(next,selected);
    });
    QObject::connect(color,&QPushButton::clicked,dialog.get(),[&] {
        const auto picked=QColorDialog::getColor(ink,dialog.get(),"Brush color");
        if (picked.isValid()) {ink=picked;syncBrush();}
    });
    for (auto *box:{size,flow,jitter}) QObject::connect(box,&QSpinBox::valueChanged,dialog.get(),syncBrush);
    QObject::connect(erase,&QCheckBox::toggled,dialog.get(),syncBrush);
    QObject::connect(buttons,&QDialogButtonBox::accepted,dialog.get(),&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,dialog.get(),&QDialog::reject);
    syncBrush();refresh();
    if (dialog->exec()!=QDialog::Accepted || (original.isEmpty() && step==0) || canvas->layers==original) return;
    auto *target=model->byId(id);
    if (!target || target->locked || target->style.value("paint.layers").toArray()!=original) return;
    model->pushUndo(true);
    if (canvas->layers.isEmpty()) target->style.remove("paint.layers");
    else target->style["paint.layers"]=canvas->layers;
    model->notify();
}
