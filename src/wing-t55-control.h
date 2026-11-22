#pragma once

#ifdef TARGET_WING
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

namespace OMC {

// Verified routing without FPGA programming, DSP control or audio enable.
class WingT55Control {
public:
    static constexpr uint32_t ProductionRoutes = 768;
    explicit WingT55Control(const std::string& receipt, bool recovery = false);
    ~WingT55Control();
    WingT55Control(const WingT55Control&) = delete;
    WingT55Control& operator=(const WingT55Control&) = delete;

    uint32_t BuildId() const { return buildId; }
    uint32_t NumInputs() const { return inputs; }
    uint32_t NumOutputs() const { return outputs; }
    std::string RoutingView(); // Passive SPIBone readback only.
    static std::string OfflineRoutingView(); // Installed catalog, no device access.
    void SetRoute(uint32_t destination, uint32_t source);
    void SetConfig(uint32_t flags);

private:
    void OpenProduction(const std::string& manifestDigest, const std::string& imageDigest,
                        uint32_t expectedBuild, unsigned mask);
    void RequireOnline();
    void RequireReady();
    uint32_t Exchange(bool read, uint32_t address, uint32_t value = 0);
    void WriteChecked(uint32_t address, uint32_t value);

    int fd = -1, lockFd = -1;
    bool recovery;
    bool writeFailed = false;
    uint32_t buildId = 0, inputs = 0, outputs = 0;
    uint32_t config = 0, routeBase = 0, routeStride = 0;
    uint32_t buildAddress = 0, statusAddress = 0;
    std::string routeCatalog;
    std::vector<uint32_t> viewSources, viewDestinations;
    std::bitset<ProductionRoutes + 1> reachableSources;
    std::bitset<ProductionRoutes> reachableDestinations;
};

} // namespace OMC
#endif
