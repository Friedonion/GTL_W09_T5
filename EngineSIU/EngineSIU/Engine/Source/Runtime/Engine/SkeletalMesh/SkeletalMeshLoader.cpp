#include "SkeletalMeshLoader.h"
#include <fbxsdk.h>

#include "Define.h"
#include "USkeletalMesh.h"
#include "Math/Matrix.h"
#include "UObject/ObjectFactory.h"

static FVector ConvertFbxVector(const FbxVector4& vec)
{
    return FVector(static_cast<float>(vec[0]), static_cast<float>(vec[1]), static_cast<float>(vec[2]));
}

static FMatrix ConvertFbxMatrix(const FbxAMatrix& m)
{
    FMatrix result;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            result.M[r][c] = static_cast<float>(m.Get(r, c));
    return result;
}

static void ExtractSkeletonHierarchy(FSkeletalMeshData& OutMeshData, FbxNode* Node, int ParentIndex)
{
    if (!Node) return;

    if (Node->GetNodeAttribute() &&
        Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        FBone Bone;
        Bone.Name = Node->GetName();
        Bone.ParentIndex = ParentIndex;
        Bone.GlobalPose = ConvertFbxMatrix(Node->EvaluateGlobalTransform());
        Bone.GlobalBindPoseInverse = FMatrix::Inverse(Bone.GlobalPose);
        OutMeshData.BoneNameToIndex.Add(Bone.Name, OutMeshData.Bones.Num());
        OutMeshData.Bones.Add(Bone);

        ParentIndex = OutMeshData.Bones.Num() - 1;
    }

    for (int i = 0; i < Node->GetChildCount(); ++i)
    {
        ExtractSkeletonHierarchy(OutMeshData, Node->GetChild(i), ParentIndex);
    }
}

static void ExtractMesh(FSkeletalMeshData& OutMeshData, FbxMesh* Mesh, FbxSkin* Skin, FbxNode* Node)
{
    const int ControlPointCount = Mesh->GetControlPointsCount();
    const FbxVector4* ControlPoints = Mesh->GetControlPoints();

    const int VertexOffset = OutMeshData.Vertices.Num();

    TArray<FSkinnedVertex> Vertices;
    Vertices.SetNum(ControlPointCount);

    // Bone weights from skin
    if (Skin)
    {
        for (int i = 0; i < Skin->GetClusterCount(); ++i)
        {
            FbxCluster* Cluster = Skin->GetCluster(i);
            FString BoneName = Cluster->GetLink()->GetName();

            const int* FoundBoneIndex = OutMeshData.BoneNameToIndex.Find(BoneName);
            if (!FoundBoneIndex) continue;
            int BoneIndex = *FoundBoneIndex;


            int* Indices = Cluster->GetControlPointIndices();
            double* Weights = Cluster->GetControlPointWeights();
            int Count = Cluster->GetControlPointIndicesCount();

            for (int j = 0; j < Count; ++j)
            {
                int CtrlPtIdx = Indices[j];
                float Weight = static_cast<float>(Weights[j]);

                for (int k = 0; k < 4; ++k)
                {
                    if (Vertices[CtrlPtIdx].BoneWeights[k] == 0.0f)
                    {
                        Vertices[CtrlPtIdx].BoneIndices[k] = BoneIndex;
                        Vertices[CtrlPtIdx].BoneWeights[k] = Weight;
                        break;
                    }
                }
            }
        }
    }

    // Vertex Position 저장
    for (int i = 0; i < ControlPointCount; ++i)
    {
        Vertices[i].Position = ConvertFbxVector(ControlPoints[i]);
    }

    // 삼각형 인덱스 저장 (offset 적용)
    const int PolygonCount = Mesh->GetPolygonCount();
    for (int i = 0; i < PolygonCount; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int VertexIndex = Mesh->GetPolygonVertex(i, j);
            OutMeshData.Indices.Add(VertexOffset + VertexIndex);
        }
    }
    for (const FSkinnedVertex& V : Vertices)
    {
        OutMeshData.Vertices.Add(V); // 또는 Add(V); 사용자 정의 TArray에 맞게
    }


    // 머티리얼 처리
    FbxSurfaceMaterial* Material = Node->GetMaterial(0);
    if (Material)
    {
        FSkeletalMeshMaterial MatData;
        MatData.MaterialName = Material->GetName();

        FbxProperty prop = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (prop.IsValid())
        {
            FbxFileTexture* Texture = prop.GetSrcObject<FbxFileTexture>(0);
            if (Texture)
            {
                MatData.DiffuseTexturePath = Texture->GetFileName();
            }
        }

        OutMeshData.Materials.Add(MatData);
    }
}

static void TraverseAndExtractMeshes(FSkeletalMeshData& OutMeshData, FbxNode* Node)
{
    if (!Node) return;

    FbxMesh* Mesh = Node->GetMesh();
    if (Mesh && Mesh->GetPolygonCount() > 0)
    {
        const int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
        FbxSkin* Skin = (DeformerCount > 0) ? static_cast<FbxSkin*>(Mesh->GetDeformer(0)) : nullptr;
        ExtractMesh(OutMeshData, Mesh, Skin, Node);
    }

    for (int i = 0; i < Node->GetChildCount(); ++i)
    {
        TraverseAndExtractMeshes(OutMeshData, Node->GetChild(i));
    }
}

USkeletalMesh* FSkeletalMeshLoader::LoadFromFBX(const FString& FilePath)
{
    FbxManager* SdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(ios);

    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
    if (!Importer->Initialize(*FilePath, -1, SdkManager->GetIOSettings()))
    {
        UE_LOG(LogLevel::Error, TEXT("FBX Import Failed: %s"), *FilePath);
        return nullptr;
    }

    FbxScene* Scene = FbxScene::Create(SdkManager, "MyScene");
    Importer->Import(Scene);
    Importer->Destroy();

    FSkeletalMeshData MeshData;

    // 본 추출
    ExtractSkeletonHierarchy(MeshData, Scene->GetRootNode(), -1);

    // 메시 추출 (재귀 순회)
    TraverseAndExtractMeshes(MeshData, Scene->GetRootNode());
    for (const auto& Vertex : MeshData.Vertices)
    {
        UE_LOG(LogLevel::Display, TEXT("Vertex Pos: %s"), *Vertex.Position.ToString());
        UE_LOG(LogLevel::Display, TEXT("Bones: %d,%d,%d,%d  Weights: %.2f %.2f %.2f %.2f"),
            Vertex.BoneIndices[0], Vertex.BoneIndices[1], Vertex.BoneIndices[2], Vertex.BoneIndices[3],
            Vertex.BoneWeights[0], Vertex.BoneWeights[1], Vertex.BoneWeights[2], Vertex.BoneWeights[3]);
    }
    // SkeletalMesh 생성
    USkeletalMesh* NewMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);
    NewMesh->SetSkeletalMeshData(MeshData);

    SdkManager->Destroy();
    return NewMesh;
}
