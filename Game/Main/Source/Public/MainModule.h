//
// Created by user on 26/05/2026.
//
#pragma once

#include "Interfaces/IModuleInterface.h"

struct MMainModule : public IModuleInterface
{
    void ModuleStartup() override;
    void ModuleShutdown() override;
};

