#include "FLoaderFBX.h" // 사용자 정의 헤더
#include "Define.h"      // 사용자 정의 타입 (TArray, TMap, FString, FName, FVector, FMatrix 등) 포함 가정

// 표준 라이브러리 헤더
#include <functional>   // std::function
#include <numeric>
#include <algorithm>
#include <vector>       // TArray 내부 구현용?
#include <map>          // TMap 내부 구현용?
#include <string>       // std::string 필요 시
#include <filesystem>   // C++17
#include <unordered_map> // 정점 중복 제거용

// FBX SDK 헤더
#include <fbxsdk.h>
#include <fbxsdk/utils/fbxrootnodeutility.h>

// 엔진 관련 헤더
#include "Components/Mesh/SkeletalMesh.h" // FBX 네임스페이스 내 데이터 구조체 정의 포함 가정
#include "Components/Material/Material.h" // UMaterial, FFbxMaterialInfo (또는 FObjMaterialInfo) 정의
// #include "UserInterface/Console.h"    // Console 클래스 (로깅 제거됨)
// #include "Launch/EngineLoop.h"        // EngineLoop::GraphicDevice 접근용 (필요 시)
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ObjectFactory.h"    // FManagerFBX 에서 사용
#include "FSkeletalMeshDebugger.h"   // FSkeletalMeshDebugger 클래스 사용

namespace  FBX {
    // --- 중간 데이터 구조체 (Internal) ---
    struct MeshRawData
    {
        FName NodeName;
        TArray<FVector> ControlPoints;
        TArray<int32> PolygonVertexIndices;
        FMatrix MeshNodeGlobalTransformAtBindTime;
        struct AttributeData {
            TArray<FbxVector4> DataVec4;
            TArray<FbxVector2> DataVec2;
            TArray<int> IndexArray;
            FbxLayerElement::EMappingMode MappingMode = FbxLayerElement::eNone;
            FbxLayerElement::EReferenceMode ReferenceMode = FbxLayerElement::eDirect;
        };

        AttributeData NormalData;
        AttributeData UVData;

        struct RawInfluence {
            FName BoneName;
            TArray<int32> ControlPointIndices;
            TArray<double> ControlPointWeights;
        };

        TArray<RawInfluence> SkinningInfluences;

        TArray<FName> MaterialNames;

        struct MaterialMappingInfo {
            FbxLayerElement::EMappingMode MappingMode = FbxLayerElement::eNone;
            TArray<int32> IndexArray;
        } MaterialMapping;
    };

    struct FBoneHierarchyNode
    {
        FName BoneName;
        FName ParentName;
        FMatrix GlobalBindPose;
        FMatrix TransformMatrix;
    };

    struct FBXInfo
    {
        FString FilePath;
        FWString FileDirectory;
        TArray<MeshRawData> Meshes;
        TMap<FName, FBoneHierarchyNode> SkeletonHierarchy;
        TArray<FName> SkeletonRootBoneNames;
        TMap<FName, FFbxMaterialInfo> Materials;
    };

    struct FControlPointSkinningData
    {
        struct FBoneInfluence
        {
            int32 BoneIndex = INDEX_NONE;
            float Weight = 0.0f;
            bool operator>(const FBoneInfluence& Other) const { return Weight > Other.Weight; }
        };
        TArray<FBoneInfluence> Influences;
        void NormalizeWeights(int32 MaxInfluences)
        {
            if (Influences.IsEmpty()) return; // 1. 영향이 없으면 종료

            // 2. 모든 가중치의 합 계산
            float TotalWeight = 0.0f;
            for (int32 i = 0; i < Influences.Num(); ++i) TotalWeight += Influences[i].Weight;

            // 3. 가중치 합으로 각 가중치 정규화 (합이 1이 되도록)
            if (TotalWeight > KINDA_SMALL_NUMBER)
            {
                for (int32 i = 0; i < Influences.Num(); ++i) Influences[i].Weight /= TotalWeight;
            }
            else if (!Influences.IsEmpty())
            {

                float EqualWeight = 1.0f / Influences.Num(); // 균등 분배
                for (int32 i = 0; i < Influences.Num(); ++i) Influences[i].Weight = EqualWeight;
            }

            // 4. 가중치 크기 순으로 정렬 (내림차순)
            Influences.Sort([](const FBoneInfluence& Lhs, const FBoneInfluence& Rhs) {
                return Lhs.Weight > Rhs.Weight;
                });

            // 5. 최대 영향 본 개수(MaxInfluences) 제한
            if (Influences.Num() > MaxInfluences)
            {
                Influences.SetNum(MaxInfluences);

                if (MaxInfluences > 0)
                {
                    Influences.RemoveAt(Influences.Num() - MaxInfluences);
                }
                else
                {
                    Influences.Empty(); // MaxInfluences가 0이면 모든 영향 제거
                }


                // 최대 개수 제한 후 다시 정규화 (제거된 본들의 가중치를 나머지 본들에 재분배)
                TotalWeight = 0.0f;
                for (int32 i = 0; i < Influences.Num(); ++i) TotalWeight += Influences[i].Weight;

                if (TotalWeight > KINDA_SMALL_NUMBER) {
                    for (int32 i = 0; i < Influences.Num(); ++i) Influences[i].Weight /= TotalWeight;
                }
                else if (!Influences.IsEmpty()) { // 모든 가중치가 0이 된 경우 (거의 발생 안 함)
                    Influences[0].Weight = 1.0f; // 첫 번째 본에 모든 가중치 할당
                    for (int32 i = 1; i < Influences.Num(); ++i) Influences[i].Weight = 0.0f;
                }
            }

            // 6. 최종 정규화 (부동 소수점 오류로 합이 정확히 1이 아닐 수 있으므로, 마지막 가중치 조정)
            if (!Influences.IsEmpty())
            {
                float CurrentSum = 0.0f;
                // 마지막 요소를 제외한 모든 요소의 가중치 합 계산
                for (int32 i = 0; i < Influences.Num() - 1; ++i) CurrentSum += Influences[i].Weight;

                int32 LastIndex = Influences.Num() - 1;
                if (LastIndex >= 0)
                {
                    Influences[LastIndex].Weight = FMath::Max(0.0f, 1.0f - CurrentSum);
                }
            }
        }
    };

    // --- Helper Function Implementations (Internal) ---

