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

#include "surface.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// Wing CSC Transport / Latch Registers & Constants
#define IOMUXC_BASE              0x020E0000u
#define GPIO2_BASE               0x020A0000u
#define GPIO3_BASE               0x020A4000u
#define GPIO5_BASE               0x020AC000u
#define ECSPI2_BASE              0x0200C000u
#define CCM_BASE                 0x020C4000u
#define CCM_CCGR1_OFF            0x006Cu
#define GPIO_DR_OFF              0x0000u
#define GPIO_GDIR_OFF            0x0004u
#define ECSPI_RXDATA_OFF         0x0000u
#define ECSPI_TXDATA_OFF         0x0004u
#define ECSPI_CONREG_OFF         0x0008u
#define ECSPI_CONFIGREG_OFF      0x000Cu
#define ECSPI_INTREG_OFF         0x0010u
#define ECSPI_STATREG_OFF        0x0018u
#define ECSPI_PERIODREG_OFF      0x001Cu
#define ECSPI_TC_BIT             0x80u
#define ECSPI_XCH_BIT            0x04u
#define CSC_LIGHT_DEFAULT_VALUE  0x0000602fu
#define CSC_LIGHT_VALUE_MASK     0x0000602fu
#define MUX_KEY_COL1             0x005Cu
#define MUX_KEY_ROW1             0x0060u
#define PAD_KEY_COL1             0x0370u
#define PAD_KEY_ROW1             0x0374u
#define SEL_UART5_RX             0x091Cu
#define UART5_ALT_MODE           0x3u
#define UART5_RX_DAISY_STOCK     0x1u
#define PAD_UART_TX_STOCK        0x00000018u
#define PAD_UART_RX_STOCK        0x0000B000u
#define CSC_RESET_BIT            23u
#define CSC_BOOT_BIT             20u
#define CSC_RESET_MASK           ((1u << CSC_RESET_BIT) | (1u << CSC_BOOT_BIT))

struct wing_csc_latch_mmio {
    int mem_fd;
    size_t map_len;
    uint8_t *iomuxc;
    uint8_t *gpio2;
    uint8_t *gpio3;
    uint8_t *ecspi2;
};

static uint32_t csc_latch_state[2] = {0, 0};

static uint32_t readl_ptr(uint8_t *base, uint32_t off)
{
    return *(volatile uint32_t *)(base + off);
}

static void writel_ptr(uint8_t *base, uint32_t off, uint32_t value)
{
    *(volatile uint32_t *)(base + off) = value;
}

static unsigned long long monotonic_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (unsigned long long)ts.tv_sec * 1000000ull +
           (unsigned long long)ts.tv_nsec / 1000ull;
}

static void wing_csc_latch_configure_pads(uint8_t *iomuxc)
{
    static const struct {
        uint32_t off;
        uint32_t value;
    } writes[] = {
        { 0x05ac, 0x0001b0b0 }, { 0x01dc, 0x00000005 },
        { 0x05a4, 0x0001b0b0 }, { 0x01d4, 0x00000005 },
        { 0x0534, 0x0001b0b0 }, { 0x0164, 0x00000005 },
        { 0x0538, 0x0001b0b0 }, { 0x0168, 0x00000005 },
        { 0x050c, 0x0001b0b0 }, { 0x013c, 0x00000002 }, { 0x07f4, 0x00000002 },
        { 0x05a8, 0x0001b0b0 }, { 0x01d8, 0x00000002 }, { 0x07f8, 0x00000002 },
        { 0x0510, 0x0001b0b0 }, { 0x0140, 0x00000002 }, { 0x07fc, 0x00000002 },
        { 0x04f4, 0x0001b0b0 }, { 0x0124, 0x00000005 },
        { 0x0410, 0x0001b008 }, { 0x00fc, 0x00000015 },
        { 0x04f0, 0x00013008 }, { 0x0120, 0x00000005 },
    };

    for (size_t i = 0; i < sizeof(writes) / sizeof(writes[0]); ++i)
        writel_ptr(iomuxc, writes[i].off, writes[i].value);
}

static void wing_csc_latch_gpio_init(struct wing_csc_latch_mmio *mmio)
{
    uint32_t value;

    value = readl_ptr(mmio->gpio2, GPIO_DR_OFF);
    value |= 0x0c000000u | 0x00020000u;
    writel_ptr(mmio->gpio2, GPIO_DR_OFF, value);
    value = readl_ptr(mmio->gpio2, GPIO_GDIR_OFF);
    value |= 0x0c000000u | 0x00020000u;
    writel_ptr(mmio->gpio2, GPIO_GDIR_OFF, value);

    value = readl_ptr(mmio->gpio3, GPIO_DR_OFF);
    value &= ~0x03000000u;
    writel_ptr(mmio->gpio3, GPIO_DR_OFF, value);
    value = readl_ptr(mmio->gpio3, GPIO_GDIR_OFF);
    value |= 0x03000000u;
    writel_ptr(mmio->gpio3, GPIO_GDIR_OFF, value);
}

static void wing_csc_latch_spi_init(uint8_t *ecspi2)
{
    writel_ptr(ecspi2, ECSPI_CONREG_OFF, 0);
    writel_ptr(ecspi2, ECSPI_CONREG_OFF, 0x00000011u);
    writel_ptr(ecspi2, ECSPI_CONFIGREG_OFF, 0x00000100u);
    writel_ptr(ecspi2, ECSPI_PERIODREG_OFF, 0);
}

static int wing_csc_latch_mmio_open(struct wing_csc_latch_mmio *mmio)
{
    memset(mmio, 0, sizeof(*mmio));
    mmio->mem_fd = -1;
    mmio->map_len = 0x4000;
    mmio->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mmio->mem_fd < 0)
        return -1;

    // Enable ECSPI2 clock gate via CCM CCGR1
    uint8_t *ccm = (uint8_t *)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, mmio->mem_fd, CCM_BASE);
    if (ccm == MAP_FAILED)
        goto fail;
    uint32_t ccgr1 = readl_ptr(ccm, CCM_CCGR1_OFF);
    ccgr1 |= 0x0000000Cu;
    writel_ptr(ccm, CCM_CCGR1_OFF, ccgr1);
    munmap(ccm, 0x1000);

    mmio->iomuxc = (uint8_t *)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, mmio->mem_fd,
                        IOMUXC_BASE);
    if (mmio->iomuxc == MAP_FAILED)
        goto fail;
    mmio->gpio2 = (uint8_t *)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, mmio->mem_fd,
                       GPIO2_BASE);
    if (mmio->gpio2 == MAP_FAILED)
        goto fail;
    mmio->gpio3 = (uint8_t *)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, mmio->mem_fd,
                       GPIO3_BASE);
    if (mmio->gpio3 == MAP_FAILED)
        goto fail;
    mmio->ecspi2 = (uint8_t *)mmap(NULL, mmio->map_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                        mmio->mem_fd, ECSPI2_BASE);
    if (mmio->ecspi2 == MAP_FAILED)
        goto fail;

    wing_csc_latch_configure_pads(mmio->iomuxc);
    wing_csc_latch_gpio_init(mmio);
    wing_csc_latch_spi_init(mmio->ecspi2);
    return 0;

