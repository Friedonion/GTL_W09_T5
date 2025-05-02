#pragma once
#include "Components/Mesh/SkinnedMeshComponent.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    USkeletalMeshComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;

    virtual int CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const override;

    // TODO: 애니메이션 기능 확장 예정
};
