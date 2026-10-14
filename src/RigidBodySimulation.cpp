#include "RigidBodySimulation.h"
#include "ElementDesign.h"
#include "SurfaceEffects.h"
#include <btBulletDynamicsCommon.h>
#include <vector>
#include <algorithm>
#include <QtMath>

namespace {
btVector3 bullet(QVector3D v){return {v.x(),v.y(),v.z()};}
btTransform bullet(const QMatrix4x4 &matrix){
    // Bullet transforms are rigid: never pass animated scale or shear into
    // the collision basis. Collision geometry retains its authored size.
    auto x=matrix.column(0).toVector3D().normalized();
    auto y=matrix.column(1).toVector3D();y=(y-x*QVector3D::dotProduct(x,y)).normalized();
    auto z=QVector3D::crossProduct(x,y).normalized();
    btTransform result;result.setIdentity();
    if(x.lengthSquared()>.5f&&y.lengthSquared()>.5f&&z.lengthSquared()>.5f)
        result.setBasis(btMatrix3x3(x.x(),y.x(),z.x(),x.y(),y.y(),z.y(),x.z(),y.z(),z.z()));
    result.setOrigin(bullet(matrix.column(3).toVector3D()));return result;
}
QMatrix4x4 qt(const btTransform &transform){btScalar values[16];transform.getOpenGLMatrix(values);QMatrix4x4 matrix;for(int r=0;r<4;++r)for(int c=0;c<4;++c)matrix(r,c)=values[c*4+r];return matrix;}
}

struct RigidBodySimulation::Impl {
    struct Body {
        QString id;
        bool active=false;
        QMatrix4x4 initialWorld,initialInverse,renderOffset;
        QVector3D center;
        std::unique_ptr<btTriangleMesh> triangles;
        std::unique_ptr<btCollisionShape> shape;
        std::unique_ptr<btDefaultMotionState> motion;
        std::unique_ptr<btRigidBody> rigid;
    };
    quint64 signature;
    qint64 tick=0;
    ElementDesign authored;
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher{&configuration};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world{&dispatcher,&broadphase,&solver,&configuration};
    std::vector<std::unique_ptr<Body>> bodies;

