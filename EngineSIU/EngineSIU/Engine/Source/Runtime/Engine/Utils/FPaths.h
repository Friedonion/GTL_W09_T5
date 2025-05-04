#pragma once
#include "Container/String.h"


namespace FPaths
{
    /**
     * 디렉토리 경로와 파일명을 결합하여 전체 경로 문자열을 반환합니다.
     * 예: Combine("C:/Folder", "file.txt") -> "C:/Folder/file.txt"
     */
    inline FString Combine(const FString& Dir, const FString& Filename)
    {
        if (Dir.IsEmpty()) return Filename;
        if (Dir[Dir.Len() - 1] == '/' || Dir[Dir.Len() - 1] == '\\')
        {
            return Dir + Filename;
        }
        return Dir + TEXT("/") + Filename;
    }

    /**
     * 전체 경로 문자열에서 디렉토리 경로만 반환합니다.
     * 예: GetPath("C:/Folder/file.txt") -> "C:/Folder"
     */
    inline FString GetPath(const FString& InPath)
    {
        int32 SlashIndex = InPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (SlashIndex == -1)
        {
            SlashIndex = InPath.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        }
        if (SlashIndex == -1)
        {
            return TEXT("");
        }
        InPath.RightChop(0).Resize(SlashIndex);
        return InPath;
    }

    /**
     * 전체 경로 문자열에서 파일 이름만 반환합니다.
     * 예: GetCleanFilename("C:/Folder/file.txt") -> "file.txt"
     */
    inline FString GetCleanFilename(const FString& InPath)
    {
        int32 SlashIndex = InPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (SlashIndex == -1)
        {
            SlashIndex = InPath.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        }
        if (SlashIndex == -1)
        {
            return InPath;
        }
        return InPath.RightChop(SlashIndex + 1);
    }
}