fail:
    if (mmio->ecspi2 && mmio->ecspi2 != MAP_FAILED)
        munmap(mmio->ecspi2, mmio->map_len);
    if (mmio->gpio3 && mmio->gpio3 != MAP_FAILED)
        munmap(mmio->gpio3, 0x1000);
    if (mmio->gpio2 && mmio->gpio2 != MAP_FAILED)
        munmap(mmio->gpio2, 0x1000);
    if (mmio->iomuxc && mmio->iomuxc != MAP_FAILED)
        munmap(mmio->iomuxc, 0x1000);
    close(mmio->mem_fd);
    return -1;
}

static void wing_csc_latch_mmio_close(struct wing_csc_latch_mmio *mmio)
{
    if (mmio->ecspi2 && mmio->ecspi2 != MAP_FAILED)
        munmap(mmio->ecspi2, mmio->map_len);
    if (mmio->gpio3 && mmio->gpio3 != MAP_FAILED)
        munmap(mmio->gpio3, 0x1000);
    if (mmio->gpio2 && mmio->gpio2 != MAP_FAILED)
        munmap(mmio->gpio2, 0x1000);
    if (mmio->iomuxc && mmio->iomuxc != MAP_FAILED)
        munmap(mmio->iomuxc, 0x1000);
    if (mmio->mem_fd >= 0)
        close(mmio->mem_fd);
}

static void wing_csc_latch_select(struct wing_csc_latch_mmio *mmio)
{
    uint32_t value;

    value = readl_ptr(mmio->gpio2, GPIO_DR_OFF);
    value &= ~0x04000000u;
    value |= 0x08000000u;
    writel_ptr(mmio->gpio2, GPIO_DR_OFF, value);
    value = readl_ptr(mmio->gpio3, GPIO_DR_OFF);
    value |= 0x03000000u;
    writel_ptr(mmio->gpio3, GPIO_DR_OFF, value);
}

static void wing_csc_latch_deselect(struct wing_csc_latch_mmio *mmio)
{
    uint32_t value;

    value = readl_ptr(mmio->gpio2, GPIO_DR_OFF);
    value |= 0x0c000000u;
    writel_ptr(mmio->gpio2, GPIO_DR_OFF, value);
    value = readl_ptr(mmio->gpio3, GPIO_DR_OFF);
    value |= 0x03000000u;
    writel_ptr(mmio->gpio3, GPIO_DR_OFF, value);
}

static int wing_csc_latch_transfer(struct wing_csc_latch_mmio *mmio, const uint32_t *words,
                                   size_t count)
{
    uint32_t con;
    unsigned long long deadline;

    if (count == 0 || count > 64)
        return -1;
    wing_csc_latch_select(mmio);

    con = readl_ptr(mmio->ecspi2, ECSPI_CONREG_OFF);
    con &= ~0xfff00008u;
    con |= (uint32_t)((count << 25) - 0x00100000u);
    writel_ptr(mmio->ecspi2, ECSPI_CONREG_OFF, con);
    writel_ptr(mmio->ecspi2, ECSPI_STATREG_OFF, ECSPI_TC_BIT);
    for (size_t i = 0; i < count; ++i)
        writel_ptr(mmio->ecspi2, ECSPI_TXDATA_OFF, words[i]);
    writel_ptr(mmio->ecspi2, ECSPI_INTREG_OFF, ECSPI_TC_BIT);
    writel_ptr(mmio->ecspi2, ECSPI_CONREG_OFF, con | ECSPI_XCH_BIT);

    deadline = monotonic_us() + 500000ull;
    while ((readl_ptr(mmio->ecspi2, ECSPI_STATREG_OFF) & ECSPI_TC_BIT) == 0) {
        if (monotonic_us() > deadline) {
            wing_csc_latch_deselect(mmio);
            errno = ETIMEDOUT;
            return -1;
        }
    }
    writel_ptr(mmio->ecspi2, ECSPI_INTREG_OFF, 0);
    for (size_t i = 0; i < count; ++i)
        (void)readl_ptr(mmio->ecspi2, ECSPI_RXDATA_OFF);
    wing_csc_latch_deselect(mmio);
    return 0;
}

static int wing_csc_latch_write_state(struct wing_csc_latch_mmio *mmio, unsigned int port,
                                      uint32_t state)
{
    uint32_t words[2];

    if (port > 1)
        return -1;
    words[0] = 0xa8000000u | port;
    words[1] = state;
    wing_csc_latch_spi_init(mmio->ecspi2);
    return wing_csc_latch_transfer(mmio, words, sizeof(words) / sizeof(words[0]));
}

static int wing_csc_latch_update(unsigned int port, uint32_t value, uint32_t mask)
{
    struct wing_csc_latch_mmio mmio;
    uint32_t next;
    int rc;

    if (port > 1) {
        errno = EINVAL;
        return -1;
    }
    next = (csc_latch_state[port] & ~mask) | value;
    if (next == csc_latch_state[port])
        return 0;
    if (wing_csc_latch_mmio_open(&mmio) != 0)
        return -1;
    rc = wing_csc_latch_write_state(&mmio, port, next);
    wing_csc_latch_mmio_close(&mmio);
    if (rc == 0)
        csc_latch_state[port] = next;
    return rc;
}

