//
// Created by user on 14/05/2026.
//

#pragma once

#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <expected>

#include "Math.h"

template <typename T> 
using TArray = std::vector<T>;

template <typename Key, typename Value> 
using TMap = std::unordered_map<Key, Value>;

template <typename T> 
using TArrayView = std::span<T>;

template <typename T>
using TOptional = std::optional<T>;

template <typename T>
using TDeque = std::deque<T>;

template <typename T, size_t N>
using TStaticArray = std::array<T, N>;

template <typename T, typename ErrorType>
using TExpected = std::expected<T, ErrorType>;

template <typename T>
using TFunction = std::function<T>;

template <typename T>
using TArrayShared = TArray<TSharedPtr<T>>;

using FColor = FVector4;
namespace FLinearColor
{
    constexpr FColor White = FColor(1, 1, 1, 1);
    constexpr FColor Grey = FColor(.66, .66, .66, 1);
    constexpr FColor Black = FColor(0, 0, 0, 1);
    constexpr FColor Magenta = FColor(1, 0, 1, 1);
}

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using FString = std::string;