    // TODO: Verify coordinate system conversions for YOUR engine!
    FVector ConvertFbxPosition(const FbxVector4& Vector) {
        return FVector(static_cast<float>(Vector[0]), static_cast<float>(Vector[1]), static_cast<float>(Vector[2])); // Example: Y-up -> Z-up LH
    }
    FVector ConvertFbxNormal(const FbxVector4& Vector) {
        return FVector(static_cast<float>(Vector[0]), static_cast<float>(Vector[1]), static_cast<float>(Vector[2])); // Example: Y-up -> Z-up LH
    }
    FVector2D ConvertFbxUV(const FbxVector2& Vector) {
        return FVector2D(static_cast<float>(Vector[0]), 1 - static_cast<float>(Vector[1]));
    }
    FLinearColor ConvertFbxColorToLinear(const FbxDouble3& Color) {
        return FLinearColor(static_cast<float>(Color[0]), static_cast<float>(Color[1]), static_cast<float>(Color[2]), 1.0f);
    }
    // TODO: Verify matrix conversion (Row/Column Major, Coord System) for YOUR engine!
    FMatrix ConvertFbxAMatrixToFMatrix(const FbxAMatrix& FbxMatrix) {
        FMatrix Result;
        // 간단한 유효성 검사 (모든 요소가 유한한지)
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double val = FbxMatrix.Get(i, j);
                if (!FMath::IsFinite(val))
                {
                    return FMatrix::Identity; // 문제가 있으면 단위 행렬 반환
                }
                Result.M[i][j] = static_cast<float>(val);
            }
        }
        return Result;
    }
    FbxAMatrix ConvertFMatrixToFbxAMatrix(const FMatrix& InMatrix)
    {
        FbxAMatrix OutMatrix;

        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Col = 0; Col < 4; ++Col)
            {
                OutMatrix.mData[Row][Col] = static_cast<double>(InMatrix.M[Row][Col]);
            }
        }

        return OutMatrix;
    }


    FWString ProcessTexturePathInternal(FbxFileTexture* Texture, const FWString& BaseDirectory)
    {
        if (!Texture) return FWString();
        const char* RelativePathAnsi = Texture->GetRelativeFileName();
        if (RelativePathAnsi && RelativePathAnsi[0] != '\0' && !BaseDirectory.empty())
        {
            FString RelativePath(RelativePathAnsi);
            std::filesystem::path BaseDirPath(BaseDirectory);
            std::filesystem::path RelPath(RelativePath.ToWideString());
            std::error_code ec;
            std::filesystem::path CombinedPath = BaseDirPath / RelPath;
            CombinedPath = std::filesystem::absolute(CombinedPath, ec);
            if (!ec) {
                CombinedPath.make_preferred();
                if (std::filesystem::exists(CombinedPath, ec) && !ec) return FWString(CombinedPath.wstring().c_str());
                ec.clear();
            }
            else { ec.clear(); }
        }
        const char* AbsolutePathAnsi = Texture->GetFileName();
        if (AbsolutePathAnsi && AbsolutePathAnsi[0] != '\0')
        {
            FString AbsolutePath(AbsolutePathAnsi);
            std::filesystem::path AbsPath(AbsolutePath.ToWideString());
            std::error_code ec;
            AbsPath.make_preferred();
            if (std::filesystem::exists(AbsPath, ec) && !ec) return FWString(AbsPath.wstring().c_str());
            ec.clear();
        }
        const char* PathToExtractFromAnsi = (AbsolutePathAnsi && AbsolutePathAnsi[0] != '\0') ? AbsolutePathAnsi : RelativePathAnsi;
        if (PathToExtractFromAnsi && PathToExtractFromAnsi[0] != '\0') {
            FString PathToExtractFrom(PathToExtractFromAnsi);
            std::filesystem::path TempPath(PathToExtractFrom.ToWideString());
            if (TempPath.has_filename()) return FWString(TempPath.filename().wstring().c_str());
        }
        return FWString();
    }

    FFbxMaterialInfo ProcessSingleMaterial(FbxSurfaceMaterial* FbxMaterial, const FWString& BaseDirectory) {
        FFbxMaterialInfo MatInfo;
        if (!FbxMaterial) return MatInfo;
        MatInfo.MaterialName = FName(FbxMaterial->GetName());
        // ... (Full material property and texture extraction logic from previous answers) ...
        // --- Property Names ---
        const char* DiffuseColorPropName = FbxSurfaceMaterial::sDiffuse;
        const char* DiffuseFactorPropName = FbxSurfaceMaterial::sDiffuseFactor;
        const char* BaseColorPropName = "BaseColor";
        const char* NormalMapPropName = FbxSurfaceMaterial::sNormalMap;
        const char* BumpMapPropName = FbxSurfaceMaterial::sBump;
        const char* MetallicPropName = "Metallic";
        const char* MetalnessPropName = "Metalness";
        const char* RoughnessPropName = "Roughness";
        const char* SpecularColorPropName = FbxSurfaceMaterial::sSpecular;
        const char* SpecularFactorPropName = FbxSurfaceMaterial::sSpecularFactor;
        const char* ShininessPropName = FbxSurfaceMaterial::sShininess;
        const char* EmissiveColorPropName = FbxSurfaceMaterial::sEmissive;
        const char* EmissiveFactorPropName = FbxSurfaceMaterial::sEmissiveFactor;
        const char* AmbientOcclusionPropName = "AmbientOcclusion";
        const char* OcclusionPropName = "Occlusion";
        const char* OpacityPropName = FbxSurfaceMaterial::sTransparencyFactor;

        auto ExtractTextureInternal = [&](const char* PropName, FWString& OutPath, bool& bOutHasTexture) {
            FbxProperty Property = FbxMaterial->FindProperty(PropName);
            bOutHasTexture = false;
            OutPath.clear();

            if (Property.IsValid())
            {
                int LayeredTextureCount = Property.GetSrcObjectCount<FbxLayeredTexture>();
                FbxFileTexture* Texture = nullptr;
                if (LayeredTextureCount > 0)
                {
                    FbxLayeredTexture* LayeredTexture = Property.GetSrcObject<FbxLayeredTexture>(0);
                    if (LayeredTexture && LayeredTexture->GetSrcObjectCount<FbxFileTexture>() > 0)
                        Texture = LayeredTexture->GetSrcObject<FbxFileTexture>(0);
                }
                else if (Property.GetSrcObjectCount<FbxFileTexture>() > 0)
                {
                    Texture = Property.GetSrcObject<FbxFileTexture>(0);
                }
                if (Texture)
                {
                    OutPath = ProcessTexturePathInternal(Texture, BaseDirectory);
                    bOutHasTexture = !OutPath.empty();
                }
            }
            };

        FbxProperty BaseColorProp = FbxMaterial->FindProperty(BaseColorPropName);
        if (!BaseColorProp.IsValid()) BaseColorProp = FbxMaterial->FindProperty(DiffuseColorPropName);
        if (BaseColorProp.IsValid()) {
            MatInfo.BaseColorFactor = ConvertFbxColorToLinear(BaseColorProp.Get<FbxDouble3>());
            FbxProperty DiffuseFactorProp = FbxMaterial->FindProperty(DiffuseFactorPropName);
            if (DiffuseFactorProp.IsValid()) MatInfo.BaseColorFactor *= static_cast<float>(DiffuseFactorProp.Get<FbxDouble>());
        }
        else { MatInfo.BaseColorFactor = FLinearColor::White; }
        ExtractTextureInternal(BaseColorPropName, MatInfo.BaseColorTexturePath, MatInfo.bHasBaseColorTexture);
        if (!MatInfo.bHasBaseColorTexture) ExtractTextureInternal(DiffuseColorPropName, MatInfo.BaseColorTexturePath, MatInfo.bHasBaseColorTexture);

        ExtractTextureInternal(NormalMapPropName, MatInfo.NormalTexturePath, MatInfo.bHasNormalTexture);
        if (!MatInfo.bHasNormalTexture) ExtractTextureInternal(BumpMapPropName, MatInfo.NormalTexturePath, MatInfo.bHasNormalTexture);

        FbxProperty EmissiveProp = FbxMaterial->FindProperty(EmissiveColorPropName);
        if (EmissiveProp.IsValid()) {
            MatInfo.EmissiveFactor = ConvertFbxColorToLinear(EmissiveProp.Get<FbxDouble3>());
            FbxProperty EmissiveFactorProp = FbxMaterial->FindProperty(EmissiveFactorPropName);
            if (EmissiveFactorProp.IsValid()) MatInfo.EmissiveFactor *= static_cast<float>(EmissiveFactorProp.Get<FbxDouble>());
        }
        else { MatInfo.EmissiveFactor = FLinearColor::Black; }
        ExtractTextureInternal(EmissiveColorPropName, MatInfo.EmissiveTexturePath, MatInfo.bHasEmissiveTexture);

        FbxProperty OpacityProp = FbxMaterial->FindProperty(OpacityPropName);
        MatInfo.OpacityFactor = 1.0f;
        if (OpacityProp.IsValid()) MatInfo.OpacityFactor = 1.0f - static_cast<float>(OpacityProp.Get<FbxDouble>());
        // ExtractTextureInternal("Opacity", MatInfo.OpacityTexturePath, MatInfo.bHasOpacityTexture);

        FbxProperty MetallicProp = FbxMaterial->FindProperty(MetallicPropName);
        if (!MetallicProp.IsValid()) MetallicProp = FbxMaterial->FindProperty(MetalnessPropName);
        FbxProperty RoughnessProp = FbxMaterial->FindProperty(RoughnessPropName);
        FWString MetallicTexPath, RoughnessTexPath, AOTexPath, SpecularTexPath;
        bool bHasMetallicTex, bHasRoughnessTex, bHasAOTex, bHasSpecularTex;
        ExtractTextureInternal(MetallicPropName, MetallicTexPath, bHasMetallicTex);
        if (!bHasMetallicTex) ExtractTextureInternal(MetalnessPropName, MetallicTexPath, bHasMetallicTex);
        ExtractTextureInternal(RoughnessPropName, RoughnessTexPath, bHasRoughnessTex);
        ExtractTextureInternal(AmbientOcclusionPropName, AOTexPath, bHasAOTex);
        if (!bHasAOTex) ExtractTextureInternal(OcclusionPropName, AOTexPath, bHasAOTex);
        ExtractTextureInternal(SpecularColorPropName, SpecularTexPath, bHasSpecularTex);

        if (MetallicProp.IsValid() || RoughnessProp.IsValid() || bHasMetallicTex || bHasRoughnessTex) {
            MatInfo.bUsePBRWorkflow = true;
            MatInfo.MetallicFactor = MetallicProp.IsValid() ? static_cast<float>(MetallicProp.Get<FbxDouble>()) : 0.0f;
            MatInfo.MetallicTexturePath = MetallicTexPath; MatInfo.bHasMetallicTexture = bHasMetallicTex;
            MatInfo.RoughnessFactor = RoughnessProp.IsValid() ? static_cast<float>(RoughnessProp.Get<FbxDouble>()) : 0.8f;
            MatInfo.RoughnessTexturePath = RoughnessTexPath; MatInfo.bHasRoughnessTexture = bHasRoughnessTex;
            MatInfo.AmbientOcclusionTexturePath = AOTexPath; MatInfo.bHasAmbientOcclusionTexture = bHasAOTex;
            MatInfo.SpecularFactor = FVector(0.04f, 0.04f, 0.04f); MatInfo.SpecularPower = 0.0f;
        }
        else {
            MatInfo.bUsePBRWorkflow = false;
            FbxProperty SpecularColorProp = FbxMaterial->FindProperty(SpecularColorPropName);
            FbxProperty SpecularFactorProp = FbxMaterial->FindProperty(SpecularFactorPropName);
            MatInfo.SpecularFactor = FVector(1.0f, 1.0f, 1.0f);
            if (SpecularColorProp.IsValid()) MatInfo.SpecularFactor = ConvertFbxColorToLinear(SpecularColorProp.Get<FbxDouble3>()).ToVector3();
            if (SpecularFactorProp.IsValid()) MatInfo.SpecularFactor *= static_cast<float>(SpecularFactorProp.Get<FbxDouble>());
            MatInfo.SpecularTexturePath = SpecularTexPath; MatInfo.bHasSpecularTexture = bHasSpecularTex;
            FbxProperty ShininessProp = FbxMaterial->FindProperty(ShininessPropName);
            MatInfo.SpecularPower = 32.0f;
            if (ShininessProp.IsValid()) MatInfo.SpecularPower = static_cast<float>(ShininessProp.Get<FbxDouble>());
            MatInfo.RoughnessFactor = FMath::Clamp(FMath::Sqrt(2.0f / (MatInfo.SpecularPower + 2.0f)), 0.0f, 1.0f);
            MatInfo.MetallicFactor = 0.0f;
        }
        MatInfo.bIsTransparent = (MatInfo.OpacityFactor < 1.0f - KINDA_SMALL_NUMBER) || MatInfo.bHasOpacityTexture;
        return MatInfo;
    }

    void ExtractAttributeRaw(FbxLayerElementTemplate<FbxVector4>* ElementVec4, FbxLayerElementTemplate<FbxVector2>* ElementVec2, MeshRawData::AttributeData& AttrData)
    {
        // 먼저 유효한 Element 포인터가 있는지 확인
        FbxLayerElement* BaseElement = ElementVec4 ? static_cast<FbxLayerElement*>(ElementVec4) : static_cast<FbxLayerElement*>(ElementVec2);
        if (!BaseElement) {
            AttrData.MappingMode = FbxLayerElement::eNone; // 요소 없음을 표시
            return;
        }

        // MappingMode와 ReferenceMode는 기본 클래스에서 가져올 수 있음
        AttrData.MappingMode = BaseElement->GetMappingMode();
        AttrData.ReferenceMode = BaseElement->GetReferenceMode();

        // GetDirectArray와 GetIndexArray는 파생 클래스 포인터에서 호출해야 함
        if (ElementVec4) // FbxVector4 타입 (예: Normal)
        {
            const auto& DirectArray = ElementVec4->GetDirectArray();
            const auto& IndexArray = ElementVec4->GetIndexArray();
            int DataCount = DirectArray.GetCount();
            int IdxCount = IndexArray.GetCount();

            AttrData.DataVec4.Reserve(DataCount);
            for (int i = 0; i < DataCount; ++i) AttrData.DataVec4.Add(DirectArray.GetAt(i));

            if (AttrData.ReferenceMode == FbxLayerElement::eIndexToDirect || AttrData.ReferenceMode == FbxLayerElement::eIndex) {
                AttrData.IndexArray.Reserve(IdxCount);
                for (int i = 0; i < IdxCount; ++i)
                    AttrData.IndexArray.Add(IndexArray.GetAt(i));
            }
        }
        else if (ElementVec2) // FbxVector2 타입 (예: UV)
        {
            const auto& DirectArray = ElementVec2->GetDirectArray();
            const auto& IndexArray = ElementVec2->GetIndexArray();
            int DataCount = DirectArray.GetCount();
            int IdxCount = IndexArray.GetCount();

            AttrData.DataVec2.Reserve(DataCount);
            for (int i = 0; i < DataCount; ++i) AttrData.DataVec2.Add(DirectArray.GetAt(i));

            if (AttrData.ReferenceMode == FbxLayerElement::eIndexToDirect || AttrData.ReferenceMode == FbxLayerElement::eIndex) {
                AttrData.IndexArray.Reserve(IdxCount);
                for (int i = 0; i < IdxCount; ++i)
                    AttrData.IndexArray.Add(IndexArray.GetAt(i));
            }
        }
        // else: 다른 타입의 Element가 있다면 여기에 추가 (예: FbxColor)
    }

    bool ExtractSingleMeshRawData(FbxNode* Node, MeshRawData& OutRawData, const TMap<FbxSurfaceMaterial*, FName>& MaterialPtrToNameMap)
    {
        FbxMesh* Mesh = Node->GetMesh();
        if (!Mesh) return false;
        OutRawData.NodeName = FName(Node->GetName());

        ExtractAttributeRaw(Mesh->GetElementNormal(0), nullptr, OutRawData.NormalData);
        ExtractAttributeRaw(nullptr, Mesh->GetElementUV(0), OutRawData.UVData);

        int32 ControlPointCount = Mesh->GetControlPointsCount();

        if (ControlPointCount <= 0) return false;

        OutRawData.ControlPoints.Reserve(ControlPointCount);

        FbxVector4* FbxControlPoints = Mesh->GetControlPoints();
        for (int32 i = 0; i < ControlPointCount; ++i) OutRawData.ControlPoints.Add(ConvertFbxPosition(FbxControlPoints[i]));

        int32 PolygonVertexCount = Mesh->GetPolygonVertexCount();
        int32 PolygonCount = Mesh->GetPolygonCount();

        if (PolygonVertexCount <= 0 || PolygonCount <= 0 || PolygonVertexCount != PolygonCount * 3) return false;
        OutRawData.PolygonVertexIndices.Reserve(PolygonVertexCount);
        int* FbxPolygonVertices = Mesh->GetPolygonVertices();
        for (int32 i = 0; i < PolygonVertexCount; ++i) OutRawData.PolygonVertexIndices.Add(FbxPolygonVertices[i]);



        int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int deformerIdx = 0; deformerIdx < DeformerCount; ++deformerIdx)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(deformerIdx, FbxDeformer::eSkin));
            if (!Skin)
                continue;
            int ClusterCount = Skin->GetClusterCount();
            for (int clusterIdx = 0; clusterIdx < ClusterCount; ++clusterIdx)
            {
                FbxCluster* Cluster = Skin->GetCluster(clusterIdx);
                if (!Cluster)
                    continue;
                FbxNode* BoneNode = Cluster->GetLink();
                if (!BoneNode)
                    continue;
                MeshRawData::RawInfluence Influence;
                Influence.BoneName = FName(BoneNode->GetName());
                int InfluenceCount = Cluster->GetControlPointIndicesCount();
                if (InfluenceCount > 0)
                {
                    Influence.ControlPointIndices.Reserve(InfluenceCount);
                    Influence.ControlPointWeights.Reserve(InfluenceCount);
                    int* Indices = Cluster->GetControlPointIndices();
                    double* Weights = Cluster->GetControlPointWeights();
                    for (int i = 0; i < InfluenceCount; ++i)
                    {
                        Influence.ControlPointIndices.Add(Indices[i]);
                        Influence.ControlPointWeights.Add(Weights[i]);
                    }
                    OutRawData.SkinningInfluences.Add(std::move(Influence));
                }
            }
        }

        int MaterialCount = Node->GetMaterialCount();
        OutRawData.MaterialNames.Reserve(MaterialCount);
        for (int matIdx = 0; matIdx < MaterialCount; ++matIdx) {
            FbxSurfaceMaterial* FbxMat = Node->GetMaterial(matIdx);
            const FName* MatNamePtr = FbxMat ? MaterialPtrToNameMap.Find(FbxMat) : nullptr;
            OutRawData.MaterialNames.Add(MatNamePtr ? *MatNamePtr : NAME_None);
        }
        FbxLayerElementMaterial* MaterialElement = Mesh->GetLayer(0) ? Mesh->GetLayer(0)->GetMaterials() : nullptr;
        if (MaterialElement) {
            OutRawData.MaterialMapping.MappingMode = MaterialElement->GetMappingMode();
            if (OutRawData.MaterialMapping.MappingMode == FbxLayerElement::eByPolygon)
            {
                const auto& IndexArray = MaterialElement->GetIndexArray(); int IdxCount = IndexArray.GetCount();
                if (IdxCount == PolygonCount) {
                    OutRawData.MaterialMapping.IndexArray.Reserve(IdxCount); for (int i = 0; i < IdxCount; ++i) OutRawData.MaterialMapping.IndexArray.Add(IndexArray.GetAt(i));
                }
                else { OutRawData.MaterialMapping.MappingMode = FbxLayerElement::eAllSame; }
            }
        }
        else { OutRawData.MaterialMapping.MappingMode = FbxLayerElement::eAllSame; }
        return true;
    }

    // In FLoaderFBX.cpp

    bool ReconstructVertexAttributes(const MeshRawData& RawMeshData, TArray<FVector>& OutControlPointNormals, TArray<FVector>& OutPolygonVertexNormals, TArray<FVector2D>& OutControlPointUVs, TArray<FVector2D>& OutPolygonVertexUVs)
    {
        int32 ControlPointCount = RawMeshData.ControlPoints.Num();
        int32 PolygonVertexCount = RawMeshData.PolygonVertexIndices.Num();

        // --- 출력 배열 초기화 및 기본값으로 채우기 ---
        OutControlPointNormals.Empty(ControlPointCount);
        OutControlPointNormals.Reserve(ControlPointCount); // 필요한 경우 용량 미리 확보
        for (int32 i = 0; i < ControlPointCount; ++i) {
            OutControlPointNormals.Add(FVector(0, 0, 1)); // 기본값
        }

        OutPolygonVertexNormals.Empty(PolygonVertexCount);
        OutPolygonVertexNormals.Reserve(PolygonVertexCount);
        for (int32 i = 0; i < PolygonVertexCount; ++i) {
            OutPolygonVertexNormals.Add(FVector(0, 0, 1)); // 기본값
        }

        OutControlPointUVs.Empty(ControlPointCount);
        OutControlPointUVs.Reserve(ControlPointCount);
        for (int32 i = 0; i < ControlPointCount; ++i) {
            OutControlPointUVs.Add(FVector2D(0, 0));    // 기본값
        }

        OutPolygonVertexUVs.Empty(PolygonVertexCount);
        OutPolygonVertexUVs.Reserve(PolygonVertexCount);
        for (int32 i = 0; i < PolygonVertexCount; ++i) {
            OutPolygonVertexUVs.Add(FVector2D(0, 0));    // 기본값
        }

        // --- 법선 데이터 처리 ---
        const MeshRawData::AttributeData& NormalData = RawMeshData.NormalData;
        if (NormalData.MappingMode != FbxLayerElement::eNone)
        {
            auto GetNormalValue = [&](int DataIndex) -> FVector {
                if (DataIndex >= 0 && NormalData.DataVec4.IsValidIndex(DataIndex)) {
                    // ConvertFbxNormal은 FBX 좌표를 엔진 좌표로 변환하는 함수여야 함
                    return ConvertFbxNormal(NormalData.DataVec4[DataIndex]);
                }
                return FVector(0, 0, 1); // 데이터 접근 실패 시 기본값
                };

            if (NormalData.MappingMode == FbxLayerElement::eByControlPoint)
            {
                if (NormalData.ReferenceMode == FbxLayerElement::eDirect)
                {
                    // DataVec4.Num()과 ControlPointCount 중 작은 값까지만 순회 (안전성)
                    int32 LoopCount = FMath::Min(ControlPointCount, NormalData.DataVec4.Num());
                    for (int32 i = 0; i < LoopCount; ++i) {
                        OutControlPointNormals[i] = GetNormalValue(i);
                    }

                }
                else if (NormalData.ReferenceMode == FbxLayerElement::eIndexToDirect)
                {
                    // IndexArray.Num()과 ControlPointCount 중 작은 값까지만 순회
                    int32 LoopCount = FMath::Min(ControlPointCount, NormalData.IndexArray.Num());
                    for (int32 i = 0; i < LoopCount; ++i) 
                    {
                        int idx = NormalData.IndexArray[i];
                        OutControlPointNormals[i] = GetNormalValue(idx);
                    }
                }
                // else: 지원하지 않는 참조 모드. 이미 기본값으로 채워져 있음.
            }
            else if (NormalData.MappingMode == FbxLayerElement::eByPolygonVertex)
            {
                if (NormalData.ReferenceMode == FbxLayerElement::eDirect)
                {
                    int32 LoopCount = FMath::Min(PolygonVertexCount, NormalData.DataVec4.Num());
                    for (int32 i = 0; i < LoopCount; ++i)
                    {
                        OutPolygonVertexNormals[i] = GetNormalValue(i);
                    }

                }
                else if (NormalData.ReferenceMode == FbxLayerElement::eIndexToDirect)
                {
                    int32 LoopCount = FMath::Min(PolygonVertexCount, NormalData.IndexArray.Num());
                    for (int32 i = 0; i < LoopCount; ++i) {
                        int idx = NormalData.IndexArray[i];
                        OutPolygonVertexNormals[i] = GetNormalValue(idx);
                    }

                }
            }
        }

        // --- UV 데이터 처리 (법선과 유사한 방식으로) ---
        const MeshRawData::AttributeData& UVData = RawMeshData.UVData;
        if (UVData.MappingMode != FbxLayerElement::eNone)
        {
            auto GetUVValue = [&](int DataIndex) -> FVector2D {
                if (DataIndex >= 0 && UVData.DataVec2.IsValidIndex(DataIndex)) {
                    return ConvertFbxUV(UVData.DataVec2[DataIndex]);
                }
                return FVector2D(0, 0); // 기본값
                };

            if (UVData.MappingMode == FbxLayerElement::eByControlPoint)
            {
                if (UVData.ReferenceMode == FbxLayerElement::eDirect)
                {
                    int32 LoopCount = FMath::Min(ControlPointCount, UVData.DataVec2.Num());
                    for (int32 i = 0; i < LoopCount; ++i)
                        OutControlPointUVs[i] = GetUVValue(i);
                    if (LoopCount < ControlPointCount) { /* 로그 */ }
                }
                else if (UVData.ReferenceMode == FbxLayerElement::eIndexToDirect)
                {
                    int32 LoopCount = FMath::Min(ControlPointCount, UVData.IndexArray.Num());
                    for (int32 i = 0; i < LoopCount; ++i) {
                        int idx = UVData.IndexArray[i];
                        OutControlPointUVs[i] = GetUVValue(idx);
                    }
                    if (LoopCount < ControlPointCount) { /* 로그 */ }
                }
            }
            else if (UVData.MappingMode == FbxLayerElement::eByPolygonVertex)
            {
                if (UVData.ReferenceMode == FbxLayerElement::eDirect)
                {
                    int32 LoopCount = FMath::Min(PolygonVertexCount, UVData.DataVec2.Num());
                    for (int32 i = 0; i < LoopCount; ++i)
                        OutPolygonVertexUVs[i] = GetUVValue(i);
                    if (LoopCount < PolygonVertexCount) { /* 로그 */ }
                }
                else if (UVData.ReferenceMode == FbxLayerElement::eIndexToDirect)
                {
                    int32 LoopCount = FMath::Min(PolygonVertexCount, UVData.IndexArray.Num());
                    for (int32 i = 0; i < LoopCount; ++i) {
                        int idx = UVData.IndexArray[i];
                        OutPolygonVertexUVs[i] = GetUVValue(idx);
                    }
                    if (LoopCount < PolygonVertexCount) { /* 로그 */ }
                }
            }
        }

        return true; // 항상 성공 (데이터 없으면 기본값 사용)
    }
    bool FinalizeVertexDataInternal(const TArray<FVector>& ControlPointPositions, const TArray<int32>& PolygonVertexIndices,
        const TArray<FVector>& ControlPointNormals, const TArray<FVector>& PolygonVertexNormals,
        const TArray<FVector2D>& ControlPointUVs, const TArray<FVector2D>& PolygonVertexUVs,
        const TArray<FControlPointSkinningData>& CpSkinData,
        FSkeletalMeshRenderData& OutSkeletalMesh)
    {
        OutSkeletalMesh.BindPoseVertices.Empty(); OutSkeletalMesh.Indices.Empty();
        std::unordered_map<FSkeletalMeshVertex, uint32> UniqueVertices;
        const int32 TotalPolygonVertices = PolygonVertexIndices.Num();

        if (TotalPolygonVertices == 0) return true;

        OutSkeletalMesh.Indices.Reserve(TotalPolygonVertices); OutSkeletalMesh.BindPoseVertices.Reserve(TotalPolygonVertices * 7 / 10);

        for (int32 PolyVertIndex = 0; PolyVertIndex < TotalPolygonVertices; ++PolyVertIndex)
        {
            const int32 ControlPointIndex = PolygonVertexIndices[PolyVertIndex];

            if (!ControlPointPositions.IsValidIndex(ControlPointIndex)) continue;

            FSkeletalMeshVertex CurrentVertex = {};

            CurrentVertex.Position = ControlPointPositions[ControlPointIndex];

            if (!PolygonVertexNormals.IsEmpty())
                CurrentVertex.Normal = PolygonVertexNormals.IsValidIndex(PolyVertIndex) ? PolygonVertexNormals[PolyVertIndex] : FVector(0, 0, 1);
            else if (!ControlPointNormals.IsEmpty())
                CurrentVertex.Normal = ControlPointNormals.IsValidIndex(ControlPointIndex) ? ControlPointNormals[ControlPointIndex] : FVector(0, 0, 1);
            else CurrentVertex.Normal = FVector(0, 0, 1);

            if (!CurrentVertex.Normal.IsNearlyZero()) CurrentVertex.Normal.Normalize(); else CurrentVertex.Normal = FVector(0, 0, 1);

            if (!PolygonVertexUVs.IsEmpty()) CurrentVertex.TexCoord = PolygonVertexUVs.IsValidIndex(PolyVertIndex) ? PolygonVertexUVs[PolyVertIndex] : FVector2D(0, 0);

            else if (!ControlPointUVs.IsEmpty()) CurrentVertex.TexCoord = ControlPointUVs.IsValidIndex(ControlPointIndex) ? ControlPointUVs[ControlPointIndex] : FVector2D(0, 0);

            else CurrentVertex.TexCoord = FVector2D(0, 0);

            CurrentVertex.BoneIndices[0] = 0; CurrentVertex.BoneWeights[0] = 1.0f;
            for (int i = 1; i < MAX_BONE_INFLUENCES; ++i) { CurrentVertex.BoneIndices[i] = 0; CurrentVertex.BoneWeights[i] = 0.0f; }
            if (CpSkinData.IsValidIndex(ControlPointIndex)) {
                const auto& Influences = CpSkinData[ControlPointIndex].Influences;
                if (!Influences.IsEmpty()) {
                    for (int32 i = 0; i < Influences.Num() && i < MAX_BONE_INFLUENCES; ++i) { CurrentVertex.BoneIndices[i] = Influences[i].BoneIndex; CurrentVertex.BoneWeights[i] = Influences[i].Weight; }
                    for (int32 i = Influences.Num(); i < MAX_BONE_INFLUENCES; ++i) { CurrentVertex.BoneIndices[i] = 0; CurrentVertex.BoneWeights[i] = 0.0f; }
                }
            }
            auto it = UniqueVertices.find(CurrentVertex);

            if (it != UniqueVertices.end()) OutSkeletalMesh.Indices.Add(it->second);

            else { uint32 NewIndex = static_cast<uint32>(OutSkeletalMesh.BindPoseVertices.Add(CurrentVertex)); UniqueVertices[CurrentVertex] = NewIndex; OutSkeletalMesh.Indices.Add(NewIndex); }
        }
        return !OutSkeletalMesh.BindPoseVertices.IsEmpty() && !OutSkeletalMesh.Indices.IsEmpty();
    }
    bool CreateMaterialSubsetsInternal(const MeshRawData& RawMeshData, const TMap<FName, int32>& MaterialNameToIndexMap, FSkeletalMeshRenderData& OutSkeletalMesh)
    {
        OutSkeletalMesh.Subsets.Empty();
        if (OutSkeletalMesh.Indices.IsEmpty()) return true; // 인덱스가 없으면 서브셋 필요 없음 (성공)
        if (OutSkeletalMesh.Materials.IsEmpty()) return false; // 재질 없이는 서브셋 생성 불가 (실패)

        const int32 PolygonCount = RawMeshData.PolygonVertexIndices.Num() / 3;
        const MeshRawData::MaterialMappingInfo& MappingInfo = RawMeshData.MaterialMapping;

        bool bSuccess = true; // 처리 성공 여부 플래그
        bool bHandled = false; // 서브셋 생성이 처리되었는지 여부

        // --- Case 1: eByPolygon 모드이고 인덱스 배열이 유효한 경우 ---
        if (MappingInfo.MappingMode == FbxLayerElement::eByPolygon && MappingInfo.IndexArray.IsValidIndex(PolygonCount - 1))
        {
            int32 CurrentMaterialLocalIndex = -1;
            uint32 CurrentSubsetStartIndex = 0;
            uint32 IndicesProcessed = 0;

            for (int PolyIndex = 0; PolyIndex < PolygonCount; ++PolyIndex)
            {
                int32 MaterialSlotIndex = MappingInfo.IndexArray[PolyIndex];
                FName MaterialName = RawMeshData.MaterialNames.IsValidIndex(MaterialSlotIndex) ? RawMeshData.MaterialNames[MaterialSlotIndex] : NAME_None;
                int32 MaterialLocalIndex = 0;
                const int32* FoundIndexPtr = MaterialNameToIndexMap.Find(MaterialName);
                if (FoundIndexPtr) MaterialLocalIndex = *FoundIndexPtr;
                else MaterialLocalIndex = 0; // 기본값 0 사용

                if (PolyIndex == 0)
                {
                    CurrentMaterialLocalIndex = MaterialLocalIndex;
                    CurrentSubsetStartIndex = 0;
                }
                else if (MaterialLocalIndex != CurrentMaterialLocalIndex)
                {
                    FMeshSubset Subset;
                    Subset.MaterialIndex = CurrentMaterialLocalIndex;
                    Subset.IndexStart = CurrentSubsetStartIndex;
                    Subset.IndexCount = IndicesProcessed - CurrentSubsetStartIndex;
                    if (Subset.IndexCount > 0) OutSkeletalMesh.Subsets.Add(Subset);
                    CurrentMaterialLocalIndex = MaterialLocalIndex;
                    CurrentSubsetStartIndex = IndicesProcessed;
                }
                IndicesProcessed += 3;
                if (PolyIndex == PolygonCount - 1)
                {
                    FMeshSubset Subset;
                    Subset.MaterialIndex = CurrentMaterialLocalIndex;
                    Subset.IndexStart = CurrentSubsetStartIndex;
                    Subset.IndexCount = IndicesProcessed - CurrentSubsetStartIndex;
                    if (Subset.IndexCount > 0) OutSkeletalMesh.Subsets.Add(Subset);
                }
            }
            bHandled = true; // eByPolygon 처리 완료
        }

        // --- Case 2: eAllSame 모드 또는 eByPolygon 실패 또는 다른 모드 ---
        // bHandled 플래그를 사용하여 위에서 처리되지 않은 경우 이 로직 실행
        if (!bHandled)
        {
            // eAllSame 로직 또는 eByPolygon 실패 시의 대체 로직
            FMeshSubset Subset;
            FName MaterialName = !RawMeshData.MaterialNames.IsEmpty() ? RawMeshData.MaterialNames[0] : NAME_None;
            int32 MaterialLocalIndex = 0;
            const int32* FoundIndexPtr = MaterialNameToIndexMap.Find(MaterialName);
            if (FoundIndexPtr) MaterialLocalIndex = *FoundIndexPtr;
            else MaterialLocalIndex = 0; // 기본값 0 사용

            Subset.MaterialIndex = MaterialLocalIndex;
            Subset.IndexStart = 0;
            Subset.IndexCount = OutSkeletalMesh.Indices.Num();
            if (Subset.IndexCount > 0)
            {
                OutSkeletalMesh.Subsets.Add(Subset);
            }
            else if (!OutSkeletalMesh.Indices.IsEmpty())
            {
                // 인덱스는 있는데 서브셋 카운트가 0인 경우?
                bSuccess = false;
            }
            bHandled = true; // 처리 완료
        }

        // --- 최종 유효성 검사 ---
        uint32 TotalSubsetIndices = 0;
        for (const auto& sub : OutSkeletalMesh.Subsets)
        {
            TotalSubsetIndices += sub.IndexCount;
        }
        if (TotalSubsetIndices != OutSkeletalMesh.Indices.Num())
        {
            // 서브셋 인덱스 합계가 전체 인덱스 수와 다르면 문제 발생
            bSuccess = false;
            OutSkeletalMesh.Subsets.Empty(); // 잘못된 서브셋 정보 제거
            // 오류 발생 시 단일 기본 서브셋 생성 시도 (선택 사항)
            if (!OutSkeletalMesh.Indices.IsEmpty() && !OutSkeletalMesh.Materials.IsEmpty())
            {
                FMeshSubset DefaultSubset;
                DefaultSubset.MaterialIndex = 0;
                DefaultSubset.IndexStart = 0;
                DefaultSubset.IndexCount = OutSkeletalMesh.Indices.Num();
                OutSkeletalMesh.Subsets.Add(DefaultSubset);
                bSuccess = true; // 기본 서브셋 생성 성공으로 간주
            }
        }

        return bSuccess;

    }

    void CalculateInitialLocalTransformsInternal(USkeleton* OutSkeleton)
    {
        if (OutSkeleton->BoneTree.IsEmpty()) return;

        OutSkeleton->CurrentPose.Resize(OutSkeleton->BoneTree.Num());

        TArray<int32> ProcessingOrder; ProcessingOrder.Reserve(OutSkeleton->BoneTree.Num());
        TArray<uint8> Processed;
        Processed.Init(false, OutSkeleton->BoneTree.Num());
        TArray<int32> Queue; Queue.Reserve(OutSkeleton->BoneTree.Num());

        for (int32 i = 0; i < OutSkeleton->BoneTree.Num(); ++i)
        {
            if (OutSkeleton->BoneTree[i].ParentIndex == INDEX_NONE)
            {
                Queue.Add(i);
            }
        }

        int32 Head = 0;
        while (Head < Queue.Num())
        {
            int32 CurrentIndex = Queue[Head++];
            if (Processed[CurrentIndex])
            {
                continue;
            }
            ProcessingOrder.Add(CurrentIndex);
            Processed[CurrentIndex] = true;
            for (int32 i = 0; i < OutSkeleton->BoneTree.Num(); ++i)
            {
                if (OutSkeleton->BoneTree[i].ParentIndex == CurrentIndex && !Processed[i])
                {
                    Queue.Add(i);
                }
            }

        }
        if (ProcessingOrder.Num() != OutSkeleton->BoneTree.Num())
        {
            for (int32 i = 0; i < OutSkeleton->BoneTree.Num(); ++i)
            {
                if (!Processed[i])
                {
                    ProcessingOrder.Add(i);
                }
            }
        }

        for (int32 BoneIndex : ProcessingOrder)
        {
            const FBoneNode& CurrentBone = OutSkeleton->BoneTree[BoneIndex];
            const FMatrix& LocalBindPose = CurrentBone.BindTransform;
            int32 ParentIdx = CurrentBone.ParentIndex;

            // 1. 현재 포즈의 로컬 변환을 로컬 바인드 포즈로 초기화 (모든 본에 대해 수행)
            OutSkeleton->CurrentPose.LocalTransforms[BoneIndex] = LocalBindPose;

            // 2. 현재 포즈의 글로벌 변환 계산
            if (ParentIdx != INDEX_NONE) // 자식 본인 경우
            {
                if (OutSkeleton->CurrentPose.GlobalTransforms.IsValidIndex(ParentIdx))
                {
                    const FMatrix& ParentGlobalTransform = OutSkeleton->CurrentPose.GlobalTransforms[ParentIdx];
                    OutSkeleton->CurrentPose.GlobalTransforms[BoneIndex] = LocalBindPose * ParentGlobalTransform;
                }
                else
                {
                    OutSkeleton->CurrentPose.GlobalTransforms[BoneIndex] = LocalBindPose; // 오류상황 : 임시 처리
                }
            }
            else // 루트 본인 경우
            {
                // 루트 글로벌 = 루트 로컬
                OutSkeleton->CurrentPose.GlobalTransforms[BoneIndex] = LocalBindPose;
            }

            // 3. 현재 포즈의 스키닝 행렬 계산 (모든 본에 대해 루프 끝에서 한 번만)
            OutSkeleton->CurrentPose.SkinningMatrices[BoneIndex] =
                OutSkeleton->CalculateSkinningMatrix(BoneIndex, OutSkeleton->CurrentPose.GlobalTransforms[BoneIndex]);
        }
    }

    void ConvertFbxMaterialToObjMaterial(const FFbxMaterialInfo& FbxInfo, FObjMaterialInfo& OutObjInfo)
    {
        // --- 기본 정보 매핑 ---
        OutObjInfo.MaterialName = FbxInfo.MaterialName.ToString(); // FName -> FString
        OutObjInfo.bTransparent = FbxInfo.bIsTransparent;

        // --- 색상 매핑 ---
        OutObjInfo.Diffuse = FbxInfo.BaseColorFactor.ToVector3(); // BaseColor -> Diffuse (Kd)
        OutObjInfo.Emissive = FbxInfo.EmissiveFactor.ToVector3(); // Emissive -> Emissive (Ke)

        // --- 워크플로우에 따른 Specular 및 Shininess 매핑 ---
        if (FbxInfo.bUsePBRWorkflow)
        {
            // PBR -> Traditional 근사 변환
            // Ks (Specular Color): PBR에서는 직접적인 대응 없음. 금속성 여부에 따라 다름.
            // 단순화를 위해 비금속 기본 반사율(F0=0.04) 또는 중간 회색 사용.
            // 또는 BaseColor를 사용할 수도 있음. 여기서는 중간 회색 사용.
            OutObjInfo.Specular = FVector(0.5f, 0.5f, 0.5f);

            // Ns (Shininess/Specular Power): Roughness에서 변환 (근사치)
            // Roughness 0 (매끈) -> Ns 높음, Roughness 1 (거침) -> Ns 낮음
            // 예시: Roughness를 [0,1] -> Glossiness [1,0] -> Ns [~1000, ~2] 범위로 매핑 시도
            float Glossiness = 1.0f - FbxInfo.RoughnessFactor;
            // 비선형 매핑 (값이 너무 커지지 않도록 조정 가능)
            OutObjInfo.SpecularScalar = FMath::Clamp(2.0f * FMath::Pow(100.0f, Glossiness), 2.0f, 1000.0f);
        }
        else
        {
            // Traditional 정보 직접 사용
            OutObjInfo.Specular = FbxInfo.SpecularFactor;     // Ks
            OutObjInfo.SpecularScalar = FbxInfo.SpecularPower; // Ns
        }

        // Ka (Ambient Color): 보통 Diffuse의 일부 또는 작은 기본값 사용
        OutObjInfo.Ambient = OutObjInfo.Diffuse * 0.1f; // Diffuse의 10%를 Ambient로 사용 (예시)

        // --- 스칼라 값 매핑 ---
        // d/Tr (Transparency): OpacityFactor (1=불투명, 0=투명) -> Transparency (1=불투명, 0=투명)
        // OBJ의 d는 불투명도, Tr은 투명도인 경우가 많으므로 주의. 여기서는 d (불투명도) 기준으로 변환.
        OutObjInfo.TransparencyScalar = FbxInfo.OpacityFactor;
        // Ni (Optical Density/IOR): FBX 정보에 직접 매핑되는 값 없음. 기본값 사용.
        OutObjInfo.DensityScalar = 1.0f; // 기본값
        // -bm (Bump Multiplier): FBX 정보에 직접 매핑되는 값 없음. 기본값 사용.
        OutObjInfo.BumpMultiplier = 1.0f; // 기본값
        // illum (Illumination Model): PBR 여부에 따라 기본 모델 선택 가능.
        OutObjInfo.IlluminanceModel = FbxInfo.bUsePBRWorkflow ? 2 : 2; // 예: 기본적으로 Phong 모델(2) 사용

        // --- 텍스처 경로 매핑 ---
        // 이름만 복사, 경로는 그대로 사용
        OutObjInfo.DiffuseTexturePath = FbxInfo.BaseColorTexturePath;
        // FbxInfo.BaseColorTexturePath에서 파일 이름만 추출하여 DiffuseTextureName 설정 (필요 시)
        if (!FbxInfo.BaseColorTexturePath.empty())
        {
            std::filesystem::path p(FbxInfo.BaseColorTexturePath);
            OutObjInfo.DiffuseTextureName = FString(p.filename().string().c_str()); // std::string -> FString
        }
        else { OutObjInfo.DiffuseTextureName.Empty(); }

        OutObjInfo.BumpTexturePath = FbxInfo.NormalTexturePath; // Normal -> Bump
        if (!FbxInfo.NormalTexturePath.empty())
        {
            std::filesystem::path p(FbxInfo.NormalTexturePath);
            OutObjInfo.BumpTextureName = FString(p.filename().string().c_str());
        }
        else { OutObjInfo.BumpTextureName.Empty(); }

        OutObjInfo.SpecularTexturePath = FbxInfo.SpecularTexturePath; // Traditional Specular
        if (!FbxInfo.SpecularTexturePath.empty())
        {
            std::filesystem::path p(FbxInfo.SpecularTexturePath);
            OutObjInfo.SpecularTextureName = FString(p.filename().string().c_str());
        }
        else { OutObjInfo.SpecularTextureName.Empty(); }

        OutObjInfo.AlphaTexturePath = FbxInfo.OpacityTexturePath; // Opacity -> Alpha (map_d)
        if (!FbxInfo.OpacityTexturePath.empty())
        {
            std::filesystem::path p(FbxInfo.OpacityTexturePath);
            OutObjInfo.AlphaTextureName = FString(p.filename().string().c_str());
        }
        else { OutObjInfo.AlphaTextureName.Empty(); }

        // Ambient Texture (map_Ka): FBX AO 맵을 사용
        OutObjInfo.AmbientTexturePath = FbxInfo.AmbientOcclusionTexturePath;
        if (!FbxInfo.AmbientOcclusionTexturePath.empty())
        {
            std::filesystem::path p(FbxInfo.AmbientOcclusionTexturePath);
            OutObjInfo.AmbientTextureName = FString(p.filename().string().c_str());
        }
        else { OutObjInfo.AmbientTextureName.Empty(); }

        // TODO: FObjMaterialInfo에 EmissiveTexturePath/Name 필드 추가 필요 시 매핑
        // OutObjInfo.EmissiveTexturePath = FbxInfo.EmissiveTexturePath;
        // if (!FbxInfo.EmissiveTexturePath.IsEmpty()) { ... }

        // --- TextureFlag 설정 ---
        // FObjMaterialInfo의 TextureFlag 비트 정의에 맞춰 설정
        // 예시: (1 << 1) = Diffuse, (1 << 2) = Bump, (1 << 3) = Specular, (1 << 4) = Alpha, (1 << 5) = Ambient
        OutObjInfo.TextureFlag = 0;
        if (FbxInfo.bHasBaseColorTexture && !FbxInfo.BaseColorTexturePath.empty()) OutObjInfo.TextureFlag |= (1 << 1);
        if (FbxInfo.bHasNormalTexture && !FbxInfo.NormalTexturePath.empty())    OutObjInfo.TextureFlag |= (1 << 2);
        if (FbxInfo.bHasSpecularTexture && !FbxInfo.SpecularTexturePath.empty()) OutObjInfo.TextureFlag |= (1 << 3); // Traditional Specular
        if (FbxInfo.bHasOpacityTexture && !FbxInfo.OpacityTexturePath.empty())   OutObjInfo.TextureFlag |= (1 << 4); // map_d
        if (FbxInfo.bHasAmbientOcclusionTexture && !FbxInfo.AmbientOcclusionTexturePath.empty()) OutObjInfo.TextureFlag |= (1 << 5); // map_Ka (AO 사용)
    }
} // End anonymous namespace

