//
// Created by user on 25/05/2026.
//
#pragma once

#include "Containers/Array.h"
#include "SmartPointers/SharedPtr.h"

template <typename T>
using TArrayShared = TArray<TSharedPtr<T>>;
