//
// Created by user on 08/06/2026.
//

#include "GenericInput.h"

void MGenericInput::StartupModule()
{
    InputHandle = MakeShared<IInputHandler>();
}


