#include "FLoaderFBX.h"
#include "EngineLoop.h"
#include <functional>


using namespace FBX;

bool FLoaderFBX::ParseSkeletalMesh(const FString& FBXPath, FSkeletalMeshRenderData& OutMeshData)
{
    FbxManager* SdkManager = FbxManager::Create();
    FbxIOSettings* IoSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(IoSettings);

    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
    if (!Importer->Initialize(*FBXPath, -1, SdkManager->GetIOSettings()))
    {
        UE_LOG(LogLevel::Display, "FBX Import Failed: %s", Importer->GetStatus().GetErrorString());
        return false;
    }

    FbxScene* Scene = FbxScene::Create(SdkManager, "FBXScene");
    Importer->Import(Scene);
    Importer->Destroy();

    FbxNode* RootNode = Scene->GetRootNode();
    if (!RootNode)
        return false;

    for (int i = 0; i < RootNode->GetChildCount(); i++)
    {
        FbxNode* Child = RootNode->GetChild(i);
        if (!Child->GetMesh()) continue;

        FbxMesh* Mesh = Child->GetMesh();

        TArray<TArray<FBoneWeight>> VertexBoneWeights;
        ExtractSkinning(Mesh, VertexBoneWeights);

        ProcessSkeletalMesh(Mesh, OutMeshData, Scene, VertexBoneWeights);
    }

    ComputeBoundingBox(OutMeshData);
    ExtractSkeleton(Scene, OutMeshData);
    SdkManager->Destroy();
    return true;
}

void FLoaderFBX::ProcessSkeletalMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutMeshData, FbxScene* Scene, const TArray<TArray<FBoneWeight>>& VertexBoneWeights)
{
    FbxVector4* ControlPoints = Mesh->GetControlPoints();
    int PolygonCount = Mesh->GetPolygonCount();

    for (int i = 0; i < PolygonCount; i++)
    {
        if (Mesh->GetPolygonSize(i) != 3) continue;

        for (int j = 0; j < 3; j++)
        {
            int CPIndex = Mesh->GetPolygonVertex(i, j);
            FbxVector4 Pos = ControlPoints[CPIndex];

            FSkeletalMeshVertex Vertex = {};
            FVector Position = ConvertVector(Pos);
            Vertex.X = Position.X;
            Vertex.Y = Position.Y;
            Vertex.Z = Position.Z;

            // Normal
            FbxVector4 Normal;
            Mesh->GetPolygonVertexNormal(i, j, Normal);
            FVector NormalV = ConvertVector(Normal);
            Vertex.NormalX = NormalV.X;
            Vertex.NormalY = NormalV.Y;
            Vertex.NormalZ = NormalV.Z;

            // UV
            FbxStringList UVSets;
            Mesh->GetUVSetNames(UVSets);
            if (UVSets.GetCount() > 0)
            {
                FbxVector2 UV;
                bool Unmapped;
                Mesh->GetPolygonVertexUV(i, j, UVSets[0], UV, Unmapped);
                FVector2D ConvertedUV = ConvertUV(UV);
                Vertex.U = ConvertedUV.X;
                Vertex.V = ConvertedUV.Y;
            }

            Vertex.R = Vertex.G = Vertex.B = 0.7f;
            Vertex.A = 1.0f;

            const auto& Weights = VertexBoneWeights[CPIndex];
            for (int w = 0; w < Weights.Num() && w < 4; ++w)
            {
                Vertex.BoneIndices[w] = Weights[w].BoneIndex;
                Vertex.BoneWeights[w] = Weights[w].Weight;
            }

            OutMeshData.Indices.Add(OutMeshData.Vertices.Num());
            OutMeshData.Vertices.Add(Vertex);
        }
    }
}

void FLoaderFBX::ExtractSkinning(FbxMesh* Mesh, TArray<TArray<FBoneWeight>>& OutVertexBoneWeights)
{
    int NumVerts = Mesh->GetControlPointsCount();
    OutVertexBoneWeights.SetNum(NumVerts);

    FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(0, FbxDeformer::eSkin);
    if (!Skin) return;

    for (int i = 0; i < Skin->GetClusterCount(); ++i)
    {
        FbxCluster* Cluster = Skin->GetCluster(i);
        if (!Cluster || !Cluster->GetLink()) continue;

        int BoneIndex = i;
        int* Indices = Cluster->GetControlPointIndices();
        double* Weights = Cluster->GetControlPointWeights();
        int Count = Cluster->GetControlPointIndicesCount();

        for (int j = 0; j < Count; ++j)
        {
            int V = Indices[j];
            float W = (float)Weights[j];
            if (W > 0.0f && V < OutVertexBoneWeights.Num())
                OutVertexBoneWeights[V].Add({ BoneIndex, W });
        }
    }

    for (auto& Weights : OutVertexBoneWeights)
    {
        if (Weights.Num() > 4)
        {
            Weights.Sort([](const FBoneWeight& A, const FBoneWeight& B) {
                return A.Weight > B.Weight;
                });
            Weights.SetNum(4);
        }

        float Total = 0.0f;
        for (auto& W : Weights) Total += W.Weight;
        if (Total > 0.0f)
        {
            for (auto& W : Weights) W.Weight /= Total;
        }
    }
}

void FLoaderFBX::ExtractSkeleton(FbxScene* Scene, FSkeletalMeshRenderData& OutMeshData)
{
    FbxNode* RootNode = Scene->GetRootNode();
    if (!RootNode) return;

    TMap<FString, int> BoneNameToIndex;

    // 먼저 Traverse 변수 선언
    std::function<void(FbxNode*, int)> Traverse;

    // 람다 정의 (자기 자신 호출 가능하게)
    Traverse = [&](FbxNode* Node, int ParentIndex)
        {
            if (Node->GetNodeAttribute() &&
                Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            {
                FSkeletonBone Bone;
                Bone.Name = Node->GetName();
                Bone.ParentIndex = ParentIndex;
                Bone.BindPoseMatrix = FMatrix::Identity;

                int BoneIndex = OutMeshData.Skeleton.Num();
                BoneNameToIndex.Add(Bone.Name, BoneIndex);
                OutMeshData.Skeleton.Add(Bone);

                ParentIndex = BoneIndex;
            }

            for (int i = 0; i < Node->GetChildCount(); ++i)
            {
                Traverse(Node->GetChild(i), ParentIndex);
            }
        };

    Traverse(RootNode, -1);
}