namespace std
{
    size_t hash<FBX::FSkeletalMeshVertex>::operator()(const FBX::FSkeletalMeshVertex& Key) const noexcept
    {
        size_t seed = 0;
        // 이제 헤더에 정의된 hash_combine을 사용할 수 있습니다.
        hash_combine(seed, std::hash<float>()(Key.Position.X));
        hash_combine(seed, std::hash<float>()(Key.Position.Y));
        hash_combine(seed, std::hash<float>()(Key.Position.Z));
        hash_combine(seed, std::hash<float>()(Key.Normal.X));
        hash_combine(seed, std::hash<float>()(Key.Normal.Y));
        hash_combine(seed, std::hash<float>()(Key.Normal.Z));
        hash_combine(seed, std::hash<float>()(Key.TexCoord.X));
        hash_combine(seed, std::hash<float>()(Key.TexCoord.Y));
        // TODO: Add Tangent hashing if member exists
        std::hash<uint32> uint_hasher;
        std::hash<float> float_hasher;
        for (int i = 0; i < MAX_BONE_INFLUENCES; ++i)
        {
            hash_combine(seed, uint_hasher(Key.BoneIndices[i]));
            hash_combine(seed, float_hasher(Key.BoneWeights[i]));
        }
        return seed;
    }
}