static int wing_enable_csc_lights(uint32_t value)
{
    if (value > CSC_LIGHT_VALUE_MASK) {
        errno = EINVAL;
        return -1;
    }

    if (wing_csc_latch_update(0, 0, 0x380001bfu) != 0)
        return -1;
    usleep(2000);
    if (wing_csc_latch_update(0, 0x00040000u, 0x00000040u) != 0)
        return -1;
    if (wing_csc_latch_update(0, value, CSC_LIGHT_VALUE_MASK) != 0)
        return -1;
    if (wing_csc_latch_update(0, 0x00000040u, 0) != 0)
        return -1;
    usleep(2000);
    if (wing_csc_latch_update(0, 0x00000080u, 0) != 0 ||
        wing_csc_latch_update(0, 0x08000000u, 0) != 0 ||
        wing_csc_latch_update(0, 0x10000000u, 0) != 0 ||
        wing_csc_latch_update(0, 0x20000000u, 0) != 0 ||
        wing_csc_latch_update(0, 0x00000100u, 0) != 0)
        return -1;
    return 0;
}

Surface::Surface(X32BaseParameter* basepar): X32Base(basepar)
{
    uart = new Uart(basepar);
    touchUart = nullptr;
    if (config->IsModelAnyWing())
    {
        touchUart = new Uart(basepar);
    }
}

void Surface::Init(void)
{
    if (state->bodyless) {

        /* 
        
        How to connect x32ctrl bodyless mode to a X32 runnig Linux:

        Developer PC
        ############

        // create two virtual serial ports and connect them together as bridge
        # socat -d -d pty,raw,link=/tmp/ttyLocal,echo=0 pty,raw,link=/tmp/ttyRemote,echo=0

        // start netcat server on port 10000
        # nc -l 10000 </tmp/ttyRemote >/tmp/ttyRemote

        X32
        ###
        
        // set serial to 115200 baud
        # stty -F /dev/ttymxc1 115200 raw -echo -echoe -echok

        // start netcat client to transmit/receive serial from/to devloper pc
        # nc <ip of Developer PC> 10000 </dev/ttymxc1 >/dev/ttymxc1

        Developer PC
        ############        
        
        // start x32ctrl with bodyless commandline parameter "-b"
        # x32ctrl -b
        
        */

        uart->Open("/tmp/ttyLocal", 115200, true);
    }
    else if (state->raspi)
    {
        uart->Open("/dev/ttyUSB0", 115200, true);
    } 
    else if (config->IsModelAnyXM32())
    {
        uart->Open("/dev/ttymxc1", 115200, true);
    }
    else if (config->IsModelAnyWing())
    {
        if (InitCscTransportWing() != 0)
        {
            helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Init CSC Transport failed!");
        }

        uart->Open("/dev/ttymxc4", 115200, true);

        if (EnableCscLightsWing(0x0000602fu) != 0)
        {
            helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Enable CSC Lights failed!");
        }

        SendWingFrame('H', (const uint8_t*)"", 0);
        SendWingStockBaseline();

        if (touchUart)
        {
            touchUartOpen = touchUart->Open("/dev/ttymxc3", 115200, true) == 0;
            if (touchUartOpen)
            {
                EnableWingTouchscreen();
            }
        }
    }

    Reset();
}

void Surface::Reset(void)
{
     helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "Reset surface ...");

    if (state->bodyless) 
    {
        // TODO: integrate in Testing GUI
    }
    else
    {  
        if (!config->IsModelAnyWing()) {
            int fd = open("/sys/class/leds/reset_surface/brightness", O_WRONLY);
            write(fd, "1", 1);
            usleep(100 * 1000);
            write(fd, "0", 1);
            close(fd);
            usleep(2000 * 1000);
        }
    }

    FaderReset();

    // X32/M32 needs "double reset"
    if (config->IsModelAnyXM32())
    {
        FaderReset();
    }

    helper->DEBUG_SURFACE(DEBUGLEVEL_NORMAL, "... Done");
}

void Surface::FaderReset()
{
    if (config->HasFaders())
    {
        if (config->IsModelAnyXM32())
        {
            // Reset touchcontrol wait time
            for(uint8_t faderindex=0; faderindex<XM32_MAX_FADERS; faderindex++)
            {
                faders[faderindex].wait = 0;
            }

            // Reset position of faders
            uint8_t maxfaderindex = 0;
            if (config->IsModelX32FullOrM32())
            {
                maxfaderindex = XM32_MAX_FADERS;
            }
            if (config->IsModelX32CompactOrProducerOrM32R())
            {
                maxfaderindex = XM32_MAX_FADERS-8;
            }

            for(uint8_t faderindex=0; faderindex<maxfaderindex; faderindex++)
            {
                faders[faderindex].position_real = 0;
                SetFaderRaw(GetBoardId(faderindex), GetFaderId(faderindex), 0);
            }
        }
        else if (config->IsModelWingCompact())
        {
            for (uint8_t i = 0; i < 13; ++i)
            {
                faders[i].wait = 0;
                faders[i].position_real = 0;
                SetFaderRaw(OMC_BOARD_WING, i, 0);
            }
        }
    }
}

uint8_t Surface::GetChannelstripIndex(uint8_t boardId, uint8_t index)
{
    if (config->IsModelAnyWing())
    {
        return index;
    }

    switch (boardId)
    {
        case X32_BOARD_L:
            return index;
        case X32_BOARD_M: // only X32 Full
            return index + 8;
        case X32_BOARD_R:
            return index + (config->IsModelX32FullOrM32() ? 16 : 8);  // 16 - X32 Full, 8 - X32 Compact/Producer
        default:
            return 0;
    }
}

uint8_t Surface::GetBoardId(uint8_t faderindex)
{
    if(config->IsModelX32FullOrM32())
    {
        if (faderindex < 8){
            return X32_BOARD_L;
        }
        if (faderindex < 16){
            return X32_BOARD_M;
        }
        return X32_BOARD_R;
    }

    if(config->IsModelX32CompactOrProducerOrM32R())
    {
        if (faderindex < 8){
            return X32_BOARD_L;
        }
        return X32_BOARD_R;
    }

    if (config->IsModelAnyWing())
    {
        return OMC_BOARD_WING;
    }

    return 0;
}

uint8_t Surface::GetFaderId(uint8_t faderindex)
{
    if(config->IsModelX32FullOrM32())
    {
        if (faderindex < 8){
            return faderindex;
        }
        if (faderindex < 16){
            return faderindex-8;
        }
        return faderindex-16;
    }

    if(config->IsModelX32CompactOrProducerOrM32R())
    {
        if (faderindex < 8){
            return faderindex;
        }
        return faderindex-8;
    }

    if (config->IsModelAnyWing())
    {
        return faderindex;
    }

    return 0;
}


// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedIncrement(uint8_t pct) {
    uint8_t num_leds_to_light = 0;
/*
    if (pct <= 50) {
        // Scale 0-50 to 0-6 LEDs
        num_leds_to_light = (uint8_t)((float)pct / 50.0f * 6.0f);
    } else {
        // Scale 51-100 to 7-12 LEDs (6 more LEDs)
        // From 51 to 100, there are 50 steps.
        // We need to add (pct - 50) steps mapped to the remaining 6 LEDs.
        // (pct - 50) / 50.0f * 6.0f
        num_leds_to_light = 6 + (uint8_t)(((float)(pct - 50) / 50.0f) * 6.0f);

        if (num_leds_to_light > 12) {
            num_leds_to_light = 12;
        }
    }
*/
    num_leds_to_light = (uint8_t)((float)pct / 100.0f * 12.5f);
    if (num_leds_to_light > 13) {
        num_leds_to_light = 13;
    }

    uint16_t led_mask = 0;
    if (num_leds_to_light > 0) {
        led_mask = (1U << num_leds_to_light) - 1;
    }

    return led_mask;
}


uint16_t Surface::CalcEncoderRingLedDirect(uint8_t num_leds_to_light)
{
    if (num_leds_to_light > 13)
    {
        num_leds_to_light = 13;
    }

    uint16_t led_mask = 0;
    if (num_leds_to_light > 0)
    {
        led_mask = (1U << num_leds_to_light) - 1;
    }

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedDecrement(uint8_t pct) {
    uint8_t num_leds_to_light = 0;
/*
    if (pct <= 50) {
        // Scale 0-50 to 0-6 LEDs
        num_leds_to_light = (uint8_t)((float)pct / 50.0f * 6.0f);
    } else {
        // Scale 51-100 to 7-12 LEDs (6 more LEDs)
        // From 51 to 100, there are 50 steps.
        // We need to add (pct - 50) steps mapped to the remaining 6 LEDs.
        // (pct - 50) / 50.0f * 6.0f
        num_leds_to_light = 6 + (uint8_t)(((float)(pct - 50) / 50.0f) * 6.0f);

        if (num_leds_to_light > 12) {
            num_leds_to_light = 12;
        }
    }
*/
    num_leds_to_light = (uint8_t)((float)pct / 100.0f * 12.5f);
    if (num_leds_to_light > 13) {
        num_leds_to_light = 13;
    }

    uint16_t led_mask = 0;
    
    // Bits von rechts nach links setzen
    for (uint8_t i = 0; i < num_leds_to_light; ++i) {
        led_mask |= (1U << (12 - i));
    }

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedPosition(uint8_t pct) {
    uint8_t led_index = (uint8_t)(((float)pct / 100.0f) * 12.0f + 0.5f); // +0.5f für Rundung

    if (led_index > 12) {
        led_index = 12;
    }

    return (1U << led_index);
}

uint16_t Surface::CalcEncoderRingLedDbfs(float dbfs, bool onlyPosition) {
    uint16_t led_mask = 0;

   // if (config->IsModelX32Rack()){
        // X32Rack: Channel Level, Mail LR Level

        // LEDs dBfs
        // 1 -50
        // 2 -40
        // 3 -30
        // 4 -24
        // 5 -18
        // 6 -12
        // 7 -9
        // 8 -6
        // 9 -3
        // 10 0
        // 11 3
        // 12 6
        // 13 10


        if (dbfs > -60) { 
            uint8_t led_index = 0;

            if (dbfs >= 10) led_index = 12;
            else if (dbfs >= 6) led_index = 11;
            else if (dbfs >= 3) led_index = 10;
            else if (dbfs >= 0) led_index = 9;
            else if (dbfs >= -3) led_index = 8;
            else if (dbfs >= -6) led_index = 7;
            else if (dbfs >= -9) led_index = 6;
            else if (dbfs >= -12) led_index = 5;
            else if (dbfs >= -18) led_index = 4;
            else if (dbfs >= -24) led_index = 3;
            else if (dbfs >= -30) led_index = 2;
            else if (dbfs >= -40) led_index = 1;
            else if (dbfs > -50) led_index = 0;

            if (onlyPosition){
                led_mask = (1U << led_index);
            } else {
                led_mask = (1U << (led_index + 1)) -1;
            }
        } 

   // } else {
    
        // led_index = (uint8_t)(((float)pct / 100.0f) * 12.0f + 0.5f); // +0.5f für Rundung

        // if (led_index > 12) {
        //     led_index = 12;
        // }
    //}

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedBalance(uint8_t pct) {
    uint16_t led_mask = 0;

    if (pct < 50) {
        float scale = (float)pct / 50.0f; // Skaliert 0-49 auf 0-0.98
        // (scale * 6) = Anzahl der LEDs, die an sind.
        uint8_t num_on_left_side = (uint8_t)roundf(scale * 6.5f);

        // Sicherstellen, dass mindestens Bit 6 an ist, wenn pct < 50
        if (num_on_left_side < 1) num_on_left_side = 1;

        // Setze die Bits von Bit 0 bis zum berechneten Index
        for (int i = 0; i < num_on_left_side; ++i) {
            if (i <= 6) { // Sicherstellen, dass wir im Bereich Bits 0-6 bleiben
                led_mask |= (1U << i);
            }
        }

        // invert LED-mask
        led_mask ^= 0xFFFF;
        led_mask &= 0b0000000001111111;

    } else { // pct >= 50
        // Skaliere (pct - 50) von 1-50 auf die Anzahl der LEDs, die von Bit 6 nach rechts zusätzlich an sein sollen.
        float scale = (float)(pct - 50) / 50.0f; // Skaliert 51-100 auf 0.02-1
        uint8_t num_on_right_side = (uint8_t)roundf(1.0f + (scale * 6.5f)); // 1 für Bit 6, plus 6 weitere LEDs

        // Sicherstellen, dass mindestens Bit 6 an ist, wenn pct > 50
        if (num_on_right_side < 1) num_on_right_side = 1;

        // Setze die Bits von Bit 6 bis zum berechneten Index
        for (int i = 0; i < num_on_right_side; ++i) {
            if ((6 + i) <= 12) { // Sicherstellen, dass wir im Bereich Bits 6-12 bleiben
                led_mask |= (1U << (6 + i));
            }
        }
    }

    return led_mask;
}

// bit 0=CCW, bit 6=center, bit 12 = CW, bit 15=encoder-backlight
// CCW <- XXXXXX X XXXXXX -> CW
uint16_t Surface::CalcEncoderRingLedWidth(uint8_t pct) {
    if (pct == 0) {
        return (1U << 6); // Setzt nur Bit 6
    }

    float scaled_value = (float)(pct - 1) / 99.0f; // Skaliert 1-100 auf 0-1

    // Anzahl der zusätzlichen LEDs, die auf der linken Seite von Bit 6 aus eingeschaltet werden sollen.
    // Max 6 LEDs (Bits 0-5).
    uint8_t num_left_additional_leds = (uint8_t)roundf(scaled_value * 6.0f);
    // Anzahl der zusätzlichen LEDs, die auf der rechten Seite von Bit 6 aus eingeschaltet werden sollen.
    // Max 7 LEDs (Bits 7-12).
    uint8_t num_right_additional_leds = (uint8_t)roundf(scaled_value * 6.0f);

    uint16_t led_mask = (1U << 6); // Starte mit Bit 6 gesetzt

    // Schalte die zusätzlichen LEDs auf der linken Seite ein
    for (int i = 0; i < num_left_additional_leds; ++i) {
        if ((6 - (i + 1)) >= 0) { // Sicherstellen, dass der Index nicht negativ wird
            led_mask |= (1U << (6 - (i + 1)));
        }
    }

    // Schalte die zusätzlichen LEDs auf der rechten Seite ein
    for (int i = 0; i < num_right_additional_leds; ++i) {
        if ((6 + (i + 1)) <= 12) { // Sicherstellen, dass der Index nicht über 15 hinausgeht
            led_mask |= (1U << (6 + (i + 1)));
        }
    }

    // Bei 100% sollen alle Bits gesetzt sein (0x1FFF).
    // Die Berechnung oben sollte dies bereits erreichen, aber eine explizite Prüfung schadet nicht.
    if (pct == 100) {
        return 0x1FFF; // Alle 13 Bits setzen
    }

    return led_mask;
}

void Surface::SetBrightnessAllBoards(uint8_t brightness) {
    SetBrightness(X32_BOARD_MAIN, brightness);
    SetBrightness(X32_BOARD_L, brightness);
    SetBrightness(X32_BOARD_M, brightness);
    SetBrightness(X32_BOARD_R, brightness);
}

// boardId = 0, 1, 4, 5, 8
// index = 0 ... 8
// brightness = 0 ... 255
void Surface::SetBrightness(uint8_t boardId, uint8_t brightness) {
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('C'); // class: C = Controlmessage
    message.AddDataByte('B'); // index
    message.AddDataByte(brightness);
    SendData(&message, true);
}

void Surface::SetContrastAllBoards(uint8_t contrast) {
    SetContrast(X32_BOARD_MAIN, contrast);
    SetContrast(X32_BOARD_L, contrast);
    SetContrast(X32_BOARD_M, contrast);
    SetContrast(X32_BOARD_R, contrast);
}

// boardId = 0, 1, 4, 5, 8
// contrast = 0 ... 255
void Surface::SetContrast(uint8_t boardId, uint8_t contrast) {
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('C'); // class: C = Controlmessage
    message.AddDataByte('C'); // index
    message.AddDataByte(contrast & 0x3F);
    SendData(&message, true);
}

void Surface::SetLed(SurfaceElementId buttonOrLed, bool ledOn, bool blink)
{
    if(blink)
    {
        blinklist.insert(buttonOrLed);
    }
    else
    {
        if (!blinklist.empty())
        {
            set<SurfaceElementId>::iterator it = blinklist.find(buttonOrLed);
            if (it != blinklist.end())
            {
                blinklist.erase(it);
            }
        }
    }

    SetLed(buttonOrLed, ledOn);
}

void Surface::SetLed(SurfaceElementId buttonOrLed, bool ledOn)
{
    SurfaceElement *element = config->GetSurfaceElement(buttonOrLed);
    SetLedRaw((uint)element->GetBoard(), (uint)element->GetIndex(), ledOn);
}

void Surface::SetLedRaw(uint board, uint index, bool ledOn)
{
    SurfaceMessage message;
    message.AddDataByte(0x80 + board);
    message.AddDataByte('L');  // class: L = LED
    message.AddDataByte(0x80); // index - fixed at 0x80 for LEDs
    if (ledOn)
    {
        message.AddDataByte(index + 0x80); // turn LED on
    }
    else
    {
        message.AddDataByte(index); // turn LED off
    }
    SendData(&message, true);
}

// set 7-Segment display on X32 Rack
// dot = 128
void Surface::SetX32RackDisplayRaw(uint8_t p_value2, uint8_t p_value1){
    SurfaceMessage message;
    message.AddDataByte(0x80);
    message.AddDataByte('D'); // Display
    message.AddDataByte(0x80);
    message.AddDataByte(p_value2); 
    message.AddDataByte(p_value1);
    SendData(&message, true);
}

// set 7-Segment display on X32 Rack
// dot = 128 or 256
void Surface::SetX32RackDisplay(uint8_t vChannelIndex){
    uint8_t vChannelNumber = vChannelIndex + 1;
    if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::NORMAL)) {
        uint8_t segment_l = vChannelNumber < 10 ? 0 : int2segment((uint8_t)(vChannelNumber/10));
        
        SetX32RackDisplayRaw(segment_l, int2segment(vChannelNumber % 10));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::AUX)) {
        SetX32RackDisplayRaw(int2segment('A'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::AUX));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::FXRET)) {
        SetX32RackDisplayRaw(int2segment('F'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::FXRET));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::BUS)) {
        uint8_t number = vChannelNumber - (uint)X32_VCHANNEL_BLOCK::BUS;
        SetX32RackDisplayRaw(int2segment((uint8_t)(number/10)), int2segment(number % 10));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::MATRIX)) {
        SetX32RackDisplayRaw(int2segment('M'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::MATRIX));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::DCA)) {
        SetX32RackDisplayRaw(int2segment('D'), int2segment(vChannelNumber - (uint)X32_VCHANNEL_BLOCK::DCA));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::MAIN)) {
        SetX32RackDisplayRaw(int2segment('M'), int2segment(' '));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::MAINSUB)) {
        SetX32RackDisplayRaw(int2segment('M'), int2segment(5));
    } else if (helper->IsInChannelBlock(vChannelIndex, X32_VCHANNEL_BLOCK::SPECIAL)) {
        SetX32RackDisplayRaw(int2segment(5), int2segment(' '));
    }
}

