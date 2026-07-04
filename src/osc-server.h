#pragma once


#include <map>

// includes for reading IP-Address
#include <sys/stat.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "base.h"
#include "WString.h"

namespace OMC
{
    class OscServer : public X32Base 
    {
        private:
            struct sockaddr_in ServerAddr;
            char TxMessage[450]; // the largest binary blob will take up to 20+(70+1))*4 bytes = 408 bytes
            int counter = 0;

            std::map<WString::String, MP_ID>* oscPaths;
            //std::map<WString::String, uint> clients;

        public:
            int UdpHandle;
            struct sockaddr_in ClientAddr;

            OscServer(X32BaseParameter* basepar);

            int8_t Init();
            void UdpHandleCommunication();
            void Sync();

            void SendUdpPacket(char* buffer, uint16_t size);
    };

}