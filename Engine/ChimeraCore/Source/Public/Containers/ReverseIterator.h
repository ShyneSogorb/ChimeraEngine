
#pragma once

template <typename Container>
struct TReverseIterator
{
    
    explicit TReverseIterator(Container& InContainer) : container(InContainer) {}
    
    decltype(auto) begin()
    {
        return container.rbegin();
    }
    
    decltype(auto) end()
    {
        return container.rend();
    }
    
private:
    
    Container& container;
    
};