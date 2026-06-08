//
// Created by user on 26/05/2026.
//

#pragma once

#include <string>

using FString = std::string;

namespace eastl
{
    template <>
    struct hash<FString>
    {
        size_t operator()(const FString& Str) const
        {
            return std::hash<std::string>{}(Str);
        }
    };
}