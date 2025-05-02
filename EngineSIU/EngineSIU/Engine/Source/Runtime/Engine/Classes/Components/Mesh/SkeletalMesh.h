#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"
#include "Engine/FLoaderFBX.h"

class USkeletalMesh : public UObject
{
    DECLARE_CLASS(USkeletalMesh, UObject)

public:
    USkeletalMesh() = default;
    virtual ~USkeletalMesh() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

    void SetData(FBX::FSkeletalMeshRenderData* InData);
    FBX::FSkeletalMeshRenderData* GetRenderData() const { return SkeletalMeshRenderData; }

    const TArray<FStaticMaterial*>& GetMaterials() const { return Materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;

private:
    FBX::FSkeletalMeshRenderData* SkeletalMeshRenderData = nullptr;
    TArray<FStaticMaterial*> Materials;
};
