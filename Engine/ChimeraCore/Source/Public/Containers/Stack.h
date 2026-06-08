//
// Created by user on 08/06/2026.
//

#pragma once

#include "Array.h"
#include "ReverseIterator.h"

template <typename T>
struct TStack
{
    
    FORCEINLINE void Push(const T& Item)
    {
        Data.Add(Item);
    }
    
    template <typename ...Args>
    FORCEINLINE void Emplace(Args&&... args)
    {
        Data.Emplace(std::forward<Args>(args)...);
    }
    
    FORCEINLINE decltype(auto) Peek(this auto&& Self)
    {
        return FWD(Self).Data.LastElement();
    }
    
    FORCEINLINE T Remove()
    {
        return MoveTemp(Data.PopElement());
    }
    
    FORCEINLINE decltype(auto) begin(this auto&& Self) { return FWD(Self).Data.rbegin(); }
    FORCEINLINE decltype(auto) end(this auto&& Self) { return FWD(Self).Data.rend(); }
    
    FORCEINLINE decltype(auto) rbegin(this auto&& Self) { return FWD(Self).Data.begin(); }
    FORCEINLINE decltype(auto) rend(this auto&& Self) { return FWD(Self).Data.end(); }
    
    FORCEINLINE decltype(auto) Reverse(this auto&& Self) { return TReverseIterator{FWD(Self).Data}; }
    
    FORCEINLINE void Empty()
    {
        Data.Empty();
    }
    
    template <typename Predicate>
    FORCEINLINE void Flush(Predicate&& predicate)
    {
        for (auto&& Element : *this)
        {
            predicate(Element);
        }
        Empty();
    }
    
    FORCEINLINE void Reserve(size_t Count)
    {
        Data.Reserve(Count);
    }
    
private:
    
    TArray<T> Data;
    
};
