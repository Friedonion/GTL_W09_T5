#include "Components/SkeletalMeshComponent.h"

#include "Engine/FLoaderFBX.h"
#include "Launch/EngineLoop.h"
#include "UObject/Casts.h"
#include "UObject/ObjectFactory.h"

#include "GameFramework/Actor.h"

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));

   // NewComponent->SkeletalMesh = SkeletalMesh;
    NewComponent->selectedSubMeshIndex = selectedSubMeshIndex;
    NewComponent->SkeletalMesh = SkeletalMesh;
    return NewComponent;
}
float AngleRad;
void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime); // 부모 클래스 Tick 호출 (필요 시)

    // 스켈레탈 메시가 없거나 애니메이션 비활성화 시 중단
    if (!SkeletalMesh)
    {
        return;
    }
    TArray< FName> BoneName;
    SkeletalMesh->GetBoneNames(BoneName);
    // 1. 움직일 본 찾기
    if (BoneName.IsEmpty()) return;
    int32 BoneIndex = SkeletalMesh->GetBoneIndexByName(BoneName[0]);

    if (BoneIndex != INDEX_NONE)
    {
        // 2. 현재 로컬 변환 가져오기
        FMatrix CurrentLocalMatrix = SkeletalMesh->GetBoneLocalMatrix(BoneIndex);

        AngleRad += FMath::DegreesToRadians(2 * DeltaTime);
        FMatrix DeltaRotation = FMatrix::CreateRotationMatrix(0.0f, AngleRad, 0); // Z축 회전 (Yaw)

        // 4. 새로운 로컬 변환 계산 (현재 로컬 변환에 델타 회전 적용)
        // 중요: 회전 순서에 따라 결과가 달라짐 (Delta * Current 또는 Current * Delta)
        // 여기서는 현재 로컬 변환 이후에 추가 회전을 적용하는 것으로 가정 (Delta * Current)
        FMatrix NewLocalMatrix = DeltaRotation * CurrentLocalMatrix;

        // 5. 새로운 로컬 변환 설정
        if (SkeletalMesh->SetBoneLocalMatrix(BoneIndex, NewLocalMatrix))
        {
            // 6. 스켈레톤 전체 월드 변환 업데이트 (로컬 변경 후 필수)
            SkeletalMesh->UpdateWorldTransforms();

            SkeletalMesh->UpdateAndApplySkinning();
        }
    }
}

void USkeletalMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    
    //StaticMesh 경로 저장
    USkeletalMesh* CurrentMesh = GetSkeletalMesh();
    if (CurrentMesh != nullptr) {

        // 1. std::wstring 경로 얻기
        std::wstring PathWString = CurrentMesh->GetObjectName();

        // 2. std::wstring을 FString으로 변환
        FString PathFString(PathWString.c_str()); // c_str()로 const wchar_t* 얻어서 FString 생성
       // PathFString = CurrentMesh->ConvertToRelativePathFromAssets(PathFString);

        FWString PathWString2 = PathFString.ToWideString();

        
        OutProperties.Add(TEXT("SkeletalMeshPath"), PathFString);
    } else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None")); // 메시 없음 명시
    }
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;

    
    // --- StaticMesh 설정 ---
    TempStr = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (TempStr) // 키가 존재하는지 확인
    {
        if (*TempStr != TEXT("None")) // 값이 "None"이 아닌지 확인
        {
            // 경로 문자열로 UStaticMesh 에셋 로드 시도
           
            if (USkeletalMesh* MeshToSet = FManagerFBX::CreateSkeletalMesh(*TempStr))
            {
                SetSkeletalMesh(MeshToSet); // 성공 시 메시 설정
                UE_LOG(LogLevel::Display, TEXT("Set StaticMesh '%s' for %s"), **TempStr, *GetName());
            }
            else
            {
                // 로드 실패 시 경고 로그
                UE_LOG(LogLevel::Warning, TEXT("Could not load StaticMesh '%s' for %s"), **TempStr, *GetName());
                SetSkeletalMesh(nullptr); // 안전하게 nullptr로 설정
            }
        }
        else // 값이 "None"이면
        {
            SetSkeletalMesh(nullptr); // 명시적으로 메시 없음 설정
            UE_LOG(LogLevel::Display, TEXT("Set StaticMesh to None for %s"), *GetName());
        }
    }
    else // 키 자체가 없으면
    {
        // 키가 없는 경우 어떻게 처리할지 결정 (기본값 유지? nullptr 설정?)
        // 여기서는 기본값을 유지하거나, 안전하게 nullptr로 설정할 수 있습니다.
        // SetStaticMesh(nullptr); // 또는 아무것도 안 함
        UE_LOG(LogLevel::Display, TEXT("StaticMeshPath key not found for %s, mesh unchanged."), *GetName());
    }
}

