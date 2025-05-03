#pragma once

#include "SkinnedMeshComponent.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
    USkeletalMeshComponent();

    virtual void TickComponent(float DeltaTime) override;
    virtual void InitializeComponent() override;

    // RenderPass가 호출
    void GetSkinnedVertexIndexBuffers(TArray<FVector>& OutVertices, TArray<uint32>& OutIndices) const;

private:
    mutable TArray<FVector> CachedSkinnedPositions; // 렌더패스에서 매 프레임 호출 시 계산되게
    mutable bool bIsSkinnedCacheDirty = true;
};
