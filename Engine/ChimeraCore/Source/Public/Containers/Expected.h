
#pragma once

#include <expected>

template <typename T, typename ErrorType>
using TExpected = std::expected<T, ErrorType>;

