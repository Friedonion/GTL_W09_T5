#include "USkeletalMesh.h"
#include "D3D11RHI/GraphicDevice.h"
#include "EngineLoop.h"
#include "SkeletalMeshTypes.h"
#include "Components/Material/Material.h"
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
        if (!Mat)
            continue;

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

    // 추가: StaticMeshRenderData 구조로 변환
    if (!RenderData)
        RenderData = new OBJ::FStaticMeshRenderData();

    RenderData->Vertices.Reserve(Vertices.Num());
    for (const FSkinnedVertex& V : Vertices)
    {
        FStaticMeshVertex SV;
        SV.X = V.Position.X;
        SV.Y = V.Position.Y;
        SV.Z = V.Position.Z;

        // 기본 색상 값 설정 (옵션)
        SV.R = 1.0f;
        SV.G = 1.0f;
        SV.B = 1.0f;
        SV.A = 1.0f;

        // 노말, 탄젠트 (없다면 0으로 초기화)
        SV.NormalX = 0.0f;
        SV.NormalY = 0.0f;
        SV.NormalZ = 1.0f;
        SV.TangentX = 1.0f;
        SV.TangentY = 0.0f;
        SV.TangentZ = 0.0f;

        SV.U = V.UV.X;
        SV.V = V.UV.Y;

        SV.MaterialIndex = 0; // 단일 머티리얼 기준

        RenderData->Vertices.Add(SV);
    }

    RenderData->Indices = Indices;
    RenderData->VertexBuffer = CPUVertexBuffer;
    RenderData->IndexBuffer = CPUIndexBuffer;
    RenderData->Materials.Reserve(Materials.Num());
    RenderData->MaterialSubsets.Empty();
    if (!Indices.IsEmpty())
    {
        FMaterialSubset Subset;
        Subset.MaterialIndex = 0;
        Subset.IndexStart = 0;
        Subset.IndexCount = Indices.Num();
        RenderData->MaterialSubsets.Add(Subset);
    }

    for (const FStaticMaterial* M : Materials)
    {
        if (M && M->Material)
        {
            RenderData->Materials.Add(M->Material->GetMaterialInfo());
        }
    }
}
