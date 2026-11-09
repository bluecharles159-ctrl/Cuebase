#pragma once
#include "ElementDesign.h"

namespace TextureLayers {
inline QString prefix(const QString &id) {
    return id == "base" ? QStringLiteral("texture.") : "texture.layer."+id+".";
}
inline QStringList order(const DObject &object) {
    QStringList ids;
    for (const auto &value : object.style.value("texture.stack").toArray()) {
        const auto id = value.toString();
        if (!id.isEmpty() && !id.contains('.') && !ids.contains(id)) ids.append(id);
    }
    if (!ids.contains("base")) ids.prepend("base");
    return ids;
}
inline QStringList files(const DObject &object) {
    QStringList keys;
    for (const auto &id : order(object)) {
        for(const auto &field:QStringList{"file","maskFile"}) {
            const auto key = prefix(id)+field;
            if (!object.style.value(key).toString().isEmpty()) keys.append(key);
        }
    }
    return keys;
}
}
