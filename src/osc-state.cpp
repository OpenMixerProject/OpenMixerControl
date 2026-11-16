#include "osc-state.h"
#include "mixerparameter.h"
#include <netinet/in.h>

namespace OMC
{
    namespace
    {
        std::string JsonString(const char* value)
        {
            std::string result;
            (void)glz::write_json(std::string(value), result);
            return result;
        }

        void Packet(int socket, in_addr_t ip, const char* data, int size)
        {
            if (size <= 0) { return; }
            sockaddr_in target{};
            target.sin_family = AF_INET;
            target.sin_addr.s_addr = ip;
            target.sin_port = htons(10033);
            sendto(socket, data, size, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
        }
    }

    OscState::OscState(X32BaseParameter* basepar, int udpSocket, std::map<String, MP_ID>* oscPaths)
        : X32Base(basepar), paths(oscPaths), socket(udpSocket)
    {
        // Additive vocabulary only: do not advertise absent expansion PRs.
        auto alias = [&](String name, MP_ID id)
        {
            if (config->GetParameter(id)->IsOSC()) { paths->insert({name, id}); }
        };
        alias("ch_sub_level", MP_ID::CHANNEL_VOLUME_SUB);
        for (uint i = 0; i < 4; ++i)
        {
            alias(String("ch_eq_") + String(i + 1) + "type", config->MpCalcId(MP_ID::CHANNEL_EQ_TYPE1, i));
        }
        for (uint i = 0; i < 8; ++i)
        {
            alias(String("dca") + String(i + 1) + "_member", config->MpCalcId(MP_ID::DCA_GROUP_1, i));
        }
        for (uint i = 0; i < 6; ++i)
        {
            alias(String("mute_group") + String(i + 1) + "_member", config->MpCalcId(MP_ID::MUTE_GROUP_1, i));
            alias(String("mute_group") + String(i + 1) + "_mute", config->MpCalcId(MP_ID::MUTE_GROUP_1_MUTE, i));
        }
    }

    void OscState::Send(in_addr_t ip, const char* path, const std::string& json)
    {
        // Topology is larger than the old fixed 450-byte value buffer.
        std::vector<char> packet(json.size() + strlen(path) + 32);
        int size = tosc_writeMessage(packet.data(), packet.size(), path, "s", json.c_str());
        Packet(socket, ip, packet.data(), size);
    }

    void OscState::SendValue(in_addr_t ip, const String& name, Mixerparameter* parameter, uint index)
    {
        String path = String("/set/") + name + "/" + String(index + 1);
        if (parameter->GetType() == MP_VALUE_TYPE::STRING)
        {
            String value = parameter->GetString(index);
            std::vector<char> packet(value.length() + path.length() + 32);
            int size = tosc_writeMessage(packet.data(), packet.size(), path.c_str(), "s", value.c_str());
            Packet(socket, ip, packet.data(), size);
        }
        else
        {
            char packet[256];
            int size = tosc_writeMessage(packet, sizeof(packet), path.c_str(), "f", parameter->GetFloat(index));
            Packet(socket, ip, packet, size);
        }
    }

    std::string OscState::Topology()
    {
        // Virtual strip indices from VCHANNEL_IDX; physical port tables are a
        // different address space. Index 70 is reserved, not a phantom strip.
        std::string json = R"({"strips":[
{"kind":"input","name":"Input","start":0,"count":32,"eq_bands":["1","2","3","4"]},
{"kind":"aux","name":"Aux","start":32,"count":8,"eq_bands":["1","2","3","4"]},
{"kind":"fx","name":"FX Return","start":40,"count":8,"eq_bands":[]},
{"kind":"bus","name":"Bus","start":48,"count":16,"eq_bands":[]},
{"kind":"matrix","name":"Matrix","start":64,"count":6,"eq_bands":[]},
{"kind":"main","name":"Mono/Sub","start":71,"count":1,"eq_bands":[]},
{"kind":"dca","name":"DCA","start":72,"count":8,"eq_bands":[]},
{"kind":"main","name":"Main LR","start":80,"count":1,"eq_bands":[]}],
"sources":[{"name":"XLR","start":0,"count":32},
{"name":"Card","start":32,"count":32},{"name":"Aux","start":64,"count":6},
{"name":"Talkback","start":70,"count":2},{"name":"DSP Return","start":72,"count":40},
{"name":"AES50 A","start":112,"count":48}],
"outputs":[{"name":"XLR","start":0,"count":16},{"name":"Ultranet","start":16,"count":16},
{"name":"Card","start":32,"count":32},{"name":"Aux","start":64,"count":6},
{"name":"Monitor output","start":70,"count":2},
{"name":"DSP Input","start":72,"count":40},{"name":"AES50 A","start":112,"count":48},
{"name":"AES50 B","start":160,"count":48}],
"fx_slots":8,"eq_bands":["1","2","3","4"],"sends":16,"matrices":6,
"colors":["#000000","#ff0000","#00ff00","#ffff00","#0000ff","#ff00ff","#00ffff","#ffffff"],
"capabilities":["topology","metadata","state_snapshot","state_subscription")";
        for (const auto& [name, id] : *paths) { json += "," + JsonString(name.c_str()); }
        for (const auto& name : config->oscCapabilities) { json += "," + JsonString(name.c_str()); }
        json += "],\"dcas\":" + std::to_string((uint)X32_VCHANNEL_BLOCK_SIZE::DCA) +
            ",\"mute_groups\":" + std::to_string(MUTE_GROUPS);

