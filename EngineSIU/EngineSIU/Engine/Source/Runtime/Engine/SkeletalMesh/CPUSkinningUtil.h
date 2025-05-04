#pragma once

#include "SkeletalMeshTypes.h"
#include "Math/Vector.h"
struct FStaticMeshVertex;

class FCPUSkinningUtil
{
public:
    static void ApplySkinning(
        const TArray<FSkinnedVertex>& InVertices,
        const TArray<FBone>& Bones,
        TArray<FStaticMeshVertex>& OutVertices);

    static void ConvertToStaticVertex(
        const TArray<FSkinnedVertex>& In,
        TArray<FStaticMeshVertex>& Out);
};
