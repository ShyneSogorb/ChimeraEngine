
#pragma once
#include <memory>

#include "StrongPointer.h"

template <typename T>
using TSharedPtr = std::shared_ptr<T>;
