

#pragma once

namespace Chimera
{
namespace Conversions {
        
    template <typename To, typename From>
    To ConvertContainer(const From& Value)
    {
        To Result;
        for (const auto& Element : Value)
        {
            Result.emplace_back(Element);
        }
        return Result;
    }
        
    
    
}
}