// --- FLoaderFBX Static Method Implementations ---
FString FbxTransformToString(const FbxAMatrix& Matrix)
{
    FbxVector4 T = Matrix.GetT(); // Translation
    FbxVector4 R = Matrix.GetR(); // Rotation (Euler)
    FbxVector4 S = Matrix.GetS(); // Scaling

    return FString::Printf(TEXT("T(%.2f, %.2f, %.2f) | R(%.2f, %.2f, %.2f) | S(%.2f, %.2f, %.2f)"),
        T[0], T[1], T[2],
        R[0], R[1], R[2],
        S[0], S[1], S[2]);
}

bool FLoaderFBX::ParseFBX(const FString& FBXFilePath, FBX::FBXInfo& OutFBXInfo)
{
    using namespace ::FBX; // Use helpers from anonymous namespace

    FbxManager* SdkManager = FbxManager::Create(); if (!SdkManager) return false;
    struct SdkManagerGuard { FbxManager*& Mgr; ~SdkManagerGuard() { if (Mgr) Mgr->Destroy(); } } SdkGuard{ SdkManager };
    FbxIOSettings* IOS = FbxIOSettings::Create(SdkManager, IOSROOT); if (!IOS) return false; SdkManager->SetIOSettings(IOS);
    FbxScene* Scene = FbxScene::Create(SdkManager, "ImportScene"); if (!Scene) return false;
    struct SceneGuard { FbxScene*& Scn; ~SceneGuard() { if (Scn) Scn->Destroy(); } } ScnGuard{ Scene };
    FbxImporter* Importer = FbxImporter::Create(SdkManager, ""); if (!Importer) return false;
    struct ImporterGuard { FbxImporter*& Imp; ~ImporterGuard() { if (Imp) Imp->Destroy(); } } ImpGuard{ Importer };

#if USE_WIDECHAR
    std::string FilepathStdString = FBXFilePath.ToAnsiString();
#else
    std::string FilepathStdString(*FBXFilePath);
#endif
    if (!Importer->Initialize(FilepathStdString.c_str(), -1, SdkManager->GetIOSettings())) return false;
    if (!Importer->Import(Scene)) return false;

    FbxAxisSystem TargetAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);
    //FbxAxisSystem TargetAxisSystem = FbxAxisSystem::DirectX;/* = FbxAxisSystem::DirectX;*/
    if (Scene->GetGlobalSettings().GetAxisSystem() != TargetAxisSystem)
        TargetAxisSystem.DeepConvertScene(Scene);

    FbxSystemUnit::m.ConvertScene(Scene);
    FbxGeometryConverter GeometryConverter(SdkManager);
    GeometryConverter.Triangulate(Scene, true);

    OutFBXInfo.FilePath = FBXFilePath; std::filesystem::path fsPath(FBXFilePath.ToWideString()); OutFBXInfo.FileDirectory = fsPath.parent_path().wstring().c_str();
    FbxNode* RootNode = Scene->GetRootNode();
    if (!RootNode) return false;

    TMap<FbxSurfaceMaterial*, FName> MaterialPtrToNameMap; OutFBXInfo.Materials.Empty();
    int NumTotalNodes = Scene->GetNodeCount();
    for (int nodeIdx = 0; nodeIdx < NumTotalNodes; ++nodeIdx)
    {
        FbxNode* CurrentNode = Scene->GetNode(nodeIdx); if (!CurrentNode) continue;
        int MaterialCount = CurrentNode->GetMaterialCount();
        for (int matIdx = 0; matIdx < MaterialCount; ++matIdx)
        {
            FbxSurfaceMaterial* FbxMat = CurrentNode->GetMaterial(matIdx);
            if (FbxMat && !MaterialPtrToNameMap.Contains(FbxMat))
            {
                FName MatName(FbxMat->GetName()); int suffix = 1; FName OriginalName = MatName;
                while (OutFBXInfo.Materials.Contains(MatName)) MatName = FName(*(OriginalName.ToString() + FString::Printf(TEXT("_%d"), suffix++)));
                FFbxMaterialInfo MatInfo = ProcessSingleMaterial(FbxMat, OutFBXInfo.FileDirectory); MatInfo.MaterialName = MatName;
                OutFBXInfo.Materials.Add(MatName, MatInfo); MaterialPtrToNameMap.Add(FbxMat, MatName);
            }
        }
    }

    TMap<FbxNode*, FName> BoneNodeToNameMap;
    TArray<FbxNode*> AllBoneNodesTemp;

    OutFBXInfo.SkeletonHierarchy.Empty(); OutFBXInfo.SkeletonRootBoneNames.Empty();

    for (int meshIdx = 0; meshIdx < Scene->GetSrcObjectCount<FbxMesh>(); ++meshIdx)
    {
        FbxMesh* Mesh = Scene->GetSrcObject<FbxMesh>(meshIdx);
        if (!Mesh) continue;

        int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

        for (int deformerIdx = 0; deformerIdx < DeformerCount; ++deformerIdx)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(deformerIdx, FbxDeformer::eSkin));
            if (!Skin) continue;

            int ClusterCount = Skin->GetClusterCount();

            for (int clusterIdx = 0; clusterIdx < ClusterCount; ++clusterIdx)
            {
                FbxCluster* Cluster = Skin->GetCluster(clusterIdx);

                if (!Cluster)
                    continue;

                FbxNode* BoneNode = Cluster->GetLink();

                if (BoneNode)
                    AllBoneNodesTemp.AddUnique(BoneNode);
            }
        }
    }

    for (FbxNode* BoneNode : AllBoneNodesTemp)
    {
        FName BoneName(BoneNode->GetName()); BoneNodeToNameMap.Add(BoneNode, BoneName);

        if (!OutFBXInfo.SkeletonHierarchy.Contains(BoneName))
        {
            FBoneHierarchyNode HierarchyNode; HierarchyNode.BoneName = BoneName;
            FbxAMatrix GlobalBindPoseMatrix;
            FbxAMatrix TransformMatrix;
            bool bBindPoseFound = false;
            for (int meshIdx = 0; meshIdx < Scene->GetSrcObjectCount<FbxMesh>() && !bBindPoseFound; ++meshIdx)
            {
                FbxMesh* Mesh = Scene->GetSrcObject<FbxMesh>(meshIdx); if (!Mesh) continue;
                int DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
                for (int deformerIdx = 0; deformerIdx < DeformerCount && !bBindPoseFound; ++deformerIdx)
                {
                    FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(deformerIdx, FbxDeformer::eSkin)); if (!Skin) continue;
                    int ClusterCount = Skin->GetClusterCount();
                    for (int clusterIdx = 0; clusterIdx < ClusterCount && !bBindPoseFound; ++clusterIdx)
                    {
                        FbxCluster* Cluster = Skin->GetCluster(clusterIdx);
                        if (Cluster && Cluster->GetLink() == BoneNode)
                        {
                            Cluster->GetTransformLinkMatrix(GlobalBindPoseMatrix);
                            Cluster->GetTransformMatrix(TransformMatrix);
                            HierarchyNode.GlobalBindPose = ConvertFbxAMatrixToFMatrix(GlobalBindPoseMatrix);
                            HierarchyNode.TransformMatrix = ConvertFbxAMatrixToFMatrix(TransformMatrix);
                            bBindPoseFound = true;
                        }
                    }
                }
            }
            if (!bBindPoseFound)
            {
                GlobalBindPoseMatrix = BoneNode->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
                HierarchyNode.GlobalBindPose = ConvertFbxAMatrixToFMatrix(GlobalBindPoseMatrix);
                HierarchyNode.TransformMatrix = FMatrix::Identity;
            }
            OutFBXInfo.SkeletonHierarchy.Add(BoneName, HierarchyNode);
        }
    }

    TArray<FName> CollectedBoneNames;

    OutFBXInfo.SkeletonHierarchy.GetKeys(CollectedBoneNames);

    for (const FName& BoneName : CollectedBoneNames)
    {
        FbxNode* CurrentFbxNode = nullptr;
        for (auto It = BoneNodeToNameMap.begin(); It != BoneNodeToNameMap.end(); ++It)
        {
            if (It->Value == BoneName) { CurrentFbxNode = It->Key; break; }
        }

        if (!CurrentFbxNode) continue;

        FbxNode* ParentFbxNode = CurrentFbxNode->GetParent();
        FName ParentName = NAME_None;
        // Traverse upwards to find the nearest *bone* parent
        while (ParentFbxNode)
        {
            const FName* FoundParentName = BoneNodeToNameMap.Find(ParentFbxNode); // Is parent a known bone?
            if (FoundParentName)
            {
                // Ensure the found parent is actually part of the hierarchy we built
                if (OutFBXInfo.SkeletonHierarchy.Contains(*FoundParentName))
                {
                    ParentName = *FoundParentName;
                    break; // Found the nearest valid bone parent
                }
            }
            ParentFbxNode = ParentFbxNode->GetParent(); // Keep going up
        }
        OutFBXInfo.SkeletonHierarchy[BoneName].ParentName = ParentName;

        if (ParentName.IsNone())
            OutFBXInfo.SkeletonRootBoneNames.AddUnique(BoneName);
    }

    // Pass 3: Mesh Data
    OutFBXInfo.Meshes.Empty();
    std::function<void(FbxNode*)> ProcessNodeRecursive = // Use std::function
        [&](FbxNode* CurrentNode)
        {
            if (!CurrentNode) return; FbxMesh* Mesh = CurrentNode->GetMesh();
            if (Mesh)
            {
                MeshRawData RawData;
                FbxAMatrix MeshNodeFbxGlobalTransform = CurrentNode->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
                RawData.MeshNodeGlobalTransformAtBindTime = ConvertFbxAMatrixToFMatrix(MeshNodeFbxGlobalTransform);
                if (ExtractSingleMeshRawData(CurrentNode, RawData, MaterialPtrToNameMap))
                {
                    OutFBXInfo.Meshes.Add(std::move(RawData));
                }
            } // Use std::move
            for (int i = 0; i < CurrentNode->GetChildCount(); ++i)
                ProcessNodeRecursive(CurrentNode->GetChild(i));
        };
    ProcessNodeRecursive(RootNode);
    for (auto& Pair : OutFBXInfo.SkeletonHierarchy)
    {
        if (Pair.Key.IsNone())
        {
            continue; // 또는 return false;
        }
        const FName& BoneName = Pair.Key;

        const FBoneHierarchyNode& BoneNode = Pair.Value;
        FString BoneInfo = FbxTransformToString(ConvertFMatrixToFbxAMatrix(BoneNode.GlobalBindPose));
        UE_LOG(LogLevel::Display, TEXT("[%s] %s"), *BoneName.ToString(), *BoneInfo);
    }
    return true;
}

