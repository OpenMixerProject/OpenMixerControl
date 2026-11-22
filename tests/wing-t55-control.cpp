// Linux host-side contract test. Link --wrap to intercept SPI; no device is opened.
#include "../src/wing-t55-control.h"
#include "../src/spibone_uapi.h"


#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <linux/spi/spidev.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

// Debian armhf redirects these APIs to large-file/time64 symbols.
#if defined(__USE_TIME_BITS64) && __TIMESIZE == 32
#define __real_open __real_open64
#define __wrap_open __wrap_open64
#define __real_lstat __real___lstat64_time64
#define __wrap_lstat __wrap___lstat64_time64
#define __real_ioctl __real___ioctl_time64
#define __wrap_ioctl __wrap___ioctl_time64
#endif

namespace {

constexpr char receiptPath[] = "/run/omc-wing-fpga.json";
constexpr char manifestPath[] = "/usr/share/wing-t55/manifest.json";
constexpr char lockPath[] = "/run/wing-t55.lock";
constexpr char digest[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
constexpr int fakeSpi = 999, fakeBone = 998, fakeDsp = 997;
constexpr uint32_t routeBase = 0x80000000;
// Intentionally not the physical bootstrap offsets: consume the manifest, not constants.
constexpr std::array<uint32_t, 5> identityAddresses{0x208, 0x20c, 0x210, 0x214, 0x218};
constexpr std::array<uint32_t, 5> identityWords{0x57494e47, 0x20000, 0x01234567, 15, 1};
std::string fixture;

enum class Fault { None, Magic, Abi, BuildId, Count, NotReady, NoAck, BadMarker, Short, IoError, Readback,
                   Offline, Capabilities };
Fault fault;
unsigned opens, recoveryFrames, writes;
uint32_t config, routed;
unsigned boneOpens, peeks, pokes, dspOpens;
uint32_t destination, source;

void Receipt(std::string sha = digest, std::string device = "/dev/spidev1.1")
{
    std::ofstream out(fixture + "/receipt");
    out << "{\"manifest\":\"" << manifestPath << "\",\"bitstream_sha256\":\"" << sha
        << "\",\"spi_device\":\"" << device << "\"}";
    assert(chmod((fixture + "/receipt").c_str(), 0600) == 0);
}

void Manifest(uint32_t abi = 65536, unsigned stride = 4, unsigned gpio = 59)
{
    std::ofstream out(fixture + "/manifest");
    out << R"({"schema_version":1,"target":"wing-compact-t55f484","build_id":305419896,
        "feature_flags":4,"num_inputs":324,"num_outputs":324,"num_aes50_ports":0,"audio_data_width":32,
        "transport":{"protocol":"litex-spibone","wires":4,"spi_mode":0,"max_hz":2000000,
        "bit_order":"msb-first","byte_order":"big-endian","address_unit":"byte","error_response":"no-marker",
        "cs_active_low":true,"control_cs":{"ball":"V6","gpio":)" << gpio << R"(}},
        "channels":{"sources":[],"destinations":[]},"abi_version":)" << abi
        << R"(,"bitstream":{"file":"wing.hex.bin","sha256":")" << digest << R"("},"registers":[)";
    const std::array names{"MAGIC", "ABI_VERSION", "BUILD_ID", "FEATURE_FLAGS", "NUM_INPUTS",
                           "NUM_OUTPUTS", "NUM_AES50_PORTS", "AUDIO_DATA_WIDTH", "STATUS"};
    for (size_t i = 0; i < names.size(); ++i) {
        // Deliberately use different offsets to catch a host that ignores the generated map.
        out << "{\"name\":\"" << names[i] << "\",\"offset\":" << 64 + 4 * i
            << R"(,"width":32,"access":"RO","reset":0},)";
    }
    out << R"({"name":"CONFIG_FLAGS","offset":516,"width":32,"access":"RW","write_mask":255},
        {"name":"ROUTE","offset":8192,"width":32,"access":"RW","selector_width":9,"count":324,"stride":)"
        << stride << "}]}";
}

void ProductionManifest(bool wdsp = false, unsigned selectorWidth = 10, unsigned tdmSlots = 16)
{
    std::ofstream out(fixture + "/manifest");
    out << R"({"schema_version":2,"target":"wing-compact-t55f484-fork","device":"T55F484",
      "abi_version":131072,"magic":1464421959,"build_id":19088743,"feature_flags":15,
      "num_inputs":768,"num_outputs":768,"num_aes50_ports":1,"audio_data_width":32,
      "audio":{"audio_data_width":32,"sample_rate_hz":48000,"bclk_hz":24576000,
        "word_format":"signed Q4.28","slots":)" << tdmSlots << R"(},
      "transport":{"device":"/dev/spibone0","compatible":"openmixer,wing-spibone","chip_select":1,
        "mode":0,"max_hz":2000000},"routing":{"base":2147483648,"count":768,"stride":4,"off":0,
      "selector_width":)" << selectorWidth << "},\"source_sha256\":\"" << digest << "\",";
    out << "\"dsp_board_profile_sha256\":\"" << digest << "\","
        << "\"bitstream\":{\"file\":\"wing-fpga.hex.bin\",\"sha256\":\"" << digest << "\"},\"csr_sha256\":{";
    out << "\"csr.csv\":\"" << digest << "\",\"csr.json\":\"" << digest
        << "\",\"software/include/generated/csr.h\":\"" << digest << "\"},";
    if (wdsp) out << R"("dsp_runtime":{"device":"/dev/spidev1.0","chip_select":0,"abi":1},)";
    out << "\"channels\":{\"sources\":[";
    // This intentionally sparse export ensures OMC does not infer other DSP slots.
    const std::array exported{0, 308, 309, 372, 419};
    for (unsigned shift : {1, 0}) {
        for (unsigned i = 0; i < exported.size(); ++i) {
            if (i) out << ',';
            out << "{\"id\":" << exported[i] + shift;
            if (i == 0) out << R"(,"dsp":1,"slot_id":0)";
            if (i == 3 || i == 4) out << R"(,"aes50":"B","channel":)" << (i == 3 ? 1 : 48);
            out << '}';
        }
        out << (shift ? "],\"destinations\":[" : "]},");
    }
    const std::array names{"wing_magic", "wing_abi_version", "wing_build_id", "wing_capabilities", "wing_status"};
    out << "\"registers\":[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i) out << ',';
        out << "{\"name\":\"" << names[i] << "\",\"offset\":" << identityAddresses[i]
            << R"(,"width":32,"access":"RO"})";
    }
    out << "]}";
}

