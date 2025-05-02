#pragma once
#include "Components/Mesh/MeshComponent.h"
#include "Mesh/StaticMesh.h"

class UStaticMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

public:
    UStaticMeshComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    
    void GetProperties(TMap<FString, FString>& OutProperties) const override;
    
    void SetProperties(const TMap<FString, FString>& InProperties) override;

    void SetselectedSubMeshIndex(const int& value) { selectedSubMeshIndex = value; }
    int GetselectedSubMeshIndex() const { return selectedSubMeshIndex; };

    virtual uint32 GetNumMaterials() const override;
    virtual UMaterial* GetMaterial(uint32 ElementIndex) const override;
    virtual uint32 GetMaterialIndex(FName MaterialSlotName) const override;
    virtual TArray<FName> GetMaterialSlotNames() const override;
    virtual void GetUsedMaterials(TArray<UMaterial*>& Out) const override;

    virtual int CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const override;
    
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }
    void SetStaticMesh(UStaticMesh* value)
    { 
        StaticMesh = value;
        if (StaticMesh == nullptr)
        {
            OverrideMaterials.SetNum(0);
            AABB = FBoundingBox(FVector::ZeroVector, FVector::ZeroVector);
        }
        else
        {
            OverrideMaterials.SetNum(value->GetMaterials().Num());
            AABB = FBoundingBox(StaticMesh->GetRenderData()->BoundingBoxMin, StaticMesh->GetRenderData()->BoundingBoxMax);
        }
    }

protected:
    UStaticMesh* StaticMesh = nullptr;
    int selectedSubMeshIndex = -1;
};
