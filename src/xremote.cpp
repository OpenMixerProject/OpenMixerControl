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
  

  This file offers a server for the XRemote-protocol

  The XRemote-protocol is used to communicate with the XEdit software.
  The communication is based on OSC-protocol.

  Parts of this file are based on the "Unofficial X32/M32 OSC Remote Protocol" v4.02 by Patrick-Gilles Maillot.
  Thank you very much for sharing your work!

  Linux UDP examples:
  - https://gist.github.com/saxbophone/f770e86ceff9d488396c0c32d47b757e
  - https://openbook.rheinwerk-verlag.de/linux_unix_programmierung/Kap11-016.htm
*/

#include "xremote.h"

#include "version.h"

#include <oscpp/server.hpp>
#include <oscpp/print.hpp>
#include <iostream>
#include <array>

const size_t kMaxPacketSize = 8192;

XRemote::XRemote(X32BaseParameter* basepar) : X32Base(basepar) 
{

}


int8_t XRemote::Init() {
    if ((UdpHandle = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        //fprintf(stderr, "Error on creating UDP-socket!");
        return -1;
    }
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(10023);
    
    if (bind(UdpHandle, (const struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) < 0) {
        //fprintf(stderr, "Error on binding UDP-socket!");
        close(UdpHandle);
        return -1;
    }
    
    return 0;
}

void XRemote::UdpHandleCommunication() 
{
    char rxData[500];
    int bytes_available = 0;
    uint8_t channel;
    data_32b value32bit;
    
    // check for bytes in UDP-buffer
    int result = ioctl(UdpHandle, FIONREAD, &bytes_available);
    if (bytes_available > 0) {
        socklen_t xremoteClientAddrLen = sizeof(ClientAddr);
        
        std::array<char,kMaxPacketSize> buffer;
        ssize_t size = recvfrom(UdpHandle, buffer.data(), bytes_available, MSG_WAITALL, (struct sockaddr *) &ClientAddr, &xremoteClientAddrLen);
        handlePacket(OSCPP::Server::Packet(buffer.data(), size));


// 		tosc_message osc;

// 		if (!tosc_parseMessage(&osc, rxData, len)) {
// 			string adrPath = string(tosc_getAddress(&osc));
//     		vector<string> address = helper->split(adrPath, "/");
// 			address.erase(address.begin()); // delete empty element
// 			string format = string(tosc_getFormat(&osc));

// 			if (address[0] == "renew") {
//                 //fprintf(stdout, "Received command: %s\n", rxData);
//             } else if (address[0] == "info") {
//                 xremote->AnswerInfo();
//             } else if (address[0] == "xinfo") {
//                 xremote->AnswerXInfo();
//             } else if (address[0] == "status") {
//                 xremote->AnswerStatus();
//             } else if (address[0] == "xremote") {
//                 // Optional: read and store IP-Address of client
// 				//xremoteSync(true);
//             } else if (address[0] == "unsubscribe") {
//                 // Optional: remove xremote client
//             } else if (address[0] == "ch") {
//                 // /ch/xx/mix/fader~~~~,f~~[float]
//                 // /ch/xx/mix/pan~~,f~~[float]
//                 // /ch/xx/mix/on~~~,i~~[int]

//                 //channel = ((rxData[4]-48)*10 + (rxData[5]-48)) - 1;
// 				channel = stoi(address[1]);

// 				if (address[2] == "mix") {
// 					if (address[3] == "fader") {
// 						float newVolume = tosc_getNextFloat(&osc);
// 						//mixer->SetVolumeOscvalue(channel-1, newVolume);
// 						helper->DEBUG_XREMOTE(DEBUGLEVEL_VERBOSE, "Ch %u: Volume set to %f\n", channel, (double)newVolume);
// 					}else if (address[3] == "pan") {
// 						// get pan-value
// 						value32bit.u8[0] = rxData[23];
// 						value32bit.u8[1] = rxData[22];
// 						value32bit.u8[2] = rxData[21];
// 						value32bit.u8[3] = rxData[20];
						
// 						//encoderValue = value32bit.f * 255.0f;
// 						//mixer->SetBalance(channel,  value32bit.f * 100.0f);
// 						helper->DEBUG_XREMOTE(DEBUGLEVEL_VERBOSE, "Ch %u: Balance set to %f\n",  channel+1, (double)(value32bit.f * 100.0f));
// 					}else if (address[3] == "on") {
// 						// get mute-state (caution: here it is "mixer-on"-state)
// 						//mixer->SetMute(channel, (rxData[20+3] == 0));
// 						helper->DEBUG_XREMOTE(DEBUGLEVEL_VERBOSE, "Ch %u: Mute set to %u\n",  channel+1, (rxData[20+3] == 0));
// 					}
// 				}else if ((rxData[7] == 'c') && (rxData[8] == 'o') && (rxData[9] == 'n')) {
// 					// config
// 					if  ((rxData[14] == 'c') && (rxData[15] == 'o') && (rxData[16] == 'l')) {
// 						// color
// 						value32bit.u8[0] = rxData[27];
// 						value32bit.u8[1] = rxData[26];
// 						value32bit.u8[2] = rxData[25];
// 						value32bit.u8[3] = rxData[24];
						
// 						if (value32bit.u32 < 8) {
// 							//fprintf(stdout, "Ch %u: Set color to %u\n",  channel+1, value32bit.u32);
// 						}else{
// 							//fprintf(stdout, "Ch %u: Set inverted color to %u\n",  channel+1, value32bit.u32 - 8 +64);
// 						}
// 					}else if  ((rxData[14] == 'n') && (rxData[15] == 'a') && (rxData[16] == 'm')) {
// 						// name
// 						String name = String(&rxData[24]);
// 						//fprintf(stdout, "Ch %u: Set name to %s\n",  channel+1, name.c_str());
// 					}else if  ((rxData[14] == 'i') && (rxData[15] == 'c') && (rxData[16] == 'o')) {
// 						// icon
// 						value32bit.u8[0] = rxData[27];
// 						value32bit.u8[1] = rxData[26];
// 						value32bit.u8[2] = rxData[25];
// 						value32bit.u8[3] = rxData[24];
						
// 						// do something with channel and value32bit.f
// 						//Serial.println("/ch/" + String(channel) + "/config/icon " + String(value32bit.u32));
// 						//fprintf(stdout, "Ch %u: Set icon to %u\n",  channel+1, value32bit.u32);
// 					}
// 				}
//             } else if (address[0] == "main") {
//                 // /main/st/mix/fader~~,f~~[float]
//                 // /main/st/mix/pan~~~~,f~~[float]
//                 // /main/st/mix/on~,i~~[int]
//                 if (len > 12) {
//                     if ((rxData[6] == 's') && (rxData[7] == 't') && (rxData[9] == 'm') && (rxData[10] == 'i') && (rxData[11] == 'x')) {
//                         if ((rxData[13] == 'f') && (rxData[14] == 'a') && (rxData[15] == 'd')) {
//                             // get fader-value
//                             value32bit.u8[0] = rxData[27];
//                             value32bit.u8[1] = rxData[26];
//                             value32bit.u8[2] = rxData[25];
//                             value32bit.u8[3] = rxData[24];
                            
//                             //float newVolume = (value32bit.f * 54.0f) - 48.0f;
//                             //mixerSetMainVolume(newVolume);
//                         }else if ((rxData[13] == 'p') && (rxData[14] == 'a') && (rxData[15] == 'n')) {
//                             // get pan-value
//                             value32bit.u8[0] = rxData[27];
//                             value32bit.u8[1] = rxData[26];
//                             value32bit.u8[2] = rxData[25];
//                             value32bit.u8[3] = rxData[24];
                            
//                             //mixerSetMainBalance(value32bit.f * 100);
//                         }else if ((rxData[13] == 'o') && (rxData[14] == 'n')) {
//                             // get mute-state
//                             // /main/st/mix/on~,i~~~
//                             // do something with channel and (rxData[20+3]) // 0 = mute off, 31 = mute on
//                         }
//                     }
//                 }
//             }else if (memcmp(rxData, "/-st", 4) == 0) {
//                 // stat
                
//                 if ((rxData[7] == 's') && (rxData[8] == 'o') && (rxData[9] == 'l') && (rxData[10] == 'o') && (rxData[11] == 's') && (rxData[12] == 'w')) {
//                     // /-stat/solosw/xx~~~~,i~~[integer]
//                     channel = ((rxData[14]-48)*10 + (rxData[15]-48)) - 1;
//                     value32bit.u8[0] = rxData[27];
//                     value32bit.u8[1] = rxData[26];
//                     value32bit.u8[2] = rxData[25];
//                     value32bit.u8[3] = rxData[24];
                    
//                     // we receive solo-values for 80 channels
// /*
//                     if (channel < 32) {
//                         mixerSetSolo(channel, (value32bit.u32 == 1));
//                     }
// */
//                 }else if ((rxData[7] == 'u') && (rxData[8] == 'r') && (rxData[9] == 'e') && (rxData[10] == 'c')) {
//                     value32bit.u8[0] = rxData[27];
//                     value32bit.u8[1] = rxData[26];
//                     value32bit.u8[2] = rxData[25];
//                     value32bit.u8[3] = rxData[24];
                    
//                     // /-stat/urec/state~~~,i~~[integer]
//                     if (value32bit.u32 == 0) {
//                         // stop
//                     }else if (value32bit.u32 == 1) {
//                         // pause
//                     }else if (value32bit.u32 == 2) {
//                         // play
//                     }else if (value32bit.u32 == 3) {
//                         // record
//                     }
//                 }
                
//                 //fprintf(stdout, "Received command: %s\n", rxData);
//             }else if (memcmp(rxData, "/bat", 4) == 0) {
//             }else if (memcmp(rxData, "/ren", 4) == 0) {
//             }else if (memcmp(rxData, "/for", 4) == 0) {
//             }else{
// 				//xremote->AnswerAny();
//                 // ignore unused commands for now
//                 //fprintf(stdout, "Received unsupported command: %s\n", rxData);
//             }
//         }else{
//             //fprintf(stdout, "Caution: len <= 0");
//         }



// 			// tosc_getFormat(&osc)); // the OSC format string, e.g. "f"
// 			// 	for (int i = 0; osc.format[i] != '\0'; i++) {
// 			// 		switch (osc.format[i]) {
// 			// 			case 'f': printf("%g ", tosc_getNextFloat(&osc)); break;
// 			// 			case 'i': printf("%i ", tosc_getNextInt32(&osc)); break;
// 			// 			// returns NULL if the buffer length is exceeded
// 			// 			case 's': printf("%s ", tosc_getNextString(&osc)); break;
// 			// 			default: continue;
// 			// 	}
// 		    // }
// 			// printf("\n");
	}


}



void XRemote::handlePacket(const OSCPP::Server::Packet& packet)
{
    if (packet.isBundle()) {
        // Convert to bundle
        OSCPP::Server::Bundle bundle(packet);

        // Print the time
        std::cout << "#bundle " << bundle.time() << std::endl;

        // Get packet stream
        OSCPP::Server::PacketStream packets(bundle.packets());

        // Iterate over all the packets and call handlePacket recursively.
        // Cuidado: Might lead to stack overflow!
        while (!packets.atEnd()) {
            handlePacket(packets.next());
        }
    } else {
        // Convert to message
        OSCPP::Server::Message msg(packet);

        // Get argument stream
        OSCPP::Server::ArgStream args(msg.args());

        // Directly compare message address to string with operator==.
        // For handling larger address spaces you could use e.g. a
        // dispatch table based on std::unordered_map.
        if (msg == "/s_new") {
            const char* name = args.string();
            const int32_t id = args.int32();
            std::cout << "/s_new" << " "
                      << name << " "
                      << id << " ";
            // Get the params array as an ArgStream
            OSCPP::Server::ArgStream params(args.array());
            while (!params.atEnd()) {
                const char* param = params.string();
                const float value = params.float32();
                std::cout << param << ":" << value << " ";
            }
            std::cout << std::endl;
        } else if (msg == "/n_set") {
            const int32_t id = args.int32();
            const char* key = args.string();
            // Numeric arguments are converted automatically
            // to float32 (e.g. from int32).
            const float value = args.float32();
            std::cout << "/n_set" << " "
                      << id << " "
                      << key << " "
                      << value << std::endl;
        } else {
            // Simply print unknown messages
            std::cout << "Unknown message: " << msg << std::endl;
        }
    }
}


void XRemote::AnswerInfo() {
    uint16_t len = sprint(TxMessage, 0, 's', "/info");
    len = sprint(TxMessage, len, 's', ",ssss");
    len = sprint(TxMessage, len, 's', GIT_VERSION);
    len = sprint(TxMessage, len, 's', "OpenX32");
    len = sprint(TxMessage, len, 's', "X32"); // must be a known device by X-Edit
    len = sprint(TxMessage, len, 's', "4.13"); // must be a supported firmware-version by X-Edit

    SendUdpPacket(TxMessage, len);
}

void XRemote::AnswerXInfo() {
    uint16_t len = sprint(TxMessage, 0, 's', "/xinfo");
    len = sprint(TxMessage, len, 's', ",ssss");
    len = sprint(TxMessage, len, 's', helper->getIpAddress().c_str());
    len = sprint(TxMessage, len, 's', "OpenX32");
    len = sprint(TxMessage, len, 's', "X32"); // must be a known device by X-Edit
    len = sprint(TxMessage, len, 's', "4.13"); // must be a supported firmware-version by X-Edit

    SendUdpPacket(TxMessage, len);
}

void XRemote::AnswerStatus() {
    uint16_t len = sprint(TxMessage, 0, 's', "/status");
    len = sprint(TxMessage, len, 's', ",sss");
    len = sprint(TxMessage, len, 's', "active");
    len = sprint(TxMessage, len, 's', helper->getIpAddress().c_str());
    len = sprint(TxMessage, len, 's', "OpenX32");

    SendUdpPacket(TxMessage, len);
}

void XRemote::SetFader(String type, uint8_t ch, float value) {
    char cmd[32] = {0};
    sprintf(cmd, "/%s/%02i/mix/fader", type.c_str(), ch+1);
    
    //SendBasicMessage((String("/") + type + String("/") + String() + String("/mix/fader")).c_str(), 'f', 'b', (char*)&value_pu);
    SendBasicMessage(cmd, 'f', 'b', (char*)&value);
}

void XRemote::SetPan(uint8_t ch, float value_pu) {
    char cmd[32] = {0};
    sprintf(cmd, "/ch/%02i/mix/pan", ch);
    SendBasicMessage(cmd, 'f', 'b', (char*)&value_pu);
}

void XRemote::SetMainFader(float value_pu) {
    SendBasicMessage("/main/st/mix/fader", 'f', 'b', (char*)&value_pu);
}

void XRemote::SetMainPan(float value_pu) {
    SendBasicMessage("/main/st/mix/pan", 'f', 'b', (char*)&value_pu);
}

void XRemote::SetName(uint8_t vchannelIndex, String name) {
    char nameArray[12] = {0};
    char cmd[50] = {0};
    name.toCharArray(nameArray, 12);
    sprintf(cmd, "/ch/%02i/config/name", vchannelIndex+1);
    SendBasicMessage(cmd, 's', 's', nameArray);
}

// 0=BLACK, 1=RED, 2=GREEN, 3=YELLOW, 4=BLUE, 5=PINK, 6=CYAN, 7=WHITE (add 64 to invert)
void XRemote::SetColor(uint8_t ch, int32_t color) {
    char cmd[32] = {0};
    sprintf(cmd, "/ch/%02i/config/color", ch);
    SendBasicMessage(cmd, 'i', 'b', (char*)&color);
}

void XRemote::SetSource(uint8_t ch, int32_t source) {
    char cmd[32] = {0};
    sprintf(cmd, "/ch/%02i/config/source", ch);
    SendBasicMessage(cmd, 'i', 'b', (char*)&source);
}

void XRemote::SetIcon(uint8_t ch, int32_t icon) {
    char cmd[32] = {0};
    sprintf(cmd, "/ch/%02i/config/icon", ch);
    SendBasicMessage(cmd, 'i', 'b', (char*)&icon);
}

void XRemote::SetCard(uint8_t card) {
    char cmd[32] = {0};
    String scmd = String("-stat/xcardtype ") + String(card);
    scmd.toCharArray(cmd, scmd.length() + 1);
    cmd[scmd.length()] = 0x10; // add linefeed
    SendBasicMessage("node", 's', 's', cmd);
}

void XRemote::SetMute(uint8_t ch, uint8_t muted) {
    char cmd[32] = {0};
    int32_t online;
    if (muted == 0) {
      online = 1;
    }else{
      online = 0;
    }
    sprintf(cmd, "/ch/%02i/mix/on", ch);
    SendBasicMessage(cmd, 'i', 'b', (char*)&online);
}

void XRemote::SetSolo(uint8_t ch, uint8_t solo) {
    char cmd[20] = {0};
    String channel;
    String str_state;
    if (ch < 10) {
      channel = "0" + String(ch);
    }else{
      channel = String(ch);
    }
    if (solo > 0) {
      str_state = String("ON");
    }else{
      str_state = String("OFF");
    }
    String scmd = String("-stat/solosw/") + channel + String(" ") + str_state;
    scmd.toCharArray(cmd, 20);
    cmd[scmd.length()] = 0x10; // add linefeed
    SendBasicMessage("node", 's', 's', cmd);
}

void XRemote::UpdateMeter(Mixer* mixer) {
    int32_t value;

    uint16_t len = sprint(TxMessage, 0, 's', "meters/0");
    len = sprint(TxMessage, len, 's', ",b"); // 4 chars
    value = (70 + 1)*4; // number of bytes
    len = sprint(TxMessage, len, 'b', (char*)&value); // big-endian
    value = 70; // number of floats
    len = sprint(TxMessage, len, 'l', (char*)&value); // little endian

    float f;
    for (uint16_t i=0; i<40; i++) {
      // TODO use right type
      len = sprint(TxMessage, len, 'l', String(mixer->dsp->rChannel[i].meterPu).c_str()); // little endian
    }
    for (uint16_t i=40; i<70; i++) {
      f = (float)rand()/(float)RAND_MAX; // 32 channels, 8 aux, 8 FX returns, 16 busse, 6 matrix
      len = sprint(TxMessage, len, 'l', (char*)&f); // little endian
    }

    SendUdpPacket(TxMessage, len);
}

////////////////////////////////////
// low-level communication functions
////////////////////////////////////

void XRemote::SendUdpPacket(char* buffer, uint16_t size) {
    sendto(
        UdpHandle,
        buffer,
        size,
        0,
        (struct sockaddr *) &ClientAddr,
        sizeof(ClientAddr)
    );
}

void XRemote::SendBasicMessage(const char* cmd, char type, char format, char* value) {
    char tmp[3];
    tmp[0] = ',';
    tmp[1] = type;
    tmp[2] = 0;
    
    uint16_t len = sprint(TxMessage, 0, 's', cmd);
    len = sprint(TxMessage, len, 's', tmp);
    len = sprint(TxMessage, len, format, value);

    if (helper->DEBUG_XREMOTE()) {
		for (uint8_t i=0; i < len;i++){
			if (TxMessage[i] == 0){
				printf("~");
			} else {
				printf("%c", TxMessage[i]);
			}
	    }
    	printf("\n");
    }

    SendUdpPacket(TxMessage, len);
}

uint16_t XRemote::sprint(char* bd, uint16_t index, char format, const char* bs) {
    /*
      Based on the work of Patrick-Gilles Maillot
      https://github.com/pmaillot/X32-Behringer/blob/master/X32.c
    */
    
    int i;
    // check format
    switch (format) {
        case 's': // string : copy characters one at a time until a 0 is found
            if (bs) {
                strcpy(bd+index, bs);
                index += (int)strlen(bs) + 1;
            } else {
                bd[index++] = 0;
            }
            // align to 4 bytes boundary if needed
            while (index & 3) bd[index++] = 0;
            break;
        case 'b': // float or int : copy the 4 bytes of float or int in big-endian order
            i = 4;
            while (i > 0)
                bd[index++] = (char)(bs[--i]);
            break;
        case 'l': // float or int : copy the 4 bytes of float or int in little-endian order
            i = 0;
            while (i < 4)
                bd[index++] = (char)(bs[i++]);
            break;
        default:
            // don't copy anything
            break;
    }
    return index;
}

uint16_t XRemote::fprint(char* bd, uint16_t index, char* text, char format, char* bs) {
    /*
      Based on the work of Patrick-Gilles Maillot
      https://github.com/pmaillot/X32-Behringer/blob/master/X32.c
    */
    
    // first copy text
    strcpy (bd+index, text);
    index += (int)strlen(text) + 1;
    // align to 4 bytes boundary if needed
    while (index & 3) bd[index++] = 0;
    // then set format, keeping #4 alignment
    bd[index++] = ',';
    bd[index++] = format;
    bd[index++] = 0;
    bd[index++] = 0;
    // based on format, set value
    return sprint(bd, index, format, bs);
}
