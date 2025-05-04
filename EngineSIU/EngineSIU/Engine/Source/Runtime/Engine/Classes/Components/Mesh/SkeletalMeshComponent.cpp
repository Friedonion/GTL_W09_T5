

#include "SkeletalMeshComponent.h"

#include "SkeletalMesh/CPUSkinningUtil.h"
#include "SkeletalMesh/USkeletalMesh.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
}

void USkeletalMeshComponent::InitializeComponent()
{
    Super::InitializeComponent();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    bIsSkinnedCacheDirty = true; // 다음 프레임에 스킨 갱신
}

void USkeletalMeshComponent::GetSkinnedVertexIndexBuffers(
    TArray<FStaticMeshVertex>& OutVertices, TArray<uint32>& OutIndices) const
{
    if (!SkeletalMesh) return;

    if (bIsSkinnedCacheDirty)
    {
        const auto& InVerts = SkeletalMesh->GetVertices();
        const auto& Bones = SkeletalMesh->GetBones();

        FCPUSkinningUtil::ApplySkinning(InVerts, Bones, CachedSkinnedVertices);
        bIsSkinnedCacheDirty = false;
    }

    OutVertices = CachedSkinnedVertices;
    OutIndices = SkeletalMesh->GetIndices();
}