        // Native source values are NOT virtual strip IDs. Zero means Off;
        // destination starts are zero-based Mixerparameter instances.
        auto group = [](const char* name, uint start, uint count)
        {
            return "{\"name\":" + JsonString(name) + ",\"start\":" + std::to_string(start) +
                ",\"count\":" + std::to_string(count) + "}";
        };
        std::string dspSources = "[" + group("Channel", DSP_BUF_IDX_DSPCHANNEL, 32) + "," +
            group("Aux", DSP_BUF_IDX_AUX, 8) + "," + group("FX Return", DSP_BUF_IDX_DSP2_FXRET, 8) + "," +
            group("Bus", DSP_BUF_IDX_MIXBUS, 16) + "," + group("FX Insert", DSP_BUF_IDX_DSP2_FXINS, 8) + "," +
            group("Main L", DSP_BUF_IDX_MAINLEFT, 1) + "," + group("Main R", DSP_BUF_IDX_MAINRIGHT, 1) + "," +
            group("Mono/Sub", DSP_BUF_IDX_MAINSUB, 1) + "," + group("Matrix", DSP_BUF_IDX_MATRIX, 6) + "," +
            group("DSP2 Aux", DSP_BUF_IDX_DSP2_AUX, 8) + "," + group("Monitor L", DSP_BUF_IDX_MONLEFT, 1) + "," +
            group("Monitor R", DSP_BUF_IDX_MONRIGHT, 1) + "," + group("Talkback", DSP_BUF_IDX_TALKBACK, 1) + "]";
        std::string socketSources = "[" + group("XLR", FPGA_INPUT_IDX_XLR, 32) + "," +
            group("Card", FPGA_INPUT_IDX_CARD, 32) + "," + group("Aux", FPGA_INPUT_IDX_AUX, 6) + "," +
            group("Internal Talkback", FPGA_INPUT_IDX_TALKBACK_INT, 1) + "," +
            group("External Talkback", FPGA_INPUT_IDX_TALKBACK_EXT, 1) + "," +
            group("DSP Return", FPGA_INPUT_IDX_DSP_RETURN, 40) + "," +
            group("AES50 A", FPGA_INPUT_IDX_AES50A, NUM_INPUT_CHANNEL - FPGA_INPUT_IDX_AES50A + 1) + "]";
        std::string socketDests = "[" + group("XLR", FPGA_OUTPUT_IDX_XLR - 1, 16) + "," +
            group("Ultranet", FPGA_OUTPUT_IDX_ULTRANET - 1, 16) + "," + group("Card", FPGA_OUTPUT_IDX_CARD - 1, 32) + "," +
            group("Aux", FPGA_OUTPUT_IDX_AUX - 1, 6) + "," + group("Monitor output", 70, 2) + "," +
            group("DSP Input", FPGA_OUTPUT_IDX_DSP - 1, MAX_FPGA_TO_DSP1_CHANNELS) + "," +
            group("AES50 A", FPGA_OUTPUT_IDX_AES50A - 1, 48) + "," + group("AES50 B", FPGA_OUTPUT_IDX_AES50B - 1, 48) + "]";
        json += ",\"routing\":{\"channels\":{\"osc\":\"route_dsp_in\",\"sources\":" + dspSources +
            ",\"dests\":[" + group("Channel", 0, 32) + "," + group("Aux", 32, 8) + "]},";
        json += "\"outputs\":{\"osc\":\"route_dsp_out\",\"sources\":" + dspSources +
            ",\"dests\":[" + group("FPGA output feed", 0, MAX_DSP1_TO_FPGA_CHANNELS) + "]},";
        json += "\"effects\":{\"osc\":\"route_dsp_out\",\"sources\":" + dspSources +
            ",\"dests\":[" + group("Effects and analysis feed", MAX_DSP1_TO_FPGA_CHANNELS, MAX_DSP1_TO_DSP2_CHANNELS) + "]},";
        json += "\"sockets\":{\"osc\":\"route_fpga\",\"sources\":" + socketSources + ",\"dests\":" + socketDests + "}}}";
        return json;
    }

