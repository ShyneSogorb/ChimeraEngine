
#pragma once

#include <span>
#include "Array.h"


template <typename InElementType> 
struct TArrayView
{
    using ElementType = InElementType;
    
    TArrayView() = default;
    
    TArrayView(ElementType* InData, size_t InSize) : Data(InData), Size(InSize) {}
    
    TArrayView(std::span<ElementType> Span) : Data(Span.data()), Size(Span.size()) {}
    
    template <typename RangeType>
    TArrayView(RangeType&& Range) : Data(Range.GetData()), Size(Range.Num()) {}
    
    ElementType& operator[](size_t Index) { return Data[Index]; }
    
    const ElementType& operator[](size_t Index) const { return Data[Index]; }
    
    size_t Num() const { return Size; }
    size_t GetSize() const { return Size; }
    
    auto begin() const { return TArray<ElementType>::template TConstIterator<ElementType>(Data); }
    auto end() const { return TArray<ElementType>::template TConstIterator<ElementType>(Data + Size); }
    
    auto begin() { return TArray<ElementType>::template TIterator<ElementType>(Data); }
    auto end() { return TArray<ElementType>::template TIterator<ElementType>(Data + Size); }
    
    ElementType* GetData() { return Data; }
    const ElementType* GetData() const { return Data; }
    
private:
    
    ElementType* Data{ nullptr };
    size_t Size{ 0 };
    
}; 

