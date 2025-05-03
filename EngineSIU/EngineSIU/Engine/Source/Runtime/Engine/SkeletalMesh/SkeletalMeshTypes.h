#pragma once
#include "Container/Array.h"
#include "Container/Map.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"


struct FSkinnedVertex
{
    FVector Position;
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
struct FSkeletalMeshMaterial
{
    FString MaterialName;
    FString DiffuseTexturePath;
    // 필요 시 추가 속성들
};
struct FSkeletalMeshData
{
    TArray<FSkinnedVertex> Vertices;
    TArray<uint32> Indices;
    TArray<FBone> Bones;
    TMap<FString, int> BoneNameToIndex;
    TArray<FSkeletalMeshMaterial> Materials;
};
