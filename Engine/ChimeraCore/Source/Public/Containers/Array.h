
#pragma once

#include <vector>

#include "Libraries/Memory.h"
#include "Macros/BuildAbstractions.h"
#include "Macros/UtilityMacros.h"

template <typename InElementType> 
struct TArray
{
    
    using ElementType = InElementType;
    static constexpr bool TriviallyCopyable =  std::is_trivially_copyable_v<ElementType>;
    static constexpr bool TriviallyDestructible = std::is_trivially_destructible_v<ElementType>;
    static constexpr bool TriviallyMoveConstructible = std::is_trivially_move_constructible_v<ElementType>;
    static constexpr bool TriviallyMoveAssignable = std::is_trivially_move_assignable_v<ElementType>;
    static constexpr bool TriviallyCopyConstructible = std::is_trivially_copy_constructible_v<ElementType>;
    static constexpr bool TriviallyCopyAssignable = std::is_trivially_copy_assignable_v<ElementType>;
    
    static constexpr size_t ElementSize = sizeof(ElementType);
    static constexpr size_t ElementAlignment = alignof(ElementType);
    
    
    TArray() = default;
    
    explicit TArray(size_t InitialSize)
    {
        Resize(InitialSize);
    }
    
    TArray(const std::vector<ElementType>& InitialData)
    {
        Resize(InitialData.size());
        std::copy(InitialData.begin(), InitialData.end(), DataPtr);
    }
    
    TArray(std::initializer_list<ElementType> InitList)
    {
        Resize(InitList.size());
        std::copy(InitList.begin(), InitList.end(), DataPtr);
    }

    template <typename IteratorElementType>
    struct TIterator
    {
        
        using PointerType = IteratorElementType*;
        
        TIterator(PointerType Ptr) : Ptr(Ptr) {}
        
        decltype(auto) operator*(this auto&& Self) { return FWDL(Self, *Self.Ptr); }
        
        decltype(auto) operator->(this auto&& Self) { return Self.Ptr; }
        
        // Prefix increment
        TIterator& operator++() { Ptr++; return *this; }
        
        // Postfix increment
        TIterator operator++(int) { TIterator tmp = *this; ++(*this); return tmp; }
        
        friend bool operator==(const TIterator& a, const TIterator& b) { return a.Ptr == b.Ptr; }
        friend bool operator!=(const TIterator& a, const TIterator& b) { return a.Ptr != b.Ptr; }
        
        PointerType Ptr;
    };
    
    template <typename PointerType>
    using TConstIterator = TIterator<const PointerType>;
    
    TConstIterator<ElementType> begin() const { return TConstIterator{DataPtr}; }
    TConstIterator<ElementType> end() const { return TConstIterator{DataPtr + Count}; }
    
    TIterator<ElementType> begin() { return TIterator{DataPtr}; }
    TIterator<ElementType> end() { return TIterator{DataPtr + Count}; }
    
    NO_DISCARD size_t Num() const { return Count; }
    NO_DISCARD size_t LastIndex() const { return Count - 1; }
    NO_DISCARD decltype(auto) LastElement(this auto&& Self) { return FWDL(Self, Self.Get(Self, Self.LastIndex())); }
    NO_DISCARD size_t GetCapacity() const { return Capacity; }
    
    void Add(const ElementType& Element)
    {
        EnsureCapacity(Count + 1);
        DataPtr[Count++] = Element;
    }
    
    template <typename... Args>
    void Emplace(Args&&... Arguments)
    {
        EnsureCapacity(Count + 1);
        ::new (DataPtr[Count++]) ElementType(std::forward<Args>(Arguments)...);
    }
    
    void Resize(size_t NewSize)
    {
        EnsureCapacity(NewSize);
        Count = NewSize;
    }
    
    void Clear()
    {
        if constexpr (!TriviallyDestructible)
        {
            for (size_t i = 0; i < Count; ++i)
                DestructElementAt(i);
        }
        Count = 0;
    }
    
    void Reserve(size_t NewCapacity)
    {
        if (NewCapacity <= Capacity) return;
        
        ElementType* NewData = FMemory::Realloc<ElementType>(DataPtr, NewCapacity * ElementSize, ElementAlignment);
            
        if (NewData != DataPtr)
        {
            MoveElementsTo(NewData);
            FMemory::Free(DataPtr);
            DataPtr = NewData;
        }
            
    }
    
    void SwapElements(size_t A, size_t B)
    {
        check(A >= Count || B >= Count);
        FMemory::Swap(DataPtr[A], DataPtr[B]);
    }
    
