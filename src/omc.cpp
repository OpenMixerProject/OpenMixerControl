/*
    ____                  __   ______ ___  
   / __ \                 \ \ / /___ \__ \ 
  | |  | |_ __   ___ _ __  \ V /  __) | ) |
  | |  | | '_ \ / _ \ '_ \  > <  |__ < / / 
  | |__| | |_) |  __/ | | |/ . \ ___) / /_ 
   \____/| .__/ \___|_| |_/_/ \_\____/____|
         | |                               
         |_|                               
  
  OpenX32 - The OpenSource Operating System for the Behringer X32 Audio Mixing Console
  Copyright 2025 OpenMixerProject
  https://github.com/OpenMixerProject/OpenX32
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  version 3 as published by the Free Software Foundation.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
*/

#include "omc.h"
#include "base-parameter.h"

namespace OMC
{
    OpenMixerControl::OpenMixerControl(X32BaseParameter* basepar)
    {
        config = basepar->config;

        if (!basepar->config->IsClientMode())
        {
            server = new CtrlServer(basepar);
        }
        client = new CtrlClient(basepar);
    }

    void OpenMixerControl::Init()
    {
        if (server) server->Init();
        client->Init();    
    }

    void OpenMixerControl::Tick10ms()
    {
        // Loop: Apply changed Mixerparameters

        //#####################################
        //
        //   Freeze changed parameter list
        //
        config->FreezeParameterList();
        //
        //
        //
        if (server) server->Tick10ms();
        client->Tick10ms();
        //
        //
        //   Unfreeze changed parameter list
        //
        config->SaveResetAndUnfreezeChangedParameterList();
        //
        //#####################################
    }

    void OpenMixerControl::Tick50ms()
    {
        client->Tick50ms();
    }

    void OpenMixerControl::Tick100ms()
    {
        if (server) server->Tick100ms();
        client->Tick100ms();
    }

    void OpenMixerControl::Tick1000ms()
    {
        if (server) server->Tick1000ms();
        client->Tick1000ms();
    }

    void OpenMixerControl::SimulatorButton()
    {
        client->SimulatorButton();
    }
}