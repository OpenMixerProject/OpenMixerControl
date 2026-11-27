#pragma once

#include "base.h"
#include "../lib_ext/small-osc/small-osc.h"
#include <netinet/in.h>
#include <chrono>

namespace OMC
{
    // Discovery describes controller state and layout, not external DSP success.
    class OscState : public X32Base
    {
        using Clock = std::chrono::steady_clock;
        struct Client
        {
            Clock::time_point renewed;
            bool subscribed = false;
            std::map<String, MP_ID>::iterator next;
            std::map<String, MP_ID>::iterator metadataNext;
            uint index = 0;
            bool snapshot = false;
            bool metadata = false;
        };
        std::map<in_addr_t, Client> clients;
        std::map<String, MP_ID>* paths;
        int socket;
        void Send(in_addr_t ip, const char* path, const std::string& json);
        void SendValue(in_addr_t ip, const String& name, Mixerparameter* parameter, uint index);
        std::string Topology();
        std::string Metadata(const String& name, Mixerparameter* parameter);

    public:
        /// Adds the discovery aliases (e.g. ch_sub_level) to a map of OSC paths.
        static void AddAliases(Config* config, std::map<String, MP_ID>* paths);

    public:
        OscState(X32BaseParameter* basepar, int udpSocket, std::map<String, MP_ID>* oscPaths);
        bool Handle(const std::vector<std::string>& address, tosc_message* message, in_addr_t ip);
        void Pump();
        void Sync();
    };
}
