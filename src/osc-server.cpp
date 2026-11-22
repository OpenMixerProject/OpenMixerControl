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

#include "osc-server.h"

#include "../lib_ext/small-osc/small-osc.h"

#include "enum.h"
#include "mixerparameter.h"
#include "wing-t55-control.h"
#include "wing-local-io.h"
#include <charconv>
#include <cstddef>
#include <string_view>

namespace OMC
{
    using namespace WString;

#ifdef TARGET_WING
    namespace {
    constexpr unsigned wingInputs = 32;
    constexpr std::string_view renamePrefix = "/set/ch_name/";

    bool ValidWingName(std::string_view name)
    {
        if (name.empty() || name.size() > 32) return false;
        bool visible = false;
        for (size_t i = 0; i < name.size();) {
            const auto b = uint8_t(name[i++]);
            uint32_t cp = b;
            if (b >= 0x80) {
                const unsigned n = b >= 0xf0 ? 3 : b >= 0xe0 ? 2 : 1;
                if (b < 0xc2 || b > 0xf4 || i + n > name.size()) return false;
                cp = b & (0x7f >> (n + 1));
                for (unsigned j = 0; j < n; ++j) {
                    const auto next = uint8_t(name[i++]);
                    if ((next & 0xc0) != 0x80) return false;
                    cp = (cp << 6) | (next & 0x3f);
                }
                if (cp < (n == 1 ? 0x80u : n == 2 ? 0x800u : 0x10000u) || cp > 0x10ffff ||
                    (cp >= 0xd800 && cp <= 0xdfff)) return false;
            }
            if (cp < 32 || (cp >= 0x7f && cp <= 0x9f) || cp == 0x2028 || cp == 0x2029) return false;
            visible |= cp != ' ' && cp != 0xa0 && cp != 0x1680 && !(cp >= 0x2000 && cp <= 0x200a) &&
                       cp != 0x202f && cp != 0x205f && cp != 0x3000;
        }
        return visible;
    }

    std::string WingStringPacket(std::string address, const std::string& value)
    {
        address.resize((address.size() + 4) & ~size_t{3});
        address.append(",s\0\0", 4);
        address += value;
        address.resize((address.size() + 4) & ~size_t{3});
        return address;
    }
    }
#endif

    OscServer::OscServer(X32BaseParameter* basepar) : X32Base(basepar) {}

#ifdef TARGET_WING
    OscServer::OscServer(X32BaseParameter* basepar, bool wingUi, WingT55Control* wingT55)
        : X32Base(basepar), wingT55(wingT55), wingUi(wingUi) {}
#endif

