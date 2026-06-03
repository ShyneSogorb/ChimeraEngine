//
// Created by user on 14/05/2026.
//

#pragma once

#include <memory>
#include <cstdio>
#include <malloc.h>
#include <cstdlib>
 
#include "Macros/CompilerAbstractions.h"
#include "Macros/UtilityMacros.h"

#define DEFAULT_ALIGNMENT alignof(double) //Same as int64

namespace FMemory
{

    NO_DISCARD FORCEINLINE void* Memmove(void* Dst, void const* Src, size_t Size)
    {
        return memmove(Dst, Src, Size);
    }
    
    NO_DISCARD FORCEINLINE void* Memset(void* Dst, int Value, size_t Size)
    {
        return memset(Dst, Value, Size);
    }
    
    NO_DISCARD FORCEINLINE void* Memzero(void* Dst, size_t Size)
    {
        return memset(Dst, 0, Size);
    }
    
    FORCEINLINE void Memcpy(void* Dst, void const* Src, size_t Size)
    {
        std::memcpy(Dst, Src, Size);
    }
    
    FORCEINLINE void MemSwap(void* Ptr1, void* Ptr2, size_t Size)
    {
        void* Temp = malloc(Size);
        check(Temp);
        Memcpy(Temp, Ptr1, Size);
        Memcpy(Ptr1, Ptr2, Size);
        Memcpy(Ptr2, Temp, Size);
        free(Temp);
    }
    
    template <typename T>
    FORCEINLINE void MemSwap(T& A, T& B)
    {
        std::swap(A, B);
    }
    
    template <typename T>
    FORCEINLINE void Swap(T& A, T& B)
    {
        if constexpr (
            std::is_trivially_constructible_v<T> &&
            std::is_trivially_destructible_v<T> &&
            std::is_trivially_copyable_v<T>
        )
        {
            MemSwap(&A, &B, sizeof(T));
        }
        else
        {
            T Temp = MoveTemp(A);
            A = MoveTemp(B);
            B = MoveTemp(Temp);
        }
    }
    
    NO_DISCARD FORCEINLINE void* Malloc(size_t Size, size_t Alignment = DEFAULT_ALIGNMENT)
    {
        return _aligned_malloc(Size, Alignment);
    }
    
    FORCEINLINE void Free(void* Ptr)
    {
        _aligned_free(Ptr);
    }
    
    template <typename T = void>
    NO_DISCARD FORCEINLINE T* Realloc(void* Ptr, size_t NewSize, size_t Alignment = DEFAULT_ALIGNMENT)
    {
        return static_cast<T*>(std::realloc(Ptr, NewSize));
    }
    
}


template <typename T>
constexpr std::remove_reference_t<T>&& MoveTemp(T&& Obj) noexcept
{
    static_assert(!std::is_lvalue_reference_v<T>, "MoveTemp does not support lvalues");
    
    using BaseType = std::remove_reference_t<T>;
    
    return static_cast<BaseType&&>(Obj);
}