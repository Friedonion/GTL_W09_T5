#pragma once
#include "Array.h"
#include "Map.h"

template<typename KeyType, typename ValueType>
TArray<KeyType> GetMapKeys(const TMap<KeyType, ValueType>& Map)
{
    TArray<KeyType> Result;
    Result.Reserve(Map.Num());

    for (auto It = Map.GetContainerPrivate().begin(); It != Map.GetContainerPrivate().end(); ++It)
    {
        Result.Add(It->first);
    }
    return Result;
}
