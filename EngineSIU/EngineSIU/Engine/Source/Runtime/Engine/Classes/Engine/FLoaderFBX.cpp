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



void FLoaderFBX::ProcessSkeletalMesh(
    FbxMesh* Mesh,
    FBX::FSkeletalMeshRenderData& OutMeshData,
    FbxScene* Scene,
    const TArray<TArray<FBoneWeight>>& VertexBoneWeights)
{
    // 제어점과 폴리곤 개수
    FbxVector4* ControlPoints = Mesh->GetControlPoints();
    int PolygonCount = Mesh->GetPolygonCount();

    // FBX 재질 레이어
    FbxLayerElementMaterial* MaterialElement = Mesh->GetElementMaterial();

    // (1) 정점 중복 방지용 키 → 버텍스 인덱스
    TMap<FString, int> VertexMap;
    // (2) 재질별 인덱스 버퍼
    TMap<int, TArray<uint32>> MatToIndices;
    MatToIndices.Empty();

    // --- 폴리곤(삼각형) 순회 ---
    for (int poly = 0; poly < PolygonCount; ++poly)
    {
        if (Mesh->GetPolygonSize(poly) != 3)
            continue;

        // —— 재질 인덱스 결정 —— 
        int MatIndex = 0;
        if (MaterialElement)
        {
            auto mapping = MaterialElement->GetMappingMode();
            auto& indexArray = MaterialElement->GetIndexArray();
            switch (mapping)
            {
            case FbxLayerElement::eAllSame:
                MatIndex = indexArray.GetAt(0);
                break;
            case FbxLayerElement::eByControlPoint:
            {
                int cp0 = Mesh->GetPolygonVertex(poly, 0);
                MatIndex = indexArray.GetAt(cp0);
            }
            break;
            case FbxLayerElement::eByPolygon:
                MatIndex = indexArray.GetAt(poly);
                break;
            case FbxLayerElement::eByPolygonVertex:
            {
                // 폴리곤-버텍스 단위 매핑: 여기서는 첫 번째 버텍스만 써서 그룹화
                MatIndex = indexArray.GetAt(poly * 3 + 0);
            }
            break;
            default:
                MatIndex = 0;
                break;
            }
        }

        for (int j = 0; j <= 2; ++j)
        {
            int cpIndex = Mesh->GetPolygonVertex(poly, j);

            // ## 1) 키 생성
            FString Key = FString::Printf(TEXT("%d_%d_%d"), cpIndex, poly, j);

            // ## 2) 신규 정점이면 생성
            int NewIndex;
            if (!VertexMap.Contains(Key))
            {
                NewIndex = OutMeshData.Vertices.Num();

                // 정점 데이터 채우기
                FSkeletalMeshVertex Vertex = {};
                FVector P = ConvertVector(ControlPoints[cpIndex]);
                Vertex.X = P.X; Vertex.Y = P.Y; Vertex.Z = P.Z;

                // 법선
                FbxVector4 N; Mesh->GetPolygonVertexNormal(poly, j, N);
                FVector NV = ConvertVector(N);
                Vertex.NormalX = NV.X; Vertex.NormalY = NV.Y; Vertex.NormalZ = NV.Z;

                // UV (첫 UV 세트만 예시)
                FbxStringList UVSets; Mesh->GetUVSetNames(UVSets);
                if (UVSets.GetCount() > 0)
                {
                    FbxVector2 UV; bool unmapped;
                    Mesh->GetPolygonVertexUV(poly, j, UVSets[0], UV, unmapped);
                    Vertex.U = UV[0]; Vertex.V = 1.0f - UV[1];
                }

                // 스키닝 정보
                const auto& Weights = VertexBoneWeights[cpIndex];
                for (int w = 0; w < Weights.Num() && w < 4; ++w)
                {
                    Vertex.BoneIndices[w] = Weights[w].BoneIndex;
                    Vertex.BoneWeights[w] = Weights[w].Weight;
                }

                // 재질 인덱스 (디버깅/디버티스팅용)
                Vertex.MaterialIndex = MatIndex;

                OutMeshData.Vertices.Add(Vertex);
                VertexMap.Add(Key, NewIndex);
            }
            else
            {
                NewIndex = VertexMap[Key];
            }

            // ## 3) 해당 재질 버킷에 인덱스 추가
            MatToIndices.FindOrAdd(MatIndex).Add(NewIndex);
        }
    }

    // --- 재질별로 플래트닝 & 서브셋 생성 ---
    uint32 Offset = 0;
    for (auto& Pair : MatToIndices)
    {
        int SubMatIdx = Pair.Key;
        TArray<uint32>& Idxs = Pair.Value;

        FMaterialSubset Subset;
        Subset.MaterialIndex = SubMatIdx;
        Subset.IndexStart = Offset;
        Subset.IndexCount = Idxs.Num();
        OutMeshData.MaterialSubsets.Add(Subset);

        for (uint32 Idx : Idxs)
        {
            OutMeshData.Indices.Add(Idx);
        }
        Offset += Idxs.Num();
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

    // --- FStaticMaterial 로 변환 및 설정 ---
    for (auto& MatInfo : RenderData->Materials)
    {
        FStaticMaterial* StaticMat = new FStaticMaterial;
        StaticMat->Material = FManagerOBJ::GetMaterial(MatInfo.MaterialName);
        StaticMat->MaterialSlotName = FName(*MatInfo.MaterialName);
        NewMesh->Materials.Add(StaticMat);
    }

    return NewMesh;
}


void FLoaderFBX::TraverseMeshNodes(FbxNode* Node, FSkeletalMeshRenderData& OutMeshData, FbxScene* Scene)
{
    if (Node->GetMesh())
    {
        FbxMesh* Mesh = Node->GetMesh();

        TSet<FString> AddedMaterials;
        int MaterialCount = Node->GetMaterialCount();
        for (int m = 0; m < MaterialCount; ++m)
        {
            FbxSurfaceMaterial* Material = Node->GetMaterial(m);
            if (!Material) continue;

            FString MatName = Material->GetName();
            if (AddedMaterials.Contains(MatName)) continue;
            AddedMaterials.Add(MatName);

            FObjMaterialInfo MatInfo;
            MatInfo.MaterialName = MatName;

            // Diffuse 텍스처 속성 처리
            FbxProperty DiffuseProperty = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
            if (DiffuseProperty.IsValid())
            {
                int TextureCount = DiffuseProperty.GetSrcObjectCount<FbxFileTexture>();
                if (TextureCount > 0)
                {
                    FbxFileTexture* Texture = DiffuseProperty.GetSrcObject<FbxFileTexture>(0);
                    MatInfo.DiffuseTexturePath = ((FString)Texture->GetFileName()).ToWideString();
                    MatInfo.DiffuseTextureName = Texture->GetName();

                    FLoaderOBJ::CreateTextureFromFile(MatInfo.DiffuseTexturePath);
                    MatInfo.TextureFlag |= (1 << 1); 

                }
            }

            OutMeshData.Materials.Add(MatInfo);
        }

        TArray<TArray<FBoneWeight>> VertexBoneWeights;
        ExtractSkinning(Mesh, VertexBoneWeights);
        ProcessSkeletalMesh(Mesh, OutMeshData, Scene, VertexBoneWeights);
    }

    for (int i = 0; i < Node->GetChildCount(); i++)
    {
        TraverseMeshNodes(Node->GetChild(i), OutMeshData, Scene);
    }
}







