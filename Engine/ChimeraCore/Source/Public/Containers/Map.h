
#pragma once
#include <EASTL/unordered_map.h>
#include "Containers/ContainersHelper.h"


template <typename KeyType, typename ValueType> 
struct TMap
{
    
    using ThisType = TMap<KeyType, ValueType>;
    using ElementType = eastl::pair<KeyType, ValueType>;
    
private:
    
    eastl::unordered_map<KeyType, ValueType> Container;
    
public:
    
    DECLARE_CONTAINER_CONSTRUCTORS(TMap)
    
    DECLARE_ITERATOR()
    
    template <typename... Args>
    void Emplace(Args&&... Arguments)
    {
        Container.emplace(std::forward<Args>(Arguments)...);
    }
    
    ValueType& operator[](const KeyType& Key) { return Container[Key]; }
    
    auto Num() const { return Container.size(); }
    
    void Clear() { Container.clear(); }
    

};
