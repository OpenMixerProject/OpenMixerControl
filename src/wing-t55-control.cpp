#include "wing-t55-control.h"

#ifdef TARGET_WING
#include "spibone_uapi.h"
#include <glaze/glaze.hpp>
#include <linux/if_alg.h>
#include <linux/magic.h>
#include <linux/spi/spidev.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace OMC {
namespace {

constexpr char receiptPath[] = "/run/omc-wing-fpga.json";
constexpr char bundleDir[] = "/usr/share/wing-t55";
constexpr char manifestPath[] = "/usr/share/wing-t55/manifest.json";
constexpr char spiDevice[] = "/dev/spidev1.1"; // ECSPI2 SS1, GPIO2_27 -> T55 V6
constexpr char dspDevice[] = "/dev/spidev1.0"; // ECSPI2 SS0, GPIO2_26 -> T55 J2
constexpr char spiboneDevice[] = "/dev/spibone0";
constexpr char bootLock[] = "/run/wing-t55.lock";
constexpr uint32_t spiSpeed = 2000000;

struct Receipt {
    std::string manifest, bitstream_sha256, spi_device;
    std::optional<std::string> manifest_sha256;
    std::optional<uint32_t> build_id, chip_select, dsp_runtime_mask;
};

struct Register {
    std::string name, access;
    uint32_t offset, width;
    std::optional<uint32_t> count, stride, selector_width, write_mask;
};

struct Manifest {
    uint32_t schema_version, abi_version, build_id, feature_flags;
    uint32_t num_inputs, num_outputs, num_aes50_ports, audio_data_width;
    std::string target;
    struct Bitstream {
        std::string file, sha256;
    } bitstream;
    std::vector<Register> registers;
    struct Transport {
        std::string protocol, bit_order, byte_order, address_unit, error_response;
        uint32_t wires, spi_mode, max_hz;
        bool cs_active_low;
        struct Select {
            std::string ball;
            uint32_t gpio;
        } control_cs;
        std::optional<Select> dsp_cs;
    } transport;
};

struct ProductionManifest {
    uint32_t schema_version, abi_version, magic, build_id, feature_flags;
    uint32_t num_inputs, num_outputs, num_aes50_ports, audio_data_width;
    std::string target, device, source_sha256, dsp_board_profile_sha256;
    struct Audio {
        uint32_t slots, audio_data_width, sample_rate_hz, bclk_hz;
        std::string word_format;
    } audio;
    Manifest::Bitstream bitstream;
    std::map<std::string, std::string> csr_sha256;
    std::vector<Register> registers;
    struct Channels {
        struct Endpoint {
            uint32_t id;
            std::optional<uint32_t> dsp, slot_id, channel;
            std::optional<std::string> aes50;
        };
        std::vector<Endpoint> sources, destinations;
    } channels;
    struct Routing {
        uint32_t base, count, stride, selector_width, off;
    } routing;
    struct Transport {
        std::string device, compatible;
        uint32_t chip_select, mode, max_hz;
    } transport;
    struct DspRuntime {
        std::string device;
        uint32_t chip_select, abi;
    };
    std::optional<DspRuntime> dsp_runtime;
};

std::string RouteCatalog(const ProductionManifest& manifest,
                         std::vector<uint32_t>& sources, std::vector<uint32_t>& destinations)
{
    std::string json = "{\"build\":" + std::to_string(manifest.build_id);
    const auto append = [&](const char* key, const auto& endpoints, auto& ids, bool source) {
        json += key;
        std::bitset<WingT55Control::ProductionRoutes + 1> used;
        for (const auto& e : endpoints) {
            unsigned group, channel;
            if (e.dsp && *e.dsp >= 1 && *e.dsp <= 4 && !e.aes50 && e.slot_id && *e.slot_id < 768) {
                group = *e.dsp;
                channel = *e.slot_id + 1;
            } else if (!e.dsp && e.aes50 == "B" && e.channel && *e.channel >= 1 && *e.channel <= 48) {
                group = 5;
                channel = *e.channel;
            } else continue; // No local I/O, reserved endpoints or A/C.
            if ((source && e.id == 0) || e.id >= 768 + unsigned(source) || used.test(e.id)) continue;
            used.set(e.id);
            if (!ids.empty()) json += ',';
            ids.push_back(e.id);
            json += '[' + std::to_string(e.id) + ',' + std::to_string(group) + ',' + std::to_string(channel) + ']';
        }
        json += ']';
    };
    append(",\"sources\":[", manifest.channels.sources, sources, true);
    append(",\"destinations\":[", manifest.channels.destinations, destinations, false);
    return json;
}

struct ScopedFd {
    const int fd;
    ~ScopedFd() { if (fd >= 0) close(fd); }
};

bool IsDigest(const std::string& hash)
{
    return hash.size() == 64 && hash.find_first_not_of("0123456789abcdef") == std::string::npos;
}

std::string Sha256(const std::string& bytes)
{
    // Same kernel SHA-256 provider as wing-t55, hashing the very bytes we parse.
    const sockaddr_alg algorithm{.salg_family = AF_ALG, .salg_type = "hash", .salg_name = "sha256"};
    const ScopedFd hash{socket(AF_ALG, SOCK_SEQPACKET | SOCK_CLOEXEC, 0)};
    if (hash.fd < 0 || bind(hash.fd, reinterpret_cast<const sockaddr*>(&algorithm), sizeof algorithm) < 0)
        throw std::runtime_error("kernel SHA-256 unavailable");
    const ScopedFd job{accept4(hash.fd, nullptr, nullptr, SOCK_CLOEXEC)};
    if (job.fd < 0) throw std::runtime_error("cannot start SHA-256");
    for (size_t offset = 0; offset < bytes.size();) {
        const size_t count = std::min<size_t>(4096, bytes.size() - offset);
        if (send(job.fd, bytes.data() + offset, count, count < bytes.size() - offset ? MSG_MORE : 0) != ssize_t(count))
            throw std::runtime_error("cannot hash complete manifest");
        offset += count;
    }
    std::array<uint8_t, 32> digest{};
    if (read(job.fd, digest.data(), digest.size()) != ssize_t(digest.size()))
        throw std::runtime_error("cannot read manifest SHA-256");
    std::string hex(64, '\0');
    constexpr char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < digest.size(); ++i) {
        hex[2*i] = digits[digest[i] >> 4];
        hex[2*i + 1] = digits[digest[i] & 15];
    }
    return hex;
}

