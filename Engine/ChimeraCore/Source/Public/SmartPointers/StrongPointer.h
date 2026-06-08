
#pragma once

template <template <typename> typename DerivedTemplate, typename T>
struct TStrongPtr {
    
    using Derived = DerivedTemplate<T>;
    
    FORCEINLINE decltype(auto) GetPtrInternal(this auto&& Self) { return FWDL(Self, Self.Ptr); }
};
