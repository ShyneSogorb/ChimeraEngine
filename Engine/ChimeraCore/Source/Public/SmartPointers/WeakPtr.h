
#pragma once
#include <memory>
#include "SmartPointers/StrongPointer.h"

template <typename T>
using TWeakPtr = std::weak_ptr<T>;