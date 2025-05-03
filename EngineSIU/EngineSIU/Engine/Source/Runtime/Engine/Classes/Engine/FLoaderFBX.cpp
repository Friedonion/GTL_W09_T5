#include "FLoaderFBX.h"
#include "EngineLoop.h"
#include <functional>
#include "UObject/ObjectFactory.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Components/Material/Material.h"
#include "FLoaderOBJ.h"
#include "HAL/PlatformType.h"

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

    FbxGeometryConverter Converter(SdkManager);
    Converter.Triangulate(Scene, true);

    FbxNode* RootNode = Scene->GetRootNode();
    if (!RootNode)
    {
        SdkManager->Destroy();
        return false;
    }

    TraverseMeshNodes(RootNode, OutMeshData, Scene);

    ComputeBoundingBox(OutMeshData);
    ExtractSkeleton(Scene, OutMeshData);
    SdkManager->Destroy();
    return true;
}


void FLoaderFBX::ProcessSkeletalMesh(FbxMesh* Mesh, FSkeletalMeshRenderData& OutMeshData, FbxScene* Scene, const TArray<TArray<FBoneWeight>>& VertexBoneWeights)
{
    FbxVector4* ControlPoints = Mesh->GetControlPoints();
    int PolygonCount = Mesh->GetPolygonCount();

    FbxNode* Node = Mesh->GetNode();
    FbxLayerElementMaterial* MaterialElement = Mesh->GetElementMaterial();

    // CPIndex + polygonVertexIdx 기준으로 유일한 정점 생성
    TMap<FString, int> VertexMap;

    for (int i = 0; i < PolygonCount; i++)
    {
        if (Mesh->GetPolygonSize(i) != 3) continue;

        int MaterialIndex = 0;
        if (MaterialElement && MaterialElement->GetMappingMode() == FbxLayerElement::eByPolygon)
        {
            MaterialIndex = MaterialElement->GetIndexArray().GetAt(i);
        }

        // 역순으로 삼각형 추가 (backface culling 대응)
        for (int j = 2; j >= 0; j--)
        {
            int CPIndex = Mesh->GetPolygonVertex(i, j);

            // 고유 키: CPIndex + UVIndex + NormalIndex 등
            FString Key = FString::Printf(TEXT("%d_%d_%d"), CPIndex, i, j);
            int* CachedIndex = VertexMap.Find(Key);
            if (CachedIndex)
            {
                OutMeshData.Indices.Add(*CachedIndex);
                continue;
            }

            FSkeletalMeshVertex Vertex = {};
            FVector Position = ConvertVector(ControlPoints[CPIndex]);
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
                FVector2D UVConv = ConvertUV(UV);
                Vertex.U = UVConv.X;
                Vertex.V = UVConv.Y;
            }

            Vertex.R = Vertex.G = Vertex.B = 0.7f;
            Vertex.A = 1.0f;
            Vertex.MaterialIndex = MaterialIndex;

            const auto& Weights = VertexBoneWeights[CPIndex];
            for (int w = 0; w < Weights.Num() && w < 4; ++w)
            {
                Vertex.BoneIndices[w] = Weights[w].BoneIndex;
                Vertex.BoneWeights[w] = Weights[w].Weight;
            }

            int NewIndex = OutMeshData.Vertices.Num();
            OutMeshData.Vertices.Add(Vertex);
            OutMeshData.Indices.Add(NewIndex);
            VertexMap.Add(Key, NewIndex);
        }
    }

    // Submesh 분할
    TMap<int, FMaterialSubset> MaterialIndexToSubset;

    for (int i = 0; i < OutMeshData.Indices.Num(); i += 3)
    {
        int TriangleIdx0 = OutMeshData.Indices[i];
        int MatIndex = OutMeshData.Vertices[TriangleIdx0].MaterialIndex;

        if (!MaterialIndexToSubset.Contains(MatIndex))
        {
            FMaterialSubset Subset;
            Subset.MaterialIndex = MatIndex;
            Subset.IndexStart = i;
            Subset.IndexCount = 3;
            MaterialIndexToSubset.Add(MatIndex, Subset);
        }
        else
        {
            MaterialIndexToSubset[MatIndex].IndexCount += 3;
        }
    }

    for (auto& Pair : MaterialIndexToSubset)
    {
        OutMeshData.MaterialSubsets.Add(Pair.Value);
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

USkeletalMesh* FManagerFBX::CreateSkeletalMesh(const FString& PathFileName)
{
    FBX::FSkeletalMeshRenderData* RenderData = new FBX::FSkeletalMeshRenderData();

    if (!FLoaderFBX::ParseSkeletalMesh(PathFileName, *RenderData))
    {
        delete RenderData;
        return nullptr;
    }

    USkeletalMesh* NewMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);
    NewMesh->SetData(RenderData);
    return NewMesh;
}

void FLoaderFBX::TraverseMeshNodes(FbxNode* Node, FSkeletalMeshRenderData& OutMeshData, FbxScene* Scene)
{
    if (Node->GetMesh())
    {
        FbxMesh* Mesh = Node->GetMesh();

        // 기존 머터리얼 로직 복붙
        int MaterialCount = Node->GetMaterialCount();
        for (int m = 0; m < MaterialCount; ++m)
        {
            FbxSurfaceMaterial* Material = Node->GetMaterial(m);
            FObjMaterialInfo MatInfo;

            if (Material)
            {
                MatInfo.MaterialName = Material->GetName();
                FbxProperty DiffuseProperty = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
                if (DiffuseProperty.IsValid())
                {
                    int TextureCount = DiffuseProperty.GetSrcObjectCount<FbxFileTexture>();
                    if (TextureCount > 0)
                    {
                        FbxFileTexture* Texture = DiffuseProperty.GetSrcObject<FbxFileTexture>(0);
                        MatInfo.DiffuseTexturePath = ((FString)Texture->GetFileName()).ToWideString();
                    }
                }
            }

            OutMeshData.Materials.Add(MatInfo);
        }

        TArray<TArray<FBoneWeight>> VertexBoneWeights;
        ExtractSkinning(Mesh, VertexBoneWeights);
        ProcessSkeletalMesh(Mesh, OutMeshData, Scene, VertexBoneWeights);
    }

    // 재귀적으로 순회
    for (int i = 0; i < Node->GetChildCount(); i++)
    {
        TraverseMeshNodes(Node->GetChild(i), OutMeshData, Scene);
    }
}