bool FLoaderFBX::ConvertToSkeletalMesh(const TArray<FBX::MeshRawData>& AllRawMeshData, const FBX::FBXInfo& FullFBXInfo, FBX::FSkeletalMeshRenderData& OutSkeletalMeshRenderData, USkeleton* OutSkeleton)
{
    using namespace ::FBX;

    if (!OutSkeleton) return false;


    OutSkeletalMeshRenderData.MeshName = AllRawMeshData[0].NodeName.ToString();
    OutSkeletalMeshRenderData.FilePath = FullFBXInfo.FilePath;
    OutSkeleton->Clear();

    // 1. 스키닝에 관련된 모든 본 및 그 부모 본들 수집 (BonesToInclude)
    TArray<FName> RelevantBoneNames;
    for (const auto& RawMeshDataInstance : AllRawMeshData)
    {
        // 모든 메시의 영향 본 수집
        for (const auto& influence : RawMeshDataInstance.SkinningInfluences)
        {
            RelevantBoneNames.AddUnique(influence.BoneName);
        }
    }
    if (RelevantBoneNames.IsEmpty() && !AllRawMeshData.IsEmpty() && !FullFBXInfo.SkeletonHierarchy.IsEmpty())
    {
        FullFBXInfo.SkeletonHierarchy.GetKeys(RelevantBoneNames);
    }

    TArray<FName> BonesToInclude = RelevantBoneNames;
    int32 CheckIndex = 0;
    while (CheckIndex < BonesToInclude.Num())
    {
        FName CurrentBoneName = BonesToInclude[CheckIndex++];
        const FBoneHierarchyNode* HNode = FullFBXInfo.SkeletonHierarchy.Find(CurrentBoneName);
        if (HNode && !HNode->ParentName.IsNone() && FullFBXInfo.SkeletonHierarchy.Contains(HNode->ParentName))
        {
            BonesToInclude.AddUnique(HNode->ParentName);
        }
    }


    // 2. 루트 본 식별 및 자식 관계 맵핑 (USkeleton에 아직 추가 안 함)
    TMap<FName, TArray<FName>> BoneChildrenMap;
    TArray<FName> RootBoneNamesForSorting;
    for (const FName& BoneName : BonesToInclude)
    {
        const FBoneHierarchyNode* HNode = FullFBXInfo.SkeletonHierarchy.Find(BoneName);
        if (HNode)
        {
            if (HNode->ParentName.IsNone() || !BonesToInclude.Contains(HNode->ParentName))
            {
                RootBoneNamesForSorting.AddUnique(BoneName);
            }
            else
            {
                BoneChildrenMap.FindOrAdd(HNode->ParentName).Add(BoneName);
            }
        }
    }

    // 3. 본들을 위상 정렬 (부모가 항상 자식보다 먼저 오도록)
    TArray<FName> SortedBoneNames; SortedBoneNames.Reserve(BonesToInclude.Num());
    TArray<FName> ProcessingQueue = RootBoneNamesForSorting;
    TMap<FName, bool> ProcessedBones;
    int32 Head = 0;
    while (Head < ProcessingQueue.Num())
    {
        FName CurrentBoneName = ProcessingQueue[Head++];
        if (ProcessedBones.Contains(CurrentBoneName))
        {
            continue;
        }
        SortedBoneNames.Add(CurrentBoneName);
        ProcessedBones.Add(CurrentBoneName, true);
        if (BoneChildrenMap.Contains(CurrentBoneName))
        {
            for (const FName& ChildBoneName : BoneChildrenMap[CurrentBoneName])
            {
                if (!ProcessedBones.Contains(ChildBoneName))
                {
                    ProcessingQueue.Add(ChildBoneName);
                }
            }
        }
    }
    if (SortedBoneNames.Num() != BonesToInclude.Num())
    {
        for (const FName& BoneName : BonesToInclude)
        {
            if (!ProcessedBones.Contains(BoneName))
            {
                SortedBoneNames.Add(BoneName);
            }
        }
    }

    // 4. 정렬된 순서대로 USkeleton에 본 추가 (단일 호출 지점)
    for (const FName& BoneName : SortedBoneNames)
    {
        const FBoneHierarchyNode* HNode = FullFBXInfo.SkeletonHierarchy.Find(BoneName);
        if (HNode)
        {
            int32 ParentIndexInSkeleton = INDEX_NONE;
            if (!HNode->ParentName.IsNone())
            {
                const uint32* FoundParentIndexPtr = OutSkeleton->BoneNameToIndex.Find(HNode->ParentName);
                if (FoundParentIndexPtr) ParentIndexInSkeleton = static_cast<int32>(*FoundParentIndexPtr);
            }
            OutSkeleton->AddBone(BoneName, ParentIndexInSkeleton, HNode->GlobalBindPose, HNode->TransformMatrix);
        }
    }

    // 2. Prepare Skinning Data
    // --- 2. 메시 데이터 통합 ---
    TArray<FVector> CombinedControlPoints_MeshNodeLocal; // 각 메시 노드 로컬 공간 기준 컨트롤 포인트
    TArray<int32> CombinedPolygonVertexIndices;
    TArray<FControlPointSkinningData> CombinedCpSkinData; // 컨트롤 포인트 기준 스키닝 데이터

    // ReconstructVertexAttributes 결과 통합
    TArray<FVector> CombinedPVNormals;
    TArray<FVector2D> CombinedPVUVs;

    uint32 GlobalVertexOffset = 0; // 전체 컨트롤 포인트에 대한 오프셋
    uint32 GlobalPolyVertOffset = 0; // 전체 폴리곤 정점에 대한 오프셋 (PVNormals, PVUVs 인덱싱용)


    for (const FBX::MeshRawData& CurrentRawMeshData : AllRawMeshData)
    {
        // 2a. 메시 노드의 글로벌 변환 가져오기 (이 변환은 정점을 월드(또는 공통) 바인드 공간으로 옮김)
        // 중요: 이 변환은 FBX 파싱 시 각 MeshRawData에 저장되어 있어야 함.
        // 여기서는 임시로 단위 행렬로 가정. 실제로는 각 메시 노드의 글로벌 변환을 사용해야 함.
        const FMatrix& MeshNodeWorldBindTransform = FMatrix::Identity;//CurrentRawMeshData.MeshNodeGlobalTransformAtBindTime; // 저장된 값 사용

        // 컨트롤 포인트 추가 (메시 노드 변환 적용)
        for (const FVector& cp_local_to_mesh_node : CurrentRawMeshData.ControlPoints)
        {
            CombinedControlPoints_MeshNodeLocal.Add(MeshNodeWorldBindTransform.TransformPosition(cp_local_to_mesh_node));
        }

        // 스키닝 데이터 추가 (컨트롤 포인트 인덱스에 GlobalVertexOffset 적용)
        int32 CurrentMeshCPCount = CurrentRawMeshData.ControlPoints.Num();
        int32 OldSize = CombinedCpSkinData.Num();
        CombinedCpSkinData.SetNum(OldSize + CurrentMeshCPCount);

        for (const auto& influence : CurrentRawMeshData.SkinningInfluences)
        {
            const uint32* SkelBoneIndexPtr = OutSkeleton->BoneNameToIndex.Find(influence.BoneName);
            if (!SkelBoneIndexPtr) continue;
            int32 SkelBoneIndex = static_cast<int32>(*SkelBoneIndexPtr);

            for (int i = 0; i < influence.ControlPointIndices.Num(); ++i)
            {
                int32 LocalCPIndex = influence.ControlPointIndices[i];
                int32 GlobalCPIndex = GlobalVertexOffset + LocalCPIndex; // 글로벌 인덱스
                float Weight = static_cast<float>(influence.ControlPointWeights[i]);

                if (CombinedCpSkinData.IsValidIndex(GlobalCPIndex) && Weight > KINDA_SMALL_NUMBER)
                {
                    CombinedCpSkinData[GlobalCPIndex].Influences.Add({ SkelBoneIndex, Weight });
                }
            }
        }

        // 정점 속성 재구성 및 통합
        TArray<FVector> TempCPNormals, TempPVNormals_ForThisMesh;
        TArray<FVector2D> TempCPUVs, TempPVUVs_ForThisMesh;
        if (!ReconstructVertexAttributes(CurrentRawMeshData, TempCPNormals, TempPVNormals_ForThisMesh, TempCPUVs, TempPVUVs_ForThisMesh))
        {
            return false; // 또는 기본값으로 계속 진행하는 로직 필요
        }
        FMatrix NormalTransform = MeshNodeWorldBindTransform;
        NormalTransform.RemoveTranslation();

        for (const FVector& normal : TempPVNormals_ForThisMesh)
        {
            CombinedPVNormals.Add(NormalTransform.TransformPosition(normal).GetSafeNormal());
        }
        for (const FVector2D& uv : TempPVUVs_ForThisMesh)
        {
            CombinedPVUVs.Add(uv);
        }



        // 폴리곤 정점 인덱스 추가 (컨트롤 포인트 인덱스에 GlobalVertexOffset 적용)
        for (int32 local_cp_idx : CurrentRawMeshData.PolygonVertexIndices)
        {
            CombinedPolygonVertexIndices.Add(GlobalVertexOffset + local_cp_idx);
        }
        GlobalVertexOffset += CurrentMeshCPCount;
        GlobalPolyVertOffset += CurrentRawMeshData.PolygonVertexIndices.Num(); // PV 속성 인덱싱을 위함
    }

    // --- 3. 통합된 스키닝 가중치 정규화 ---
    for (int32 cpIdx = 0; cpIdx < CombinedCpSkinData.Num(); ++cpIdx)
    {
        CombinedCpSkinData[cpIdx].NormalizeWeights(MAX_BONE_INFLUENCES);
    }


    OutSkeletalMeshRenderData.BindPoseVertices.Empty();
    OutSkeletalMeshRenderData.Indices.Empty();
    std::unordered_map<FSkeletalMeshVertex, uint32> UniqueFinalVertices;
    uint32 FinalVertexIndexCounter = 0;

    for (int32 FinalPolyVertIdx = 0; FinalPolyVertIdx < CombinedPolygonVertexIndices.Num(); ++FinalPolyVertIdx)
    {
        int32 GlobalCPIndex = CombinedPolygonVertexIndices[FinalPolyVertIdx]; // 폴리곤이 참조하는 컨트롤 포인트의 글로벌 인덱스

        FSkeletalMeshVertex CurrentFinalVertex = {};

        if (CombinedControlPoints_MeshNodeLocal.IsValidIndex(GlobalCPIndex))
        {
            CurrentFinalVertex.Position = CombinedControlPoints_MeshNodeLocal[GlobalCPIndex];
        }
        else { /* 오류 처리 또는 기본값 */ CurrentFinalVertex.Position = FVector::ZeroVector; }

        if (CombinedPVNormals.IsValidIndex(FinalPolyVertIdx))
        { // PV속성은 최종 폴리곤 정점 순서를 따름
            CurrentFinalVertex.Normal = CombinedPVNormals[FinalPolyVertIdx];
        }
        else
        {
            CurrentFinalVertex.Normal = FVector(0, 0, 1);
        }
        if (!CurrentFinalVertex.Normal.IsNearlyZero())
            CurrentFinalVertex.Normal.Normalize();
        else
            CurrentFinalVertex.Normal = FVector(0, 0, 1);

        if (CombinedPVUVs.IsValidIndex(FinalPolyVertIdx))
        {
            CurrentFinalVertex.TexCoord = CombinedPVUVs[FinalPolyVertIdx];
        }
        else
        {
            CurrentFinalVertex.TexCoord = FVector2D::ZeroVector;
        }

        if (CombinedCpSkinData.IsValidIndex(GlobalCPIndex))
        {
            const auto& InfluencesForCP = CombinedCpSkinData[GlobalCPIndex].Influences;
            // NormalizeWeights가 이미 호출되었으므로, 여기서 가져온 가중치는 정규화된 상태여야 함.
            for (int32 i = 0; i < InfluencesForCP.Num() && i < MAX_BONE_INFLUENCES; ++i)
            {
                CurrentFinalVertex.BoneIndices[i] = InfluencesForCP[i].BoneIndex;
                CurrentFinalVertex.BoneWeights[i] = InfluencesForCP[i].Weight;
            }
            for (int32 i = InfluencesForCP.Num(); i < MAX_BONE_INFLUENCES; ++i)
            {
                CurrentFinalVertex.BoneIndices[i] = 0;
                CurrentFinalVertex.BoneWeights[i] = 0.0f;
            }
        }
        else
        {
            // 스키닝 정보 없는 경우, 루트 본에 100% 가중치 (또는 다른 처리)
            CurrentFinalVertex.BoneIndices[0] = 0;
            CurrentFinalVertex.BoneWeights[0] = 1.0f;
        }


        auto it = UniqueFinalVertices.find(CurrentFinalVertex);
        if (it != UniqueFinalVertices.end())
        {
            OutSkeletalMeshRenderData.Indices.Add(it->second);
        }
        else
        {
            uint32 NewIndex = FinalVertexIndexCounter++;
            OutSkeletalMeshRenderData.BindPoseVertices.Add(CurrentFinalVertex);
            UniqueFinalVertices[CurrentFinalVertex] = NewIndex;
            OutSkeletalMeshRenderData.Indices.Add(NewIndex);
        }
    }

    // 5. Populate Materials
    OutSkeletalMeshRenderData.Materials.Empty();
    TMap<FName, int32> MatNameToIndexMap;
    for (const FBX::MeshRawData& CurrentRawMeshData : AllRawMeshData)
    {
        for (const FName& MatName : CurrentRawMeshData.MaterialNames)
        {
            if (MatName.IsNone())
            {
                if (!MatNameToIndexMap.Contains(NAME_None))
                {
                    int32 DefIdx = OutSkeletalMeshRenderData.Materials.Add(FFbxMaterialInfo());
                    MatNameToIndexMap.Add(NAME_None, DefIdx);
                }
                continue;
            }

            const FFbxMaterialInfo* MatInfoPtr = FullFBXInfo.Materials.Find(MatName);
            if (MatInfoPtr && !MatNameToIndexMap.Contains(MatName))
            {
                int32 NewIdx = OutSkeletalMeshRenderData.Materials.Add(*MatInfoPtr);
                MatNameToIndexMap.Add(MatName, NewIdx);
            }
            else if (!MatInfoPtr && !MatNameToIndexMap.Contains(MatName))
            {
                int32 DefIdx = OutSkeletalMeshRenderData.Materials.Add(FFbxMaterialInfo());
                MatNameToIndexMap.Add(MatName, DefIdx);
            }
        }
    }
    if (OutSkeletalMeshRenderData.Materials.IsEmpty() && !OutSkeletalMeshRenderData.Indices.IsEmpty())
    {
        OutSkeletalMeshRenderData.Materials.Add(FFbxMaterialInfo());
        MatNameToIndexMap.Add(NAME_None, 0);
    }
    for (const FBX::MeshRawData& CurrentRawMeshData : AllRawMeshData)
    {
        if (!CreateMaterialSubsetsInternal(CurrentRawMeshData, MatNameToIndexMap, OutSkeletalMeshRenderData))
        {
            if (!OutSkeletalMeshRenderData.Indices.IsEmpty() && !OutSkeletalMeshRenderData.Materials.IsEmpty())
            {
                OutSkeletalMeshRenderData.Subsets.Empty();
                FMeshSubset DefSub;
                DefSub.MaterialIndex = 0;
                DefSub.IndexStart = 0;
                DefSub.IndexCount = OutSkeletalMeshRenderData.Indices.Num();
                OutSkeletalMeshRenderData.Subsets.Add(DefSub);
            }
            else
            {
                return false;
            }
        }
    }
    uint32 TotalSubIdx = 0;
    for (const auto& sub : OutSkeletalMeshRenderData.Subsets)
        TotalSubIdx += sub.IndexCount;
    if (TotalSubIdx != OutSkeletalMeshRenderData.Indices.Num())
        return false;

    // 7. Calculate Initial Local Transforms
    CalculateInitialLocalTransformsInternal(OutSkeleton);

    // 8. TODO: Calculate Tangents (Requires Tangent member in FSkeletalMeshVertex and averaging logic)


    // 9. Calculate Bounding Box
    TArray<FVector> PositionsForBoundsCalculation;
    PositionsForBoundsCalculation.Reserve(OutSkeletalMeshRenderData.BindPoseVertices.Num());

    for (const FBX::FSkeletalMeshVertex& VertexToSkinForBounds : OutSkeletalMeshRenderData.BindPoseVertices)
    {
        FVector FinalPosForBounds = FVector::ZeroVector;
        bool bWasSkinned = false; // 실제로 스키닝 계산이 수행되었는지

        // VertexToSkinForBounds.Position은 이미 메시 노드의 월드 변환까지 적용된 공통 공간 기준 위치
        const FVector& BasePositionForSkining = VertexToSkinForBounds.Position;

        for (int j = 0; j < MAX_BONE_INFLUENCES; ++j)
        {
            uint32 BoneIdx = VertexToSkinForBounds.BoneIndices[j];
            float Weight = VertexToSkinForBounds.BoneWeights[j];

            if (Weight <= KINDA_SMALL_NUMBER) continue;
            if (!OutSkeleton->BoneTree.IsValidIndex(BoneIdx)) continue;

            bWasSkinned = true;
            const FBoneNode& BoneNode = OutSkeleton->BoneTree[BoneIdx];
            const FMatrix& GeometryOffset = BoneNode.GeometryOffsetMatrix;

            FinalPosForBounds += GeometryOffset.TransformPosition(BasePositionForSkining) * Weight;
        }

        if (!bWasSkinned)
        {
            // 스키닝 가중치가 전혀 없는 정점 (예: 모든 가중치가 0이거나, 유효한 본 인덱스가 없음)
            FinalPosForBounds = BasePositionForSkining; // 원본 위치 (메시 노드 월드 변환 적용된 상태) 그대로 사용
        }

        PositionsForBoundsCalculation.Add(FinalPosForBounds);
    }

    TArray<FBX::FSkeletalMeshVertex> TempVerticesForBounds;

    TempVerticesForBounds.Reserve(PositionsForBoundsCalculation.Num());

    for (const FVector& Pos : PositionsForBoundsCalculation)
    {
        FBX::FSkeletalMeshVertex TempVtx;
        TempVtx.Position = Pos;
        TempVerticesForBounds.Add(TempVtx);
    }
    FLoaderFBX::ComputeBoundingBox(TempVerticesForBounds, OutSkeletalMeshRenderData.Bounds.min, OutSkeletalMeshRenderData.Bounds.max);


    return true;
}

