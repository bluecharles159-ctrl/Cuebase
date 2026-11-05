#pragma once
#include "ElementDesign.h"
namespace SurfaceMasks {
QString fileSignature(const DObject &object);
void apply(QImage &surface,const QRect &bounds,const DObject &object);
}
