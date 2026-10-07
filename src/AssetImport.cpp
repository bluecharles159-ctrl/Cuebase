#include "AssetImport.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <QFileInfo>
#include <QUuid>
#include <functional>

QVector<DObject> AssetImport::meshes(const QString &path,QString *error){
    Assimp::Importer importer;
    const auto *scene=importer.ReadFile(path.toUtf8().constData(),aiProcess_JoinIdenticalVertices|aiProcess_SortByPType|aiProcess_ValidateDataStructure);
    QVector<DObject> objects;
    if(!scene||!scene->mRootNode){if(error)*error=QString::fromUtf8(importer.GetErrorString());return objects;}
    // Imported node transforms are baked into each independent editable mesh.
    std::function<void(const aiNode*,aiMatrix4x4)> visit=[&](const aiNode *node,aiMatrix4x4 parent){
        const auto matrix=parent*node->mTransformation;
        for(unsigned i=0;i<node->mNumMeshes;++i){const auto *source=scene->mMeshes[node->mMeshes[i]];
            if(!source->HasPositions()||!source->HasFaces())continue;
            DObject object;object.id=QUuid::createUuid().toString(QUuid::WithoutBraces);object.type="Mesh";
            object.name=QString::fromUtf8(node->mName.C_Str());if(object.name.isEmpty())object.name=QFileInfo(path).completeBaseName();
            for(unsigned j=0;j<source->mNumVertices;++j){auto point=matrix*source->mVertices[j];object.mesh.v.append({point.x,point.y,point.z});}
            for(unsigned j=0;j<source->mNumFaces;++j){const auto &face=source->mFaces[j];if(face.mNumIndices<3)continue;QVector<int> indices;for(unsigned k=0;k<face.mNumIndices;++k)indices.append(face.mIndices[k]);object.mesh.f.append(indices);}
            if(matrix.Determinant()<0)object.mesh.flipNormals();
            if(source->mMaterialIndex<scene->mNumMaterials){aiColor4D color;if(aiGetMaterialColor(scene->mMaterials[source->mMaterialIndex],AI_MATKEY_COLOR_DIFFUSE,&color)==AI_SUCCESS){object.mat.color=QColor::fromRgbF(qBound(0.f,color.r,1.f),qBound(0.f,color.g,1.f),qBound(0.f,color.b,1.f));object.mat.alpha=qBound(0.f,color.a,1.f);}}
            if(!object.mesh.isEmpty()){object.captureDefaults();objects.append(object);}
        }
        for(unsigned i=0;i<node->mNumChildren;++i)visit(node->mChildren[i],matrix);
    };visit(scene->mRootNode,aiMatrix4x4());
    if(objects.isEmpty()&&error)*error="This file contains no polygon meshes.";return objects;
}