void CheckDirectory(const char* path)
{
    struct stat st{};
    if (lstat(path, &st) < 0 || !S_ISDIR(st.st_mode) || st.st_uid || (st.st_mode & 0022)) {
        throw std::runtime_error(std::string("untrusted or missing directory: ") + path);
    }
}

template<class T>
T LoadJson(const char* path, bool receipt, const std::string& digest = {})
{
    const int file = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (file < 0) throw std::system_error(errno, std::generic_category(), path);
    struct stat st{};
    if (fstat(file, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid ||
        (receipt ? st.st_gid || (st.st_mode & 07777) != 0600 : (st.st_mode & 0022) != 0) ||
        st.st_size <= 0 || st.st_size > 1024 * 1024) {
        close(file);
        throw std::runtime_error(std::string("untrusted or invalid file: ") + path);
    }
    std::string json(st.st_size, '\0');
    size_t used = 0;
    while (used < json.size()) {
        const auto n = read(file, json.data() + used, json.size() - used);
        if (n <= 0) break;
        used += n;
    }
    close(file);
    if (used != json.size()) throw std::runtime_error(std::string("short read: ") + path);
    if (!digest.empty() && Sha256(json) != digest) throw std::runtime_error("WING manifest SHA-256 mismatch");
    T result{};
    constexpr glz::opts options{.error_on_unknown_keys = false, .error_on_missing_keys = true};
    const auto error = glz::read<options>(result, json);
    if (error) {
        throw std::runtime_error(std::string("invalid JSON in ") + path + ": " + glz::format_error(error, json));
    }
    return result;
}

const Register& FindRegister(const std::vector<Register>& registers, const char* name, const char* access)
{
    const Register* match = nullptr;
    for (const auto& reg : registers) {
        if (reg.name != name) continue;
        if (match || reg.access != access || reg.width != 32 || reg.offset % 4) {
            throw std::runtime_error(std::string("invalid manifest register: ") + name);
        }
        match = &reg;
    }
    if (!match) throw std::runtime_error(std::string("missing manifest register: ") + name);
    return *match;
}

void PutBE(uint8_t* out, uint32_t word)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = word >> (24 - 8 * i);
}

