#include "Components/Mesh/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "EngineLoop.h"
#include "GameFramework/Actor.h"
#include "UObject/Casts.h"
#include "Math/Quat.h"
#include "Engine/FLoaderFBX.h" 

UObject* USkeletalMeshComponent::Duplicate(UObject* InOuter)
{
    ThisClass* NewComponent = Cast<ThisClass>(Super::Duplicate(InOuter));
    // TODO: 이후 애니메이션 상태 등 복사 시 추가 구현
    return NewComponent;
}

void USkeletalMeshComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);

    if (SkeletalMesh)
    {
        FString PathFString = FString(SkeletalMesh->GetRenderData()->ObjectName.c_str());
        OutProperties.Add(TEXT("SkeletalMeshPath"), PathFString);
    }
    else
    {
        OutProperties.Add(TEXT("SkeletalMeshPath"), TEXT("None"));
    }
}

void USkeletalMeshComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);

    const FString* MeshPath = InProperties.Find(TEXT("SkeletalMeshPath"));
    if (MeshPath)
    {
        if (*MeshPath != TEXT("None"))
        {
            if (USkeletalMesh* LoadedMesh = FManagerFBX::CreateSkeletalMesh(*MeshPath))
            {
                SetSkeletalMesh(LoadedMesh);
                UE_LOG(LogLevel::Display, TEXT("Set SkeletalMesh '%s' for %s"), **MeshPath, *GetName());
            }
            else
            {
                UE_LOG(LogLevel::Warning, TEXT("Could not load SkeletalMesh '%s' for %s"), **MeshPath, *GetName());
                SetSkeletalMesh(nullptr);
            }
        }
        else
        {
            SetSkeletalMesh(nullptr);
        }
    }
}

int USkeletalMeshComponent::CheckRayIntersection(const FVector& InRayOrigin, const FVector& InRayDirection, float& OutHitDistance) const
{
    if (!SkeletalMesh) return 0;
    if (!AABB.Intersect(InRayOrigin, InRayDirection, OutHitDistance)) return 0;

    const auto* RenderData = SkeletalMesh->GetRenderData();
    const auto& Vertices = RenderData->Vertices;
    const auto& Indices = RenderData->Indices;

    OutHitDistance = FLT_MAX;
    int HitCount = 0;

    for (int i = 0; i + 2 < Indices.Num(); i += 3)
    {
        const FVector v0(Vertices[Indices[i + 0]].X, Vertices[Indices[i + 0]].Y, Vertices[Indices[i + 0]].Z);
        const FVector v1(Vertices[Indices[i + 1]].X, Vertices[Indices[i + 1]].Y, Vertices[Indices[i + 1]].Z);
        const FVector v2(Vertices[Indices[i + 2]].X, Vertices[Indices[i + 2]].Y, Vertices[Indices[i + 2]].Z);

        float HitDistance = 0.f;
        if (IntersectRayTriangle(InRayOrigin, InRayDirection, v0, v1, v2, HitDistance))
        {
            OutHitDistance = FMath::Min(OutHitDistance, HitDistance);
            ++HitCount;
        }
    }
    return HitCount;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh || !SkeletalMesh->GetRenderData())
        return;

    const auto& RenderData = SkeletalMesh->GetRenderData();
    const auto& Bones = RenderData->Skeleton;

    TArray<FMatrix> BoneMatrices;
    BoneMatrices.SetNum(Bones.Num());

    static float TotalRotation = 0.0f;
    TotalRotation += 30;

    // Y축 회전 쿼터니언 → 행렬
    FQuat QuatRotation = FQuat(FVector(0, 1, 0), FMath::DegreesToRadians(TotalRotation));
    FMatrix Rotation = QuatRotation.ToMatrix();

    for (int i = 0; i < Bones.Num(); ++i)
    {
        if (i == 0)
        {
            BoneMatrices[0] = Bones[0].BindPoseMatrix * Rotation;
        }

        else
        {
            int Parent = Bones[i].ParentIndex;
            BoneMatrices[i] = Bones[i].BindPoseMatrix * BoneMatrices[Parent];
        }
    }

    // 스키닝
    const auto& OriginalVertices = RenderData->OriginalVertices;
    TArray<FBX::FSkeletalMeshVertex> SkinnedVertices;
    ApplyCPUSkinning(OriginalVertices, Bones, BoneMatrices, SkinnedVertices);

    if (!SkinnedVertexBuffer)
    {
        SkinnedVertexBuffer = FEngineLoop::Renderer.CreateDynamicVertexBuffer(TEXT("CPU_SkinnedVB"), SkinnedVertices);
    }
    else
    {
        FEngineLoop::Renderer.UpdateVertexBuffer(SkinnedVertexBuffer, SkinnedVertices.GetData(), SkinnedVertices.Num() * sizeof(FBX::FSkeletalMeshVertex));
    }
}





void USkeletalMeshComponent::ComputeGlobalBoneMatrices(const TArray<FBX::FSkeletonBone>& Bones, TArray<FMatrix>& OutGlobalTransforms)
{
    OutGlobalTransforms.SetNum(Bones.Num());

    for (int i = 0; i < Bones.Num(); ++i)
    {
        const auto& Bone = Bones[i];
        if (Bone.ParentIndex >= 0)
        {
            OutGlobalTransforms[i] = Bones[i].BindPoseMatrix * OutGlobalTransforms[Bone.ParentIndex];
        }
        else
        {
            OutGlobalTransforms[i] = Bones[i].BindPoseMatrix;
        }
    }
}

void USkeletalMeshComponent::ApplyCPUSkinning(const TArray<FBX::FSkeletalMeshVertex>& InVertices,const TArray<FBX::FSkeletonBone>& Bones,const TArray<FMatrix>& BoneMatrices, TArray<FBX::FSkeletalMeshVertex>& OutVertices)
{
    OutVertices = InVertices;

    for (int i = 0; i < InVertices.Num(); ++i)
    {
        const auto& V = InVertices[i];
        FVector SkinnedPos = FVector::ZeroVector;

        for (int j = 0; j < 4; ++j)
        {
            int BoneIdx = V.BoneIndices[j];
            float Weight = V.BoneWeights[j];
            if (Weight <= 0.0f) continue;

            const FMatrix& BoneMatrix = BoneMatrices[BoneIdx];
            const FMatrix& InverseBindPose =FMatrix::Inverse(Bones[BoneIdx].BindPoseMatrix);
            const FMatrix SkinMatrix = BoneMatrix * InverseBindPose;

            FVector LocalPos(V.X, V.Y, V.Z);
            SkinnedPos += SkinMatrix.TransformPosition(LocalPos) * Weight;
        }

        OutVertices[i].X = SkinnedPos.X;
        OutVertices[i].Y = SkinnedPos.Y;
        OutVertices[i].Z = SkinnedPos.Z;
    }
}