uint8_t Surface::int2segment(int8_t p_value){
    switch (p_value){
        case 0:
            return 63;
        case 1:
            return 6;
        case 2:
            return 91;
        case 3:
            return 79;
        case 4:
            return 102;
        case 5:
            return 109;
        case 6:
            return 125;
        case 7:
            return 7;
        case 8:
            return 127;
        case 9:
            return 111;
        case 'a':
        case 'A':
            return 119;
        case 'b':
        case 'B':
            return 124;
        case 'd':
        case 'D':
            return 94;
        case 'f':
        case 'F':
            return 1 + 32 + 64 + 16;
        case 'm':
        case 'M':
            return 55;
        case ' ':
            return 0;
        default:
            return 0;
    }
}

// boardId = 0, 1, 4, 5, 8
// index = 0 ... 8
// leds = 8-bit bitwise (bit 0=-60dB ... 4=-6dB, 5=Clip, 6=Gate, 7=Comp)
void Surface::SetMeterLed(uint8_t boardId, uint8_t index, uint8_t leds)
{
  // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
  // 0x4C, index, leds.b[]
  SurfaceMessage message;
  message.AddDataByte(0x80 + boardId); // start message for specific boardId
  message.AddDataByte('M'); // class: M = Meter
  message.AddDataByte(index); // index
  message.AddDataByte(leds);
  SendData(&message, true);
}

