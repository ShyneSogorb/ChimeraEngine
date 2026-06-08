
#pragma once

#include <EASTL/span.h>
#include "Containers/Array.h"

template <typename InElementType> 
struct TArrayView
{
    using ElementType = InElementType;

    TArrayView() = default;
    TArrayView(ElementType* InData, size_t InSize) : Span(InData, InSize) {}
    TArrayView(const TArrayView& Other) : Span(Other.Span) {}
    TArrayView(const TArray<ElementType>& Array) : Span(Array.GetData(), Array.Num()) {}
    TArrayView(TArray<ElementType>& Array) : Span(Array.GetData(), Array.Num()) {}
    
    TArrayView(std::initializer_list<ElementType> List) : Span(List) {}
    
    ElementType& operator[](size_t Index) { return Span[Index]; }
    
    const ElementType& operator[](size_t Index) const { return Span[Index]; }
    
    auto Num() const { return Span.size(); }
    auto GetSize() const { return Num(); }
    
    decltype(auto) begin(this auto&& Self) { return FWD(Self).Span.begin(); }
    decltype(auto) end(this auto&& Self) { return FWD(Self).Span.end(); }
    
    decltype(auto) GetData(this auto&& Self) { return FWD(Self).Span.data(); }
    
private:
    
    eastl::span<ElementType> Span;
    
}; 

