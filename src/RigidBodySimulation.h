#pragma once
#include <QMatrix4x4>
#include <QString>
#include <memory>
class ElementDesign;

// Fixed-step simulation cache. Copies of a composition detach this runtime before use.
class RigidBodySimulation {
public:
    RigidBodySimulation(const ElementDesign &scene,quint64 signature);
    ~RigidBodySimulation();
    quint64 signature() const;
    qint64 time() const;
    void advance(qint64 milliseconds);
    QMatrix4x4 worldMatrix(const QString &id,const QMatrix4x4 &fallback) const;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