int OpenSpi(const char* path, uint32_t speed)
{
    const int fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), path);
    uint32_t mode = SPI_MODE_0; // MSB-first and active-low GPIO CS, owned by spidev
    uint8_t bits = 8;
    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        const int error = errno;
        close(fd);
        throw std::system_error(error, std::generic_category(), path);
    }
    return fd;
}

void Transfer(int fd, const uint8_t* tx, uint8_t* rx, uint32_t size, uint32_t speed, const char* name)
{
    // One full-duplex transfer: CS stays asserted for request, poll, and response.
    spi_ioc_transfer transfer{};
    transfer.tx_buf = reinterpret_cast<uintptr_t>(tx);
    transfer.rx_buf = reinterpret_cast<uintptr_t>(rx);
    transfer.len = size;
    transfer.speed_hz = speed;
    transfer.bits_per_word = 8;
    const int count = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
    if (count < 0) throw std::system_error(errno, std::generic_category(), name);
    if (uint32_t(count) != size) throw std::runtime_error(std::string("short ") + name);
}

} // namespace

WingT55Control::WingT55Control(const std::string& receipt, bool recovery) : recovery(recovery)
{
    if (receipt != receiptPath) throw std::runtime_error("expected /run/omc-wing-fpga.json load receipt");
    CheckDirectory("/run");
    struct statfs fs{};
    if (statfs("/run", &fs) < 0 || fs.f_type != TMPFS_MAGIC)
        throw std::runtime_error("WING requires boot-local /run tmpfs");
    if (!recovery) {
        // Serialize with the loader for the lifetime of the verified session.
        lockFd = open(bootLock, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        struct stat st{};
        try {
            if (lockFd < 0 || fstat(lockFd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid || st.st_gid ||
                (st.st_mode & 07777) != 0600 || flock(lockFd, LOCK_SH | LOCK_NB) < 0)
                throw std::runtime_error("WING boot lock unavailable or loader active");
            const auto loaded = LoadJson<Receipt>(receiptPath, true);
            if (loaded.manifest != manifestPath || loaded.spi_device != spiboneDevice ||
                !loaded.build_id || loaded.chip_select != 1 || !IsDigest(loaded.bitstream_sha256) ||
                !loaded.manifest_sha256 || !IsDigest(*loaded.manifest_sha256))
                throw std::runtime_error("invalid WING SPIBone load receipt");
            OpenProduction(*loaded.manifest_sha256, loaded.bitstream_sha256, *loaded.build_id,
                           loaded.dsp_runtime_mask.value_or(0));
        } catch (...) {
            if (fd >= 0) close(fd);
            if (lockFd >= 0) close(lockFd);
            throw;
        }
        return;
    }
    const auto loaded = LoadJson<Receipt>(receiptPath, true);
    if (loaded.manifest != manifestPath || loaded.spi_device != spiDevice ||
        !IsDigest(loaded.bitstream_sha256)) {
        throw std::runtime_error("invalid T55 load receipt (bundle, SS1 device or digest)");
    }
    CheckDirectory(bundleDir);
    const auto manifest = LoadJson<Manifest>(manifestPath, false);
    if (manifest.schema_version != 1 || manifest.target != "wing-compact-t55f484" ||
        manifest.bitstream.file.empty() || manifest.bitstream.sha256 != loaded.bitstream_sha256) {
        throw std::runtime_error("T55 manifest does not match the loaded image");
    }
    if ((manifest.abi_version >> 16) != 1) throw std::runtime_error("unsupported T55 manifest ABI major");
    const auto& transport = manifest.transport;
    if (transport.protocol != "litex-spibone" || transport.wires != 4 || transport.spi_mode != SPI_MODE_0 ||
        !transport.cs_active_low || transport.control_cs.ball != "V6" || transport.control_cs.gpio != 59 ||
        transport.max_hz < spiSpeed || transport.bit_order != "msb-first" || transport.byte_order != "big-endian" ||
        transport.address_unit != "byte" || transport.error_response != "no-marker") {
        throw std::runtime_error("unsupported T55 manifest SPIBone transport");
    }
    const std::array ro{
        "MAGIC", "ABI_VERSION", "BUILD_ID", "FEATURE_FLAGS", "NUM_INPUTS", "NUM_OUTPUTS",
        "NUM_AES50_PORTS", "AUDIO_DATA_WIDTH", "STATUS"
    };
    std::array<uint32_t, ro.size()> addresses{};
    for (size_t i = 0; i < ro.size(); ++i) addresses[i] = FindRegister(manifest.registers, ro[i], "RO").offset;
    const auto& flags = FindRegister(manifest.registers, "CONFIG_FLAGS", "RW");
    if (flags.write_mask != 0xff) throw std::runtime_error("unsupported T55 CONFIG_FLAGS write mask");
    config = flags.offset;
    const auto& route = FindRegister(manifest.registers, "ROUTE", "RW");
    const auto selectorWidth = route.selector_width.value_or(0);
    if (!manifest.num_outputs || route.count != manifest.num_outputs || route.stride != 4 ||
        !selectorWidth || selectorWidth > 32 || !manifest.num_inputs ||
        manifest.num_inputs >= (uint64_t{1} << selectorWidth)) {
        throw std::runtime_error("invalid T55 route range or selector width");
    }
    const uint64_t routeEnd = uint64_t(route.offset) + uint64_t(manifest.num_outputs) * *route.stride;
    if (routeEnd > (uint64_t{1} << 32)) throw std::runtime_error("T55 route address overflow");
    auto inRoute = [&](uint32_t addr) { return addr >= route.offset && addr < routeEnd; };
    if (inRoute(config)) throw std::runtime_error("T55 route/config register overlap");
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (addresses[i] == config || inRoute(addresses[i]) ||
            std::find(addresses.begin(), addresses.begin() + i, addresses[i]) != addresses.begin() + i) {
            throw std::runtime_error("T55 identity register overlap");
        }
    }

    fd = OpenSpi(spiDevice, spiSpeed);
    try {
        const std::array expected{0x4f4d4650u, manifest.abi_version, manifest.build_id,
            manifest.feature_flags, manifest.num_inputs, manifest.num_outputs,
            manifest.num_aes50_ports, manifest.audio_data_width};
        for (size_t i = 0; i < expected.size(); ++i) {
            if (Exchange(true, addresses[i]) != expected[i]) {
                throw std::runtime_error(std::string("T55 FPGA/manifest mismatch: ") + ro[i]);
            }
        }
        if (!(Exchange(true, addresses.back()) & 1)) throw std::runtime_error("T55 FPGA fabric not ready");
    } catch (...) {
        close(fd);
        throw;
    }
    buildId = manifest.build_id;
    inputs = manifest.num_inputs;
    outputs = manifest.num_outputs;
    routeBase = route.offset;
    routeStride = *route.stride;
}

void WingT55Control::OpenProduction(const std::string& manifestDigest, const std::string& imageDigest,
                                    uint32_t expectedBuild, unsigned mask)
{
    CheckDirectory(bundleDir);
    const auto manifest = LoadJson<ProductionManifest>(manifestPath, false, manifestDigest);
    constexpr uint32_t magic = 0x57494e47, abi = 0x00020000;
    constexpr uint32_t required = 0x0d; // SPIBone, DSP Q4.28 and physical AES50 B
    if (manifest.schema_version != 2 || manifest.target != "wing-compact-t55f484-fork" ||
        manifest.device != "T55F484" || manifest.magic != magic || manifest.abi_version != abi ||
        manifest.bitstream.file.empty() || manifest.bitstream.sha256 != imageDigest ||
        manifest.build_id != expectedBuild || !IsDigest(manifest.source_sha256) ||
        std::stoul(manifest.source_sha256.substr(0, 8), nullptr, 16) != expectedBuild ||
        (manifest.feature_flags & required) != required ||
        manifest.num_aes50_ports != 1 || manifest.audio_data_width != 32 ||
        manifest.audio.slots != 16 || manifest.audio.audio_data_width != 32 ||
        manifest.audio.sample_rate_hz != 48000 || manifest.audio.bclk_hz != 24576000 ||
        manifest.audio.word_format != "signed Q4.28" ||
        !IsDigest(manifest.dsp_board_profile_sha256))
        throw std::runtime_error("unsupported WING manifest or loaded image mismatch");
    for (const auto* file : {"csr.csv", "csr.json", "software/include/generated/csr.h"}) {
        const auto entry = manifest.csr_sha256.find(file);
        if (entry == manifest.csr_sha256.end() || !IsDigest(entry->second))
            throw std::runtime_error("missing WING generated CSR digest");
    }
    const auto& transport = manifest.transport;
    if (transport.device != spiboneDevice || transport.compatible != "openmixer,wing-spibone" ||
        transport.chip_select != 1 || transport.mode != 0 || transport.max_hz != spiSpeed)
        throw std::runtime_error("invalid WING SPIBone manifest transport");
    if ((mask & ~0x0bu) || (mask && (!(manifest.feature_flags & 2) || !manifest.dsp_runtime ||
        manifest.dsp_runtime->device != dspDevice || manifest.dsp_runtime->chip_select != 0 ||
        manifest.dsp_runtime->abi != 1)))
        throw std::runtime_error("invalid WING WDSP qualification");

    const auto& route = manifest.routing;
    const uint64_t end = uint64_t(route.base) + uint64_t(route.count) * route.stride;
    if (manifest.num_inputs != ProductionRoutes || manifest.num_outputs != ProductionRoutes ||
        route.count != manifest.num_outputs || route.stride != 4 || route.selector_width != 10 ||
        route.off != 0 || route.base % 4 || end > (uint64_t{1} << 32))
        throw std::runtime_error("invalid WING routing geometry");
    // Capacity is not availability. The generated image map omits A/C and reserved slots.
    reachableSources.set(0); // OFF
    for (const auto& [id, dsp, slot, channel, aes50] : manifest.channels.sources) {
        if (id > manifest.num_inputs || reachableSources.test(id))
            throw std::runtime_error("invalid WING source map");
        reachableSources.set(id);
    }
    for (const auto& [id, dsp, slot, channel, aes50] : manifest.channels.destinations) {
        if (id >= manifest.num_outputs || reachableDestinations.test(id))
            throw std::runtime_error("invalid WING destination map");
        reachableDestinations.set(id);
    }
    if (manifest.channels.sources.empty() || manifest.channels.destinations.empty())
        throw std::runtime_error("missing WING route endpoints");
    const std::array names{"wing_magic", "wing_abi_version", "wing_build_id", "wing_capabilities", "wing_status"};
    const std::array expected{magic, abi, expectedBuild, manifest.feature_flags};
    std::array<uint32_t, names.size()> addresses{};
    for (size_t i = 0; i < names.size(); ++i) {
        const auto address = FindRegister(manifest.registers, names[i], "RO").offset;
        if ((address >= route.base && address < end) ||
            std::find(addresses.begin(), addresses.begin() + i, address) != addresses.begin() + i)
            throw std::runtime_error("overlapping WING register addresses");
        addresses[i] = address;
    }
    fd = open(spiboneDevice, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), spiboneDevice);
    RequireOnline();
    for (size_t i = 0; i < expected.size(); ++i) {
        if (Exchange(true, addresses[i]) != expected[i])
            throw std::runtime_error(std::string("WING FPGA/manifest mismatch: ") + names[i]);
    }
    if (!(Exchange(true, addresses.back()) & 1)) throw std::runtime_error("WING routing not ready");
    buildId = expectedBuild;
    inputs = manifest.num_inputs;
    outputs = manifest.num_outputs;
    routeBase = route.base;
    routeStride = route.stride;
    buildAddress = addresses[2];
    statusAddress = addresses.back();
    routeCatalog = RouteCatalog(manifest, viewSources, viewDestinations);
}

