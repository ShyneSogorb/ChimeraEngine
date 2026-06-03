

#pragma once

#include "fmt/core.h"

#define FWD(Variable) std::forward<decltype(Variable)>(Variable)
#define FWDL(Variable, Expr) std::forward_like<decltype(Variable)>(Expr)

#define check(expr) \
    do { \
        if (!(expr)) UNLIKELY { \
            fmt::print("Check failed: {} at {}:{}\n", #expr, __FILE__, __LINE__); \
            std::abort(); \
        } \
    } while (0)