#pragma once
#pragma once

class FString;
class USkeletalMesh;

class FFBXImporter
{
public:
    static USkeletalMesh* LoadSkeletalMesh(const FString& FilePath);
};