void ProductionReceipt(std::string image = digest, std::string hash = {}, unsigned mask = 0)
{
    if (hash.empty()) {
        // Calculate independently of OMC's kernel SHA-256 implementation.
        FILE* command = popen(("sha256sum " + fixture + "/manifest").c_str(), "r");
        char hex[65]{};
        assert(command && fscanf(command, "%64[0-9a-f]", hex) == 1 && pclose(command) == 0);
        hash = hex;
    }
    std::ofstream out(fixture + "/receipt");
    out << "{\"manifest\":\"" << manifestPath << "\",\"bitstream_sha256\":\"" << image
        << "\",\"manifest_sha256\":\"" << hash << "\",\"build_id\":19088743,\"chip_select\":1,"
        << "\"spi_device\":\"/dev/spibone0\"";
    if (mask) out << ",\"dsp_runtime_mask\":" << mask;
    out << '}';
    assert(chmod((fixture + "/receipt").c_str(), 0600) == 0);
}

void Fails(const char* reason, auto call)
{
    try {
        call();
    } catch (const std::exception& e) {
        if (std::strstr(e.what(), reason)) {
            std::cout << "Rejected: " << e.what() << '\n';
            return;
        }
        std::cerr << "Expected " << reason << ", got " << e.what() << '\n';
    }
    std::abort();
}
}

extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...)
{
    if (!strcmp(path, "/dev/spibone0")) {
        ++boneOpens;
        return fakeBone;
    }
    if (!strcmp(path, "/dev/spidev1.0")) {
        ++dspOpens;
        return fakeDsp;
    }
    if (!strcmp(path, "/dev/spidev1.1")) {
        ++opens;
        return fakeSpi;
    }
    if (!strcmp(path, receiptPath)) return __real_open((fixture + "/receipt").c_str(), flags);
    if (!strcmp(path, manifestPath)) return __real_open((fixture + "/manifest").c_str(), flags);
    if (!strcmp(path, lockPath)) return __real_open((fixture + "/lock").c_str(), flags);
    std::abort(); // Unexpected device/file access by the transport.
}

extern "C" int __real_lstat(const char*, struct stat*);
extern "C" int __wrap_lstat(const char* path, struct stat* st)
{
    if (!strcmp(path, "/run") || !strcmp(path, "/usr/share/wing-t55")) return __real_lstat(fixture.c_str(), st);
    std::abort();
}

extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd) { return fd == fakeSpi || fd == fakeBone || fd == fakeDsp ? 0 : __real_close(fd); }

extern "C" int __real_ioctl(int, unsigned long, ...);

extern "C" int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    va_start(args, request);
    if (fd == fakeBone) {
        if (request == SPIBONE_IOC_GET_STATE) {
            *va_arg(args, __u32*) = fault == Fault::Offline ? SPIBONE_STATE_CONFIG : SPIBONE_STATE_ONLINE;
            va_end(args);
            return 0;
        }
        assert(request == SPIBONE_IOC_PEEK || request == SPIBONE_IOC_POKE);
        auto& word = *va_arg(args, spibone_wb_xfer*);
        va_end(args);
        if (request == SPIBONE_IOC_POKE) {
            assert(word.addr >= routeBase && word.addr < routeBase + 768 * 4 && word.addr % 4 == 0);
            assert(word.val <= 768);
            ++pokes;
            if (fault == Fault::IoError) { errno = EIO; return -1; }
            destination = (word.addr - routeBase) / 4;
            source = word.val;
        } else {
            ++peeks;
            if (word.addr >= routeBase) {
                const auto id = (word.addr - routeBase) / 4;
                assert(id == destination || id == 0 || id == 372 || id == 419);
                word.val = id == destination ? (fault == Fault::Readback ? source ^ 1 : source) : id == 419 ? 373 : 325;
            } else {
                constexpr std::array faults{Fault::Magic, Fault::Abi, Fault::BuildId, Fault::Capabilities, Fault::NotReady};
                const auto i = std::find(identityAddresses.begin(), identityAddresses.end(), word.addr) - identityAddresses.begin();
                assert(i < identityWords.size());
                word.val = identityWords[i] ^ unsigned(fault == faults[i]);
            }
        }
        return 0;
    }
    assert(fd == fakeSpi);
    if (request == SPI_IOC_WR_MODE32) {
        assert(*va_arg(args, uint32_t*) == SPI_MODE_0);
    } else if (request == SPI_IOC_WR_BITS_PER_WORD) {
        assert(*va_arg(args, uint8_t*) == 8);
    } else if (request == SPI_IOC_WR_MAX_SPEED_HZ) {
        assert(*va_arg(args, uint32_t*) == 2000000);
    } else {
        assert(request == SPI_IOC_MESSAGE(1));
        const auto& transfer = *va_arg(args, spi_ioc_transfer*);
        va_end(args);
        ++recoveryFrames;
        assert(transfer.len == 73 && transfer.cs_change == 0 && transfer.bits_per_word == 8 && transfer.speed_hz == 2000000);
        const auto* tx = reinterpret_cast<const uint8_t*>(transfer.tx_buf);
        auto* rx = reinterpret_cast<uint8_t*>(transfer.rx_buf);
        assert(tx[0] == 0 || tx[0] == 1);
        const bool read = tx[0] == 1;
        const uint32_t address = uint32_t(tx[1]) * 16777216 + uint32_t(tx[2]) * 65536 + tx[3] * 256 + tx[4];
        const unsigned header = read ? 5 : 9;
        for (unsigned i = header; i < transfer.len; ++i) assert(tx[i] == 0xff);
        std::memset(rx, 0xff, transfer.len);
        if (fault == Fault::NoAck) return transfer.len;
        if (fault == Fault::IoError) { errno = EIO; return -1; }
        if (fault == Fault::Short) return transfer.len - 1;
        // Test the last permitted ACK for both reads and writes.
        rx[header + 63] = fault == Fault::BadMarker ? 0xe1 : tx[0];
        uint32_t* reg = address == 516 ? &config : address == 0x250c ? &routed : nullptr;
        if (!read) {
            assert(reg);
            const std::array<uint8_t, 4> routeBytes{0, 0, 1, 0x44};
            const std::array<uint8_t, 4> cfgBytes{0, 0, 0, 0xa5};
            assert(!memcmp(tx + 5, (reg == &routed ? routeBytes : cfgBytes).data(), 4));
            ++writes;
            if (fault != Fault::Readback) *reg = reg == &routed ? 324 : 0xa5;
        } else {
            const std::array identity{fault == Fault::Magic ? 0u : 0x4f4d4650u,
                fault == Fault::Abi ? 65537u : 65536u, fault == Fault::BuildId ? 0x12345679u : 0x12345678u,
                4u, fault == Fault::Count ? 323u : 324u, 324u, 0u, 32u, fault == Fault::NotReady ? 0u : 1u};
            assert(reg || (address >= 64 && address <= 96 && address % 4 == 0));
            const uint32_t value = reg ? *reg : identity[(address - 64) / 4];
            for (unsigned i = 0; i < 4; ++i) rx[header + 64 + i] = value >> (24 - 8 * i);
        }
        return transfer.len;
    }
    va_end(args);
    return 0;
}

