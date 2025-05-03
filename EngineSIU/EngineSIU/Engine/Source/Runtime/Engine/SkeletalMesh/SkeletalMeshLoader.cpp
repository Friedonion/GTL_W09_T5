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

static void ExtractMesh(FSkeletalMeshData& OutMeshData, FbxMesh* Mesh, FbxSkin* Skin)
{
    const int ControlPointCount = Mesh->GetControlPointsCount();
    const FbxVector4* ControlPoints = Mesh->GetControlPoints();

    TArray<FSkinnedVertex> Vertices;
    Vertices.SetNum(ControlPointCount);

    // Bone weights from skin
    if (Skin)
    {
        for (int i = 0; i < Skin->GetClusterCount(); ++i)
        {
            FbxCluster* Cluster = Skin->GetCluster(i);
            FString BoneName = Cluster->GetLink()->GetName();
            int BoneIndex = OutMeshData.BoneNameToIndex[BoneName];

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

    // Position 저장
    for (int i = 0; i < ControlPointCount; ++i)
    {
        Vertices[i].Position = ConvertFbxVector(ControlPoints[i]);
    }

    // 삼각형 인덱스
    const int PolygonCount = Mesh->GetPolygonCount();
    for (int i = 0; i < PolygonCount; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int VertexIndex = Mesh->GetPolygonVertex(i, j);
            OutMeshData.Indices.Add(VertexIndex);
        }
    }

    OutMeshData.Vertices = Vertices;
}

USkeletalMesh* FSkeletalMeshLoader::LoadFromFBX(const FString& FilePath)
{
    FbxManager* SdkManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
    SdkManager->SetIOSettings(ios);

    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
    if (!Importer->Initialize((*FilePath), -1, SdkManager->GetIOSettings()))
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

    // 메시 찾기
    FbxNode* RootNode = Scene->GetRootNode();
    for (int i = 0; i < RootNode->GetChildCount(); ++i)
    {
        FbxNode* Child = RootNode->GetChild(i);
        FbxMesh* Mesh = Child->GetMesh();
        if (!Mesh) continue;

        const int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
        if (DeformerCount > 0)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(0));
            ExtractMesh(MeshData, Mesh, Skin);
        }
        FbxSurfaceMaterial* Material = Child->GetMaterial(0);
        if (Material)
        {
            FSkeletalMeshMaterial MatData;
            MatData.MaterialName = Material->GetName();

            // Diffuse 텍스처 경로 추출
            FbxProperty prop = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
            if (prop.IsValid())
            {
                FbxFileTexture* Texture = prop.GetSrcObject<FbxFileTexture>(0);
                if (Texture)
                {
                    MatData.DiffuseTexturePath = Texture->GetFileName();
                }
            }

            MeshData.Materials.Add(MatData);
        }

    }

    // SkeletalMesh 생성
    //USkeletalMesh* NewMesh = NewObject<USkeletalMesh>();
    USkeletalMesh* NewMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);
    NewMesh->SetSkeletalMeshData(MeshData);

    SdkManager->Destroy();
    return NewMesh;
}