uint32 USkeletalMeshComponent::GetNumMaterials() const
{
    if (SkeletalMesh == nullptr) return 0;

    return SkeletalMesh->GetMaterials().Num();
}

UMaterial* USkeletalMeshComponent::GetMaterial(uint32 ElementIndex) const
{
    if (SkeletalMesh != nullptr)
    {
        if (OverrideMaterials[ElementIndex] != nullptr)
        {
            return OverrideMaterials[ElementIndex];
        }
    
        if (SkeletalMesh->GetMaterials().IsValidIndex(ElementIndex))
        {
            return SkeletalMesh->GetMaterials()[ElementIndex]->Material;
        }
    }
    return nullptr;
}

uint32 USkeletalMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
    if (SkeletalMesh == nullptr) return -1;

    return SkeletalMesh->GetMaterialIndex(MaterialSlotName);
}

TArray<FName> USkeletalMeshComponent::GetMaterialSlotNames() const
{
    TArray<FName> MaterialNames;
    if (SkeletalMesh == nullptr) return MaterialNames;

    for (const FStaticMaterial* Material : SkeletalMesh->GetMaterials())
    {
        MaterialNames.Emplace(Material->MaterialSlotName);
    }

    return MaterialNames;
}

void USkeletalMeshComponent::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    if (SkeletalMesh == nullptr) return;
    SkeletalMesh->GetUsedMaterials(Out);
    for (int materialIndex = 0; materialIndex < GetNumMaterials(); materialIndex++)
    {
        if (OverrideMaterials[materialIndex] != nullptr)
        {
            Out[materialIndex] = OverrideMaterials[materialIndex];
        }
    }
}

int USkeletalMeshComponent::CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const
{
    if (!AABB.Intersect(InRayOrigin, InRayDirection, OutHitDistance))
    {
        return 0;
    }
    if (SkeletalMesh == nullptr)
    {
        return 0;
    }

    OutHitDistance = FLT_MAX;

    int IntersectionNum = 0;

    FBX::FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetRenderData();
    const TArray<FBX::FSkeletalMeshVertex>& Vertices = RenderData->BindPoseVertices;
    const int32 VertexNum = Vertices.Num();
    if (VertexNum == 0)
    {
        return 0;
    }

    const TArray<UINT>& Indices = RenderData->Indices;
    const int32 IndexNum = Indices.Num();
    const bool bHasIndices = (IndexNum > 0);

    int32 TriangleNum = bHasIndices ? (IndexNum / 3) : (VertexNum / 3);
    for (int32 i = 0; i < TriangleNum; i++)
    {
        int32 Idx0 = i * 3;
        int32 Idx1 = i * 3 + 1;
        int32 Idx2 = i * 3 + 2;

        if (bHasIndices)
        {
            Idx0 = Indices[Idx0];
            Idx1 = Indices[Idx1];
            Idx2 = Indices[Idx2];
        }

        // 각 삼각형의 버텍스 위치를 FVector로 불러옵니다.
        FVector v0 = FVector(Vertices[Idx0].Position.X, Vertices[Idx0].Position.Y, Vertices[Idx0].Position.Z);
        FVector v1 = FVector(Vertices[Idx1].Position.X, Vertices[Idx1].Position.Y, Vertices[Idx1].Position.Z);
        FVector v2 = FVector(Vertices[Idx2].Position.X, Vertices[Idx2].Position.Y, Vertices[Idx2].Position.Z);

        float HitDistance = FLT_MAX;
        if (IntersectRayTriangle(InRayOrigin, InRayDirection, v0, v1, v2, HitDistance))
        {
            OutHitDistance = FMath::Min(HitDistance, OutHitDistance);
            IntersectionNum++;
        }

    }
    return IntersectionNum;
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* value)
{
    SkeletalMesh = value;
    if (SkeletalMesh == nullptr)
    {
        OverrideMaterials.SetNum(0);
        AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);
    }
    else
    {
        OverrideMaterials.SetNum(value->GetMaterials().Num());
        AABB = FBoundingBox(SkeletalMesh->GetRenderData()->Bounds.min, SkeletalMesh->GetRenderData()->Bounds.max);
    }
}