// preamp = 8-bit bitwise (bit 0=Sig, 1=-30dB ... 6=-3dB, 7=Clip)
// meter = 32-bit bitwise (bit 0=-45dB ... 15=-4, 16=-2, 19=Clip, 20+=unused)
void Surface::SetMeterLedMain_Rack(uint8_t preamp, uint32_t meterL, uint32_t meterR, uint32_t meterSolo)
{
    SurfaceMessage message;
    message.AddDataByte(0x80); // start message for specific boardId
    message.AddDataByte('M'); // class: M = Meter
    message.AddDataByte(0); // index
    message.AddDataByte(preamp);

    // Example for Big Meters on X32Rack (!different scale to X32Full/Compact)
    //message.AddDataByte(0b11110000);  // first nibble shifted 4 to left
    //message.AddDataByte(0b11111111);  // bit 4..12 shifted 4 to right
    //message.AddDataByte(0b10110111);  // last bits are crazy splitted :-/

    message.AddDataByte((uint8_t)(meterL<<4));
    message.AddDataByte((uint8_t)(meterL>>4));
    message.AddDataByte((((uint8_t)(meterL>>12))&0b10000111) | (((uint8_t)(meterL>>11))&0b00110000));
    message.AddDataByte(0);
    message.AddDataByte((uint8_t)(meterR<<4));
    message.AddDataByte((uint8_t)(meterR>>4));
    message.AddDataByte((((uint8_t)(meterR>>12))&0b10000111) | (((uint8_t)(meterR>>11))&0b00110000));
    message.AddDataByte(0x00);
    message.AddDataByte((uint8_t)(meterSolo<<4));
    message.AddDataByte((uint8_t)(meterSolo>>4));
    message.AddDataByte((((uint8_t)(meterSolo>>12))&0b10000111) | (((uint8_t)(meterSolo>>11))&0b00110000));
    message.AddDataByte(0x00);
    SendData(&message, true);
}

void Surface::SetMeterLedMain_Producer(uint8_t preamp, uint8_t dynamics, uint32_t meterL, uint32_t meterR, uint32_t meterSolo)  {
    
    // TODO
    // meter scale is from Rack
    // dynamics is like Full/COmpact
    // --> has to be tested
    
}

// leds = 8-bit bitwise (bit 0=-60dB ... 4=-6dB, 5=Clip, 6=Gate, 7=Comp)
// leds = 32-bit bitwise (bit 0=-57dB ... 22=-2, 23=-1, 24=Clip)
void Surface::SetMeterLedMain_FullOrCompact(uint8_t preamp, uint8_t dynamics, uint32_t meterL, uint32_t meterR, uint32_t meterSolo) {
    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x4C, index, leds.b[]
    SurfaceMessage message;
    message.AddDataByte(0x80 + 1); // start message for specific boardId
    message.AddDataByte('M'); // class: M = Meter
    message.AddDataByte(0); // index
    message.AddDataByte(preamp);
    message.AddDataByte(dynamics);
    message.AddDataByte((uint8_t)meterL);
    message.AddDataByte((uint8_t)(meterL>>8));
    message.AddDataByte((uint8_t)(meterL>>16));
    message.AddDataByte(0x00);
    message.AddDataByte((uint8_t)meterR);
    message.AddDataByte((uint8_t)(meterR>>8));
    message.AddDataByte((uint8_t)(meterR>>16));
    message.AddDataByte(0x00);
    message.AddDataByte((uint8_t)meterSolo);
    message.AddDataByte((uint8_t)(meterSolo>>8));
    message.AddDataByte((uint8_t)(meterSolo>>16));
    message.AddDataByte(0x00);
    SendData(&message, true);
}

// boardId = 0, 1, 4, 5, 8
// index
// ledMode = 0=increment, 1=absolute position, 2=balance l/r, 3=width/spread, 4=decrement, 5=direct -> the number of leds to turn on (max. 13)
// ledPct = percentage 0...100
// backlight = enable or disable
void Surface::SetEncoderRing(uint8_t boardId, uint8_t index, uint8_t ledMode, uint8_t ledPct, bool backlight) {
    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x52, index, leds.w[]
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('R'); // class: R = Ring
    message.AddDataByte(index); // index

    uint16_t leds = 0;
    switch (ledMode) {
        case 0: // standard increment-method
            leds = CalcEncoderRingLedIncrement(ledPct);
            break;
        case 1: // absolute position
            leds = CalcEncoderRingLedPosition(ledPct);
            break;
        case 2: // balance left/right
            leds = CalcEncoderRingLedBalance(ledPct);
            break;
        case 3: // spread/width
            leds = CalcEncoderRingLedWidth(ledPct);
            break;
        case 4: // decrement-method
            leds = CalcEncoderRingLedDecrement(ledPct);
            break;
        case 5: // direct number of leds (max 13)
            leds = CalcEncoderRingLedDirect(ledPct);
            break;
        case 6: // direct position of led (max 13)
            leds = (1U << (ledPct-1));
            break;
    }
    message.AddDataByte(leds & 0xFF);
    if (backlight) {
        message.AddDataByte(((leds & 0x7F00) >> 8) + 0x80); // turn backlight on
    }else{
        message.AddDataByte(((leds & 0x7F00) >> 8)); // turn backlight off
    }
    SendData(&message, true);
}

