
#pragma once
#include <array>

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
            Data[i++] = Element;
        }
    }
    
    constexpr ElementType& operator[](size_t Index) { return Data[Index]; }
    constexpr const ElementType& operator[](size_t Index) const { return Data[Index]; }
    
    constexpr auto begin() { return Data.begin(); }
    constexpr auto end() { return Data.begin(); }
    
    constexpr auto begin() const { return Data.begin(); }
    constexpr auto end() const { return Data.begin(); }
    
    constexpr ElementType* GetData() { return Data; }
    constexpr const ElementType* GetData() const { return Data; }
    
private:
    
    ElementType Data[N];
    
};
