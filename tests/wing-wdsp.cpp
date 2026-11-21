// SPDX-License-Identifier: GPL-3.0-only
#include "wing-wdsp.h"
#include "spibone_uapi.h"
#include <linux/magic.h>
#include <linux/spi/spidev.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

#include <array>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
std::string dir;
int control, dsp, dspOpens, transfers, controlOpens;
bool offline, noEntropy, nonBootLocal;
unsigned expectedCore, resetMask, changedCsr;
constexpr uint32_t nonce = 0x19274690;
enum BadReply { Good, Empty, Short, Early, Magic, Abi, Core, Nonce, RequestCrc, ReplyCrc, Status, Flags, Reserved };
BadReply bad;

void Put(uint8_t* p, uint32_t v)
{
    for (int i = 3; i >= 0; --i, v >>= 8) p[i] = v & 255;
}

void Receipt(const std::string& digest, const std::string& image, unsigned mask)
{
    std::ofstream out(dir + "/receipt");
    out << R"({"manifest":"/usr/share/wing-t55/manifest.json","build_id":305419896,"chip_select":1,)"
        << R"("spi_device":"/dev/spibone0","dsp_runtime_mask":)" << mask
        << R"(,"manifest_sha256":")" << digest << R"(","bitstream_sha256":")" << image << "\"}\n";
}

void Try(unsigned core, bool success, int count, int opens, bool checkControl = true)
{
    expectedCore = core;
    transfers = dspOpens = controlOpens = 0;
    try {
        OMC::IdentifyWingDsp(core);
        assert(success);
    } catch (const std::runtime_error& e) {
        if (success || transfers != count || dspOpens != opens) std::cerr << e.what() << '\n';
        assert(!success);
    }
    assert(transfers == count && dspOpens == opens);
    if (!checkControl) assert(controlOpens == 0);
}
} // namespace

extern "C" int __real_open(const char*, int, ...);
extern "C" int __real_fstat(int, struct stat*);
extern "C" int __wrap_fstat(int fd, struct stat* st)
{
    const int ret = __real_fstat(fd, st);
    if (!ret && st->st_uid == geteuid() && st->st_gid == getegid()) st->st_uid = st->st_gid = 0;
    return ret;
}

extern "C" int __wrap_lstat(const char* path, struct stat* st)
{
    assert(path == std::string("/run") || path == std::string("/usr/share/wing-t55"));
    *st = {};
    st->st_mode = S_IFDIR | 0755;
    return 0;
}

extern "C" int __wrap_statfs(const char* path, struct statfs* fs)
{
    assert(path == std::string("/run"));
    fs->f_type = nonBootLocal ? 0 : TMPFS_MAGIC;
    return 0;
}

extern "C" int __wrap_open(const char* path, int flags, ...)
{
    assert((flags & (O_NOFOLLOW | O_CLOEXEC)) == (O_NOFOLLOW | O_CLOEXEC));
    const std::string p(path);
    if (p == "/run/wing-t55.lock") return __real_open((dir + "/lock").c_str(), flags);
    if (p == "/run/omc-wing-fpga.json") return __real_open((dir + "/receipt").c_str(), flags);
    if (p == "/usr/share/wing-t55/manifest.json") return __real_open((dir + "/manifest").c_str(), flags);
    if (p == "/dev/spibone0") {
        ++controlOpens;
        return control = __real_open("/dev/null", flags);
    }
    assert(p == "/dev/spidev1.0");
    ++dspOpens;
    return dsp = __real_open("/dev/null", flags);
}

extern "C" ssize_t __wrap_getrandom(void* out, size_t size, unsigned flags)
{
    assert(size == sizeof nonce && flags == 1);
    std::memcpy(out, &nonce, size);
    return noEntropy ? -1 : size;
}

