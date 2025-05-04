#pragma once


#include "SkeletalMeshTypes.h"
#include <fbxsdk.h>

class UMaterial;
class FString;
class USkeletalMesh;

class FSkeletalMeshLoader
{
public:
    static USkeletalMesh* LoadFromFBX(const FString& FilePath);
    static UMaterial* CreateMaterialFromFbx(FbxSurfaceMaterial* FbxMat, const FString& FbxFilePath);


};
