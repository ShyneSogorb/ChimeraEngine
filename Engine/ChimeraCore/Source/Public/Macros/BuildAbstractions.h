//
// Created by user on 27/05/2026.
//

#pragma once

#include "CompilerAbstractions.h"


#ifdef SHIPPING_BUILD

    #define FORCEINLINE_DEBUG FORCEINLINE

#else

    #define FORCEINLINE_DEBUG NOINLINE

#endif