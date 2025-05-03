#include "FFBXImporter.h"
#include "SkeletalMeshLoader.h"

USkeletalMesh* FFBXImporter::LoadSkeletalMesh(const FString& FilePath)
{
    return FSkeletalMeshLoader::LoadFromFBX(FilePath);
}
