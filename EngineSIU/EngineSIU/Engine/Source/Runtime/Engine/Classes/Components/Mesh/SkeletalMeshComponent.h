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
    virtual void TickComponent(float DeltaTime) override;

    void ComputeGlobalBoneMatrices(const TArray<FBX::FSkeletonBone>& Bones, TArray<FMatrix>& OutGlobalTransforms);
   
    void ApplyCPUSkinning(const TArray<FBX::FSkeletalMeshVertex>& InVertices,const TArray<FBX::FSkeletonBone>& Bones,const TArray<FMatrix>& BoneMatrices,TArray<FBX::FSkeletalMeshVertex>& OutVertices);
    
    ID3D11Buffer* SkinnedVertexBuffer = nullptr;

};
