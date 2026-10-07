#pragma once
#include "ElementDesign.h"
#include <QHash>
#include <cmath>
#include <QtMath>

inline Mesh mirrorGeometry(Mesh mesh,const Modifier &modifier,const QMatrix4x4 &basis){
    mesh.transform(basis.inverted());
    const auto &opt=modifier.options;
    const bool axes[]={modifier.ax,modifier.ay,modifier.az};
    const double tolerance=qMax(0.000001,opt["mergeDistance"].toDouble(.001));
    for(int axis=0;axis<3;++axis){
        if(!axes[axis])continue;
        const QString name=QString("xyz").mid(axis,1);
        const float offset=opt["offset."+name].toDouble();
        const bool bisect=opt["bisect."+name].toBool(),flip=opt["flip."+name].toBool();
        if(bisect){
            Mesh clipped;
            for(const auto &face:mesh.f){QVector<QVector3D> points;
                for(int i=0;i<face.size();++i){auto a=mesh.v[face[i]],b=mesh.v[face[(i+1)%face.size()]];
                    const double da=(a[axis]-offset)*(flip?-1:1),db=(b[axis]-offset)*(flip?-1:1);
                    if(da>=0)points.append(a);
                    if((da<0)!=(db<0))points.append(a+(b-a)*float(da/(da-db)));
                }
                if(points.size()<3)continue;QVector<int> indices;for(auto point:points){indices.append(clipped.v.size());clipped.v.append(point);}clipped.f.append(indices);
            }
            mesh=clipped;
        }
        if(opt["clipping"].toBool())for(auto &point:mesh.v)if(qAbs(point[axis]-offset)<=tolerance)point[axis]=offset;
        Mesh reflected=mesh;for(auto &point:reflected.v)point[axis]=2*offset-point[axis];reflected.flipNormals();mesh.append(reflected);
    }
    if(opt["merge"].toBool(true)){
        Mesh welded;QHash<QString,QVector<int>> cells;QVector<int> remap;
        auto key=[](qint64 x,qint64 y,qint64 z){return QString::number(x)+","+QString::number(y)+","+QString::number(z);};
        for(auto point:mesh.v){const qint64 x=qint64(std::floor(point.x()/tolerance)),y=qint64(std::floor(point.y()/tolerance)),z=qint64(std::floor(point.z()/tolerance));int found=-1;
            for(int a=-1;a<=1&&found<0;++a)for(int b=-1;b<=1&&found<0;++b)for(int c=-1;c<=1&&found<0;++c)for(int index:cells.value(key(x+a,y+b,z+c)))if((welded.v[index]-point).lengthSquared()<=tolerance*tolerance){found=index;break;}
            if(found<0){found=welded.v.size();welded.v.append(point);cells[key(x,y,z)].append(found);}remap.append(found);
        }
        for(auto face:mesh.f){QVector<int> clean;for(int index:face){int v=remap[index];if(clean.isEmpty()||clean.last()!=v)clean.append(v);}if(clean.size()>1&&clean.first()==clean.last())clean.removeLast();QSet<int> unique(clean.begin(),clean.end());if(unique.size()>=3)welded.f.append(clean);}mesh=welded;
    }
    mesh.transform(basis);return mesh;
}
