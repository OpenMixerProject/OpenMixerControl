#pragma once


#include <map>

// includes for reading IP-Address
#include <sys/stat.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "base.h"

class OscServer : public X32Base 
{
    private:
        struct sockaddr_in ServerAddr;
        char TxMessage[450]; // the largest binary blob will take up to 20+(70+1))*4 bytes = 408 bytes
        int counter = 0;

        map<String, MP_ID> oscPaths;

    public:
        int UdpHandle;
        struct sockaddr_in ClientAddr;

        OscServer(X32BaseParameter* basepar);

        int8_t Init();
        void UdpHandleCommunication();

        void SendUdpPacket(char* buffer, uint16_t size);
        void SendBasicMessage(const char* cmd, char type, char format, char* value);
};