    ElementType&& Pop()
    {
        check(Count > 0); 
        return MoveTemp(DataPtr[--Count]);
    }
    
    void AddAtStable(size_t Index, const ElementType& Element)
    {
        check(Index <= Count);
        EnsureCapacity(Count + 1);
        
        ShiftElementsForward(Index);
        
        DataPtr[Index] = Element;
        ++Count;
    }
    
    ElementType&& RemoveAtStable(size_t Index)
    {
        check(Index < Count);
        ElementType&& PoppedElement = MoveTemp(DataPtr[Index]);
        ShiftElementsBackward(Index);
        --Count;
        return MoveTemp(PoppedElement);
    }
    
    ElementType&& RemoveAtSwap(size_t Index)
    {
        check(Index < Count);
        SwapElements(DataPtr[Index], LastElement());
        return Pop();
    }
    
    ElementType&& RemoveAt(size_t Index)
    {
        return RemoveAtStable(Index);
    }
    
    ElementType* GetData() { return DataPtr; }
    const ElementType* GetData() const { return DataPtr; }
    
    
    
    decltype(auto) Get(this auto&& Self,size_t Index)
    {
        return FWD(Self).operator[](Index);
    }
    
    //////////////////////////////////////OPERATORS
    
    FORCEINLINE_DEBUG decltype(auto) operator[](this auto&& Self, size_t Index)
    {
        check(Index < FWD(Self).Count);
        return FWDL(Self, Self.GetData()[Index]);
    }
    
protected:
    
    void ShiftElementsForward(size_t FromIndex, size_t ShiftAmount = 1)
    // Desplaza Count elementos desde FromIndex, ShiftAmount posiciones hacia adelante
    {
        if (Count == 0 || ShiftAmount == 0 || FromIndex >= Count) return;
        const size_t ElementsToMove = Count - FromIndex;
        
        if constexpr (TriviallyCopyable)
        {
            FMemory::Memmove(
                DataPtr + FromIndex + ShiftAmount,
                DataPtr + FromIndex,
                ElementsToMove * ElementSize
            );
        }
        else
        {
            // Hacia adelante: iterar de atrás hacia delante para evitar solapamiento
            for (size_t i = ElementsToMove; i --> 0;)
            {
                DataPtr[FromIndex + ShiftAmount + i] = MoveTemp(DataPtr[FromIndex + i]);
            }
        }
    }
    
    void ShiftElementsBackward(size_t FromIndex, size_t ShiftAmount = 1)
    {
        if (Count == 0 || ShiftAmount == 0 || FromIndex >= Count) return;
        const size_t ElementsToMove = Count - FromIndex;
        
        if constexpr (TriviallyCopyable)
        {
            FMemory::Memmove(
                DataPtr + FromIndex - ShiftAmount,
                DataPtr + FromIndex,
                ElementsToMove * ElementSize
            );
        }
        else
        {
            // Hacia atrás: iterar de delante hacia atrás
            for (size_t i = 0; i < ElementsToMove; ++i)
            {
                DataPtr[FromIndex - ShiftAmount + i] = MoveTemp(DataPtr[FromIndex + i]);
            }
        }
    }
    
    FORCEINLINE_DEBUG void MoveElementsTo(ElementType* DataTarget)
    {
        if constexpr (TriviallyCopyable)
        {
            FMemory::Memcpy(DataTarget, DataPtr, Count);
        }
        else
        {
            for (size_t i = 0; i < Count; ++i)
            {
                ::new (DataTarget[i]) ElementType(MoveTemp(DataPtr[i]));
            }
        }
        if (!TriviallyDestructible)
        {
            for (size_t i = 0; i < Count; ++i)
            {
                DestructElementAt(i);
            }
        }
    }
    
    template <typename... Args>
    FORCEINLINE void ConstructNewAt(size_t Index, Args&& ...args)
    {
        ::new (DataPtr[Index]) ElementType(std::forward<Args>(args)...);
    }
    
    FORCEINLINE void DestructElementAt(size_t Index)
    {
        DataPtr[Index].~ElementType();
    }
    
    FORCEINLINE void EnsureCapacity(size_t MinCapacity)
    {
        if (MinCapacity > Capacity)
        {
            size_t IncreaseTimes = (MinCapacity / Capacity) + 1;
            size_t NewCapacity = Capacity * IncreaseTimes;
            Reserve(NewCapacity);
        }
    }

    
private:
    
    ////////////////////////////////  Data storage and metadata  //
    
    ElementType* DataPtr { nullptr };
    size_t Count { 0 };
    size_t Capacity { 0 };
    
    std::vector<ElementType> Data;
};




