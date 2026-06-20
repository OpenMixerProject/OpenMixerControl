#pragma once

#include "mixer.h"

#include "ctrl-server.h"
#include "osc-server.h"
#include "wsm.h"

#if ENABLE_ARTNET
    #include "artnet.h"
#endif

using namespace std;

class CtrlServer : public X32Base
{
    using enum MP_ID;

    private:
        
        bool intialized = false; // is the whole mixer ready?

        Mixer* mixer;
        
        OscServer* osc_server;
        WSM* wsm;
        
        void my_handler(int s);

        void ProcessUartDataAdda();
        void ProcessUartDataAES50();

        uint autosavewait = 0;
        void AutoSave();

    public:

        CtrlServer(X32BaseParameter* basepar);
        
        void Init();
        void writeConfigEntry(Mixerparameter *const &parameter, uint index);
        void Tick10ms(void);
        //void Tick50ms(void);
        void Tick100ms(void);

        void UdpHandleCommunication_WSM(void);
        
        void syncXRemote(bool syncAll);
}; 