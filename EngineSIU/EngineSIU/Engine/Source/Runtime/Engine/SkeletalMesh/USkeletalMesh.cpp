#include "USkeletalMesh.h"
#include "D3D11RHI/GraphicDevice.h"
#include "EngineLoop.h"
#include "SkeletalMeshTypes.h"
#include "Components/Material/Material.h"
#include "Container/MapUtils.h"
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
    for (UMaterial* Mat : InData.Materials)
    {
        if (!Mat) continue;

        FStaticMaterial* NewSlot = new FStaticMaterial();
        NewSlot->Material = Mat;
        NewSlot->MaterialSlotName = Mat->GetMaterialInfo().MaterialName;
        Materials.Add(NewSlot);
    }

    UE_LOG(LogLevel::Display, TEXT("SkeletalMesh Loaded: %d verts, %d indices, %d bones"), Vertices.Num(), Indices.Num(), Bones.Num());

    if (Vertices.Num() > 0)
    {
        CPUVertexBuffer = FEngineLoop::Renderer.CreateDynamicVertexBuffer(TEXT("SkeletalMesh_CPU"), Vertices);
    }

    if (Indices.Num() > 0)
    {
        CPUIndexBuffer = FEngineLoop::Renderer.CreateImmutableIndexBuffer(TEXT("SkeletalMesh_CPU"), Indices);
    }

    if (!RenderData)
        RenderData = new OBJ::FStaticMeshRenderData();

    // Vertex 변환
    RenderData->Vertices.Reserve(Vertices.Num());
    for (const FSkinnedVertex& V : Vertices)
    {
        FStaticMeshVertex SV;
        SV.X = V.Position.X;
        SV.Y = V.Position.Y;
        SV.Z = V.Position.Z;

        SV.R = 1.0f; SV.G = 1.0f; SV.B = 1.0f; SV.A = 1.0f;

        SV.NormalX = 0.0f; SV.NormalY = 0.0f; SV.NormalZ = 1.0f;
        SV.TangentX = 1.0f; SV.TangentY = 0.0f; SV.TangentZ = 0.0f;

        SV.U = V.UV.X;
        SV.V = V.UV.Y;
        SV.MaterialIndex = V.MaterialIndex;

        RenderData->Vertices.Add(SV);
    }

    RenderData->VertexBuffer = CPUVertexBuffer;
    RenderData->IndexBuffer = CPUIndexBuffer;

    RenderData->Materials.Empty();
    for (const FStaticMaterial* M : Materials)
    {
        if (M && M->Material)
        {
            RenderData->Materials.Add(M->Material->GetMaterialInfo());
        }
    }

    // MaterialIndex 기반 Submesh 분리
    TMap<uint32, TArray<uint32>> SubmeshIndexMap;
    for (int32 i = 0; i < Indices.Num(); i += 3)
    {
        uint32 Index0 = Indices[i];
        uint32 MatIndex = 0;
        if (RenderData->Vertices.IsValidIndex(Index0))
        {
            MatIndex = RenderData->Vertices[Index0].MaterialIndex;
        }

        TArray<uint32>& FaceList = SubmeshIndexMap.FindOrAdd(MatIndex);
        FaceList.Add(Indices[i]);
        FaceList.Add(Indices[i + 1]);
        FaceList.Add(Indices[i + 2]);
    }

    // 키 수집 및 수동 정렬
    TArray<uint32> SortedKeys = GetMapKeys(SubmeshIndexMap);
    SortedKeys.Sort([](uint32 A, uint32 B)
        {
            return A < B;
        });

    // 인덱스 및 서브셋 적용
    RenderData->Indices.Empty();
    RenderData->MaterialSubsets.Empty();
    for (uint32 MatIndex : SortedKeys)
    {
        const TArray<uint32>& FaceIndices = *SubmeshIndexMap.Find(MatIndex);

        FMaterialSubset Subset;
        Subset.MaterialIndex = MatIndex;
        Subset.IndexStart = RenderData->Indices.Num();
        Subset.IndexCount = FaceIndices.Num();

        for (uint32 Index : FaceIndices)
        {
            RenderData->Indices.Add(Index);
        }

        RenderData->MaterialSubsets.Add(Subset);
    }
}
