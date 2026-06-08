
#pragma once
#include <memory>
#include "SmartPointers/StrongPointer.h"

template <typename T>
using TSharedRef = std::shared_ptr<T>;

template <typename T, typename ... Args>
[[nodiscard]] TSharedRef<T> MakeShared(Args&&... args)
{
    return std::make_shared<T>(FWD(args)...);
}