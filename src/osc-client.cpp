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

#include "osc-client.h"
#include "../lib_ext/small-osc/small-osc.h"
#include "mixerparameter.h"

namespace OMC
{

    OscClient::OscClient(X32BaseParameter* basepar) : X32Base(basepar) 
    {
    }

    int8_t OscClient::Init()
    {
        // Bind on UDP Port 10032

        if ((UdpHandle = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Error on creating UDP-socket!");
            return -1;
        }

        string server_ip = app->get_option("--client")->as<string>();

        ServerAddr.sin_family = AF_INET;
        ServerAddr.sin_addr.s_addr = inet_addr(server_ip.c_str());
        ServerAddr.sin_port = htons(10032);
        
        MyAddr.sin_family = AF_INET;
        MyAddr.sin_addr.s_addr = INADDR_ANY;
        MyAddr.sin_port = htons(10033);
        
        if (bind(UdpHandle, (const struct sockaddr *)&MyAddr, sizeof(MyAddr)) < 0)
        {
            helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Error on binding UDP-socket!");
            close(UdpHandle);
            return -1;
        }

        helper->Log("OSC Client listening on Port %d\n", ntohs(MyAddr.sin_port));

        // Build map of osc-paths from Mixerparameters
        oscPaths = config->GetOscPaths();
        
        return 0;
    }

    void OscClient::UdpHandleCommunication() 
    {
        char rxData[500];
        int bytes_available = 0;
        
        // check for bytes in UDP-buffer
        ioctl(UdpHandle, FIONREAD, &bytes_available);
        
        if (bytes_available > 0)
        {
            socklen_t serverAddrLen = sizeof(ServerAddr);
        
            size_t len = recvfrom(UdpHandle, rxData, bytes_available, MSG_WAITALL, (struct sockaddr *) &ServerAddr, &serverAddrLen);

            tosc_message osc;
            if (!tosc_parseMessage(&osc, rxData, len))
            {
                // Server has send data

                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(ServerAddr.sin_addr), str, INET_ADDRSTRLEN);

                helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Received OSC-Message from Server: %s Port: %d", str, ntohs(ServerAddr.sin_port));

                string adrPath = string(tosc_getAddress(&osc));
                vector<string> address = helper->split(adrPath, "/");
                address.erase(address.begin()); // delete empty element

                String osc_format = String(tosc_getFormat(&osc));

                helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "  Path: %s Format: %s", adrPath.c_str(), osc_format.c_str());

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
                    helper->DEBUG_OSC(DEBUGLEVEL_NORMAL, "Received unsupported command: %s", rxData);
                }
            }
            else
            {
                helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Could not parse OSC-Message");
            }
        }
    }

    void OscClient::UdpSendToServerSet(MP_ID parameterId, WString::String strValue, float floatValue, uint index)
    {
        Mixerparameter* parameter = config->GetParameter(parameterId);
        
        int len = tosc_writeMessage(TxMessage, 450, (String("/set/") + parameter->GetOSC() + "/" + String(index + 1)).c_str(), "f", floatValue);
        SendUdpPacket(TxMessage, len);
    }

    void OscClient::UdpSendToServerChange(MP_ID parameterId, int amount, uint index)
    {
        Mixerparameter* parameter = config->GetParameter(parameterId);
        
        int len = tosc_writeMessage(TxMessage, 450, (String("/change/") + parameter->GetOSC() + "/" + String(index + 1)).c_str(), "f", (float)amount);
        SendUdpPacket(TxMessage, len);
    }

    void OscClient::UdpSendToServerToogle(MP_ID parameterId, uint index)
    {
        Mixerparameter* parameter = config->GetParameter(parameterId);
        
        int len = tosc_writeMessage(TxMessage, 450, (String("/toggle/") + parameter->GetOSC() + "/" + String(index + 1)).c_str(), "");
        SendUdpPacket(TxMessage, len);
    }

    void OscClient::UdpSendToServerReset(MP_ID parameterId, uint index)
    {
        Mixerparameter* parameter = config->GetParameter(parameterId);
        
        int len = tosc_writeMessage(TxMessage, 450, (String("/reset/") + parameter->GetOSC() + "/" + String(index + 1)).c_str(), "");
        SendUdpPacket(TxMessage, len);
    }


    ////////////////////////////////////
    // low-level communication functions
    ////////////////////////////////////

    void OscClient::SendUdpPacket(char* buffer, uint16_t size)
    {
        helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "OSC-Message (raw): %s", buffer);

        sendto(
            UdpHandle,
            buffer,
            size,
            0,
            (struct sockaddr *) &ServerAddr,
            sizeof(ServerAddr)
        );

        if (helper->DEBUG_OSC(DEBUGLEVEL_TRACE) && size > 0)
        {
            char str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ServerAddr.sin_addr), str, INET_ADDRSTRLEN);

            helper->DEBUG_OSC(DEBUGLEVEL_TRACE, "Sent OSC-Message to Server: %s Port: %d", str, ntohs(ServerAddr.sin_port));
        }
    }

    void OscClient::SendBasicMessage(const char* cmd, char type, char format, char* value)
    {
        // char tmp[3];
        // tmp[0] = ',';
        // tmp[1] = type;
        // tmp[2] = 0;
        
        // uint16_t len = sprint(TxMessage, 0, 's', cmd);
        // len = sprint(TxMessage, len, 's', tmp);
        // len = sprint(TxMessage, len, format, value);

        // if (helper->DEBUG_OSC()) {
        // 	for (uint8_t i=0; i < len;i++){
        // 		if (TxMessage[i] == 0){
        // 			printf("~");
        // 		} else {
        // 			printf("%c", TxMessage[i]);
        // 		}
        //     }
        // 	printf("\n");
        // }

        // SendUdpPacket(TxMessage, len);
    }
}