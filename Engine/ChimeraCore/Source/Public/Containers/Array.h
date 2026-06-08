
#pragma once

#include <EASTL/vector.h>

#include "ContainersHelper.h"
#include "ReverseIterator.h"
#include "Libraries/Memory.h"
#include "Macros/BuildAbstractions.h"
#include "Macros/CompilerAbstractions.h"

template <typename InElementType> 
struct TArray
{
    using ElementType = InElementType;

private:
    
    eastl::vector<InElementType> Container;
    
    static constexpr bool TriviallyCopyable =  std::is_trivially_copyable_v<ElementType>;
    static constexpr bool TriviallyDestructible = std::is_trivially_destructible_v<ElementType>;
    static constexpr bool TriviallyMoveConstructible = std::is_trivially_move_constructible_v<ElementType>;
    static constexpr bool TriviallyMoveAssignable = std::is_trivially_move_assignable_v<ElementType>;
    static constexpr bool TriviallyCopyConstructible = std::is_trivially_copy_constructible_v<ElementType>;
    static constexpr bool TriviallyCopyAssignable = std::is_trivially_copy_assignable_v<ElementType>;
    
    static constexpr size_t ElementSize = sizeof(ElementType);
    static constexpr size_t ElementAlignment = alignof(ElementType);
    
       
public:
    
    DECLARE_CONTAINER_CONSTRUCTORS(TArray)
    
    DECLARE_ITERATOR()
    DECLARE_CONTAINER_ASSIGNMENT_OPERATORS(TArray)
    
    NO_DISCARD FORCEINLINE size_t Num() const { return Container.size(); }
    NO_DISCARD FORCEINLINE size_t LastIndex() const { return Num() - 1; }
    NO_DISCARD FORCEINLINE decltype(auto) Get(this auto&& Self,size_t Index){ return FWD(Self).Container[Index]; }
    NO_DISCARD FORCEINLINE decltype(auto) LastElement(this auto&& Self) { return FWDL(Self, Self.Get(Self.LastIndex())); }
    NO_DISCARD FORCEINLINE size_t GetCapacity() const { return Container.capacity(); }
    
    void Add(const ElementType& Element)
    {
        Container.push_back(Element);
    }
    
    template <typename... Args>
    void Emplace(Args&&... Arguments)
    {
        Container.emplace_back(std::forward<Args>(Arguments)...);
    }
    
    void Resize(size_t NewSize)
    {
        Container.resize(NewSize);
    }
    
    FORCEINLINE decltype(auto) Reverse(this auto&& Self) { return TReverseIterator{FWD(Self).Container}; }
    
    void Clear()
    {
        Container.clear();
    }
    
    void Reserve(size_t NewCapacity)
    {
        Container.reserve(NewCapacity);
    }
    
    void SwapElements(size_t A, size_t B)
    {
        Container.swap(A, B);
    }
    
    void Pop()
    {
        Container.pop_back();
    }
    
    ElementType PopElement()
    {
        ElementType Temp = MoveTemp(LastElement());
        Container.pop_back();
        return Temp; //NRVO should kick in here
    }
    
    void AddAtStable(size_t Index, const ElementType& Element)
    {
        Container.DoInsertValue(Index, Element);
    }
    
    void AddAtSwap(size_t Index, const ElementType& Element)
    {
        Container.push_back(Element);
        SwapElements(Index, LastIndex());
    }
    
    template <typename... Args>
    void EmplaceAtStable(size_t Index, Args&&... Arguments)
    {
        Container.DoInsertAt(Index, std::forward<Args>(Arguments)...);
    }
    
    template <typename... Args>
    void EmplaceAtSwap(size_t Index, Args&&... Arguments)
    {
        Container.emplace_back(std::forward<Args>(Arguments)...);
        SwapElements(Index, LastIndex());
    }
    
    ElementType RemoveElementAtStable(size_t Index)
    {
        ElementType Temp = MoveTemp(Container[Index]);
        Container.erase(Container.begin() + Index);
        return MoveTemp(Temp);
    }
    
    ElementType RemoveElementAtSwap(size_t Index)
    {
        ElementType Temp = MoveTemp(Container[Index]);
        Container.erase_unsorted(Container.begin() + Index);
        return MoveTemp(Temp);
    }
    
    ElementType RemoveElementAt(size_t Index)
    {
        return RemoveElementAtStable(Index);
    }
    
    decltype(auto) GetData(this auto&& Self) { return FWD(Self).Container.data(); }
    

    
    //////////////////////////////////////OPERATORS
    
    FORCEINLINE_DEBUG decltype(auto) operator[](this auto&& Self, size_t Index)
    {
        return FWDL(Self, Self.Container[Index]);
    }
    
};