bool FLoaderFBX::CreateTextureFromFile(const FWString& Filename)
{
    if (FEngineLoop::ResourceManager.GetTexture(Filename))
    {
        return true;
    }

    HRESULT hr = FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, nullptr, Filename.c_str());

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void FLoaderFBX::ComputeBoundingBox(const TArray<FBX::FSkeletalMeshVertex>& InVertices, FVector& OutMinVector, FVector& OutMaxVector)
{
    if (InVertices.IsEmpty())
    {
        OutMinVector = FVector::ZeroVector; OutMaxVector = FVector::ZeroVector;
        return;
    }
    OutMinVector = InVertices[0].Position;
    OutMaxVector = InVertices[0].Position;
    for (int32 i = 1; i < InVertices.Num(); ++i)
    {
        OutMinVector = FVector::Min(OutMinVector, InVertices[i].Position);
        OutMaxVector = FVector::Max(OutMaxVector, InVertices[i].Position);
    }
}

void FLoaderFBX::CalculateTangent(FBX::FSkeletalMeshVertex& PivotVertex, const FBX::FSkeletalMeshVertex& Vertex1, const FBX::FSkeletalMeshVertex& Vertex2) { /* TODO: Implement if needed */ }


// --- FManagerFBX Static Method Implementations ---

FBX::FSkeletalMeshRenderData* FManagerFBX::LoadFBXSkeletalMeshAsset(const FString& PathFileName, USkeleton* OutSkeleton)
{
    using namespace FBX;
    if (!OutSkeleton) return nullptr; // USkeleton 객체 필요

    FSkeletalMeshRenderData** FoundDataPtr = FBXSkeletalMeshMap.Find(PathFileName);

    if (FoundDataPtr)
    {
        // 캐시된 RenderData가 있으면, USkeleton 정보만 채워야 할 수도 있음
        // TODO: 캐시된 RenderData와 함께 USkeleton을 어떻게 처리할지 정책 결정 필요
        //       간단하게는 캐시 히트 시에도 파싱/변환을 다시 수행하거나,
        //       캐시된 RenderData에서 스켈레톤 정보를 읽어 OutSkeleton을 채우는 로직 추가
        // 여기서는 일단 캐시 히트 시 바로 반환 (USkeleton 채우는 로직 누락 가능성)
        return *FoundDataPtr;
    }

    FBXInfo ParsedInfo;
    if (!FLoaderFBX::ParseFBX(PathFileName, ParsedInfo)) return nullptr;
    if (ParsedInfo.Meshes.IsEmpty()) return nullptr;
    FSkeletalMeshRenderData* NewRenderData = new FSkeletalMeshRenderData();
    if (!FLoaderFBX::ConvertToSkeletalMesh(ParsedInfo.Meshes, ParsedInfo, *NewRenderData, OutSkeleton))
    {
        delete NewRenderData;
        return nullptr;
    }
    FBXSkeletalMeshMap.Add(PathFileName, NewRenderData);
    return NewRenderData;
}

