
#pragma once
#include <EASTL/deque.h>
#include "ContainersHelper.h"

template <typename T>
struct TDeque {
    
    void Add(const T& Element) { Container.push_back(Element); }
    void AddFront(const T& Element) { Container.push_front(Element); }
    
    template <typename... Args>
    T& Emplace(Args&&... Arguments) { Container.emplace_back(std::forward<Args>(Arguments)...); return Container.back(); }
    template <typename... Args>
    T& EmplaceFront(Args&&... Arguments) { Container.emplace_front(std::forward<Args>(Arguments)...); return Container.front(); }
    
    T& Front() { return Container.front(); }
    const T& Front() const { return Container.front(); }
    T& Back() { return Container.back(); }
    const T& Back() const { return Container.back(); }
    
    void PopFront() { Container.pop_front(); }
    void PopBack() { Container.pop_back(); }
    
    auto Num() const { return Container.size(); }
    
    void Clear() { Container.clear(); }
    
    DECLARE_ITERATOR()
    
private:
    eastl::deque<T> Container;
};

