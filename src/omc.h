#pragma once

#include "ctrl-server.h"
#include "ctrl-client.h"

namespace OMC
{

class OpenMixerControl
{
    private:

        bool runAsClient = false;

        CtrlServer* server = 0;
        CtrlClient* client = 0;

    public:

        OpenMixerControl(X32BaseParameter* basepar);
        
        void Init();
        
        void Tick10ms(void);
        void Tick50ms(void);
        void Tick100ms(void);

        void SimulatorButton();
};

}