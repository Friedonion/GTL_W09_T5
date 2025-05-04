#pragma once

#include "SkinnedMeshComponent.h"

struct FSkinnedVertex;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    USkeletalMeshComponent();

    virtual void TickComponent(float DeltaTime) override;
    virtual void InitializeComponent() override;

    // RenderPass가 호출
    void GetSkinnedVertexIndexBuffers(
        TArray<FStaticMeshVertex>& OutVertices, TArray<uint32>& OutIndices) const;

    int GetSelectedSubMeshIndex() const { return SelectedSubMeshIndex; }
    void SetSelectedSubMeshIndex(int Index) { SelectedSubMeshIndex = Index; }
private:
    mutable mutable TArray<FStaticMeshVertex> CachedSkinnedVertices;
    
    mutable bool bIsSkinnedCacheDirty = true;
    int SelectedSubMeshIndex = 0;
};
