#pragma once

#include "ctrl-server.h"
#include "ctrl-client.h"

namespace OMC
{
    class OpenMixerControl
    {
        private:

            CtrlServer* server = 0;
            CtrlClient* client = 0;
            Config* config = 0;

        public:

            OpenMixerControl(X32BaseParameter* basepar);
            
            void Init();
            
            void Tick10ms();
            void Tick50ms();
            void Tick100ms();
            void Tick1000ms();

            void SimulatorButton();
    };
}