    Impl(const ElementDesign &scene,quint64 hash):signature(hash),authored(scene){
        // A face resting on the ground needs multiple contacts to avoid an
        // artificial sideways impulse from a single corner contact.
        configuration.setPlaneConvexMultipointIterations(3,3);
        configuration.setConvexConvexMultipointIterations(3,3);
        authored.m_physics.reset();
        for(auto &object:authored.objects)object.phys.enabled=false;
        world.setGravity(btVector3(0,0,-qMax(0.,scene.world.gravity)));
        world.getSolverInfo().m_numIterations=20;
        world.getSolverInfo().m_splitImpulse=true;
        if(scene.world.floor){
            auto floor=std::make_unique<Body>();floor->shape=std::make_unique<btStaticPlaneShape>(btVector3(0,0,1),0);
            btTransform origin;origin.setIdentity();floor->motion=std::make_unique<btDefaultMotionState>(origin);
            btRigidBody::btRigidBodyConstructionInfo info(0,floor->motion.get(),floor->shape.get());info.m_friction=.7;info.m_restitution=.2;
            floor->rigid=std::make_unique<btRigidBody>(info);world.addRigidBody(floor->rigid.get());bodies.push_back(std::move(floor));
        }
        for(const auto &source:scene.objects){
            if(!source.phys.enabled||!source.isRenderable())continue;
            const auto *raw=authored.find(source.id);if(!raw)continue;
            const auto object=raw->evaluated(0);auto body=std::make_unique<Body>();body->id=source.id;body->active=source.phys.active;
            body->initialWorld=authored.worldMatrix(object,0);body->initialInverse=body->initialWorld.inverted();
            QVector<QVector3D> vertices;QVector<QVector<int>> faces;
            if(object.type=="Mesh"){const auto &mesh=authored.evalMesh(object,0);vertices=mesh.v;faces=mesh.f;}
            else {
                const auto outline=surfaceOutline(QRectF(-object.w*kPxPerUnit/2,-object.h*kPxPerUnit/2,object.w*kPxPerUnit,object.h*kPxPerUnit),object).toFillPolygon();
                const float thickness=qBound(.002,source.style["physics.thickness"].toDouble(.04),10.)/2;
                for(const auto &point:outline){vertices.append(QVector3D(point.x()/kPxPerUnit,point.y()/kPxPerUnit,-thickness));vertices.append(QVector3D(point.x()/kPxPerUnit,point.y()/kPxPerUnit,thickness));}
            }
            if(vertices.isEmpty())continue;
            if(body->active&&object.type=="Mesh"&&vertices.size()>=3){
                QVector3D normal;
                for(int i=1;i+1<vertices.size()&&normal.lengthSquared()<1e-10f;++i)normal=QVector3D::crossProduct(vertices[i]-vertices[0],vertices[i+1]-vertices[0]);
                normal.normalize();bool planar=normal.lengthSquared()>.5f;
                for(const auto &v:vertices)planar=planar&&qAbs(QVector3D::dotProduct(v-vertices[0],normal))<.00001f;
                if(planar){const float half=qBound(.002,source.style["physics.thickness"].toDouble(.04),10.)/2;const auto original=vertices;vertices.clear();for(const auto &v:original){vertices.append(v-normal*half);vertices.append(v+normal*half);}}
            }
            QVector3D low(1e20f,1e20f,1e20f),high(-1e20f,-1e20f,-1e20f);
            for(auto &v:vertices){v=body->initialWorld.map(v);low={qMin(low.x(),v.x()),qMin(low.y(),v.y()),qMin(low.z(),v.z())};high={qMax(high.x(),v.x()),qMax(high.y(),v.y()),qMax(high.z(),v.z())};}
            body->center=(low+high)/2;
            for(auto &v:vertices)v-=body->center;
            if(!body->active&&object.type=="Mesh"&&!faces.isEmpty()){
                body->triangles=std::make_unique<btTriangleMesh>();
                for(const auto &face:faces)for(int i=1;i+1<face.size();++i){const int a=face[0],b=face[i],c=face[i+1];if(a<0||b<0||c<0||a>=vertices.size()||b>=vertices.size()||c>=vertices.size())continue;body->triangles->addTriangle(bullet(vertices[a]),bullet(vertices[b]),bullet(vertices[c]),true);}
                if(body->triangles->getNumTriangles())body->shape=std::make_unique<btBvhTriangleMeshShape>(body->triangles.get(),true);
            }
            if(!body->shape){auto hull=std::make_unique<btConvexHullShape>();for(const auto &v:vertices)hull->addPoint(bullet(v),false);hull->recalcLocalAabb();hull->optimizeConvexHull();body->shape=std::move(hull);}
            body->shape->setMargin(qBound(.0001,source.style["physics.margin"].toDouble(.005),.2));
            btTransform initial;initial.setIdentity();initial.setOrigin(bullet(body->center));
            body->motion=std::make_unique<btDefaultMotionState>(initial);const btScalar mass=body->active?qBound(.001,source.phys.mass,1000000.):0.;btVector3 inertia(0,0,0);if(mass)body->shape->calculateLocalInertia(mass,inertia);
            btRigidBody::btRigidBodyConstructionInfo info(mass,body->motion.get(),body->shape.get(),inertia);
            info.m_friction=qBound(0.,source.phys.friction,2.);info.m_restitution=qBound(0.,source.phys.bounce,1.);info.m_linearDamping=.02;info.m_angularDamping=.02;
            body->rigid=std::make_unique<btRigidBody>(info);body->rigid->setSleepingThresholds(.03,.04);
            if(body->active&&source.style["physics.planar"].toBool(source.type!="Mesh")){body->rigid->setLinearFactor(btVector3(1,0,1));body->rigid->setAngularFactor(btVector3(0,1,0));}
            if(!body->active){body->rigid->setCollisionFlags((body->rigid->getCollisionFlags()&~btCollisionObject::CF_STATIC_OBJECT)|btCollisionObject::CF_KINEMATIC_OBJECT);body->rigid->setActivationState(DISABLE_DEACTIVATION);}
            else {const float extent=qMax(.002f,std::min({high.x()-low.x(),high.y()-low.y(),high.z()-low.z()}));body->rigid->setCcdMotionThreshold(extent*.5f);body->rigid->setCcdSweptSphereRadius(extent*.2f);}
            QMatrix4x4 shift;shift.translate(-body->center);body->renderOffset=shift*body->initialWorld;
            world.addRigidBody(body->rigid.get());bodies.push_back(std::move(body));
        }
    }
    ~Impl(){for(auto &body:bodies)world.removeRigidBody(body->rigid.get());bodies.clear();}
    void advance(qint64 milliseconds){
        const qint64 target=qMax<qint64>(0,milliseconds)/8;
        while(tick<target){
            const qint64 next=(tick+1)*8;
            for(auto &body:bodies)if(!body->active&&!body->id.isEmpty())if(const auto *object=authored.find(body->id)){
                auto delta=authored.worldMatrix(*object,next)*body->initialInverse;
                QMatrix4x4 center;center.translate(body->center);const auto pose=delta*center;
                body->motion->setWorldTransform(bullet(pose));body->rigid->activate(true);
            }
            world.stepSimulation(btScalar(.008),0);++tick;
        }
    }
};

