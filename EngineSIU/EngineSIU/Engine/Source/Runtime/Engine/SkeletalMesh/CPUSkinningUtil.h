#pragma once

#include "SkeletalMeshTypes.h"
#include "Math/Vector.h"

class FCPUSkinningUtil
{
public:
    static void ApplySkinning(const TArray<FSkinnedVertex>& InVertices,
        const TArray<FBone>& Bones,
        TArray<FVector>& OutSkinnedPositions);
};
