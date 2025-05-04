#include "SkeletalMeshLoader.h"
#include <fbxsdk.h>

#include "Define.h"
#include "USkeletalMesh.h"
#include "Components/Material/Material.h"
#include "Engine/FLoaderOBJ.h"
#include "Math/Matrix.h"
#include "UObject/ObjectFactory.h"
#include "Utils/FPaths.h"

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

static void ExtractMesh(FSkeletalMeshData& OutMeshData, FbxMesh* Mesh, FbxSkin* Skin, FbxNode* Node, const FString& FilePath)
{
    const int ControlPointCount = Mesh->GetControlPointsCount();
    const FbxVector4* ControlPoints = Mesh->GetControlPoints();

    const int VertexOffset = OutMeshData.Vertices.Num();

    TArray<FSkinnedVertex> Vertices;
    Vertices.SetNum(ControlPointCount);
    FbxGeometryElementUV* UVElement = Mesh->GetElementUV();
    FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();

    // Bone weights (per ControlPointIndex)
    TMap<int, FSkinnedVertex> ControlPointInfluence;
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

    // Polygon → Vertex
    for (int polyIndex = 0; polyIndex < Mesh->GetPolygonCount(); ++polyIndex)
    {
        int MaterialIndex = 0;
        if (MaterialElement && MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
        {
            MaterialIndex = MaterialElement->GetIndexArray().GetAt(polyIndex);
        }

        for (int vertIndex = 0; vertIndex < 3; ++vertIndex)
        {
            int ctrlPtIdx = Mesh->GetPolygonVertex(polyIndex, vertIndex);
            FbxVector4 fbxPos = Mesh->GetControlPointAt(ctrlPtIdx);
            FSkinnedVertex V;

            V.Position = ConvertFbxVector(fbxPos);
            V.MaterialIndex = MaterialIndex;

            if (ControlPointInfluence.Contains(ctrlPtIdx))
            {
                V = ControlPointInfluence[ctrlPtIdx];
                V.Position = ConvertFbxVector(fbxPos); // 보정
                V.MaterialIndex = MaterialIndex;
            }

            if (UVElement)
            {
                int uvIndex = Mesh->GetTextureUVIndex(polyIndex, vertIndex);
                FbxVector2 fbxUV = UVElement->GetDirectArray().GetAt(uvIndex);
                V.UV = FVector2D(static_cast<float>(fbxUV[0]), 1.0f - static_cast<float>(fbxUV[1]));
            }

            OutMeshData.Indices.Add(OutMeshData.Vertices.Num()); // 새로운 정점 인덱스
            OutMeshData.Vertices.Add(V);
        }
    }

    // 머티리얼
    for (int i = 0; i < Node->GetMaterialCount(); ++i)
    {
        FbxSurfaceMaterial* Material = Node->GetMaterial(i);
        if (Material)
        {
            UMaterial* NewMaterial = FSkeletalMeshLoader::CreateMaterialFromFbx(Material, FilePath);
            if (NewMaterial)
                OutMeshData.Materials.Add(NewMaterial);
        }
    }

}

static void TraverseAndExtractMeshes(FSkeletalMeshData& OutMeshData, FbxNode* Node, const FString& FilePath)
{
    if (!Node) return;

    FbxMesh* Mesh = Node->GetMesh();
    if (Mesh && Mesh->GetPolygonCount() > 0)
    {
        const int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
        FbxSkin* Skin = (DeformerCount > 0) ? static_cast<FbxSkin*>(Mesh->GetDeformer(0)) : nullptr;
        ExtractMesh(OutMeshData, Mesh, Skin, Node,FilePath);
    }

    for (int i = 0; i < Node->GetChildCount(); ++i)
    {
        TraverseAndExtractMeshes(OutMeshData, Node->GetChild(i),FilePath);
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

    // 메시 추출
    TraverseAndExtractMeshes(MeshData, Scene->GetRootNode(), FilePath);

    // --- MaterialIndex 기반 Submesh 분리 ---
    TMap<uint32, TArray<uint32>> SubmeshIndexMap;
    for (int32 i = 0; i < MeshData.Indices.Num(); i += 3)
    {
        uint32 Index0 = MeshData.Indices[i];
        uint32 MatIndex = 0;
        if (MeshData.Vertices.IsValidIndex(Index0))
        {
            MatIndex = MeshData.Vertices[Index0].MaterialIndex;
        }

        TArray<uint32>& FaceList = SubmeshIndexMap.FindOrAdd(MatIndex);
        FaceList.Add(MeshData.Indices[i]);
        FaceList.Add(MeshData.Indices[i + 1]);
        FaceList.Add(MeshData.Indices[i + 2]);
    }

    MeshData.Indices.Empty();
    MeshData.MaterialSubsets.Empty();

    // 정렬
    TArray<uint32> SortedKeys;
    for (const auto& Pair : SubmeshIndexMap)
    {
        SortedKeys.Add(Pair.Key);
    }
    for (int i = 0; i < SortedKeys.Num(); ++i)
    {
        for (int j = i + 1; j < SortedKeys.Num(); ++j)
        {
            if (SortedKeys[j] < SortedKeys[i])
            {
                uint32 Temp = SortedKeys[i];
                SortedKeys[i] = SortedKeys[j];
                SortedKeys[j] = Temp;
            }
        }
    }

    // Submesh 정보 적용
    for (uint32 MatIndex : SortedKeys)
    {
        const TArray<uint32>& FaceIndices = *SubmeshIndexMap.Find(MatIndex);

        FMaterialSubset Subset;
        Subset.MaterialIndex = MatIndex;
        Subset.IndexStart = MeshData.Indices.Num();
        Subset.IndexCount = FaceIndices.Num();

        MeshData.Indices.Append(FaceIndices);
        MeshData.MaterialSubsets.Add(Subset);
    }

    // 디버깅 출력
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

UMaterial* FSkeletalMeshLoader::CreateMaterialFromFbx(FbxSurfaceMaterial* FbxMat, const FString& FbxFilePath)
{
    if (!FbxMat)
        return nullptr;

    UMaterial* NewMaterial = FObjectFactory::ConstructObject<UMaterial>(nullptr);
    if (!NewMaterial)
        return nullptr;

    FObjMaterialInfo MatInfo;
    MatInfo.MaterialName = FbxMat->GetName();

    auto ExtractColor = [](FbxProperty Prop, FVector& OutColor)
        {
            if (Prop.IsValid())
            {
                FbxDouble3 Color = Prop.Get<FbxDouble3>();
                OutColor = FVector(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]));
            }
        };

    ExtractColor(FbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse), MatInfo.Diffuse);
    ExtractColor(FbxMat->FindProperty(FbxSurfaceMaterial::sSpecular), MatInfo.Specular);
    ExtractColor(FbxMat->FindProperty(FbxSurfaceMaterial::sEmissive), MatInfo.Emissive);

    // Transparency
    FbxProperty TransparencyProp = FbxMat->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
    if (TransparencyProp.IsValid())
    {
        double Transparency = TransparencyProp.Get<FbxDouble>();
        MatInfo.TransparencyScalar = static_cast<float>(Transparency);
        MatInfo.bTransparent = (Transparency < 1.0f);
    }

    // Shininess
    FbxProperty ShininessProp = FbxMat->FindProperty(FbxSurfaceMaterial::sShininess);
    if (ShininessProp.IsValid())
    {
        double Shininess = ShininessProp.Get<FbxDouble>();
        MatInfo.SpecularScalar = static_cast<float>(Shininess);
    }

    // Diffuse Texture
    FbxProperty DiffuseProp = FbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
    if (DiffuseProp.IsValid())
    {
        FbxFileTexture* Texture = DiffuseProp.GetSrcObject<FbxFileTexture>(0);
        if (Texture)
        {
            FString AbsPath = Texture->GetFileName();
            FString RelPath = Texture->GetRelativeFileName();

            FWString TexturePath;
            if (!RelPath.IsEmpty())
            {
                FString BaseFolder;
                int32 LastSlash = FbxFilePath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
                if (LastSlash != -1)
                {
                    BaseFolder = FbxFilePath.Left(LastSlash);
                }

                FString CombinedPath = BaseFolder + TEXT("/") + RelPath;
                CombinedPath = CombinedPath.Replace(TEXT("\\"), TEXT("/"));

                TexturePath = CombinedPath.ToWideString();
            }
            else
            {
                TexturePath = AbsPath.ToWideString();
            }

            MatInfo.DiffuseTexturePath = TexturePath;
            MatInfo.DiffuseTextureName = (FPaths::GetCleanFilename(FString(TexturePath)));
            MatInfo.TextureFlag |= (1 << 1);

            FLoaderOBJ::CreateTextureFromFile(TexturePath);
        }
    }
    //MatInfo.DiffuseTexturePath = L"C:/Users/Jungle/Documents/GitHub/GTL_W09_T5/EngineSIU/EngineSIU/Contents/FBX/Textures/Atlas_00001.png";
    NewMaterial->SetMaterialInfo(MatInfo);
    return NewMaterial;
}