void Surface::SetEncoderRingDbfs(uint8_t boardId, uint8_t index, float dbfs, bool muted, bool backlight) {
    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x52, index, leds.w[]
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('R'); // class: R = Ring
    message.AddDataByte(index); // index

    uint16_t leds = CalcEncoderRingLedDbfs(dbfs, muted);

    message.AddDataByte(leds & 0xFF);
    if (backlight) {
        message.AddDataByte(((leds & 0x7F00) >> 8) + 0x80); // turn backlight on
    }else{
        message.AddDataByte(((leds & 0x7F00) >> 8)); // turn backlight off
    }
    SendData(&message, true);
}

// boardId = 0, 4, 5, 8
// index = 0 ... 8
// color = 0=BLACK, 1=RED, 2=GREEN, 3=YELLOW, 4=BLUE, 5=PINK, 6=CYAN, 7=WHITE
// icon = 0xA0 (none), 0xA1 ... 0xE9
// sizeA/B = 0x00 (small) or 0x20 (large)
// xA/B = horizontal position in pixel
// yA/B = vertical position in pixel
// strA/B = String of Text to be displayed
void Surface::SetLcd(
    uint8_t boardId, uint8_t index, uint8_t color,
    uint8_t xicon, uint8_t yicon, uint8_t icon, 
    uint8_t sizeA, uint8_t xA, uint8_t yA, const char* strA,
    uint8_t sizeB, uint8_t xB, uint8_t yB, const char* strB
    ) {
    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x44, i, color.b, script[]

    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('D'); // class: D = Display
    message.AddDataByte(index); // index
    message.AddDataByte(color & 0x0F); // use only 4 bits (bit 0=R, 1=G, 2=B, 3=Inverted)

    // begin of script
    // transmit icon
    message.AddDataByte(icon);
    message.AddDataByte(xicon);
    message.AddDataByte(yicon);

    // transmit first text
    message.AddDataByte(sizeA + strlen(strA)); // size + textLength
    message.AddDataByte(xA);
    message.AddDataByte(yA);
    message.AddString(strA); // this is ASCII, so we can omit byte-stuffing

    message.AddDataByte(sizeB + strlen(strB)); // size + textLength
    message.AddDataByte(xB);
    message.AddDataByte(yB);
    message.AddString(strB); // this is ASCII, so we can omit byte-stuffing
    SendData(&message, true);
}

void Surface::SetLcdX(LcdData* p_data, uint8_t p_textCount) {
    SurfaceMessage message;
    message.AddDataByte(0x80 + p_data->boardId);
    message.AddDataByte('D'); // class: D = Display
    message.AddDataByte(p_data->lcdIndex); 
    message.AddDataByte((p_data->color) & 0x0F);
    message.AddDataByte(p_data->icon.icon);
    message.AddDataByte(p_data->icon.x);
    message.AddDataByte(p_data->icon.y);
    for (int i=0;i<p_textCount;i++){
        message.AddDataByte(p_data->texts[i].size + strlen(p_data->texts[i].text.c_str())); // size + textLength
        message.AddDataByte(p_data->texts[i].x);
        message.AddDataByte(p_data->texts[i].y);
        message.AddString(p_data->texts[i].text.c_str()); // this is ASCII, so we can omit byte-stuffing  
    }
    SendData(&message, true);
}

void Surface::Blink()
{
    if (blinkwait == 0)
    {
        blinkstate = !blinkstate;

        for(SurfaceElementId button : blinklist) {
            SetLed(button, blinkstate);
        }

        blinkwait = 5;
    }

    blinkwait--; 
}

void Surface::SetFader(uint8_t boardId, uint8_t index, uint16_t position)
{
    uint8_t faderindex = GetChannelstripIndex(boardId, index);
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Want to move fader at index %d to %d", faderindex, position);
    faders[faderindex].position_wanted = position;
}

void Surface::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value)
{
     uint8_t faderindex = GetChannelstripIndex(boardId, index);
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Fader at index %d moved to %d", faderindex, value);
    faders[faderindex].position_wanted = value;
    faders[faderindex].position_real = value;
    faders[faderindex].wait = 10; // wait 100x 10ms
}

void Surface::Touchcontrol()
{
    uint8_t maxfaderindex = 0;
    if (config->IsModelX32FullOrM32())
    {
        maxfaderindex = XM32_MAX_FADERS;
    } 
    else if (config->IsModelX32CompactOrProducerOrM32R())
    {
        maxfaderindex = XM32_MAX_FADERS-8;
    } 
    else if (config->IsModelWingCompact())
    {
        maxfaderindex = 13;
    }

    for(uint8_t faderindex=0; faderindex<maxfaderindex; faderindex++)
    {
        if (faders[faderindex].wait > 0)
        {
            faders[faderindex].wait--;
            helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "Reduced wait time on fader at index %d to %d", faderindex, faders[faderindex].wait);
        }
        else if (faders[faderindex].position_real != faders[faderindex].position_wanted)
        {
            helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Move fader at index %d from %d to %d", faderindex, faders[faderindex].position_real, faders[faderindex].position_wanted);
            faders[faderindex].position_real = faders[faderindex].position_wanted;
            SetFaderRaw(GetBoardId(faderindex), GetFaderId(faderindex), faders[faderindex].position_wanted);
        }
    }
}

