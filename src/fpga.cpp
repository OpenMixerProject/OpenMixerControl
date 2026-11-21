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

#include "fpga.h"

namespace OMC
{

Fpga::Fpga(X32BaseParameter* basepar): X32Base(basepar) {
	uart = new Uart(basepar);
	spi = new SPI(basepar);
	fpgaSpiRegsAvailable = false;
	fpgaCaps = 0;
	fpgaCtrlReg = 0x00000014u; // Port A & B Master, 48 kHz, Internal 16 MHz clock
	aes50SpiRxLen[0] = 0;
	aes50SpiRxLen[1] = 0;
	memset(AES50PhantomPowerState, 0, sizeof(AES50PhantomPowerState));
	memset(AES50HeadampGains, 0, sizeof(AES50HeadampGains));

	if (!(state->bodyless || config->IsModelAnyWing()))
	{
    	if (spi->UploadBitstreamFpgaLattice() == -1)
		{
			// uploading the Lattice-Bitstream was not successful (or not set), so try to upload the Xilinx-bitstream
	        spi->UploadBitstreamFpgaXilinx();
	    }
	    spi->OpenConnectionFpga();

	    uint32_t magic = 0;
	    if (spi->FpgaReadReg32(FPGA_REG_MAGIC, &magic) && magic == FPGA_MAGIC_OX32) {
	        fpgaSpiRegsAvailable = true;
	        spi->FpgaReadReg32(FPGA_REG_CAPS, &fpgaCaps);
	        spi->FpgaReadReg32(FPGA_REG_CTRL, &fpgaCtrlReg);
	        helper->DEBUG_FPGA(DEBUGLEVEL_NORMAL,
	            "OpenX32 SPI register bridge detected (CAPS=0x%08x, PortB=%u, SRC=%u/%u, CS2K=%u)",
	            fpgaCaps, GetPortBMode(), (fpgaCaps >> 8) & 1u, (fpgaCaps >> 9) & 1u, (fpgaCaps >> 11) & 1u);
	    }
	}

	// configData -> at the moment all bits are unused
	configData = 0b00000000;
	if (!config->IsModelAnyWing() && !fpgaSpiRegsAvailable)
	{
		SendConfig();
	}

	AES50Counter = 0;
	AES50Device = 'C'; // X32Fullsize
}

void Fpga::Init()
{
	if (!fpgaSpiRegsAvailable)
	{
		const uint speed = 115200;
		String serial;

		if (state->bodyless) {
			serial = "/tmp/ttyLocalFpga";
		}
		else if (state->raspi)
		{
			//serial = "/dev/ttymxc3";
			return;
		}
		else
		{
			if (config->IsModelAnyXM32())
			{
				serial = "/dev/ttymxc3";
			}
			else if (config->IsModelAnyWing())
			{
				// TODO
				return;
			}
		}

		helper->DEBUG_FPGA(DEBUGLEVEL_NORMAL, "opening %s with %d baud", serial.c_str(), speed);
		uart->Open(serial.c_str(), speed, true);
	}

	// Send all Routing config to FPGA
	SendRoutingToFpga(0);
}

// get the absolute input-source (global channel-number)
uint8_t Fpga::GetInputIndex(uint8_t group, uint8_t channel) {
	// input-sources:
	// 1-32 = XLR-inputs
	// 33-64 = Card-inputs
	// 65-72 = AUX-inputs
	// 73-112 = DSP-outputs
	// 113-160 = AES50A-inputs
	// 161-208 = AES50B-inputs

	if (channel == 0){
		return 0;
	}

	switch (group) {
		case 'x': // XLR-Inputs 1-32
			return channel;
			break;
		case 'c': // Card-Inputs 1-32
			return channel + 32;
			break;
		case 'a': // Aux-Inputs 1-8
			return channel + 64;
			break;
		case 'd': // DSP-Outputs 1-40
			return channel + 72;
			break;
		case 'A': // AES50A-Inputs 1-48
			return channel + 112;
			break;
		case 'B': // AES50B-Inputs 1-48
			return channel + 160;
			break;
	}

	return 0;
}

uint8_t Fpga::GetOutputIndex(uint8_t group, uint8_t channel) {
	// output-taps
	// 1-16 = XLR-outputs
	// 17-32 = UltraNet/P16-outputs
	// 33-64 = Card-outputs
	// 65-72 = AUX-outputs
	// 73-112 = DSP-inputs
	// 113-160 = AES50A-outputs
	// 161-208 = AES50B-outputs

	if (channel == 0){
		return 0;
	}

	switch (group) {
		case 'x': // XLR-Inputs 1-16
			return channel;
			break;
		case 'p': // UltraNet/P16-outputs 1-16
			return channel + 16;
			break;
		case 'c': // Card-Inputs 1-32
			return channel + 32;
			break;
		case 'a': // Aux-Inputs 1-8
			return channel + 64;
			break;
		case 'd': // DSP-Outputs 1-40
			return channel + 72;
			break;
		case 'A': // AES50A-Inputs 1-48
			return channel + 112;
			break;
		case 'B': // AES50B-Inputs 1-48
			return channel + 160;
			break;
	}

	return 0;
}

void Fpga::GetInputGroupAndChannelByIndex(uint8_t sourceIndex, uint8_t* group, uint8_t* channel) {
	// Inputs
	//
	// 1-32 = XLR-inputs
	// 33-64 = Card-inputs
	// 65-72 = AUX-inputs
	// 73-112 = DSP-return
	// 113-160 = AES50A-inputs
	// 161-208 = AES50B-inputs

	if (sourceIndex == 0) {
		// OFF
		*group = 0;
		*channel = 0;
	}else if ((sourceIndex >= 1) && ( sourceIndex <= 32)) {
		*group = 'x';
		*channel = sourceIndex;
	}else if ((sourceIndex >= 33) && ( sourceIndex <= 64)) {
		*group = 'c';
		*channel = sourceIndex - 32;
	}else if ((sourceIndex >= 65) && ( sourceIndex <= 72)) {
		*group = 'a';
		*channel = sourceIndex - 64;
	}else if ((sourceIndex >= 73) && ( sourceIndex <= 112)) {
		*group = 'd';
		*channel = sourceIndex - 72;
	}else if ((sourceIndex >= 113) && ( sourceIndex <= 160)) {
		*group = 'A';
		*channel = sourceIndex - 112;
	}else if ((sourceIndex >= 161) && ( sourceIndex <= 208)) {
		*group = 'B';
		*channel = sourceIndex - 160;
	}
}

void Fpga::GetOutputGroupAndChannelByIndex(uint8_t outputIndex, uint8_t* group, uint8_t* channel) {
	// output-taps
	// 1-16 = XLR-outputs
	// 17-32 = UltraNet/P16-outputs
	// 33-64 = Card-outputs
	// 65-72 = AUX-outputs
	// 73-112 = DSP-inputs
	// 113-160 = AES50A-outputs
	// 161-208 = AES50B-outputs

	if (outputIndex == 0) {
		*group = 0;
		*channel = 0;
		return;
	}

	if ((outputIndex >= 1) && ( outputIndex <= 16)) {
		*group = 'x';
		*channel = outputIndex;
	}else if ((outputIndex >= 17) && ( outputIndex <= 32)) {
		*group = 'p';
		*channel = outputIndex - 16;
	}else if ((outputIndex >= 33) && ( outputIndex <= 64)) {
		*group = 'c';
		*channel = outputIndex - 32;
	}else if ((outputIndex >= 65) && ( outputIndex <= 72)) {
		*group = 'a';
		*channel = outputIndex - 64;
	}else if ((outputIndex >= 73) && ( outputIndex <= 112)) {
		*group = 'd';
		*channel = outputIndex - 72;
	}else if ((outputIndex >= 113) && ( outputIndex <= 160)) {
		*group = 'A';
		*channel = outputIndex - 112;
	}else if ((outputIndex >= 161) && ( outputIndex <= 208)) {
		*group = 'B';
		*channel = outputIndex - 160;
	}
}

// void Fpga::fpgaRoutingGetSourceGroupAndChannelByDspChannel(uint8_t dspChannel, uint8_t* group, uint8_t* channel) {
//     // first lets find the connected source to this dspChannel
//     uint8_t sourceIndex = fpgaRoutingGetOutputSource('d', dspChannel);
//     fpgaRoutingGetSourceGroupAndChannelByIndex(sourceIndex, group, channel);
// }

String Fpga::GetInputName(uint8_t group, uint8_t channel) {
	// Inputs
	//       0 = OFF
	//    1-32 = XLR-inputs
	//   33-64 = Card-inputs
	//   65-72 = AUX-inputs
	//  73-112 = DSP1 Return
	// 113-160 = AES50A-inputs
	// 161-208 = AES50B-inputs

	if (channel == 0){
		return "Off";
	}

	switch (group) {
		case 'x': // XLR-Inputs 1-32
			return String("XLR In ") + String(channel);
		case 'c': // Card-Inputs 1-32
			return String("Card In ") + String(channel);
		case 'a': // Aux-Inputs 1-8
			return String("AUX In ") + String(channel);
		case 'd': // DSP-Outputs 1-40
			return String("DSP1 Return ") + String(channel);
		case 'A': // AES50A-Inputs 1-48
			return String("AES50A In ") + String(channel);
		case 'B': // AES50B-Inputs 1-48
			return String("AES50B In ") + String(channel);
	}

	return "";
}

String Fpga::GetInputNameByIndex(uint8_t sourceIndex) {
	uint8_t group = 0;
	uint8_t channel = 0;
	GetInputGroupAndChannelByIndex(sourceIndex, &group, &channel); // index = 0 .. 208
	return GetInputName(group, channel);
}

// get the output destination name
String Fpga::GetOutputName(uint8_t group, uint8_t channel) {
	// output-taps
	// 1-16 = XLR-outputs
	// 17-32 = UltraNet/P16-outputs
	// 33-64 = Card-outputs
	// 65-72 = AUX-outputs
	// 73-112 = DSP-inputs
	// 113-160 = AES50A-outputs
	// 161-208 = AES50B-outputs

	if (channel == 0) {
		return "Off";
	}

	switch(group){
		case 'x':
			return "XLR Out " + String(channel);
		case 'p':
			return "P16-" + String(channel);
		case 'c':
			return "Card Out " + String(channel);
		case 'a':
			switch (channel)
			{
				case 1 ... 6:
					return "AUX Out " + String(channel);
				case 7:
					return String("Monitor L");
				case 8:
					return String("Monitor R");
			}
			break;
		case 'd':
			return "DSP In " + String(channel);
		case 'A':
			return "AES50A Out " + String(channel);
		case 'B':
			return "AES50B Out " + String(channel);
	}

	return "";
}

String Fpga::GetOutputNameByIndex(uint8_t index) {
	uint8_t group = 0;
	uint8_t channel = 0;
	GetOutputGroupAndChannelByIndex(index, &group, &channel); // index = 1..208
	return GetOutputName(group, channel);
}

// helper-function to send the audio-routing to the fpga
void Fpga::SendRoutingToFpga(int channel) {
	if (state->bodyless || config->IsModelAnyWing()) {
		return;
	}

	uint8_t txData[2];
	uint8_t rxData[2];

	// FPGA expects data in this format:
	// 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
	// |------- DATA ------||---- ADDR ----|

	if ((channel >= 0) && (channel < 208))
	{
		// send only one routing-data to FPGA

		txData[0] = (uint8_t)config->GetUint(ROUTING_FPGA, channel);
		txData[1] = channel;

		helper->DEBUG_FPGA(DEBUGLEVEL_NORMAL, "SendRoutingToFpga() %s -> %s",
			config->GetParameter(ROUTING_FPGA)->GetFormatedValue(channel).c_str(),
			GetOutputNameByIndex(channel+1).c_str()
		);

		spi->SendFpgaData(&txData[0], &rxData[0], 2);

		if (!fpgaSpiRegsAvailable && ((rxData[0] != txData[0]) || (rxData[1] != txData[1])))
		{
			// Legacy FPGA echoed MOSI back to MISO; new spi_reg_bridge does not echo on 16-bit writes
			helper->Error("FPGA-Routing: Received values (0x%02x 0x%02x) does not match the sent values (0x%02x 0x%02x)\n", rxData[0], rxData[1], txData[0], txData[1]);
		}
	}
	else
	{
		// send all routing-data to FPGA

		for (uint8_t i = 0; i < 208; i++)
		{
			helper->DEBUG_FPGA(DEBUGLEVEL_TRACE, "send routing data %d", channel+1);
			txData[0] = (uint8_t)config->GetUint(ROUTING_FPGA, i);
			txData[1] = i; 
			spi->SendFpgaData(&txData[0], &rxData[0], 2);

			if (!fpgaSpiRegsAvailable && ((rxData[0] != txData[0]) || (rxData[1] != txData[1])))
			{
				helper->Error("FPGA-Routing: Received values (0x%02x 0x%02x) does not match the sent values (0x%02x 0x%02x)\n", rxData[0], rxData[1], txData[0], txData[1]);
			}
		}
	}
}

bool Fpga::GetConfigBit(uint8_t bitNumber) {
	return (configData & (1 << bitNumber)) != 0;
}

void Fpga::SetConfigBit(uint8_t bitNumber, bool value) {
	if (value) {
		configData |= (1 << bitNumber);
	} else {
		configData &= ~(1 << bitNumber);
	}

	// send data to FPGA
	if (state->bodyless || state->raspi) {
		return;
	}

	SendConfig();
}

void Fpga::SendConfig() {
	uint8_t txData[2];
	uint8_t rxData[2];
	txData[0] = configData;
	txData[1] = 255; // address
	spi->SendFpgaData(&txData[0], &rxData[0], 2);

	if (!fpgaSpiRegsAvailable && ((rxData[0] != txData[0]) || (rxData[1] != txData[1])))
	{
		// Legacy FPGA echoed MOSI back to MISO
		helper->Error("FPGA-ConfigBits: Received values (0x%02x 0x%02x) does not match the sent values (0x%02x 0x%02x)\n", rxData[0], rxData[1], txData[0], txData[1]);
	}
}

void Fpga::AES50SetPhantomPowerState(uint8_t aes50Port, uint8_t channel, bool phantomState) {
	if (aes50Port > 1 || channel == 0 || channel > 48) {
		helper->Error("AES50SetPhantomPowerState: Invalid AES50 port (%d) or channel (%d)!", aes50Port, channel);
		return;
	}

	AES50PhantomPowerState[aes50Port][channel - 1] = phantomState; // store the phantom power state in the local array

	// preload AES50Counter to send the headamp message with the new phantom power state within the next 200ms
	// but allow bulk-updates without sending multiple messages in a short time by resetting the counter
	AES50Counter = 18;
}


void Fpga::AES50SetHeadampGain(uint8_t aes50Port, uint8_t channel, float gain) {
	if (aes50Port > 1 || channel == 0 || channel > 48) {
		helper->Error("AES50SetHeadampGain: Invalid AES50 port (%d) or channel (%d)", aes50Port, channel);
		return;
	}

	if (gain < -2.0f || gain > 45.5f) {
		helper->Error("AES50SetHeadampGain: Gain value out of range (%.2f). Must be between -2.0dB and 45.5dB!", (double)gain);
		return;
	}

	AES50HeadampGains[aes50Port][channel - 1] = gain; // store the headamp gain in the local array

	// preload AES50Counter to send the headamp message with the new phantom power state within the next 200ms
	// but allow bulk-updates without sending multiple messages in a short time by resetting the counter
	AES50Counter = 18;
}


void Fpga::AES50ProcessRxBuffer(uint8_t aes50Port, char* buf, uint readBytes)
{
	char tmpName[17];

	// check the checksum of the read message
	if ((readBytes > 2) && (readBytes >= (uint)((uint8_t)buf[1] * 4u)) && (AES50CalcChecksum(buf, false))) {
		switch ((uint8_t)buf[0]) {
			case 0x01:
				// headamp-configuration-message
				break;
			case 0x05:
				// device-type and properties-message
				break;
			case 0x10:
				// device- and channelname-message
				memcpy(&tmpName[0], &buf[8], 16);
				tmpName[16] = '\0';
				helper->DEBUG_FPGA(DEBUGLEVEL_NORMAL, "AES50 %c device: %s", aes50Port == 0 ? 'A' : 'B', tmpName);
				break;
			default:
				break;
		}
	}
}

void Fpga::AES50Receive()
{
	if (fpgaSpiRegsAvailable) {
		uint8_t numPorts = (GetPortBMode() == FPGA_PORTB_AES50) ? 2 : 1;
		for (uint8_t port = 0; port < numPorts; port++) {
			uint16_t rxReg = (port == 0) ? FPGA_REG_AUX_A_RX : FPGA_REG_AUX_B_RX;
			for (int iter = 0; iter < 64; iter++) {
				uint32_t v = 0;
				if (!spi->FpgaReadReg32(rxReg, &v) || !(v & 0x0100u)) {
					break;
				}
				if (aes50SpiRxLen[port] < FPGA_UART_BUFFER_SIZE) {
					aes50SpiRxBuf[port][aes50SpiRxLen[port]++] = (char)(v & 0xFFu);
				}
				if (aes50SpiRxLen[port] >= 4) {
					uint expected = (uint)((uint8_t)aes50SpiRxBuf[port][1]) * 4u;
					if (expected < 4 || expected > FPGA_UART_BUFFER_SIZE) {
						aes50SpiRxLen[port] = 0;
					} else if (aes50SpiRxLen[port] >= expected) {
						AES50ProcessRxBuffer(port, aes50SpiRxBuf[port], expected);
						aes50SpiRxLen[port] = 0;
					}
				}
			}
		}
		return;
	}

	uint readBytes = uart->Rx(&fpgaRxBufferUart[0], sizeof(fpgaRxBufferUart));
	AES50ProcessRxBuffer(0, &fpgaRxBufferUart[0], readBytes);
}

// this function is called every 100ms
void Fpga::AES50Tick() {
	if ((AES50Device < 65) || (AES50Device > 105)) {
		AES50Device = 65;
	}

	uint8_t numPorts = (fpgaSpiRegsAvailable && GetPortBMode() == FPGA_PORTB_AES50) ? 2 : 1;

	AES50Counter++;
	if (AES50Counter == 10) {
		// send device-properties every 2 seconds
		for (uint8_t port = 0; port < numPorts; port++) {
			AES50SendDeviceTypeAndProperty(port);
		}
	}else if (AES50Counter >= 20) {
		// send headamp-controls every 2 seconds
		AES50Counter = 0;

		//for (uint8_t port = 0; port < numPorts; port++) AES50SendHeadampMessage(port);
	}
}

void Fpga::AES50SendDeviceTypeAndProperty(uint8_t aes50Port) {
	fpgaTxBufferUart[0] = 0x05;
	fpgaTxBufferUart[1] = (16 / 4); // message-length in 32-bit words
	fpgaTxBufferUart[2] = 0; // checksum (will be set in AES50Send-Function)
	fpgaTxBufferUart[3] = 0; // checksum (will be set in AES50Send-Function)

	// AES50-DeviceChars for the last four connected devices
	// (AES50 seems to support up to 6, but only the last 4 are transmitted)
	fpgaTxBufferUart[4] = 0; // fourth device in AES50-chain
	fpgaTxBufferUart[5] = 0; // third device in AES50-chain
	fpgaTxBufferUart[6] = 0; // second device in AES50-chain
	fpgaTxBufferUart[7] = AES50Device; // first device in AES50-chain

	// data[8 .. 13] <- '0' if no headamp-control, '2' if headamp-
	// control is present
	fpgaTxBufferUart[8] = '2'; // AES50 Ch 1-8 have headamp-controls
	fpgaTxBufferUart[9] = '2'; // AES50 Ch 9-16 have headamp-controls
	fpgaTxBufferUart[10] = '2'; // AES50 Ch 17-24 have headamp-controls
	fpgaTxBufferUart[11] = '2'; // AES50 Ch 25-32 have headamp-controls
	fpgaTxBufferUart[12] = '2'; // AES50 Ch 33-40 have headamp-controls
	fpgaTxBufferUart[13] = '2'; // AES50 Ch 41-48 have headamp-controls

	// byte-stuffing to have full divider by 4
	fpgaTxBufferUart[14] = 0x00;
	fpgaTxBufferUart[15] = 0x00;

	// calculate and add checksum to begin of message
	AES50CalcChecksum(&fpgaTxBufferUart[0], true);

	// send data to AES50 IP-core (SPI aux or legacy UART)
	AES50Send(aes50Port, &fpgaTxBufferUart[0], 16);
}

void Fpga::AES50SendNames(uint8_t aes50Port) {
	fpgaTxBufferUart[0] = 0x10;
	fpgaTxBufferUart[1] = (408 / 4); // message-length in 32-bit words
	fpgaTxBufferUart[2] = 0; // checksum (will be set in AES50Send-Function)
	fpgaTxBufferUart[3] = 0; // checksum (will be set in AES50Send-Function)

	// AES50-DeviceChars for the last four connected devices
	// (AES50 seems to support up to 6, but only the last 4 are transmitted)
	fpgaTxBufferUart[4] = 0; // fourth device in AES50-chain
	fpgaTxBufferUart[5] = 0; // third device in AES50-chain
	fpgaTxBufferUart[6] = 0; // second device in AES50-chain
	fpgaTxBufferUart[7] = AES50Device; // first device in AES50-chain

	// the next bytes are zero-terminated ASCII-strings.
	// First clear the data-array with zeros
	for (int i = 8; i < 408; i++) {
		fpgaTxBufferUart[i] = 0x00;
	}

	// insert zero-terminated device-string of current AES50-device
	sprintf((char*)&fpgaTxBufferUart[8], "X32");
	fpgaTxBufferUart[24] = 0x02; // add a 0x02 as "START OF TEXT"

	// from data[25] 48x 7-char ASCII-Name for each channel is
	// sent followed by a 0x02
	for (int i = 0; i < 48; i++) {
		sprintf((char*)&fpgaTxBufferUart[25 + (i * 8)], "IN % u", i + 1);
		fpgaTxBufferUart[25 + (i * 8) + 7] = 0x02;
	}
	fpgaTxBufferUart[407] = 0x0A; // add a linefeed at the end

	// calculate and add checksum to begin of message
	AES50CalcChecksum(&fpgaTxBufferUart[0], true);

	// send data to AES50 IP-core (SPI aux or legacy UART)
	AES50Send(aes50Port, &fpgaTxBufferUart[0], 408);
}

float gainMap(float in, float inMin, float inMax, float outMin, float outMax) {
  return ((in - inMin) * (outMax - outMin) / (inMax - inMin)) + outMin;
}

void Fpga::AES50SendHeadampMessage(uint8_t aes50Port) {
	if (aes50Port > 1) return;
	fpgaTxBufferUart[0] = 0x01;
	fpgaTxBufferUart[1] = (60 / 4); // message-length in 32-bit words
	fpgaTxBufferUart[2] = 0; // checksum (will be set in AES50Send-Function)
	fpgaTxBufferUart[3] = 0; // checksum (will be set in AES50Send-Function)

	// AES50-DeviceChars for the last four connected devices
	// (AES50 seems to support up to 6, but only the last 4 are transmitted)
	fpgaTxBufferUart[4] = 0; // fourth device in AES50-chain
	fpgaTxBufferUart[5] = 0; // third device in AES50-chain
	fpgaTxBufferUart[6] = 0; // second device in AES50-chain
	fpgaTxBufferUart[7] = AES50Device; // first device in AES50-chain

	for (int i = 0; i < 48; i++) {
		fpgaTxBufferUart[8 + i] = (uint8_t) roundf(gainMap(AES50HeadampGains[aes50Port][i], -2.0f, 45.5f, 0, 19));
		if (AES50PhantomPowerState[aes50Port][i]) {
			fpgaTxBufferUart[8 + i] |= 0x80;
		}
	}

	// a single 32-bit word with zeros for finalizing the message
	fpgaTxBufferUart[56] = 0;
	fpgaTxBufferUart[57] = 0;
	fpgaTxBufferUart[58] = 0;
	fpgaTxBufferUart[59] = 0;

	// calculate and add checksum to begin of message
	AES50CalcChecksum(&fpgaTxBufferUart[0], true);

	// send data to AES50 IP-core (SPI aux or legacy UART)
	AES50Send(aes50Port, &fpgaTxBufferUart[0], 60);
}

void Fpga::AES50Send(char* data, uint len) {
	AES50Send(0, data, len);
}

void Fpga::AES50Send(uint8_t aes50Port, char* data, uint len) {
	if (aes50Port > 1) return;

	if (fpgaSpiRegsAvailable) {
		uint16_t txReg  = (aes50Port == 0) ? FPGA_REG_AUX_A_TX  : FPGA_REG_AUX_B_TX;
		uint16_t cntReg = (aes50Port == 0) ? FPGA_REG_AUX_A_CNT : FPGA_REG_AUX_B_CNT;
		for (uint i = 0; i < len; i++) {
			// u_spi_tx_fifo is 15 bytes deep and drains at 3.125 Mbps (~3.2 us/byte)
			for (int wait = 0; wait < 20; wait++) {
				uint32_t cnt = 0;
				if (spi->FpgaReadReg32(cntReg, &cnt) && ((cnt >> 16) < 14u)) {
					break;
				}
				usleep(10);
			}
			spi->FpgaWriteReg32(txReg, (uint8_t)data[i]);
		}
		return;
	}

	if (aes50Port != 0) return; // legacy UART only serves Port A

	MessageBase* message = new MessageBase();

	message->AddRawDataArray(data, len);

	if (helper->DEBUG_FPGA(DEBUGLEVEL_TRACE)) {
		printf("DEBUG_FPGA: Transmit: ");
    	for (int i = 0; i < message->current_length; i++){
        	printf("%c", message->buffer[i]);
    	}
    	printf("\n");
	}

	uart->Tx(message);

	delete(message);
}

bool Fpga::SetAes50SampleRate(uint8_t aes50Port, bool fs96) {
	if (!fpgaSpiRegsAvailable || aes50Port > 1) return false;
	uint32_t fsBit  = (aes50Port == 0) ? (1u << 0) : (1u << 1);
	uint32_t rstBit = (aes50Port == 0) ? (1u << 8) : (1u << 9);
	if (fs96) fpgaCtrlReg |= fsBit;
	else      fpgaCtrlReg &= ~fsBit;

	// Assert soft-reset while changing fs96_i (aes50_par_core requires static fs96 while rst=0)
	if (!spi->FpgaWriteReg32(FPGA_REG_CTRL, fpgaCtrlReg | rstBit)) return false;
	usleep(1000);
	return spi->FpgaWriteReg32(FPGA_REG_CTRL, fpgaCtrlReg & ~rstBit);
}

bool Fpga::Cs2000WriteReg(uint8_t reg, uint8_t val) {
	if (!fpgaSpiRegsAvailable || !(fpgaCaps & (1u << 11))) return false;
	uint32_t cmd = ((uint32_t)reg << 8) | val;
	if (!spi->FpgaWriteReg32(FPGA_REG_CS2K_CMD, cmd)) return false;
	if (!spi->FpgaWriteReg32(FPGA_REG_CS2K_STROBE, 1u)) return false;
	for (int i = 0; i < 100; i++) {
		uint32_t echo = 0;
		if (spi->FpgaReadReg32(FPGA_REG_CS2K_ECHO, &echo) && !(echo & 0x80000000u)) {
			return (echo & 0xFFFFu) == cmd;
		}
		usleep(20);
	}
	return false;
}

bool Fpga::Cs2000WriteRatio(uint32_t ratio) {
	bool ok = Cs2000WriteReg(0x05, 0x09); // Freeze = 1
	for (int i = 0; i < 4 && ok; i++) {
		ok = Cs2000WriteReg((uint8_t)(0x06 + i), (uint8_t)(ratio >> (24 - 8 * i)));
	}
	if (ok) ok = Cs2000WriteReg(0x05, 0x01); // Freeze = 0 -> apply atomically
	return ok;
}

bool Fpga::SetClockSource(uint8_t clkSrc, bool slaveFs96) {
	if (!fpgaSpiRegsAvailable || clkSrc > FPGA_CLK_SRC_AES50B) return false;

	// Update CLK_SRC[11:10] and Port A/B master/slave mode bits ([3:2] / [5:4])
	fpgaCtrlReg &= ~((3u << 10) | (3u << 4) | (3u << 2));
	fpgaCtrlReg |= ((uint32_t)clkSrc << 10);
	fpgaCtrlReg |= ((clkSrc == FPGA_CLK_SRC_AES50A ? 0u : 1u) << 2);
	fpgaCtrlReg |= ((clkSrc == FPGA_CLK_SRC_AES50B ? 0u : 1u) << 4);

	uint32_t rstMask = (1u << 8) | (1u << 9);
	if (!spi->FpgaWriteReg32(FPGA_REG_CTRL, fpgaCtrlReg | rstMask)) return false;

	uint32_t ratio = CS2K_RATIO_16M_TO_49M;
	if (clkSrc != FPGA_CLK_SRC_INTERNAL) {
		ratio = slaveFs96 ? CS2K_RATIO_AES50_96K : CS2K_RATIO_AES50_48K;
	}
	bool ok = Cs2000WriteRatio(ratio);
	usleep(1000);
	ok = spi->FpgaWriteReg32(FPGA_REG_CTRL, fpgaCtrlReg & ~rstMask) && ok;
	return ok;
}

bool Fpga::GetAes50Status(uint32_t* statusReg, uint32_t* dropA, uint32_t* dropB) {
	if (!fpgaSpiRegsAvailable) return false;
	bool ok = true;
	if (statusReg) ok = spi->FpgaReadReg32(FPGA_REG_STATUS, statusReg) && ok;
	if (dropA)     ok = spi->FpgaReadReg32(FPGA_REG_DROP_A, dropA) && ok;
	if (dropB)     ok = spi->FpgaReadReg32(FPGA_REG_DROP_B, dropB) && ok;
	return ok;
}

bool Fpga::AES50CalcChecksum(char* buf, bool insertChecksum)
{
	//uint8_t messageID = buf[0];
	uint8_t len = buf[1];
	uint8_t deviceChar = buf[7];

	// initializing
	uint32_t ids4 = (uint32_t)deviceChar << 24;
	uint16_t cks = (uint16_t)(ids4 >> 16) ^ (uint16_t)(ids4 & 0xFFFF);

	uint32_t* dataWords = (uint32_t*)&buf[8];

	if (len > 2) {
		for (int i = 0; i <= (len - 3); i++) {
			uint32_t currentWord = dataWords[i];
			cks -= (uint16_t)(currentWord >> 16) ^ (uint16_t)(currentWord & 0xFFFF);
		}
	}
	uint16_t finalCks = cks ^ 0xFFFF;

	if (insertChecksum) {
		// insert calculated checksum into message
		buf[2] = (uint8_t)(finalCks & 0xFF);
		buf[3] = (uint8_t)(finalCks >> 8);

		return true;
	}else{
		// just compare calculated checksum against message
		return ((buf[2] == (uint8_t)(finalCks & 0xFF)) && (buf[3] == (uint8_t)(finalCks >> 8)));
	}
}

int Fpga::wrapRingBufferIndex(int index, int bufLen) {
	if (index < 0) {
		return (index + bufLen);
	}else if (index >= bufLen) {
		return (index - bufLen);
	}else{
		return index;
	}
}

}