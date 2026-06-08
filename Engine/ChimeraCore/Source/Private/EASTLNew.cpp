
#pragma once

#include <cstdlib>
#include <cstdint>
#include <malloc.h>
#include <fmt/base.h>

#include "Macros/CompilerAbstractions.h"

NOINLINE void* __cdecl operator new[](size_t Size, const char* Name, int Flags,
                                   unsigned DebugFlags, const char* File, int Line)
{
    void* Ptr = ::operator new[](Size);
    
    // Podrías registrar la allocación aquí
    fmt::println("[ALLOC] {} bytes | {} | {}:{}", Size, Name ? Name : "?", File, Line);
    
    return Ptr;
}

NOINLINE void* __cdecl operator new[](size_t Size, size_t Alignment, size_t AlignmentOffset,
                              const char* Name, int Flags, unsigned DebugFlags,
                              const char* File, int Line)
{
    return ::operator new[](Size);
}