std::string WingT55Control::OfflineRoutingView()
{
    try {
        CheckDirectory(bundleDir);
        const auto manifest = LoadJson<ProductionManifest>(manifestPath, false);
        if (manifest.schema_version == 2 && manifest.target == "wing-compact-t55f484-fork") {
            std::vector<uint32_t> sources, destinations;
            return RouteCatalog(manifest, sources, destinations) + ",\"status\":\"offline\"}";
        }
    } catch (const std::exception&) {} // No catalog installed; do not probe a device.
    return R"({"status":"no_catalog"})";
}

std::string WingT55Control::RoutingView()
{
    if (routeCatalog.empty()) return R"({"status":"no_catalog"})";
    try {
        RequireReady();
        std::string states;
        for (auto id : viewDestinations) {
            const auto source = Exchange(true, routeBase + id * routeStride);
            if (!states.empty()) states += ',';
            states += (source == 0 || std::find(viewSources.begin(), viewSources.end(), source) != viewSources.end())
                ? std::to_string(source) : "null";
        }
        RequireReady();
        return routeCatalog + ",\"status\":\"readback\",\"selectors\":[" + states + "]}";
    } catch (const std::exception&) {
        return routeCatalog + ",\"status\":\"unavailable\"}";
    }
}

void WingT55Control::RequireOnline()
{
    __u32 state = SPIBONE_STATE_OFFLINE;
    if (ioctl(fd, SPIBONE_IOC_GET_STATE, &state) != 0 || state != SPIBONE_STATE_ONLINE)
        throw std::runtime_error("WING SPIBone is not online");
}

