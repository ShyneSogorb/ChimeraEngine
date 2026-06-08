
#pragma once

#include <EASTL/expected.h>

template <typename T, typename ErrorType>
struct TExpected{
    
private:
    eastl::expected<T, ErrorType> Expected;
};