    int8_t OscServer::Init()
    {
        // Bind on UDP Port 10032

        if ((UdpHandle = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Error on creating UDP-socket!");
            return -1;
        }
        
        ServerAddr.sin_family = AF_INET;
        ServerAddr.sin_addr.s_addr = INADDR_ANY;
#ifdef TARGET_WING
        if (wingUi) ServerAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#endif
        ServerAddr.sin_port = htons(10032);
        
        if (bind(UdpHandle, (const struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) < 0)
        {
            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Error on binding UDP-socket!");
            close(UdpHandle);
            UdpHandle = -1;
            return -1;
        }

        // Build map of osc-paths from Mixerparameters
        oscPaths = config->GetOscPaths();
#ifdef TARGET_WING
        if (wingUi) return 0;
#endif
        config->oscCapabilities.insert("surface_subscription");
        config->surfaceObserver = [this](SurfaceElement* element, const char* action, float value)
        {
            ObserveSurface(element, action, value);
        };
        
        return 0;
    }

    void OscServer::UdpHandleCommunication() 
    {
        if (UdpHandle < 0) return;
        ExpireObservers();
        alignas(uint32_t) char rxData[500];
        int bytes_available = 0;
        
        // check for bytes in UDP-buffer
        ioctl(UdpHandle, FIONREAD, &bytes_available);
        
        if (bytes_available > 0)
        {
            struct sockaddr_in senderAddr{};
            socklen_t senderAddrLen = sizeof(senderAddr);
#ifdef TARGET_WING
            if (wingUi) {
                const auto len = recvfrom(UdpHandle, rxData, sizeof rxData, MSG_TRUNC,
                                          (struct sockaddr *) &senderAddr, &senderAddrLen);
                if (len > 0 && len <= sizeof rxData && senderAddr.sin_addr.s_addr == htonl(INADDR_LOOPBACK) &&
                    senderAddr.sin_port == htons(10033)) {
                    ClientAddr = senderAddr;
                    HandleWingUi({rxData, size_t(len)});
                }
                return;
            }
#endif
            const auto len = recvfrom(UdpHandle, rxData, sizeof rxData, 0,
                                      (struct sockaddr *) &senderAddr, &senderAddrLen);
            if (len <= 0 || len >= sizeof rxData) return;

            tosc_message osc;
            if (!tosc_parseMessage(&osc, rxData, len))
            {
                // Client has send data: remember it as the target for Sync().
                // Only valid OSC messages update the client address, so a stray
                // UDP packet can not redirect the parameter updates.
                ClientAddr = senderAddr;
                hasClients = true;

                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(ClientAddr.sin_addr), str, INET_ADDRSTRLEN);

                helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Received OSC-Message %s from Client: %s Port: %d", rxData, str, ntohs(ClientAddr.sin_port));

                // if (clients.count(str))
                // {
                //     // known client
                //     // reset timeout counter
                //     clients.at(str) = 0;
                //     helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Known OSC-Client: %s", str);
                // }
                // else
                // {
                //     // new client
                //     clients.insert({str, 0});
                //     helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "New OSC-Client: %s", str);
                // }

                string adrPath = string(tosc_getAddress(&osc));
                vector<string> address = helper->split(adrPath, "/");
                address.erase(address.begin()); // delete empty element

                String osc_format = String(tosc_getFormat(&osc));

                helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Path: %s Format: %s", adrPath.c_str(), osc_format.c_str());

                if (address.size() == 3 && address[0] == "subscribe" && address[1] == "surface" && address[2] == "1")
                {
                    if (osc_format != "f") { return; }
                    in_addr_t ip = ClientAddr.sin_addr.s_addr;
                    bool enable = tosc_getNextFloat(&osc) >= 0.5f;
                    if (enable && (surfaceObservers.contains(ip) || surfaceObservers.size() < 16))
                    { surfaceObservers[ip] = std::chrono::steady_clock::now(); }
                    else { surfaceObservers.erase(ip); enable = false; }
                    int size = tosc_writeMessage(TxMessage, sizeof(TxMessage), "/subscribe/surface/1", "f", enable ? 1.0f : 0.0f);
                    SendUdpPacket(TxMessage, size);
                    return;
                }
                if (address.size() != 3) { return; }

                // Find matching Mixerparameter
                uint found = oscPaths->count(String(address[1].c_str()));
                if (found == 1) 
                {
                    Mixerparameter* parameter = config->GetParameter(oscPaths->at(String(address[1].c_str())));

                    if (address[0] == "set")
                    {
                        // Set
                        if (osc_format == "s")
                        {
                            int index = stoi(address[2].c_str()) - 1;
                            const char* str = tosc_getNextString(&osc);

                            helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Index: %d Value: %s", index, str);

                            parameter->Set(str, index);
                            config->SetParameterChanged(parameter->GetId(), index);
                        }
                        else if (osc_format == "f")
                        {
                            int index = stoi(address[2].c_str()) - 1;
                            float value = tosc_getNextFloat(&osc);

                            helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Index: %d Value: %f", index, value);

                            parameter->Set(value, index);
                            config->SetParameterChanged(parameter->GetId(), index);
                        }
                        else 
                        {
                            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "ERROR: unsupported format %s", osc_format.c_str());
                        }
                    }
                    else if (address[0] == "change")
                    {
                        // Change
                        if (osc_format == "f")
                        {
                            int index = stoi(address[2].c_str()) - 1;
                            int amount = tosc_getNextFloat(&osc);

                            helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Index: %d Amount: %d", index, amount);

                            parameter->Change(amount, index);
                            config->SetParameterChanged(parameter->GetId(), index);
                        }
                        else
                        {
                            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "ERROR: unsupported format %s", osc_format.c_str());
                        }
                    }
                    else if (address[0] == "toggle")
                    {
                        // Toggle (format is ignored)
                        int index = stoi(address[2].c_str()) - 1;

                        helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Index: %d", index);

                        parameter->Toggle(index);
                        config->SetParameterChanged(parameter->GetId(), index);
                    }
                    else if (address[0] == "reset")
                    {
                        // Change (format is ignored)
                        int index = stoi(address[2].c_str()) - 1;

                        helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Index: %d", index);

                        parameter->Reset(index);
                        config->SetParameterChanged(parameter->GetId(), index);
                    }
                }
                else if (found > 1) 
                {
                    helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "ERROR: more than 1 Mixerparameter matches");
                }
                else 
                {
                    helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Received unsupported command: >>%s<<", rxData);
                }
            }
            else
            {
                helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Could not parse OSC-Message");
            }
        }
    }

    void OscServer::ExpireObservers()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto it = surfaceObservers.begin(); it != surfaceObservers.end(); )
        {
            if (now - it->second >= std::chrono::seconds(10)) { it = surfaceObservers.erase(it); }
            else { ++it; }
        }
    }

    void OscServer::ObserveSurface(SurfaceElement* element, const char* action, float value)
    {
        ExpireObservers();
        String name = "";
        bool separator = false;
        String label = element->GetName();
        for (uint i = 0; i < label.length(); ++i)
        {
            char c = label[i];
            if (c >= 'A' && c <= 'Z') { c += 'a' - 'A'; }
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            {
                if (separator && name.length()) { name += '_'; }
                name += c;
                separator = false;
            }
            else { separator = true; }
        }
        String path = String("/") + action + "/" + name + "/1";
        int size = tosc_writeMessage(TxMessage, sizeof(TxMessage), path.c_str(), "f", value);
        if (size <= 0) { return; }
        for (const auto& [ip, renewed] : surfaceObservers)
        {
            sockaddr_in target{};
            target.sin_family = AF_INET;
            target.sin_addr.s_addr = ip;
            target.sin_port = htons(10033);
            sendto(UdpHandle, TxMessage, size, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
        }
    }

#ifdef TARGET_WING
    void OscServer::HandleWingUi(std::string_view packet)
    {
        if (packet.front() != '/') return;
        const auto addressEnd = packet.find('\0');
        if (addressEnd == std::string_view::npos) return;
        const auto tag = (addressEnd + 4) & ~size_t{3};
        if (tag + 4 > packet.size()) return;
        const auto address = packet.substr(0, addressEnd);
        const auto format = packet.substr(tag, 4);
        const auto payload = packet.substr(tag + 4);
        if (format == std::string_view(",\0\0\0", 4) && payload.empty())
            ReplyWingUi(address);
        else if (format == std::string_view(",f\0\0", 4) && payload.size() == 4 && address == "/subscribe/state/1")
            ReplyWingUi("/get/state/1");
        else if (format == std::string_view(",s\0\0", 4) && address.starts_with(renamePrefix))
            RenameWingInput(address.substr(renamePrefix.size()), payload);
    }

    void OscServer::RenameWingInput(std::string_view index, std::string_view payload)
    {
        unsigned channel = 0;
        const auto [end, error] = std::from_chars(index.data(), index.data() + index.size(), channel);
        if (error != std::errc{} || end != index.data() + index.size() || channel == 0 || channel > wingInputs) return;
        const auto nul = payload.find('\0');
        const auto name = payload.substr(0, nul);
        const bool valid = nul != std::string_view::npos && payload.size() == ((nul + 4) & ~size_t{3}) &&
            payload.find_first_not_of('\0', nul) == std::string_view::npos && ValidWingName(name);
        std::string value;
        if (valid) {
            config->Set(CHANNEL_NAME, String(std::string(name).c_str()), channel - 1);
            value = config->GetString(CHANNEL_NAME, channel - 1).c_str();
        } else {
            value = "Use a non-blank name, up to 32 UTF-8 bytes, without control characters.";
        }
        const auto packet = WingStringPacket((valid ? "/result/ch_name/" : "/result/ch_name_error/") + std::to_string(channel), value);
        SendUdpPacket(packet.data(), packet.size());
    }

    void OscServer::ReplyWingUi(std::string_view request)
    {
        // Only the input bank of the current OMC channel layout is exported.
        const auto send = [this](int bytes) {
            if (bytes > 0 && bytes <= int(sizeof TxMessage)) SendUdpPacket(TxMessage, bytes);
        };
        const auto string = [&](const char* address, const char* value) {
            send(tosc_writeMessage(TxMessage, sizeof TxMessage, address, "s", value));
        };
        const bool topology = request == "/get/topology/1";
        const bool metadata = request == "/get/metadata/1";
        const bool snapshot = request == "/get/state/1";
        if (request == "/get/t55_matrix/1") {
            const auto view = WingStringPacket("/set/t55_matrix/1",
                wingT55 ? wingT55->RoutingView() : WingT55Control::OfflineRoutingView());
            SendUdpPacket(view.data(), view.size());
            return;
        }
        if (!(topology || metadata || snapshot)) return;
        if (topology) {
            // The reference catalog is not part of either routable endpoint array.
            // SmallOSC can truncate strings larger than half its output buffer.
            static const auto packet = WingStringPacket("/set/topology/1",
                R"({"strips":[{"kind":"input","name":"Channels","start":0,"count":32}],"sources":[],"outputs":[],"capabilities":["ch_name"],"t55_matrix":true,"local_io":)"
                + WingLocalIo::ReferenceJson() + '}');
            SendUdpPacket(packet.data(), packet.size());
            return;
        }

        // Repeat metadata ahead of each snapshot; the UI discards its old
        // catalog when it accepts a new topology. This is software state only.
        struct Field { MP_ID id; const char* osc; };
        constexpr Field fields[] = {{CHANNEL_NAME, "ch_name"}, {CHANNEL_VOLUME, "fader"}, {CHANNEL_MUTE, "ch_mute"}};
        for (auto [id, osc] : fields) {
            auto* parameter = config->GetParameter(id);
            char address[64], json[160];
            snprintf(address, sizeof address, "/set/metadata/%s/1", osc);
            snprintf(json, sizeof json, R"({"instances":%u,"readonly":%s,"audio_status":"%s"})", wingInputs,
                id == CHANNEL_NAME ? "false" : "true", id == CHANNEL_NAME ? "software_only" : "state_only_no_dispatch");
            string(address, json);
            if (!snapshot) continue;
            for (unsigned i = 0; i < wingInputs; ++i) {
                snprintf(address, sizeof address, "/set/%s/%u", osc, i + 1);
                if (id == CHANNEL_NAME) string(address, parameter->GetString(i).c_str());
                else send(tosc_writeMessage(TxMessage, sizeof TxMessage, address, "f", parameter->GetFloat(i)));
            }
        }
        if (snapshot) send(tosc_writeMessage(TxMessage, sizeof TxMessage, "/state/complete/1", ""));
    }
#endif

    void OscServer::Sync()
    {
#ifdef TARGET_WING
        if (wingUi) return;
#endif
        if (!hasClients)
        {
            return;
        }

        for (auto const& [parameter_id, indexSet] : *(config->GetChangedParameterList()))
        {
            // loop parameter_id
            Mixerparameter* parameter = config->GetParameter(parameter_id);

            // only parameters with an OSC path are part of the protocol
            if (!parameter->IsOSC())
            {
                continue;
            }

            // for (indexes)
            for (uint index : indexSet)
            {
                // for (clients)
                // for (auto const& [client, timeoutvalue] : clients)
                // {
                    int len = 0;
                    if (parameter->GetType() == MP_VALUE_TYPE::STRING)
                    {
                        len = tosc_writeMessage(TxMessage, 450, (String("/set/") + parameter->GetOSC() + "/" + String(index + 1)).c_str(), "s", parameter->GetString(index).c_str());
                    }
                    else
                    {
                        len = tosc_writeMessage(TxMessage, 450, (String("/set/") + parameter->GetOSC() + "/" + String(index + 1)).c_str(), "f", parameter->GetFloat(index));
                    }
                
                    SendUdpPacket(TxMessage, len);
                //}
            }
        }
    }


    ////////////////////////////////////
    // low-level communication functions
    ////////////////////////////////////

    void OscServer::SendUdpPacket(const char* buffer, uint16_t size)
    {
        helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "OSC-Message (raw, size=%d): %s", size, buffer);

        ClientAddr.sin_port = htons(10033);;

        size_t len_send = sendto(
            UdpHandle,
            buffer,
            size,
            0,
            (struct sockaddr *) &ClientAddr,
            sizeof(ClientAddr)
        );

        if (len_send == 0)
        {
            helper->Error("eee");
        }

        if (helper->DEBUG_OSC(DEBUGLEVEL_TRACE) && size > 0)
        {
            char str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ClientAddr.sin_addr), str, INET_ADDRSTRLEN);

            helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Sent OSC-Message to Client: %s Port: %d", str, ntohs(ClientAddr.sin_port));
        }
    }
}
