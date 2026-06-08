
#pragma once
#include <EASTL/functional.h>


template <typename T>
struct TFunction
{
    
    TFunction() = default;
    TFunction(std::nullptr_t) : Function(nullptr) {}
    TFunction(const TFunction& Other) : Function(Other.Function) {}
    TFunction(TFunction&& Other) : Function(std::move(Other.Function)) {}
    
    template <typename Functor, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Functor>, TFunction>>>
    TFunction(Functor&& InFunctor) : Function(FWD(InFunctor)) {}
    
    decltype(auto) operator()(auto&&... args) { return Function(std::forward<decltype(args)>(args)...); }
    
private:
    eastl::function<T> Function;
};