RigidBodySimulation::RigidBodySimulation(const ElementDesign &scene,quint64 signature):d(std::make_unique<Impl>(scene,signature)){}
RigidBodySimulation::~RigidBodySimulation()=default;
quint64 RigidBodySimulation::signature()const{return d->signature;}
qint64 RigidBodySimulation::time()const{return d->tick*8;}
void RigidBodySimulation::advance(qint64 ms){d->advance(ms);}
QMatrix4x4 RigidBodySimulation::worldMatrix(const QString &id,const QMatrix4x4 &fallback)const{for(const auto &body:d->bodies)if(body->id==id&&body->active)return qt(body->rigid->getWorldTransform())*body->renderOffset;return fallback;}

quint64 ElementDesign::physicsSignature()const{quint64 signature=qHash(toJson());for(const auto &object:objects)for(auto it=object.overrides.cbegin();it!=object.overrides.cend();++it)signature^=qHashMulti(0,object.id,it.key(),it.value());return signature;}
void ElementDesign::ensurePhysics(qint64 ms)const{
    const auto signature=physicsSignature();
    if(!m_physics||m_physics.use_count()>1||m_physics->signature()!=signature||qMax<qint64>(0,ms)/8*8<m_physics->time())m_physics=std::make_shared<RigidBodySimulation>(*this,signature);
    m_physics->advance(ms);
}
QMatrix4x4 ElementDesign::physicsWorldMatrix(const DObject &o,qint64 ms)const{ensurePhysics(ms);return m_physics->worldMatrix(o.id,o.parentInverse*matrixOf(o.at(ms)));}
DProps ElementDesign::propsAt(const DObject &o,qint64 ms)const{
    DProps p=o.at(ms);if(!o.phys.enabled||!o.phys.active||!o.isRenderable())return p;
    const auto local=parentMatrix(o,ms).inverted()*physicsWorldMatrix(o,ms);p.loc=local.column(3).toVector3D();
    auto x=local.column(0).toVector3D(),y=local.column(1).toVector3D(),z=local.column(2).toVector3D();
    float sx=x.length(),sy=y.length(),sz=z.length();if(QVector3D::dotProduct(QVector3D::crossProduct(x,y),z)<0)sx=-sx;p.scl={sx,sy,sz};
    if(qAbs(sx)>1e-8)x/=sx;if(sy>1e-8)y/=sy;if(sz>1e-8)z/=sz;
    const float ey=qAsin(qBound(-1.f,-x.z(),1.f));const bool regular=qAbs(x.z())<.99999f;
    p.rot={qRadiansToDegrees(regular?qAtan2(y.z(),z.z()):qAtan2(-z.y(),y.y())),qRadiansToDegrees(ey),qRadiansToDegrees(regular?qAtan2(x.y(),x.x()):0.f)};return p;
}
