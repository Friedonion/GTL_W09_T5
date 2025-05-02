#include "Components/Mesh/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "EngineLoop.h"
#include "GameFramework/Actor.h"
#include "UObject/Casts.h"
#include "Engine/FLoaderFBX.h" 

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    // TODO: 이후 애니메이션 상태 등 복사 시 추가 구현
    return NewComponent;
}

void USkeletalMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);

    if (SkeletalMesh)
    {
        FString PathFString = FString(SkeletalMesh->GetRenderData()->ObjectName.c_str());
        OutProperties.Add(TEXT("SkeletalMeshPath"), PathFString);
    }
    else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None"));
    }
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);

    const FString* MeshPath = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (MeshPath)
    {
        if (*MeshPath != TEXT("None"))
        {
            if (USkeletalMesh* LoadedMesh = FManagerFBX::CreateSkeletalMesh(*MeshPath))
            {
                SetSkeletalMesh(LoadedMesh);
                UE_LOG(LogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **MeshPath, *GetName());
            }
            else
            {
                UE_LOG(LogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **MeshPath, *GetName());
                SetSkeletalMesh(nullptr);
            }
        }
        else
        {
            SetSkeletalMesh(nullptr);
        }
    }
}

int USkeletalMeshComponent::CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const
{
    if (!SkeletalMesh) return 0;
    if (!AABB.Intersect(InRayOrigin, InRayDirection, OutHitDistance)) return 0;

    const auto* RenderData = SkeletalMesh->GetRenderData();
    const auto& Vertices = RenderData->Vertices;
    const auto& Indices = RenderData->Indices;

    OutHitDistance = FLT_MAX;
    int HitCount = 0;

    for (int i = 0; i + 2 < Indices.Num(); i += 3)
    {
        const FVector v0(Vertices[Indices[i + 0]].X, Vertices[Indices[i + 0]].Y, Vertices[Indices[i + 0]].Z);
        const FVector v1(Vertices[Indices[i + 1]].X, Vertices[Indices[i + 1]].Y, Vertices[Indices[i + 1]].Z);
        const FVector v2(Vertices[Indices[i + 2]].X, Vertices[Indices[i + 2]].Y, Vertices[Indices[i + 2]].Z);

        float HitDistance = 0.f;
        if (IntersectRayTriangle(InRayOrigin, InRayDirection, v0, v1, v2, HitDistance))
        {
            OutHitDistance = FMath::Min(OutHitDistance, HitDistance);
            ++HitCount;
        }
    }
    return HitCount;
}
