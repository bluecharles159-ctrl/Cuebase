#pragma once
#include "ElementDesign.h"
#include <QtMath>
#include <algorithm>

// Face offsets, subdivided edge profiles and vertex caps share their boundary vertices.
inline Mesh bevelGeometry(const Mesh &mesh,const Modifier &modifier){
    if(mesh.isEmpty()||modifier.width<=0||!mesh.isClosed())return mesh;
    const auto &options=modifier.options;const int segments=qBound(1,options["segments"].toInt(1),16);
    const double profile=qBound(0.0,options["profile"].toDouble(.5),1.0);
    const bool vertices=options["affect"].toString()=="Vertices",clamp=options["clamp"].toBool(true);
    const auto adjacency=mesh.edgeFaces();const auto normals=mesh.vertexNormals();
    QHash<EdgeKey,double> widths;
    for(auto it=adjacency.begin();it!=adjacency.end();++it){double width=modifier.width;
        double angle=it.value().size()==2?qAcos(qBound(-1.f,QVector3D::dotProduct(mesh.faceNormal(it.value()[0]),mesh.faceNormal(it.value()[1])),1.f)):M_PI;
        if(options["limit"].toString("None")=="Angle"&&angle<qDegreesToRadians(options["angle"].toDouble(30)))width=0;
        const QString mode=options["widthType"].toString("Offset");
        if(mode=="Width")width/=qMax(.01,2*qSin(angle/2));
        else if(mode=="Depth")width/=qMax(.01,qCos(angle/2));
        else if(mode=="Percent")width=modifier.width*.01*(mesh.v[ekA(it.key())]-mesh.v[ekB(it.key())]).length();
        widths[it.key()]=width;
    }
    Mesh result;QHash<int,QVector<int>> caps;
    auto add=[&](QVector3D point,int original){int index=result.v.size();result.v.append(point);caps[original].append(index);return index;};
    auto curve=[&](QVector3D a,QVector3D b,QVector3D origin,double t){const auto control=(a+b)*float(.5*(1-2*profile))+origin*float(2*profile);return a*float((1-t)*(1-t))+control*float(2*t*(1-t))+b*float(t*t);};
    if(vertices){
        for(int fi=0;fi<mesh.f.size();++fi){const auto &face=mesh.f[fi];QVector<int> inset;
            for(int k=0;k<face.size();++k){int vi=face[k],prev=face[(k+face.size()-1)%face.size()],next=face[(k+1)%face.size()];auto p=mesh.v[vi];auto e1=mesh.v[prev]-p,e2=mesh.v[next]-p;
                double a=widths.value(ekey(vi,prev)),b=widths.value(ekey(vi,next));if(clamp){a=qMin(a,e1.length()*.45);b=qMin(b,e2.length()*.45);}
                auto start=p+e1.normalized()*float(a),end=p+e2.normalized()*float(b);
                for(int j=0;j<=segments;++j)inset.append(add(curve(start,end,p,double(j)/segments),vi));
            }result.f.append(inset);
        }
    }else{
        QVector<int> base;
        for(int fi=0;fi<mesh.f.size();++fi){base.append(result.v.size());const auto &face=mesh.f[fi];QVector<int> inset;
            for(int k=0;k<face.size();++k){int vi=face[k],prev=face[(k+face.size()-1)%face.size()],next=face[(k+1)%face.size()];auto p=mesh.v[vi];auto e1=(mesh.v[prev]-p).normalized(),e2=(mesh.v[next]-p).normalized();
                const double sine=qMax(.001,double(QVector3D::crossProduct(e1,e2).length()));
                auto displacement=(e1*float(widths.value(ekey(vi,next)))+e2*float(widths.value(ekey(vi,prev))))/float(sine);
                if(clamp&&displacement.length()>0){double maximum=qMin((mesh.v[prev]-p).length(),(mesh.v[next]-p).length())*.45;if(displacement.length()>maximum)displacement=displacement.normalized()*float(maximum);}
                inset.append(add(p+displacement,vi));
            }result.f.append(inset);
        }
        for(auto it=adjacency.begin();it!=adjacency.end();++it){if(it.value().size()!=2)continue;
            int f1=it.value()[0],f2=it.value()[1];const auto &a=mesh.f[f1];const auto &b=mesh.f[f2];int edge=0;
            for(int k=0;k<a.size();++k)if(ekey(a[k],a[(k+1)%a.size()])==it.key()){edge=k;break;}
            int va=a[edge],vb=a[(edge+1)%a.size()];int a0=base[f1]+edge,b0=base[f1]+(edge+1)%a.size(),a1=base[f2]+b.indexOf(va),b1=base[f2]+b.indexOf(vb);
            int previousA=a0,previousB=b0;
            for(int j=1;j<=segments;++j){double t=double(j)/segments;int nextA=j==segments?a1:add(curve(result.v[a0],result.v[a1],mesh.v[va],t),va);int nextB=j==segments?b1:add(curve(result.v[b0],result.v[b1],mesh.v[vb],t),vb);
                result.f.append({previousB,previousA,nextA,nextB});previousA=nextA;previousB=nextB;
            }
        }
    }
    for(auto it=caps.begin();it!=caps.end();++it){auto ring=it.value();const auto origin=mesh.v[it.key()];auto normal=normals[it.key()];auto x=QVector3D::crossProduct(normal,qAbs(normal.z())<.9?QVector3D(0,0,1):QVector3D(0,1,0)).normalized();auto y=QVector3D::crossProduct(normal,x);
        std::sort(ring.begin(),ring.end(),[&](int a,int b){auto p=result.v[a]-origin,q=result.v[b]-origin;return qAtan2(QVector3D::dotProduct(p,y),QVector3D::dotProduct(p,x))<qAtan2(QVector3D::dotProduct(q,y),QVector3D::dotProduct(q,x));});
        if(ring.size()>=3){result.f.append(ring);if(QVector3D::dotProduct(result.faceNormal(result.f.size()-1),normal)<0)std::reverse(result.f.last().begin(),result.f.last().end());}
    }
    // Weld equal boundary points, including vertices where an angle-limited edge was untouched.
    Modifier weld;weld.ax=weld.ay=weld.az=false;weld.options["mergeDistance"]=.000001;return mirrorGeometry(result,weld,QMatrix4x4());
}
