#pragma once
#include "ElementDesign.h"
namespace SurfaceTreatments {
int padding(const DObject &object);
void apply(QImage &surface,const QRect &bounds,const DObject &object);
}
