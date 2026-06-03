
#pragma once
#include <memory>

template <typename T>
using TSharedRef = std::shared_ptr<T>;

template <typename T, typename ... Args>
[[nodiscard]] TSharedRef<T> MakeShared(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}