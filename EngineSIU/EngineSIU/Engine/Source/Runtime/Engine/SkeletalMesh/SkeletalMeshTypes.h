#pragma once
#include "Container/Array.h"
#include "Container/Map.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"


struct FMaterialSubset;
class UMaterial;
struct FObjMaterialInfo;

struct FSkinnedVertex
{
    FVector Position;
    FVector Normal;
    FVector Tangent;
    FVector2D UV;
    uint32 MaterialIndex = 0;

    int BoneIndices[4] = { -1, -1, -1, -1 };
    float BoneWeights[4] = { 0, 0, 0, 0 };
};


struct FBone
{
    FString Name;
    int ParentIndex = -1;
    FMatrix GlobalBindPoseInverse;
    FMatrix GlobalPose; // Reference pose matrix (no animation)
};

struct FSkeletalMeshData
{
    TArray<FSkinnedVertex> Vertices;
    TArray<uint32> Indices;
    TArray<FBone> Bones;
    TMap<FString, int> BoneNameToIndex;
    TArray<UMaterial*> Materials;
    TArray<FMaterialSubset> MaterialSubsets;
};
