#pragma once
#include "Data.h"

bool exportGraphicPackage(const Element &element,const QString &path,QString *error);
QString importGraphicPackage(Library &library,const QString &path,QString *error);
