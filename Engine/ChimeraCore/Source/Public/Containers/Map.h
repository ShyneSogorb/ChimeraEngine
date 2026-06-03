
#pragma once
#include <unordered_map>
#include <EASTL/unordered_map.h>


template <typename KeyType, typename ValueType> 
struct TMap
{
    TMap() = default;
    
    template <typename... Args>
    void Emplace(Args&&... Arguments)
    {
        Map.emplace(std::forward<Args>(Arguments)...);
    }
    
    ValueType& operator[](const KeyType& Key) { return Map[Key]; }
    
    const ValueType& operator[](const KeyType& Key) const { return Map.at(Key); }
    
    size_t Num() const { return Map.size(); }
    
private:
    
    eastl::unordered_map<KeyType, ValueType> Map;
};
