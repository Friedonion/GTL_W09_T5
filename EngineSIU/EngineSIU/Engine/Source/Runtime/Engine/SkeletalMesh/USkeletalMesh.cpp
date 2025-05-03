#include "USkeletalMesh.h"
#include "D3D11RHI/GraphicDevice.h"
#include "EngineLoop.h"
#include "SkeletalMeshTypes.h"
#include "Engine/FLoaderOBJ.h"

USkeletalMesh::USkeletalMesh() {}

USkeletalMesh::~USkeletalMesh()
{
    if (CPUVertexBuffer)
    {
        CPUVertexBuffer->Release();
        CPUVertexBuffer = nullptr;
    }

    if (CPUIndexBuffer)
    {
        CPUIndexBuffer->Release();
        CPUIndexBuffer = nullptr;
    }
}

UObject* USkeletalMesh::Duplicate(UObject* InOuter)
{
    return nullptr; // TODO: 필요 시 구현
}

void USkeletalMesh::SetSkeletalMeshData(const FSkeletalMeshData& InData)
{
    Vertices = InData.Vertices;
    Indices = InData.Indices;
    Bones = InData.Bones;

    Materials.Empty();

    UE_LOG(LogLevel::Display, TEXT("SkeletalMesh Loaded: %d verts, %d indices, %d bones"), Vertices.Num(), Indices.Num(), Bones.Num());

    // GPU VertexBuffer 생성 (CPU Skinning 결과로 만들어진 버텍스를 전달받는 용도)
    if (Vertices.Num() > 0)
    {
        CPUVertexBuffer = FEngineLoop::Renderer.CreateDynamicVertexBuffer(TEXT("SkeletalMesh_CPU"), Vertices);
    }

    if (Indices.Num() > 0)
    {
        CPUIndexBuffer = FEngineLoop::Renderer.CreateImmutableIndexBuffer(TEXT("SkeletalMesh_CPU"), Indices);
    }
}
