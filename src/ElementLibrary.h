#pragma once
#include "ElementDesign.h"
#include "Data.h"
#include <QWidget>

bool parseStudioText(DObject &object, const QString &input);
bool setStudioField(ElementDesign &design,const QString &id,const QString &value,QString *error=nullptr);
QImage elementThumbnail(const Element &element);
void refreshElementThumbnail(Element &element);
bool editElementPresentation(Element &element, QWidget *parent=nullptr);
QStringList elementPresetNames();
ElementDesign elementPreset(const QString &name);

class ElementPreview:public QWidget {
public:
    explicit ElementPreview(QWidget *parent=nullptr);
    void setElement(const Element &element);
    void setTime(qint64 time);
    QRectF crop{0,0,1,1};
    bool cropping=false;
    qint64 time=0;
protected:
    void paintEvent(QPaintEvent*)override;
    void mousePressEvent(QMouseEvent*)override;
    void mouseMoveEvent(QMouseEvent*)override;
    void mouseReleaseEvent(QMouseEvent*)override;
private:
    Element element;
    ElementDesign design;
    QRectF frame()const;
    QPointF cropStart;
};
