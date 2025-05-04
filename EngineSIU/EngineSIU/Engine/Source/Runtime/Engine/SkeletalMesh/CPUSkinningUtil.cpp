#include "CPUSkinningUtil.h"

#include "Define.h"

void FCPUSkinningUtil::ApplySkinning(
    const TArray<FSkinnedVertex>& InVertices,
    const TArray<FBone>& Bones,
    TArray<FStaticMeshVertex>& OutVertices)
{
    OutVertices.SetNum(InVertices.Num());

    for (int i = 0; i < InVertices.Num(); ++i)
    {
        const FSkinnedVertex& In = InVertices[i];
        FStaticMeshVertex& Out = OutVertices[i];

        FVector SkinnedPos = FVector::ZeroVector;
        float TotalWeight = 0.0f;

        for (int j = 0; j < 4; ++j)
        {
            int BoneIndex = In.BoneIndices[j];
            float Weight = In.BoneWeights[j];

            if (Weight > 0.0f && BoneIndex >= 0 && BoneIndex < Bones.Num())
            {
                const FMatrix& SkinMatrix = Bones[BoneIndex].GlobalPose * Bones[BoneIndex].GlobalBindPoseInverse;
                SkinnedPos += SkinMatrix.TransformPosition(In.Position) * Weight;
                TotalWeight += Weight;
            }
        }

        Out.X = (TotalWeight > 0) ? SkinnedPos.X : In.Position.X;
        Out.Y = (TotalWeight > 0) ? SkinnedPos.Y : In.Position.Y;
        Out.Z = (TotalWeight > 0) ? SkinnedPos.Z : In.Position.Z;

        Out.NormalX = In.Normal.X;
        Out.NormalY = In.Normal.Y;
        Out.NormalZ = In.Normal.Z;

        Out.TangentX = In.Tangent.X;
        Out.TangentY = In.Tangent.Y;
        Out.TangentZ = In.Tangent.Z;

        Out.U = In.UV.X;
        Out.V = In.UV.Y;

        Out.R = Out.G = Out.B = 1.0f; Out.A = 1.0f; // 임시
        Out.MaterialIndex = In.MaterialIndex;
    }
}
void FCPUSkinningUtil::ConvertToStaticVertex(
    const TArray<FSkinnedVertex>& In,
    TArray<FStaticMeshVertex>& Out)
{
    Out.SetNum(In.Num());
    for (int32 i = 0; i < In.Num(); ++i)
    {
        const FSkinnedVertex& SV = In[i];
        FStaticMeshVertex& V = Out[i];

        V.X = SV.Position.X;
        V.Y = SV.Position.Y;
        V.Z = SV.Position.Z;

        V.R = 1.0f; V.G = 1.0f; V.B = 1.0f; V.A = 1.0f;

        V.NormalX = SV.Normal.X;
        V.NormalY = SV.Normal.Y;
        V.NormalZ = SV.Normal.Z;

        V.TangentX = SV.Tangent.X;
        V.TangentY = SV.Tangent.Y;
        V.TangentZ = SV.Tangent.Z;

        V.U = SV.UV.X;
        V.V = SV.UV.Y;

        V.MaterialIndex = 0;
    }
}