void FManagerFBX::CombineMaterialIndex(FBX::FSkeletalMeshRenderData& OutFSkeletalMesh) { /* No-op */ }
bool FManagerFBX::SaveSkeletalMeshToBinary(const FWString& FilePath, const FBX::FSkeletalMeshRenderData& SkeletalMesh) { return false; /* TODO */ }
bool FManagerFBX::LoadSkeletalMeshFromBinary(const FWString& FilePath, FBX::FSkeletalMeshRenderData& OutSkeletalMesh) { return false; /* TODO */ }

// Parameter type corrected
UMaterial* FManagerFBX::CreateMaterial(const FBX::FFbxMaterialInfo& materialInfo)
{
    FString MatKey = materialInfo.MaterialName.ToString();
    UMaterial** FoundMatPtr = materialMap.Find(MatKey); if (FoundMatPtr) return *FoundMatPtr;
    UMaterial* NewMat = FObjectFactory::ConstructObject<UMaterial>(nullptr); // Use your factory
    if (NewMat)
    {
        FObjMaterialInfo objInfo;

        ConvertFbxMaterialToObjMaterial(materialInfo, objInfo);

        NewMat->SetMaterialInfo(objInfo);

        if (materialInfo.bHasBaseColorTexture && !materialInfo.BaseColorTexturePath.empty())
        {
            FLoaderFBX::CreateTextureFromFile(materialInfo.BaseColorTexturePath);
        }
        if (materialInfo.bHasNormalTexture && !materialInfo.NormalTexturePath.empty())
        {
            FLoaderFBX::CreateTextureFromFile(materialInfo.NormalTexturePath);
        }

        materialMap.Add(MatKey, NewMat);
    }
    return NewMat;
}

