
#include "SkeletalMesh.h"
#include "Engine/FLoaderFBX.h"
#include "Engine/FLoaderOBJ.h"
#include "UObject/ObjectFactory.h"
#include "EngineLoop.h"

USkeletalMesh::~USkeletalMesh()
{
    if (!SkeletalMeshRenderData) return;
    if (SkeletalMeshRenderData->Vertices.Num() == 0) return;
    if (SkeletalMeshRenderData->VertexBuffer)
    {
        SkeletalMeshRenderData->VertexBuffer->Release();
        SkeletalMeshRenderData->VertexBuffer = nullptr;
    }
    if (SkeletalMeshRenderData->IndexBuffer)
    {
        SkeletalMeshRenderData->IndexBuffer->Release();
        SkeletalMeshRenderData->IndexBuffer = nullptr;
    }
}

UObject* USkeletalMesh::Duplicate(UObject* InOuter)
{
    return nullptr; // TODO: 리소스 복제 시 구현
}

void USkeletalMesh::SetData(FBX::FSkeletalMeshRenderData* InData)
{
    SkeletalMeshRenderData = InData;

    if (!InData || InData->Vertices.Num() == 0) return;
    InData->VertexBuffer = FEngineLoop::Renderer.CreateImmutableVertexBuffer(InData->DisplayName, InData->Vertices);
    InData->IndexBuffer = FEngineLoop::Renderer.CreateImmutableIndexBuffer(InData->DisplayName, InData->Indices);

    for (const auto& MatInfo : InData->Materials)
    {
        FStaticMaterial* NewSlot = new FStaticMaterial();
        UMaterial* NewMaterial = FManagerOBJ::CreateMaterial(MatInfo);
        NewSlot->Material = NewMaterial;
        NewSlot->MaterialSlotName = MatInfo.MaterialName;
        Materials.Add(NewSlot);
    }
}

uint32 USkeletalMesh::GetMaterialIndex(FName MaterialSlotName) const
{
    for (uint32 i = 0; i < Materials.Num(); ++i)
    {
        if (Materials[i]->MaterialSlotName == MaterialSlotName)
            return i;
    }
    return INDEX_NONE;
}

void USkeletalMesh::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    for (auto* MatSlot : Materials)
    {
        Out.Add(MatSlot->Material);
    }
}
