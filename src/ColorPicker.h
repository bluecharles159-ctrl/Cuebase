#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <functional>

namespace ColorPicker {
class Spectrum final:public QWidget {
public:
    QColor color;
    std::function<void(QColor)> changed;
    explicit Spectrum(QColor value):color(value){setFixedSize(260,170);setCursor(Qt::CrossCursor);setToolTip("Drag to set saturation and brightness");}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);QLinearGradient saturation(0,0,width(),0);
        saturation.setColorAt(0,Qt::white);saturation.setColorAt(1,QColor::fromHsvF(qMax(0.0,color.hsvHueF()),1,1));p.fillRect(rect(),saturation);
        QLinearGradient value(0,0,0,height());value.setColorAt(0,Qt::transparent);value.setColorAt(1,Qt::black);p.fillRect(rect(),value);
        p.setRenderHint(QPainter::Antialiasing);QPointF point(color.hsvSaturationF()*width(),(1-color.valueF())*height());
        p.setPen(QPen(Qt::black,3));p.drawEllipse(point,5,5);p.setPen(QPen(Qt::white,1));p.drawEllipse(point,5,5);
    }
    void pick(QPointF pos){color=QColor::fromHsvF(qMax(0.0,color.hsvHueF()),qBound(0.0,pos.x()/width(),1.0),qBound(0.0,1-pos.y()/height(),1.0),color.alphaF());update();if(changed)changed(color);}
    void mousePressEvent(QMouseEvent *e) override{if(e->button()==Qt::LeftButton)pick(e->position());}
    void mouseMoveEvent(QMouseEvent *e) override{if(e->buttons()&Qt::LeftButton)pick(e->position());}
};
inline QColor getColor(const QColor &initial=Qt::white,QWidget *parent=nullptr,const QString &title="Color",QColorDialog::ColorDialogOptions options={}) {
    Q_UNUSED(options);
    QDialog dialog(parent);dialog.setWindowTitle(title);dialog.setModal(true);
    dialog.setStyleSheet("QDialog{background:#303030;border-radius:10px;} QLabel{color:#ddd;font-size:11px;} QLineEdit{background:#454545;color:white;border:0;border-radius:4px;padding:5px;} QPushButton{padding:5px 12px;border-radius:4px;background:#505050;color:white;} QSlider::groove:horizontal{height:9px;border-radius:4px;background:#555;} QSlider::handle:horizontal{width:10px;margin:-3px 0;border-radius:5px;background:#ddd;}");
    auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(14,14,14,14);layout->setSpacing(9);
    auto *spectrum=new Spectrum(initial);layout->addWidget(spectrum);
    auto *hue=new QSlider(Qt::Horizontal);hue->setRange(0,359);hue->setValue(qMax(0,initial.hsvHue()));hue->setToolTip("Hue");
    hue->setStyleSheet("QSlider::groove:horizontal{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 red,stop:0.17 yellow,stop:0.33 lime,stop:0.5 cyan,stop:0.67 blue,stop:0.83 magenta,stop:1 red);}");layout->addWidget(hue);
    auto *alpha=new QSlider(Qt::Horizontal);alpha->setRange(0,255);alpha->setValue(initial.alpha());alpha->setToolTip("Opacity");layout->addWidget(new QLabel("Opacity"));layout->addWidget(alpha);
    auto *hex=new QLineEdit(initial.name(QColor::HexArgb));hex->setToolTip("Hex color: #RRGGBB or #AARRGGBB");layout->addWidget(hex);
    auto *preview=new QLabel;preview->setFixedHeight(24);layout->addWidget(preview);
    auto refresh=[=]{QSignalBlocker h(hue),a(alpha);hue->setValue(qMax(0,spectrum->color.hsvHue()));alpha->setValue(spectrum->color.alpha());hex->setText(spectrum->color.name(QColor::HexArgb));preview->setStyleSheet("border-radius:4px;background:"+spectrum->color.name(QColor::HexArgb)+";");spectrum->update();};
    spectrum->changed=[=](QColor){refresh();};
    QObject::connect(hue,&QSlider::valueChanged,&dialog,[=](int v){spectrum->color.setHsv(v,spectrum->color.hsvSaturation(),spectrum->color.value(),spectrum->color.alpha());refresh();});
    QObject::connect(alpha,&QSlider::valueChanged,&dialog,[=](int v){spectrum->color.setAlpha(v);refresh();});
    QObject::connect(hex,&QLineEdit::editingFinished,&dialog,[=]{QColor value(hex->text());if(value.isValid())spectrum->color=value;refresh();});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);refresh();
    return dialog.exec()==QDialog::Accepted?spectrum->color:QColor();
}
}
