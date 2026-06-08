
#pragma once

#define DECLARE_ITERATOR() \
    auto begin  (this auto&& Self) { return FWD(Self).Container.begin();    } \
    auto end    (this auto&& Self) { return FWD(Self).Container.end();      } \
    auto rbegin (this auto&& Self) { return FWD(Self).Container.rbegin();   } \
    auto rend   (this auto&& Self) { return FWD(Self).Container.rend();     }

//defines generic constructors for containers
#define DECLARE_CONTAINER_CONSTRUCTORS(SelfType) \
    SelfType() = default; \
    SelfType(const SelfType& Other) : Container(Other.Container) {} \
    SelfType(SelfType&& Other) : Container(MoveTemp(Other.Container)) {} \
    SelfType(std::initializer_list<ElementType> InitList) : Container(InitList) {} \
    SelfType(const decltype(Container)& Other) : Container(Other) {} \
    SelfType(decltype(Container)&& Other) : Container(MoveTemp(Other)) {} \
    SelfType(size_t InitialSize) : Container(InitialSize) {} \
    //template <typename... Args> SelfType(Args&&... InArgs) : Container(std::forward<Args>(InArgs)...) {} \
    //template <typename ContainerType> SelfType(const ContainerType& Other) : Container(Other.data(), Other.size()) {} \
    //template <typename ContainerType> SelfType(ContainerType&& Other) : Container(FWDL(Other, Other.data()), Other.size()) {}

#define DECLARE_CONTAINER_ASSIGNMENT_OPERATORS(SelfType) \
    SelfType& operator=(const SelfType& Other) { Container = Other.Container; return *this; } \
    SelfType& operator=(SelfType&& Other) { Container = MoveTemp(Other.Container); return *this; } \
    SelfType& operator=(std::initializer_list<ElementType> InitList) { Container = InitList; return *this; } \
    SelfType& operator=(const decltype(Container)& Other) { Container = Other; return *this; } \
    SelfType& operator=(decltype(Container)&& Other) { Container = MoveTemp(Other); return *this; } \
    template <typename ContainerType> SelfType& operator=(const ContainerType& Other) { Container.assign(Other.begin(), Other.end()); return *this; }