void WingT55Control::RequireReady()
{
    RequireOnline();
    if (Exchange(true, buildAddress) != buildId || !(Exchange(true, statusAddress) & 1))
        throw std::runtime_error("WING FPGA identity or readiness lost");
}

WingT55Control::~WingT55Control()
{
    close(fd);
    if (lockFd >= 0) close(lockFd);
}

uint32_t WingT55Control::Exchange(bool read, uint32_t address, uint32_t value)
{
    if (!recovery) {
        // Mala's SPIBone word transfers; loader state and raw ioctls stay out of OMC.
        spibone_wb_xfer word{address, value};
        if (ioctl(fd, read ? SPIBONE_IOC_PEEK : SPIBONE_IOC_POKE, &word) < 0)
            throw std::system_error(errno, std::generic_category(), read ? "SPIBone PEEK" : "SPIBone POKE");
        return word.val;
    }
    constexpr size_t pollBytes = 64; // bounded wire-time budget; no retry on ambiguous writes
    constexpr size_t frameBytes = 5 + pollBytes + 4;
    std::array<uint8_t, frameBytes> tx, rx{};
    tx.fill(0xff);
    const uint8_t command = read ? 1 : 0;
    const size_t header = read ? 5 : 9;
    tx[0] = command;
    PutBE(tx.data() + 1, address);
    if (!read) PutBE(tx.data() + 5, value);

    Transfer(fd, tx.data(), rx.data(), frameBytes, spiSpeed, "T55 SPIBone transfer");

    for (size_t i = header; i < header + pollBytes; ++i) {
        if (rx[i] == 0xff) continue;
        if (rx[i] != command) throw std::runtime_error("invalid T55 SPIBone response marker");
        uint32_t result = 0;
        if (read) for (size_t j = 1; j <= 4; ++j) result = (result << 8) | rx[i + j];
        return result;
    }
    throw std::runtime_error("T55 SPIBone ACK timeout (no response or Wishbone error)");
}

void WingT55Control::WriteChecked(uint32_t address, uint32_t value)
{
    if (writeFailed) throw std::runtime_error("T55 writes disabled after an ambiguous transfer");
    if (!recovery) RequireReady();
    try {
        Exchange(false, address, value);
        if (Exchange(true, address) != value) throw std::runtime_error("T55 FPGA write readback mismatch");
        if (!recovery) RequireReady();
    } catch (...) {
        writeFailed = true;
        throw;
    }
}

void WingT55Control::SetRoute(uint32_t destination, uint32_t source)
{
    if (destination >= outputs || source > inputs ||
        (!recovery && (!reachableDestinations.test(destination) || !reachableSources.test(source))))
        throw std::runtime_error("T55 route ID out of range or unavailable");
    WriteChecked(routeBase + destination * routeStride, source);
}

void WingT55Control::SetConfig(uint32_t flags)
{
    if (!recovery) throw std::runtime_error("CONFIG_FLAGS belongs only to T55 Recovery");
    if (flags > 0xff) throw std::runtime_error("T55 config flags exceed the 8-bit ABI");
    WriteChecked(config, flags);
}

} // namespace OMC
#endif
