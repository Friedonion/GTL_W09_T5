#include "SkinnedMeshComponent.h"
#include "Components/Material/Material.h"
UMaterial* USkinnedMeshComponent::GetMaterial(uint32 ElementIndex) const
{
    if (SkeletalMesh)
    {
        if (OverrideMaterials.IsValidIndex(ElementIndex) && OverrideMaterials[ElementIndex])
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

uint32 USkinnedMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
    return SkeletalMesh ? SkeletalMesh->GetMaterialIndex(MaterialSlotName) : INDEX_NONE;
}

TArray<FName> USkinnedMeshComponent::GetMaterialSlotNames() const
{
    TArray<FName> Names;
    if (!SkeletalMesh) return Names;

    for (const FStaticMaterial* Mat : SkeletalMesh->GetMaterials())
    {
        Names.Add(Mat->MaterialSlotName);
    }
    return Names;
}

void USkinnedMeshComponent::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    if (!SkeletalMesh) return;
    SkeletalMesh->GetUsedMaterials(Out);
    for (int i = 0; i < OverrideMaterials.Num(); ++i)
    {
        if (OverrideMaterials[i])
        {
            Out[i] = OverrideMaterials[i];
        }
    }
}