int main()
{
    char dir[] = "/tmp/omc-t55-test.XXXXXX";
    assert(mkdtemp(dir));
    fixture = dir;
    const auto check = [] { return OMC::WingT55Control(receiptPath, true); };
    const auto route = [&] { check().SetRoute(323, 324); };
    Fails("load receipt", [] { OMC::WingT55Control wrong("/tmp/stale-receipt"); });
    Fails("No such file", check);
    Manifest();
    Receipt(digest, "/dev/spidev1.0");
    Fails("invalid T55 load receipt", check);
    Receipt(std::string(64, 'f'));
    Fails("does not match", check);
    Receipt();
    assert(chmod((fixture + "/receipt").c_str(), 0644) == 0);
    Fails("untrusted", check);
    Receipt();
    Manifest(131072);
    Fails("unsupported", check);
    Manifest(65536, 4, 58);
    Fails("manifest SPIBone transport", check);
    Manifest(65536, 8);
    Fails("route range", check);
    assert(opens == 0 && recoveryFrames == 0 && writes == 0);

    Manifest();
    for (auto [mode, reason] : {std::pair{Fault::Magic, "MAGIC"}, {Fault::Abi, "ABI_VERSION"},
             {Fault::BuildId, "BUILD_ID"}, {Fault::Count, "NUM_INPUTS"}, {Fault::NotReady, "not ready"},
             {Fault::NoAck, "ACK timeout"}, {Fault::BadMarker, "invalid T55 SPIBone response"},
             {Fault::Short, "short"}, {Fault::IoError, "Input/output error"}}) {
        fault = mode;
        Fails(reason, route);
        assert(writes == 0);
    }
    fault = Fault::None;
    auto control = check();
    assert(control.BuildId() == 0x12345678 && control.NumInputs() == 324 && control.NumOutputs() == 324);
    const auto before = recoveryFrames;
    Fails("out of range", [&] { control.SetRoute(324, 0); });
    Fails("out of range", [&] { control.SetRoute(0, 325); });
    Fails("8-bit", [&] { control.SetConfig(256); });
    assert(recoveryFrames == before && writes == 0);
    fault = Fault::NoAck;
    Fails("ACK timeout", [&] { control.SetRoute(323, 324); });
    Fails("writes disabled", [&] { control.SetRoute(323, 324); });
    assert(recoveryFrames == before + 1); // Never retry a write with an uncertain outcome.
    fault = Fault::None;
    auto readbackControl = check();
    fault = Fault::Readback;
    Fails("readback mismatch", [&] { readbackControl.SetRoute(323, 324); });
    assert(writes == 1 && routed == 0);
    fault = Fault::None;
    route();
    check().SetConfig(0xa5);
    assert(writes == 3 && routed == 324 && config == 0xa5);

    const auto production = [] { return OMC::WingT55Control(receiptPath); };
    const unsigned recoveryOpens = opens;
    Fails("boot lock", production);
    auto lock = __real_open((fixture + "/lock").c_str(), O_CREAT | O_RDWR, 0600);
    assert(lock >= 0 && flock(lock, LOCK_EX | LOCK_NB) == 0);
    Fails("loader active", production);
    assert(flock(lock, LOCK_UN) == 0);
    Fails("invalid WING SPIBone load receipt", production); // Recovery receipt not a fallback.
    ProductionManifest();
    ProductionReceipt(digest, std::string(64, 'f'));
    Fails("manifest SHA-256 mismatch", production);
    ProductionReceipt(std::string(64, 'f'));
    Fails("loaded image mismatch", production);
    ProductionReceipt(digest, {}, 8);
    Fails("WDSP qualification", production);
    ProductionManifest(false, 10, 32);
    ProductionReceipt();
    Fails("unsupported WING manifest", production); // Withdrawn TDM32 build despite identical ABI number.
    ProductionManifest(false, 9);
    ProductionReceipt();
    Fails("routing geometry", production);
    assert(boneOpens == 0 && peeks == 0 && pokes == 0 && dspOpens == 0 && opens == recoveryOpens);

    ProductionManifest();
    ProductionReceipt();
    fault = Fault::Offline;
    Fails("not online", production);
    assert(peeks == 0 && pokes == 0);
    for (auto [mode, reason] : {std::pair{Fault::Magic, "wing_magic"}, {Fault::Abi, "wing_abi_version"},
             {Fault::BuildId, "wing_build_id"}, {Fault::Capabilities, "wing_capabilities"}, {Fault::NotReady, "not ready"}}) {
        fault = mode;
        Fails(reason, production);
        assert(pokes == 0 && dspOpens == 0 && opens == recoveryOpens);
    }
    fault = Fault::None;
    {
        auto loaded = production();
        assert(flock(lock, LOCK_EX | LOCK_NB) < 0 && errno == EWOULDBLOCK);
        assert(loaded.NumInputs() == 768 && loaded.NumOutputs() == 768);
        loaded.SetRoute(419, 373); // B48 <- B1; both indexes would truncate to 8 bits.
        assert(destination == 419 && source == 373 && pokes == 1);
        loaded.SetRoute(372, 0); // OFF
        assert(destination == 372 && source == 0 && pokes == 2);
        const auto view = loaded.RoutingView();
        assert(view.find(R"("sources":[[1,1,1],[373,5,1],[420,5,48]])") != std::string::npos);
        assert(view.find(R"("selectors":[null,0,373])") != std::string::npos);
        fault = Fault::Offline;
        assert(loaded.RoutingView().find(R"("status":"unavailable")") != std::string::npos);
        Fails("not online", [&] { loaded.SetRoute(308, 1); });
        fault = Fault::BuildId;
        Fails("identity or readiness lost", [&] { loaded.SetRoute(308, 1); });
        fault = Fault::None;
        const auto ioBefore = peeks;
        for (unsigned unused : {260, 307, 324, 420, 468, 767, 768})
            Fails("unavailable", [&] { loaded.SetRoute(unused, 0); });
        for (unsigned unused : {261, 308, 325, 421, 469, 768, 769})
            Fails("unavailable", [&] { loaded.SetRoute(308, unused); });
        Fails("only to T55 Recovery", [&] { loaded.SetConfig(1); });
        assert(pokes == 2 && peeks == ioBefore && dspOpens == 0);
        fault = Fault::Readback;
        Fails("readback mismatch", [&] { loaded.SetRoute(308, 1); });
        fault = Fault::None;
        Fails("writes disabled", [&] { loaded.SetRoute(308, 1); });
        assert(pokes == 3);
    }
    assert(flock(lock, LOCK_EX | LOCK_NB) == 0 && flock(lock, LOCK_UN) == 0);
    close(lock);
    std::filesystem::remove_all(fixture);
    std::cout << "PASS: Recovery frames, WING receipt/identity, SPIBone ioctls, 10-bit routing and write readback\n";
}