UMaterial* FManagerFBX::GetMaterial(const FString& name) { UMaterial** Ptr = materialMap.Find(name); return Ptr ? *Ptr : nullptr; }

USkeletalMesh* FManagerFBX::CreateSkeletalMesh(const FString& filePath)
{
    FWString MeshKey = filePath.ToWideString();


    if (SkeletalMeshMap.Contains(MeshKey))
        return SkeletalMeshMap[MeshKey];

    USkeletalMesh* NewMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);
    NewMesh->Skeleton = FObjectFactory::ConstructObject<USkeleton>(NewMesh); // Outer를 NewMesh로 설정 예시
    if (!NewMesh->Skeleton)
    {
        return nullptr;
    }

    FBX::FSkeletalMeshRenderData* RenderData = LoadFBXSkeletalMeshAsset(filePath, NewMesh->Skeleton);
    if (!RenderData)
    {
        return nullptr;
    }

    NewMesh->SetData(RenderData);

    SkeletalMeshMap.Add(MeshKey, NewMesh);
    return NewMesh;
}

const TMap<FWString, USkeletalMesh*>& FManagerFBX::GetSkeletalMeshes() { return SkeletalMeshMap; }
USkeletalMesh* FManagerFBX::GetSkeletalMesh(const FWString& name)
{
    if (SkeletalMeshMap.Contains(name))
        return SkeletalMeshMap[name];

    return CreateSkeletalMesh(FString(name.c_str()));
}