    std::string OscState::Metadata(const String& name, Mixerparameter* parameter)
    {
        bool string = parameter->GetType() == MP_VALUE_TYPE::STRING;
        const char* audio = "hardware_dependent";
        switch (parameter->GetId())
        {
            case MP_ID::CHANNEL_PHASE_INVERT:
            case MP_ID::CHANNEL_GATE_ENABLE:
            case MP_ID::CHANNEL_COMPRESSOR_ENABLE:
            case MP_ID::CHANNEL_LOWCUT_ENABLE:
            case MP_ID::MONITOR_VOLUME:
            case MP_ID::MONITOR_TAPPOINT:
                audio = "state_only_no_dispatch";
                break;
            case MP_ID::CHANNEL_NAME:
            case MP_ID::CHANNEL_COLOR:
            case MP_ID::CHANNEL_COLOR_INVERTED:
            case MP_ID::DEBUG_VALUE:
                audio = "state_only";
                break;
            default: break;
        }
        return "{\"name\":" + JsonString(name.c_str()) + ",\"type\":\"" + (string ? "string" : "number") +
            "\",\"instances\":" + std::to_string(parameter->GetInstances()) + ",\"min\":" +
            (string ? "null" : std::to_string(parameter->GetMin())) + ",\"max\":" +
            (string ? "null" : std::to_string(parameter->GetMax())) + ",\"readonly\":" +
            (parameter->IsReadonly() ? "true" : "false") + ",\"audio_status\":" + JsonString(audio) + "}";
    }

    bool OscState::Handle(const std::vector<std::string>& address, tosc_message* message, in_addr_t ip)
    {
        if (address.size() != 3 || address[2] != "1") { return false; }
        std::string format(tosc_getFormat(message));
        if (address[0] == "get" && address[1] == "topology" && format.empty())
        {
            Send(ip, "/set/topology/1", Topology());
            return true;
        }
        bool subscribe = address[0] == "subscribe" && address[1] == "state";
        bool snapshot = address[0] == "get" && address[1] == "state" && format.empty();
        bool metadata = address[0] == "get" && address[1] == "metadata" && format.empty();
        if (!subscribe && !snapshot && !metadata) { return false; }
        if (subscribe && format != "f") { return true; }
        bool enable = !subscribe || tosc_getNextFloat(message) >= 0.5f;
        if (!clients.contains(ip) && clients.size() >= 16) { enable = false; }
        if (enable)
        {
            bool fresh = !clients.contains(ip) || !clients.at(ip).subscribed;
            auto& client = clients[ip];
            client.renewed = Clock::now();
            if (subscribe) { client.subscribed = true; }
            if (snapshot || metadata || (subscribe && fresh))
            {
                client.next = paths->begin();
                client.index = 0;
                client.snapshot = true;
                client.metadata = metadata;
            }
        }
        else if (subscribe) { clients.erase(ip); }
        if (subscribe)
        {
            char packet[64];
            int size = tosc_writeMessage(packet, sizeof(packet), "/subscribe/state/1", "f", enable ? 1.0f : 0.0f);
            Packet(socket, ip, packet, size);
        }
        return true;
    }

    void OscState::Pump()
    {
        auto now = Clock::now();
        for (auto it = clients.begin(); it != clients.end(); )
        {
            auto& [ip, client] = *it;
            if (now - client.renewed >= std::chrono::seconds(10)) { it = clients.erase(it); continue; }
            for (uint budget = 0; client.snapshot && budget < 32; ++budget)
            {
                if (client.next == paths->end())
                {
                    char packet[64];
                    const char* path = client.metadata ? "/metadata/complete/1" : "/state/complete/1";
                    int size = tosc_writeMessage(packet, sizeof(packet), path, "f", 1.0f);
                    Packet(socket, ip, packet, size);
                    client.snapshot = false;
                    break;
                }
                const auto& [name, id] = *client.next;
                auto* parameter = config->GetParameter(id);
                if (client.metadata)
                {
                    Send(ip, (String("/set/metadata/") + name + "/1").c_str(), Metadata(name, parameter));
                    ++client.next;
                }
                else
                {
                    SendValue(ip, name, parameter, client.index);
                    if (++client.index >= parameter->GetInstances()) { ++client.next; client.index = 0; }
                }
            }
            if (!client.subscribed && !client.snapshot) { it = clients.erase(it); }
            else { ++it; }
        }
    }

    void OscState::Sync()
    {
        for (const auto& [name, id] : *paths)
        {
            auto* parameter = config->GetParameter(id);
            for (uint index : config->GetChangedParameterIndexes(std::vector<MP_ID>{id}))
            {
                if (index >= parameter->GetInstances()) { continue; }
                for (const auto& [ip, client] : clients)
                {
                    if (client.subscribed && Clock::now() - client.renewed < std::chrono::seconds(10))
                    { SendValue(ip, name, parameter, index); }
                }
            }
        }
    }
}
