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
#include <cstddef>

namespace OMC
{
    using namespace WString;

    OscServer::OscServer(X32BaseParameter* basepar) : X32Base(basepar) 
    {
    }

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
        ServerAddr.sin_port = htons(10032);
        
        if (bind(UdpHandle, (const struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) < 0)
        {
            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Error on binding UDP-socket!");
            close(UdpHandle);
            return -1;
        }

        // Build map of osc-paths from Mixerparameters
        oscPaths = config->GetOscPaths();
        config->oscCapabilities.insert("surface_subscription");
        config->surfaceObserver = [this](SurfaceElement* element, const char* action, float value)
        {
            ObserveSurface(element, action, value);
        };
        
        return 0;
    }

    void OscServer::UdpHandleCommunication() 
    {
        ExpireObservers();
        char rxData[500];
        int bytes_available = 0;
        
        // check for bytes in UDP-buffer
        ioctl(UdpHandle, FIONREAD, &bytes_available);
        
        if (bytes_available > 0)
        {
            socklen_t xremoteClientAddrLen = sizeof(ClientAddr);
        
            ssize_t len = recvfrom(UdpHandle, rxData, sizeof(rxData), 0, (struct sockaddr *) &ClientAddr, &xremoteClientAddrLen);
            if (len <= 0 || len >= sizeof(rxData)) { return; }

            tosc_message osc;
            if (!tosc_parseMessage(&osc, rxData, len))
            {
                // Client has send data

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

    void OscServer::Sync()
    {
        if (!hasClients)
        {
            return;
        }

        for (auto const& [parameter_id, indexSet] : *(config->GetChangedParameterList()))
        {
            // loop parameter_id
            Mixerparameter* parameter = config->GetParameter(parameter_id);

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

    void OscServer::SendUdpPacket(char* buffer, uint16_t size)
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
