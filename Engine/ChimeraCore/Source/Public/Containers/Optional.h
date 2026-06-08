
#pragma once
#include <EASTL/optional.h>

template <typename T>
struct TOptional{
    
    TOptional() = default;
    TOptional(const T& Value) : Optional(Value) {}
    TOptional(T&& Value) : Optional(std::move(Value)) {}
    
    T& operator*() { return *Optional; }
    const T& operator*() const { return *Optional; }
    
    explicit operator bool() const { return Optional.has_value(); }
    
private:
    eastl::optional<T> Optional;
};
