#pragma once


#include <map>
#include <chrono>

// includes for reading IP-Address
#include <sys/stat.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "base.h"
#include "WString.h"
#include "../lib_ext/small-osc/small-osc.h"

namespace OMC
{
    class Card;
    class OscServer : public X32Base 
    {
        private:
            struct sockaddr_in ServerAddr;
            char TxMessage[450]; // the largest binary blob will take up to 20+(70+1))*4 bytes = 408 bytes
            int counter = 0;

            bool hasClients = false;

            std::map<WString::String, MP_ID>* oscPaths;
            //std::map<WString::String, uint> clients;
            Card* recorder = nullptr;
            bool HandleCommand(const std::vector<std::string>& address, const char* format, tosc_message* message);

            std::map<in_addr_t, std::chrono::steady_clock::time_point> surfaceObservers;
            void ObserveSurface(SurfaceElement* element, const char* action, float value);
            void ExpireObservers();

        public:
            int UdpHandle;
            
            struct sockaddr_in ClientAddr;

            OscServer(X32BaseParameter* basepar);

            int8_t Init();
            void UdpHandleCommunication();
            void Sync();

            void SetRecorder(Card* card) { recorder = card; }
            void SendUdpPacket(char* buffer, uint16_t size);
    };

}
