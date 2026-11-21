// SPDX-License-Identifier: GPL-3.0-only
#include "wing-wdsp.h"

#ifdef TARGET_WING
// SPIBone UAPI imported unchanged from Mala's OpenAudioFPGA (7d2869a).
#include "spibone_uapi.h"
#include <glaze/glaze.hpp>
#include <linux/if_alg.h>
#include <linux/magic.h>
#include <linux/spi/spidev.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>

namespace OMC
{
namespace {

constexpr char manifestPath[] = "/usr/share/wing-t55/manifest.json";
constexpr char controlDevice[] = "/dev/spibone0";
constexpr char dspDevice[] = "/dev/spidev1.0";
constexpr uint32_t coreMask = 0x0b; // DSP3/A9 is not qualified for reset.
constexpr uint32_t magic = 0x57494e47, fabricAbi = 0x00020000, capabilities = 0x0f;
constexpr uint32_t dspSpeed = 10000;

struct Fd {
    explicit Fd(int fd) : value(fd) {}
    ~Fd() { if (value >= 0) close(value); }
    Fd(const Fd&) = delete;
    const int value;
};

void Require(bool ok, const char* reason)
{
    if (!ok) throw std::runtime_error(reason);
}

struct Receipt {
    std::string manifest, manifest_sha256, bitstream_sha256, spi_device;
    uint32_t build_id, chip_select, dsp_runtime_mask;
};

struct Manifest {
    uint32_t schema_version, abi_version, magic, build_id, feature_flags;
    std::string target, device;
    struct Bitstream { std::string sha256; } bitstream;
    struct Transport {
        std::string device, compatible;
        uint32_t chip_select, mode, max_hz;
    } transport;
    struct Runtime {
        std::string device;
        uint32_t chip_select, abi;
    } dsp_runtime;
};

void CheckDirectory(const char* path)
{
    struct stat st{};
    Require(lstat(path, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == 0 && !(st.st_mode & 0022),
            "untrusted WING boot or bundle directory");
}

struct stat CheckFile(int fd, bool bootFile)
{
    struct stat st{};
    Require(fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == 0 &&
            (bootFile ? st.st_gid == 0 && (st.st_mode & 07777) == 0600 : !(st.st_mode & 0022)),
            "missing or untrusted WING boot record, lock or manifest");
    return st;
}

std::string ReadFile(const char* path, bool bootFile)
{
    Fd fd(open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    const auto st = CheckFile(fd.value, bootFile);
    Require(st.st_size > 0 && st.st_size <= 1024 * 1024, "invalid WING JSON file size");
    std::string data(st.st_size, '\0');
    for (size_t offset = 0; offset < data.size();) {
        const auto n = read(fd.value, data.data() + offset, data.size() - offset);
        Require(n > 0, "short WING JSON file read");
        offset += n;
    }
    char extra;
    Require(read(fd.value, &extra, 1) == 0, "WING JSON file changed while reading");
    return data;
}

template<class T>
T Parse(const std::string& data)
{
    constexpr glz::opts opts{.error_on_unknown_keys = false, .error_on_missing_keys = true};
    T result{};
    Require(!glz::read<opts>(result, data), "invalid WING boot record or manifest JSON");
    return result;
}

bool IsDigest(const std::string& value)
{
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

std::string Sha256(const std::string& data)
{
    const sockaddr_alg algorithm{.salg_family = AF_ALG, .salg_type = "hash", .salg_name = "sha256"};
    Fd hash(socket(AF_ALG, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
    Require(hash.value >= 0 && bind(hash.value, reinterpret_cast<const sockaddr*>(&algorithm), sizeof algorithm) == 0,
            "kernel SHA-256 unavailable");
    Fd job(accept4(hash.value, nullptr, nullptr, SOCK_CLOEXEC));
    Require(job.value >= 0, "cannot start SHA-256");
    for (size_t offset = 0; offset < data.size();) {
        const auto n = std::min<size_t>(4096, data.size() - offset);
        Require(send(job.value, data.data() + offset, n, n < data.size() - offset ? MSG_MORE : 0) == ssize_t(n),
                "cannot hash complete WING manifest");
        offset += n;
    }
    std::array<uint8_t, 32> digest;
    Require(read(job.value, digest.data(), digest.size()) == ssize_t(digest.size()), "cannot read SHA-256");
    std::string result;
    constexpr char hex[] = "0123456789abcdef";
    for (auto byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}

uint32_t ReadCsr(int fd, uint32_t address)
{
    spibone_wb_xfer reg{address, 0};
    Require(ioctl(fd, SPIBONE_IOC_PEEK, &reg) == 0, "cannot read WING identity/boot CSR");
    return reg.val;
}

// The T55 ABI fixes these addresses; the manifest cannot redirect a read.
void CheckFabric(int fd, const Receipt& receipt)
{
    uint32_t state = 0;
    Require(ioctl(fd, SPIBONE_IOC_GET_STATE, &state) == 0 && state == SPIBONE_STATE_ONLINE,
            "WING SPIBone not online");
    Require(ReadCsr(fd, 0x1800) == magic && ReadCsr(fd, 0x1804) == fabricAbi &&
            ReadCsr(fd, 0x1808) == receipt.build_id && ReadCsr(fd, 0x180c) == capabilities &&
            (ReadCsr(fd, 0x1810) & 1), "live WING FPGA identity or fabric-ready mismatch");
    Require(ReadCsr(fd, 0x1814) == receipt.dsp_runtime_mask &&
            ReadCsr(fd, 0x1818) == 0 && ReadCsr(fd, 0x181c) == 0,
            "live WING reset mask mismatch or FS/VALID enabled");
}

void PutWord(uint8_t* out, uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = word >> (24 - 8 * i);
}

uint32_t ReadWord(const uint8_t* in)
{
    return uint32_t(in[0]) << 24 | uint32_t(in[1]) << 16 | uint32_t(in[2]) << 8 | in[3];
}

uint32_t Crc32(const uint8_t* data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}

enum class Operation : uint8_t { Identify = 0, Bypass = 1, Set = 2 };

class Session
{
public:
    explicit Session(unsigned target) : core(target) {}

    void Run(Operation op = Operation::Identify, const std::array<float, 6>* values = nullptr) const
    {
        Require(core >= 1 && core <= 4 && (coreMask & (1u << (core - 1))), "unsupported WING DSP core");
        CheckDirectory("/run");
        struct statfs fs{};
        Require(statfs("/run", &fs) == 0 && fs.f_type == TMPFS_MAGIC, "WING requires a boot-local /run tmpfs");
        // W2 takes the exclusive lock before replacing the receipt or resetting a core.
        Fd lock(open("/run/wing-t55.lock", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
        CheckFile(lock.value, true);
        Require(flock(lock.value, LOCK_SH | LOCK_NB) == 0, "WING loader is active");
        const auto receipt = Parse<Receipt>(ReadFile("/run/omc-wing-fpga.json", true));
        Require(receipt.manifest == manifestPath && receipt.spi_device == controlDevice && receipt.chip_select == 1 &&
                IsDigest(receipt.manifest_sha256) && IsDigest(receipt.bitstream_sha256) &&
                !(receipt.dsp_runtime_mask & ~coreMask), "invalid WING boot record");
        Require(receipt.dsp_runtime_mask & (1u << (core - 1)), "WING DSP is not boot-qualified");

        CheckDirectory("/usr/share/wing-t55");
        const auto bytes = ReadFile(manifestPath, false);
        Require(Sha256(bytes) == receipt.manifest_sha256, "WING manifest digest mismatch");
        const auto m = Parse<Manifest>(bytes);
        Require(m.schema_version == 2 && m.abi_version == fabricAbi && m.magic == magic &&
                m.feature_flags == capabilities && m.target == "wing-compact-t55f484-fork" && m.device == "T55F484" &&
                m.build_id == receipt.build_id && m.bitstream.sha256 == receipt.bitstream_sha256 &&
                m.transport.device == controlDevice && m.transport.compatible == "openmixer,wing-spibone" &&
                m.transport.chip_select == 1 && m.transport.mode == 0 && m.transport.max_hz == 2000000 &&
                m.dsp_runtime.device == dspDevice && m.dsp_runtime.chip_select == 0 && m.dsp_runtime.abi == 1,
                "incompatible WING manifest or boot record");

        Fd control(open(controlDevice, O_RDWR | O_NOFOLLOW | O_CLOEXEC));
        CheckFabric(control.value, receipt);
        Fd dsp(open(dspDevice, O_RDWR | O_NOFOLLOW | O_CLOEXEC));
        uint32_t mode = SPI_MODE_0, speed = dspSpeed;
        uint8_t bits = 8;
        Require(dsp.value >= 0 && ioctl(dsp.value, SPI_IOC_WR_MODE32, &mode) == 0 &&
                ioctl(dsp.value, SPI_IOC_WR_BITS_PER_WORD, &bits) == 0 &&
                ioctl(dsp.value, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == 0, "cannot configure WING SS0");
        Command(dsp.value, Operation::Identify, nullptr);
        if (op != Operation::Identify) {
            // Identify can succeed just before the FPGA goes offline; recheck before writing.
            CheckFabric(control.value, receipt);
            Command(dsp.value, op, values);
        }
    }

private:
    void Command(int fd, Operation op, const std::array<float, 6>* values) const
    {
        uint32_t nonce;
        Require(getrandom(&nonce, sizeof nonce, GRND_NONBLOCK) == sizeof nonce, "cannot obtain WDSP nonce");

        constexpr size_t firstReply = 1 + 40, replyBytes = 24, pollBytes = 8;
        std::array<uint8_t, firstReply + pollBytes + replyBytes> tx{}, rx{};
        tx[0] = core; // FPGA consumes selector; WDSP starts at the next byte.
        auto* frame = tx.data() + 1;
        PutWord(frame, 0x57445350); // WDSP
        PutWord(frame + 4, nonce);
        frame[8] = static_cast<uint8_t>(op);
        frame[9] = op == Operation::Identify ? 0 : 5; // FX class; index 0 is the first stereo pair.
        frame[11] = core;
        static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4);
        if (values) {
            for (size_t i = 0; i < values->size(); ++i)
                PutWord(frame + 12 + 4 * i, std::bit_cast<uint32_t>((*values)[i]));
        }
        const auto crc = Crc32(frame, 36);
        PutWord(frame + 36, crc);

        // Allow the slave's poll loop to observe SS0 deasserted between frames.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        spi_ioc_transfer transfer{};
        transfer.tx_buf = reinterpret_cast<uintptr_t>(tx.data());
        transfer.rx_buf = reinterpret_cast<uintptr_t>(rx.data());
        transfer.len = tx.size();
        transfer.speed_hz = dspSpeed;
        transfer.bits_per_word = 8;
        Require(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) == int(tx.size()),
                op == Operation::Identify ? "failed or short WING SS0 Identify" :
                "failed or short WING SS0 update: outcome unknown, no retry");

        for (size_t i = firstReply; i + replyBytes <= rx.size(); ++i) {
            const auto* reply = rx.data() + i;
            if (ReadWord(reply) != 0x57445352 || ReadWord(reply + 4) != 1 || ReadWord(reply + 8) != nonce ||
                reply[12] != core || ReadWord(reply + 16) != crc || ReadWord(reply + 20) != Crc32(reply, 20)) continue;
            Require(reply[13] == 0 && reply[14] == 1 && reply[15] == 0, "WDSP rejected command or SPORT not armed");
            return;
        }
        throw std::runtime_error(op == Operation::Identify ? "no matching WDSP v1 Identify reply" :
                                 "no matching WDSP v1 update ACK: outcome unknown, no retry");
    }

    const unsigned core;
};

} // namespace

void IdentifyWingDsp(unsigned core)
{
    Session(core).Run();
}

void SetWingOverdrive(const std::array<float, 6>* values)
{
    // Reject out-of-range and non-finite values before opening SS0.
    constexpr std::array low{1.f, -1.f, -1.f, 0.f, 0.f, 0.f};
    constexpr std::array high{16.f, 0.999999f, 1.f, 1.f, 1.f, 1.f};
    if (values) {
        for (size_t i = 0; i < values->size(); ++i)
            Require((*values)[i] >= low[i] && (*values)[i] <= high[i], "overdrive value outside WDSP range");
    }
    Session(4).Run(values ? Operation::Set : Operation::Bypass, values);
}

} // namespace OMC
#endif
