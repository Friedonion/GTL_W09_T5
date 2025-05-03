#include "CPUSkinningUtil.h"

void FCPUSkinningUtil::ApplySkinning(const TArray<FSkinnedVertex>& InVertices,
    const TArray<FBone>& Bones,
    TArray<FVector>& OutSkinnedPositions)
{
    OutSkinnedPositions.SetNum(InVertices.Num());

    for (int i = 0; i < InVertices.Num(); ++i)
    {
        const FSkinnedVertex& Vertex = InVertices[i];
        FVector SkinnedPos = FVector::ZeroVector;

        for (int j = 0; j < 4; ++j)
        {
            int BoneIndex = Vertex.BoneIndices[j];
            float Weight = Vertex.BoneWeights[j];

            if (Weight > 0.0f && BoneIndex >= 0 && BoneIndex < Bones.Num())
            {
                const FMatrix& SkinMatrix = Bones[BoneIndex].GlobalPose * Bones[BoneIndex].GlobalBindPoseInverse;
                FVector Transformed = SkinMatrix.TransformPosition(Vertex.Position);
                SkinnedPos += Transformed * Weight;
            }
        }

        OutSkinnedPositions[i] = SkinnedPos;
    }
}
