#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Define.h"

struct FSkeletalMeshData;
struct FSkinnedVertex;
struct FBone;
class UMaterial;
class FSkinnedVertexBuffer;

class USkeletalMesh : public UObject
{
    DECLARE_CLASS(USkeletalMesh, UObject)

public:
    USkeletalMesh();
    virtual ~USkeletalMesh() override;

    virtual UObject* Duplicate(UObject* InOuter) override;

    void SetSkeletalMeshData(const FSkeletalMeshData& InData);

    const TArray<FSkinnedVertex>& GetVertices() const { return Vertices; }
    const TArray<uint32>& GetIndices() const { return Indices; }
    const TArray<FBone>& GetBones() const { return Bones; }

    const TArray<FStaticMaterial*>& GetUsedMaterials() const { return Materials; }

    ID3D11Buffer* GetCPUVertexBuffer() const { return CPUVertexBuffer; }
    ID3D11Buffer* GetCPUIndexBuffer() const { return CPUIndexBuffer; }

    OBJ::FStaticMeshRenderData* GetRenderData() const { return RenderData; }



private:
    TArray<FSkinnedVertex> Vertices;
    TArray<uint32> Indices;
    TArray<FBone> Bones;

    TArray<FStaticMaterial*> Materials;

    ID3D11Buffer* CPUVertexBuffer = nullptr;
    ID3D11Buffer* CPUIndexBuffer = nullptr;
    OBJ::FStaticMeshRenderData* RenderData = nullptr;
};
