#pragma once
#include "Components/PrimitiveComponent.h"
#include "Components/Material/Material.h"

class UMeshComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)
public:
    UMeshComponent() = default;

    virtual UObject* Duplicate(UObject* InOuter) override;

    virtual void GetProperties(TMap<FString, FString>& OutProperties) const override;
    virtual void SetProperties(const TMap<FString, FString>& InProperties) override;
    
#pragma region Material
    virtual uint32 GetNumMaterials() const { return 0; }
    virtual UMaterial* GetMaterial(uint32 ElementIndex) const;
    virtual uint32 GetMaterialIndex(FName MaterialSlotName) const;
    virtual UMaterial* GetMaterialByName(FName MaterialSlotName) const;
    virtual TArray<FName> GetMaterialSlotNames() const;
    virtual void SetMaterial(uint32 ElementIndex, UMaterial* Material);
    virtual void SetMaterialByName(FName MaterialSlotName, UMaterial* Material);
    virtual void GetUsedMaterials(TArray<UMaterial*>& Out) const;
#pragma endregion
protected:
    TArray<UMaterial*> OverrideMaterials;
public:
    TArray<UMaterial*>& GetOverrideMaterials() { return OverrideMaterials; }
};

