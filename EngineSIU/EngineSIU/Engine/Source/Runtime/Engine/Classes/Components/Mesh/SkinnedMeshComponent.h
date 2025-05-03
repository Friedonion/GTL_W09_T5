#pragma once

#include "Components/MeshComponent.h"

class USkeletalMesh;

class USkinnedMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

public:
    USkinnedMeshComponent() = default;

    void SetSkeletalMesh(USkeletalMesh* InMesh) { SkeletalMesh = InMesh; }
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

protected:
    USkeletalMesh* SkeletalMesh = nullptr;
};
