
#pragma once
#include <EASTL/array.h>

template <typename InElementType, size_t N>
struct TStaticArray
{
    using ElementType = InElementType;
    constexpr size_t Num() const { return N; }
    
    constexpr TStaticArray() = default;
    
    constexpr TStaticArray(std::initializer_list<ElementType> InitList)
    {
        size_t i = 0;
        for (const auto& Element : InitList)
        {
            if (i >= N) break;
            Container[i++] = Element;
        }
    }
    
    constexpr ElementType& operator[](size_t Index) { return Container[Index]; }
    constexpr const ElementType& operator[](size_t Index) const { return Container[Index]; }
    
    DECLARE_ITERATOR()
    
    constexpr ElementType* GetData() { return Container.data(); }
    constexpr const ElementType* GetData() const { return Container.data(); }
    
private:
    
    eastl::array<ElementType, N> Container;
    
};