// position = 0x0000 ... 0x0FFF
void Surface::SetFaderRaw(uint8_t boardId, uint8_t index, uint16_t position)
{
    SurfaceMessage message;
    
    if (config->IsModelAnyXM32())
    {
        message.AddDataByte(0x80 + boardId); // start message for specific boardId
    }
    if (config->IsModelAnyXM32())
    {
        message.AddDataByte('F'); // class: F = Fader
    }
    message.AddDataByte(index); // index
    message.AddDataByte((position & 0xFF)); // LSB
    message.AddDataByte((char)((position & 0x0F00) >> 8)); // MSB
    
    helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "Set fader position on board %d at index %d to %d", boardId, index, position);

    if (config->IsModelAnyXM32())
    {
        SendData(&message, true);
    }
    if (config->IsModelAnyWing())
    {
        uint8_t payload[3];
        payload[0] = index;
        payload[1] = (uint8_t)(position & 0xFF);
        payload[2] = (uint8_t)((position >> 8) & 0x0F);
        SendWingFrame('F', payload, 3);
    }
}

// incoming message has the form: 0xFE 0x8i Class Index Data[] 0xFE
// Checksum is calculated using the following equation:
// chksum = ( 0xFE - i - class - index - sumof(data[]) - sizeof(data[]) ) and 0x7F
uint8_t Surface::calculateChecksum(const char* data, uint16_t len)
{
  // a single message can contain up to max. 64 chars
  int32_t sum = 0xFE;
  for (uint8_t i = 0; i < (len-1); i++) {
    sum -= data[i];
  }
  sum -= (len - 3); // remove 2-byte HEADER (0xFE 0x8i) and 1-byte end (0xFE)

  // write the calculated sum to the last element of the array
  return (sum & 0x7F);
}

int Surface::SendData(MessageBase* message, bool addChecksum)
{
    if (config->IsModelAnyXM32())
    {
        message->AddRawByte(0xFE); // Endbyte

        if (addChecksum)
        {
            char checksum = 0;
            if (message->current_length >= 2) 
            {
                // at least start- and end-byte
                checksum = calculateChecksum(message->buffer, message->current_length);
            }

            // add checksum to message and send data via serial-port
            message->AddRawByte(checksum);
        }

        return uart->Tx(message);
    }
    return 0;
}

int Surface::SendWingFrame(Uart* targetUart, uint8_t cmd, const uint8_t* payload, size_t len)
{
    if (!config->IsModelAnyWing() || !targetUart)
    {
        return 0;
    }

    MessageBase msg;
    msg.AddRawByte('*'); // WING_FRAME_STAR
    
    if (cmd == 0x2a) {
        msg.AddRawByte(0x2a);
        msg.AddRawByte(0x40);
    } else {
        msg.AddRawByte(cmd);
    }

    for (size_t i = 0; i < len; ++i) {
        if (payload[i] == 0x2a) {
            msg.AddRawByte(0x2a);
            msg.AddRawByte(0x40);
        } else {
            msg.AddRawByte(payload[i]);
        }
    }

    msg.AddRawByte('*');
    uint8_t chk = helper->CalculateWingChecksum(payload, len);
    if (chk == 0x2a) {
        msg.AddRawByte(0x2a);
        msg.AddRawByte(0x40);
    } else {
        msg.AddRawByte(chk);
    }

    return targetUart->Tx(&msg);
}

int Surface::SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len)
{
    return SendWingFrame(uart, cmd, payload, len);
}

void Surface::EnableWingTouchscreen()
{
    if (!config->IsModelAnyWing() || !touchUart || !touchUartOpen)
    {
        return;
    }

    uint8_t payload[1] = { 1 };
    int rc = SendWingFrame(touchUart, 'I', payload, sizeof(payload));
    helper->DEBUG_SURFACE(
        rc >= 0 ? DEBUGLEVEL_VERBOSE : DEBUGLEVEL_NORMAL,
        "WING touchscreen enable %s",
        rc >= 0 ? "sent" : "failed"
    );
}

int Surface::InitCscTransportWing()
{
    const size_t map_len = 0x1000;
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    uint8_t *iomuxc;
    uint8_t *gpio5;
    uint32_t value;

    if (mem_fd < 0)
        return -1;
    iomuxc = (uint8_t *)mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, IOMUXC_BASE);
    if (iomuxc == MAP_FAILED) {
        close(mem_fd);
        return -1;
    }
    gpio5 = (uint8_t *)mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, GPIO5_BASE);
    if (gpio5 == MAP_FAILED) {
        munmap(iomuxc, map_len);
        close(mem_fd);
        return -1;
    }

    writel_ptr(iomuxc, PAD_KEY_COL1, PAD_UART_TX_STOCK);
    writel_ptr(iomuxc, MUX_KEY_COL1, UART5_ALT_MODE);
    writel_ptr(iomuxc, PAD_KEY_ROW1, PAD_UART_RX_STOCK);
    writel_ptr(iomuxc, MUX_KEY_ROW1, UART5_ALT_MODE);
    writel_ptr(iomuxc, SEL_UART5_RX, UART5_RX_DAISY_STOCK);
    value = readl_ptr(gpio5, GPIO_GDIR_OFF);
    writel_ptr(gpio5, GPIO_GDIR_OFF, value | CSC_RESET_MASK);
    value = readl_ptr(gpio5, GPIO_DR_OFF);
    writel_ptr(gpio5, GPIO_DR_OFF, value & ~CSC_RESET_MASK);
    usleep(50000);
    value = readl_ptr(gpio5, GPIO_DR_OFF);
    value |= (1u << CSC_RESET_BIT);
    value &= ~(1u << CSC_BOOT_BIT);
    writel_ptr(gpio5, GPIO_DR_OFF, value);
    usleep(500000);

    munmap(gpio5, map_len);
    munmap(iomuxc, map_len);
    close(mem_fd);
    return 0;
}

int Surface::EnableCscLightsWing(uint32_t value)
{
    return wing_enable_csc_lights(value);
}

void Surface::SendWingStockBaseline()
{
    static const struct {
        uint8_t cmd;
        size_t len;
    } blocks[] = {
        { 'B', 9 }, { 'L', 10 }, { 'l', 13 }, { 'C', 36 }, { 'M', 26 },
    };

    uint8_t payload[64];
    memset(payload, 0, sizeof(payload));

    for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i) {
        SendWingFrame(blocks[i].cmd, payload, blocks[i].len);
        usleep(20000);
    }
    for (uint8_t id = 0x38; id <= 0x3f; ++id) {
        uint8_t u_payload[4] = { id, 0, 50, 0x08 };
        SendWingFrame('U', u_payload, sizeof(u_payload));
        usleep(20000);
    }
}
