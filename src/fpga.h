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

// OpenAudioFPGA modular Wishbone register map (via /dev/spibone0)
#define FPGA_WB_ROUTING_BASE   0x80000000u
#define FPGA_WB_AES50_BASE     0x81000000u // 0x40 byte stride per AES50 port (0..N-1)
#define FPGA_AES50_PORT_STRIDE 0x40u
#define FPGA_AES50_OFF_INFO    0x00u       // [31:16]=0xA502, [15:8]=num_ports, [7:4]=idx, [0]=src
#define FPGA_AES50_OFF_CTRL    0x04u       // [0]=fs96, [2:1]=sys_mode, [8]=soft_rst
#define FPGA_AES50_OFF_STATUS  0x08u       // [0]=aes_ok, [1]=rx_ok, [2]=tx_st, [3]=fmt_v, [15:8]=fmt
#define FPGA_AES50_OFF_DROPS   0x0Cu
#define FPGA_AES50_OFF_AUX_TX  0x10u
#define FPGA_AES50_OFF_AUX_RX  0x14u
#define FPGA_AES50_OFF_AUX_ST  0x18u
#define FPGA_AES50_MAGIC       0xA502u

#define FPGA_WB_CLKCTRL_BASE   0x82000000u // Standalone CS2000-CP / clock mux peripheral
#define FPGA_CLK_REG_MAGIC     (FPGA_WB_CLKCTRL_BASE + 0x00u) // "CS2K" (0x4353324B)
#define FPGA_CLK_REG_CTRL      (FPGA_WB_CLKCTRL_BASE + 0x04u) // [2:0]=clk_src
#define FPGA_CLK_REG_STATUS    (FPGA_WB_CLKCTRL_BASE + 0x08u) // [0]=pll_lock, [1]=boot_done
#define FPGA_CLK_REG_CS2K_CMD  (FPGA_WB_CLKCTRL_BASE + 0x40u)
#define FPGA_CLK_REG_CS2K_STAT (FPGA_WB_CLKCTRL_BASE + 0x44u)
#define FPGA_CLK_REG_CS2K_GO   (FPGA_WB_CLKCTRL_BASE + 0x48u)
#define FPGA_CLK_MAGIC_CS2K    0x4353324Bu

#define FPGA_MAX_AES50_PORTS   3u          // 2 on X32, 3 on WING

// CS2000-CP 12.20 fixed-point ratio values (PLL_OUT = 49.152 MHz)
// AES50 recovered sync clock (clk_to_pll_o) is 64*fs: 3.072 MHz @ 48 kHz, 6.144 MHz @ 96 kHz
#define CS2K_RATIO_INT_16M     0x003126E9u // 16.000 MHz * 3.072 = 49.152 MHz
#define CS2K_RATIO_AES50_48K   0x01000000u //  3.072 MHz * 16    = 49.152 MHz
#define CS2K_RATIO_AES50_96K   0x00800000u //  6.144 MHz * 8     = 49.152 MHz

namespace OMC
{

struct Aes50PortStatus {
    bool aesOk;
    bool rxOk;
    bool txStarted;
    bool fmtValid;
    bool pllLocked;
    uint8_t fmt;
    uint32_t dropCount;
};

enum FpgaClockSource : uint8_t {
    FPGA_CLK_INTERNAL = 0,
    FPGA_CLK_AES50A   = 1,
    FPGA_CLK_AES50B   = 2,
    FPGA_CLK_AES50C   = 3
};

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
        void AES50Send(uint8_t aes50Port, char* data, uint len);
        void AES50Send(char* data, uint len) { AES50Send(0, data, len); }

        void AES50Tick();
        void AES50SendDeviceTypeAndProperty(uint8_t aes50Port = 0);
        void AES50SendNames(uint8_t aes50Port = 0);
        void AES50SendHeadampMessage(uint8_t aes50Port = 0);

        bool HasWbRegs() const { return fpgaWbRegsAvailable; }
        uint8_t GetAes50PortCount() const { return aes50PortCount; }
        bool HasCs2kCtrl() const { return clkCtrlAvailable; }
        bool SetAes50SampleRate(uint8_t aes50Port, bool fs96);
        bool SetClockSource(FpgaClockSource src);
        bool Cs2000WriteReg(uint8_t reg, uint8_t val);
        bool Cs2000WriteRatio(uint32_t ratio12_20);
        bool GetAes50Status(uint8_t aes50Port, Aes50PortStatus* outStatus);

    private:
        Uart* uart;
        char fpgaRxBufferUart[FPGA_UART_BUFFER_SIZE];
        char fpgaTxBufferUart[FPGA_UART_BUFFER_SIZE];
        char aes50WbRxBuf[FPGA_MAX_AES50_PORTS][FPGA_UART_BUFFER_SIZE];
        uint aes50WbRxLen[FPGA_MAX_AES50_PORTS];
        bool fpgaWbRegsAvailable;
        bool clkCtrlAvailable;
        bool aes50SrcAvailable;
        uint8_t aes50PortCount;
        uint8_t activeClkSrc;
        uint32_t aes50PortCtrl[FPGA_MAX_AES50_PORTS];
        uint8_t pllUnlockTicks;
        uint8_t configData;
        uint8_t AES50Counter;
        bool AES50PhantomPowerState[FPGA_MAX_AES50_PORTS][48]; // [AES50A/B/C][Channel1-48]
        float AES50HeadampGains[FPGA_MAX_AES50_PORTS][48]; // [AES50A/B/C][Channel1-48]

        uint8_t AES50Device;
        void AES50ProcessRxBuffer(uint8_t aes50Port, char* buf, uint readBytes);
        bool AES50CalcChecksum(char* buf, bool insertChecksum);
        int wrapRingBufferIndex(int index, int bufLen);
};

}