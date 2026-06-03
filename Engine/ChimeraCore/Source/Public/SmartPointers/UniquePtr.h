
#pragma once
#include <memory>

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

template <typename T, typename... Args>
[[nodiscard]] TUniquePtr<T> MakeUnique(Args&&... args)
{
    return std::make_unique<T>(std::forward<T>(args)...);
}