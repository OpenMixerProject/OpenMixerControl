#pragma once

#include <stdint.h>
#include "../lib/WString.h"
#include "base.h"
#include "uart.h"
#include "message-base.h"
#include "spi.h"

// includes for UDP-communication
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#define FPGA_MAX_PACKET_LENGTH 32
#define FPGA_PACKET_LENGTH 8 // *xxxxSS#
#define FPGA_IDX_ROUTING   0    // reserve 200 slots
#define FPGA_IDX_CH_VOL    200  // here we need 40 values here
#define FPGA_UART_BUFFER_SIZE  450 // slightly more than the longest message (408 bytes)

// OpenX32 32-bit SPI Register Map (fpga/openx32_core.vhd + spi_reg_bridge.vhd)
#define FPGA_REG_MAGIC        0x0000u  // "OX32" (0x4F583332)
#define FPGA_REG_CAPS         0x0004u  // [15:12]=PortB mode (1=AES50,2=INFERNO,3=ACE), [11]=CS2K, [9:8]=SRC B/A
#define FPGA_REG_CTRL         0x0008u  // [11:10]=CLK_SRC, [9:8]=rstB/A, [5:4]=modeB, [3:2]=modeA, [1:0]=fs96 B/A
#define FPGA_REG_STATUS       0x000Cu  // [31:24]=fmtB, [19:16]=flagsB, [15:8]=fmtA, [7]=PLL_LOCK, [3:0]=flagsA
#define FPGA_REG_DROP_A       0x0010u
#define FPGA_REG_DROP_B       0x0014u
#define FPGA_REG_AUX_A_TX     0x0020u
#define FPGA_REG_AUX_A_RX     0x0024u
#define FPGA_REG_AUX_A_CNT    0x0028u
#define FPGA_REG_AUX_B_TX     0x0030u
#define FPGA_REG_AUX_B_RX     0x0034u
#define FPGA_REG_AUX_B_CNT    0x0038u
#define FPGA_REG_CS2K_CMD     0x0040u
#define FPGA_REG_CS2K_ECHO    0x0044u
#define FPGA_REG_CS2K_STROBE  0x0048u

#define FPGA_MAGIC_OX32       0x4F583332u
#define FPGA_PORTB_AES50      1u
#define FPGA_PORTB_INFERNO    2u
#define FPGA_PORTB_ACE        3u

#define FPGA_CLK_SRC_INTERNAL 0u
#define FPGA_CLK_SRC_AES50A   1u
#define FPGA_CLK_SRC_AES50B   2u

#define CS2K_RATIO_16M_TO_49M 0x003126E9u  // 16 MHz * 3.072 = 49.152 MHz
#define CS2K_RATIO_AES50_48K  0x01000000u  // 3.072 MHz (64*48k) * 16 = 49.152 MHz
#define CS2K_RATIO_AES50_96K  0x00800000u  // 6.144 MHz (64*96k) * 8  = 49.152 MHz

namespace OMC
{

class Fpga : public X32Base
{
    using enum MP_ID;

    public:

        SPI* spi;

        Fpga(X32BaseParameter* basepar);
        void Init();

        uint8_t GetInputIndex(uint8_t group, uint8_t channel);
        uint8_t GetOutputIndex(uint8_t group, uint8_t channel);
        void GetInputGroupAndChannelByIndex(uint8_t sourceIndex, uint8_t* group, uint8_t* channel);
        void GetOutputGroupAndChannelByIndex(uint8_t outputIndex, uint8_t* group, uint8_t* channel);
        void RoutingGetSourceGroupAndChannelByDspChannel(uint8_t dspChannel, uint8_t* group, uint8_t* channel);

        String GetInputName(uint8_t group, uint8_t channel);
        String GetInputNameByIndex(uint8_t sourceIndex);
        String GetOutputName(uint8_t group, uint8_t p_chan);
        String GetOutputNameByIndex(uint8_t index);

        void SendRoutingToFpga(int channel);

        bool GetConfigBit(uint8_t bitNumber);
        void SetConfigBit(uint8_t bitNumber, bool value);
        void SendConfig();
    
        void AES50SetPhantomPowerState(uint8_t aes50Port, uint8_t channel, bool state);
        void AES50SetHeadampGain(uint8_t aes50Port, uint8_t channel, float gain);

        void AES50Receive();
        void AES50Send(char* data, uint len);
        void AES50Send(uint8_t aes50Port, char* data, uint len);

        void AES50Tick();
        void AES50SendDeviceTypeAndProperty(uint8_t aes50Port = 0);
        void AES50SendNames(uint8_t aes50Port = 0);
        void AES50SendHeadampMessage(uint8_t aes50Port = 0);

        bool HasSpiRegs() const { return fpgaSpiRegsAvailable; }
        uint8_t GetPortBMode() const { return (uint8_t)((fpgaCaps >> 12) & 0x0Fu); }
        bool SetAes50SampleRate(uint8_t aes50Port, bool fs96);
        bool SetClockSource(uint8_t clkSrc, bool slaveFs96 = false);
        bool Cs2000WriteReg(uint8_t reg, uint8_t val);
        bool Cs2000WriteRatio(uint32_t ratio);
        bool GetAes50Status(uint32_t* statusReg, uint32_t* dropA = nullptr, uint32_t* dropB = nullptr);

    private:
        Uart* uart;
        char fpgaRxBufferUart[FPGA_UART_BUFFER_SIZE];
        char fpgaTxBufferUart[FPGA_UART_BUFFER_SIZE];
        char aes50SpiRxBuf[2][FPGA_UART_BUFFER_SIZE];
        uint aes50SpiRxLen[2];
        bool fpgaSpiRegsAvailable;
        uint32_t fpgaCaps;
        uint32_t fpgaCtrlReg;
        uint8_t configData;
        uint8_t AES50Counter;
        bool AES50PhantomPowerState[2][48]; // [AES50A/B][Channel1-48]
        float AES50HeadampGains[2][48]; // [AES50A/B][Channel1-48]
        
      	uint8_t AES50Device;
        void AES50ProcessRxBuffer(uint8_t aes50Port, char* buf, uint readBytes);
        bool AES50CalcChecksum(char* buf, bool insertChecksum);
        int wrapRingBufferIndex(int index, int bufLen);
};

}