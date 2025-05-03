#pragma once

#include "SkeletalMeshTypes.h"

class FString;
class USkeletalMesh;

class FSkeletalMeshLoader
{
public:
    static USkeletalMesh* LoadFromFBX(const FString& FilePath);
};