extern "C" int __wrap_ioctl(int fd, unsigned long op, ...)
{
    va_list args;
    va_start(args, op);
    if (fd == control) {
        if (op == SPIBONE_IOC_GET_STATE) {
            *va_arg(args, uint32_t*) = offline ? SPIBONE_STATE_OFFLINE : SPIBONE_STATE_ONLINE;
        } else {
            assert(op == SPIBONE_IOC_PEEK); // No write ioctl may reach the FPGA.
            auto* reg = va_arg(args, spibone_wb_xfer*);
            const std::array values{0x57494e47u, 0x20000u, 0x12345678u, 0x0fu, 0x101u, resetMask, 0u, 0u};
            assert(reg->addr >= 0x1800 && reg->addr <= 0x181c && reg->addr % 4 == 0);
            reg->val = values[(reg->addr - 0x1800) / 4] ^ (reg->addr == changedCsr ? 1 : 0);
        }
        va_end(args);
        return 0;
    }
    assert(fd == dsp);
    if (op == SPI_IOC_WR_MODE32) assert(*va_arg(args, uint32_t*) == 0);
    else if (op == SPI_IOC_WR_BITS_PER_WORD) assert(*va_arg(args, uint8_t*) == 8);
    else if (op == SPI_IOC_WR_MAX_SPEED_HZ) assert(*va_arg(args, uint32_t*) == 10000);
    else {
        assert(op == SPI_IOC_MESSAGE(1));
        ++transfers;
        const auto& xfer = *va_arg(args, spi_ioc_transfer*);
        va_end(args);
        assert(xfer.len == 73 && xfer.bits_per_word == 8 && xfer.cs_change == 0 && xfer.speed_hz == 10000);
        const auto* tx = reinterpret_cast<const uint8_t*>(xfer.tx_buf);
        auto* rx = reinterpret_cast<uint8_t*>(xfer.rx_buf);
        std::array<uint8_t, 73> expected{};
        expected[0] = expected[12] = expectedCore;
        std::memcpy(expected.data() + 1, "WDSP", 4);
        Put(expected.data() + 5, nonce);
        const uint32_t crc = crc32(0, expected.data() + 1, 36);
        Put(expected.data() + 37, crc);
        assert(std::memcmp(tx, expected.data(), expected.size()) == 0);
        const int exclusive = __real_open((dir + "/lock").c_str(), O_RDWR);
        assert(exclusive >= 0 && flock(exclusive, LOCK_EX | LOCK_NB) < 0);
        close(exclusive);
        if (bad != Empty) {
            auto* reply = rx + (bad == Early ? 40 : 49);
            std::memcpy(reply, bad == Magic ? "WDSP" : "WDSR", 4);
            Put(reply + 4, bad == Abi ? 2 : 1);
            Put(reply + 8, nonce ^ (bad == Nonce));
            reply[12] = bad == Core ? 3 : expectedCore;
            reply[13] = bad == Status ? 2 : 0;
            reply[14] = bad == Flags ? 3 : 1;
            reply[15] = bad == Reserved ? 1 : 0;
            Put(reply + 16, crc ^ (bad == RequestCrc));
            Put(reply + 20, crc32(0, reply, 20) ^ (bad == ReplyCrc));
        }
        return xfer.len - (bad == Short);
    }
    va_end(args);
    return 0;
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    dir = argv[1];
    const std::string digest = argv[2], image = argv[3];
    Try(4, false, 0, 0, false); // No receipt.
    Receipt(digest, image, 0);
    Try(4, false, 0, 0, false);
    Receipt(digest, image, 8);
    Try(2, false, 0, 0, false);
    Try(3, false, 0, 0, false);
    Receipt(digest, image, 12); // Reject the whole record, not merely DSP3.
    Try(4, false, 0, 0, false);
    Receipt(image, image, 8); // Hash mismatch.
    Try(4, false, 0, 0, false);
    Receipt(digest, std::string(64, '1'), 8); // Image mismatch.
    Try(4, false, 0, 0, false);
    Receipt(digest, image, 0x0b);
    const auto path = dir + "/receipt";
    assert(chmod(path.c_str(), 0644) == 0);
    Try(4, false, 0, 0, false);
    assert(chmod(path.c_str(), 0600) == 0);
    const int exclusive = __real_open((dir + "/lock").c_str(), O_RDWR);
    assert(exclusive >= 0 && flock(exclusive, LOCK_EX | LOCK_NB) == 0);
    Try(4, false, 0, 0, false);
    close(exclusive);
    nonBootLocal = true;
    Try(4, false, 0, 0, false);
    nonBootLocal = false;
    resetMask = 0x0b;
    offline = true;
    Try(4, false, 0, 0);
    offline = false;
    for (changedCsr = 0x1800; changedCsr <= 0x181c; changedCsr += 4) Try(4, false, 0, 0);
    noEntropy = true;
    Try(4, false, 0, 1);
    noEntropy = false;
    for (auto core : {1u, 2u, 4u}) Try(core, true, 1, 1);
    for (bad = Empty; bad <= Reserved; bad = static_cast<BadReply>(bad + 1)) Try(4, false, 1, 1);
    std::cout << "PASS: W2 boot gates and WDSP v1 identity/reply failures\n";
}
