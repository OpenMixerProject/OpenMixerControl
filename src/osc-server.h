#pragma once


#include <map>
#include <chrono>
#include <string_view>

// includes for reading IP-Address
#include <sys/stat.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "base.h"
#include "WString.h"

namespace OMC
{
#ifdef TARGET_WING
    class WingT55Control;
#endif
    class OscServer : public X32Base 
    {
        private:
            struct sockaddr_in ServerAddr;
            char TxMessage[450]; // the largest binary blob will take up to 20+(70+1))*4 bytes = 408 bytes
            int counter = 0;

            bool hasClients = false;

            std::map<WString::String, MP_ID>* oscPaths;
#ifdef TARGET_WING
            WingT55Control* wingT55 = nullptr;
            bool wingUi = false;
            void HandleWingUi(std::string_view packet);
            void ReplyWingUi(std::string_view request);
            void RenameWingInput(std::string_view address, std::string_view payload);
#endif
            //std::map<WString::String, uint> clients;

            std::map<in_addr_t, std::chrono::steady_clock::time_point> surfaceObservers;
            void ObserveSurface(SurfaceElement* element, const char* action, float value);
            void ExpireObservers();

        public:
            int UdpHandle = -1;
            
            struct sockaddr_in ClientAddr;

            OscServer(X32BaseParameter* basepar);
#ifdef TARGET_WING
            OscServer(X32BaseParameter* basepar, bool wingUi, WingT55Control* wingT55);
#endif

            int8_t Init();
            void UdpHandleCommunication();
            void Sync();

            void SendUdpPacket(const char* buffer, uint16_t size);
    };

}
