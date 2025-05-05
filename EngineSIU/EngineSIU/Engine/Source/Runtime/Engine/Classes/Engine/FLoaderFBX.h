#pragma once

#include "Define.h"
#include <fbxsdk.h>


class USkeletalMesh;

namespace FBX
{
    struct FBoneWeight
    {
        int BoneIndex;
        float Weight;
    };

    struct FSkeletonBone
    {
        FString Name;
        int32 ParentIndex;
        FMatrix BindPoseMatrix;
    };

    struct FSkeletalMeshVertex : public FStaticMeshVertex
    {
        uint8 BoneIndices[4] = { 0 };
        float BoneWeights[4] = { 0 };
    };

    struct FSkeletalMeshRenderData
    {
        FWString ObjectName;
        FString DisplayName;

        TArray<FSkeletalMeshVertex> Vertices;
        TArray<UINT> Indices;

        ID3D11Buffer* VertexBuffer = nullptr;
        ID3D11Buffer* IndexBuffer = nullptr;

        TArray<FObjMaterialInfo> Materials;
        TArray<FMaterialSubset> MaterialSubsets;

        FVector BoundingBoxMin;
        FVector BoundingBoxMax;

        TArray<FSkeletonBone> Skeleton;

        TArray<FBX::FSkeletalMeshVertex> OriginalVertices;
    };
}

class FLoaderFBX
{
public:
    static bool ParseSkeletalMesh(const FString& FBXPath, FBX::FSkeletalMeshRenderData& OutMeshData);
    static void TraverseMeshNodes(FbxNode* Node, FBX::FSkeletalMeshRenderData& OutMeshData, FbxScene* Scene);

private:
    static void ExtractSkinning(FbxMesh* Mesh, TArray<TArray<FBX::FBoneWeight>>& OutVertexBoneWeights);
    static void ProcessSkeletalMesh(FbxMesh* Mesh, FBX::FSkeletalMeshRenderData& OutMeshData, FbxScene* Scene, const TArray<TArray<FBX::FBoneWeight>>& VertexBoneWeights);
    static void ExtractSkeleton(FbxScene* Scene, FBX::FSkeletalMeshRenderData& OutMeshData);


    static FVector ConvertVector(FbxVector4 Vec)
    {
        return FVector((float)Vec[0], (float)Vec[1], (float)Vec[2]);
    }

    static FVector2D ConvertUV(FbxVector2 Vec)
    {
        return FVector2D((float)Vec[0], 1.f - (float)Vec[1]);
    }

    static void ComputeBoundingBox(FBX::FSkeletalMeshRenderData& MeshData)
    {
        FVector Min(FLT_MAX), Max(-FLT_MAX);
        for (auto& V : MeshData.Vertices)
        {
            Min.X = std::min(Min.X, V.X);
            Min.Y = std::min(Min.Y, V.Y);
            Min.Z = std::min(Min.Z, V.Z);
            Max.X = std::max(Max.X, V.X);
            Max.Y = std::max(Max.Y, V.Y);
            Max.Z = std::max(Max.Z, V.Z);
        }
        MeshData.BoundingBoxMin = Min;
        MeshData.BoundingBoxMax = Max;
    }
    static FMatrix ConvertFbxMatrix(const FbxAMatrix& Src)
    {
        FMatrix Result;
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Col = 0; Col < 4; ++Col)
            {
                Result.M[Row][Col] = static_cast<float>(Src.Get(Row, Col));
            }
        }
        return Result;
    }

};


struct FManagerFBX
{
    static USkeletalMesh* CreateSkeletalMesh(const FString& PathFileName);
};

