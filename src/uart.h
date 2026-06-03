#pragma once

#include <cstdint>
#include <stdio.h>
#include <string.h>
#ifndef __APPLE__
#include <linux/input.h>
#endif
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h> // for FIONREAD
#include <unistd.h>

#include "base.h"
#include "message-base.h"
#include "types.h"

#ifdef BODYLESS_SDL2
#include <vector>
#include <algorithm>
#endif

class Uart : public X32Base
 {
    
    private:
        int fd;
#ifdef BODYLESS_SDL2
        std::vector<char> emulated_rx_buffer;
        std::vector<char> emulated_tx_buffer;
#endif

    public:
        Uart(X32BaseParameter* basepar);
        int Open(const char* ttydev, uint32_t baudrate, bool raw);
        int Tx(MessageBase* message);
        int Rx(char* buf, uint16_t bufLen);
        //void MirrorBack();
        void FlushRxBuffer();
#ifdef BODYLESS_SDL2
        void WriteEmulatedRx(const char* data, size_t len);
        size_t ReadEmulatedTx(char* buf, size_t max_len);
        bool HasEmulatedTx() const;
#endif
};