//
// Created by user on 14/05/2026.
//

#pragma once

#include <memory>

namespace FMemory
{
    inline void* Memcpy(void* Dst, void const* Src, size_t Size)
    {
        return memcpy(Dst, Src, Size);
    }
}

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T>
using TSharedRef = std::shared_ptr<T>;

template <typename T, typename ... Args>
TSharedRef<T> MakeShared(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T>
using TWeakPtr = std::weak_ptr<T>;

template <typename T>
auto MoveTemp(T&& Obj)
{
    return std::move(Obj);
}