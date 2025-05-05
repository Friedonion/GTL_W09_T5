#pragma once
#include "GameFramework/Actor.h"

class USkeletalMeshComponent;
class USkeletalMesh;

class ASkeletalMeshActor : public AActor
{
    DECLARE_CLASS(ASkeletalMeshActor, AActor)

public:
    ASkeletalMeshActor();

    virtual void Tick(float DeltaTime) override;
    virtual UObject* Duplicate(UObject* InOuter) override;

    USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

protected:
    UPROPERTY(USkeletalMeshComponent*, SkeletalMeshComponent, = nullptr);
};
