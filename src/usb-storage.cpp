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

#include "usb-storage.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <limits.h>
#include <linux/fs.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../lib_ext/doctest/doctest/doctest.h"

namespace OMC
{

namespace
{

std::string ReadTrimmedFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return "";
    }
    std::string line;
    std::getline(file, line);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
    {
        line.pop_back();
    }
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
    {
        start++;
    }
    return line.substr(start);
}

std::string ReadWholeFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void WriteLe32(uint8_t* dst, uint32_t value)
{
    dst[0] = static_cast<uint8_t>(value & 0xFFu);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    dst[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    dst[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
}

} // namespace

UsbStorage::UsbStorage(X32BaseParameter* basepar) : X32Base(basepar)
{
}

UsbStorage::~UsbStorage()
{
    if (formatThread.joinable())
    {
        formatThread.join();
    }
}

void UsbStorage::Init()
{
    currentDevice = UsbDeviceInfo{};
    currentDevice.mountPoint = mountPoint;
    if (!state->bodyless)
    {
        RefreshDeviceState();
    }
    else
    {
        PublishStateToConfig();
    }
}

void UsbStorage::Sync()
{
    if (config->HasParameterChanged(USB_STORAGE_FORMAT_COMMAND))
    {
        if (config->GetUint(USB_STORAGE_FORMAT_COMMAND) != 0)
        {
            USB_FORMAT_TARGET target = static_cast<USB_FORMAT_TARGET>(config->GetUint(USB_STORAGE_FORMAT_TARGET));
            StartFormatAsync(target, DEFAULT_VOLUME_LABEL);
            // Reset command trigger back to idle
            config->Set(USB_STORAGE_FORMAT_COMMAND, 0u);
        }
    }
}

void UsbStorage::Tick1000ms()
{
    if (formatStateDirty.exchange(false))
    {
        std::lock_guard<std::mutex> lock(deviceMutex);
        PublishStateToConfig();
        return;
    }

    if (state->bodyless || formatInProgress.load())
    {
        return;
    }

    RefreshDeviceState();
}

bool UsbStorage::IsSameDeviceOrPartition(const std::string& candidate, const std::string& baseDevice)
{
    if (baseDevice.empty() || candidate.size() < baseDevice.size())
    {
        return false;
    }
    if (candidate == baseDevice)
    {
        return true;
    }
    if (candidate.compare(0, baseDevice.size(), baseDevice) != 0)
    {
        return false;
    }
    // Require a trailing partition digit (e.g. /dev/sda1, /dev/sda12) so /dev/sda never matches /dev/sdaa1
    for (size_t i = baseDevice.size(); i < candidate.size(); i++)
    {
        if (!std::isdigit(static_cast<unsigned char>(candidate[i])))
        {
            return false;
        }
    }
    return true;
}

bool UsbStorage::IsSafeRemovableUsbDevice(const std::string& blockName, const std::string& sysBlockRoot)
{
    if (blockName.empty() || blockName.size() > 16)
    {
        return false;
    }

    // Reject path traversal or unexpected characters
    for (char c : blockName)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }

    // Explicitly only allow SCSI disk names ("sd[a-z]+") and never internal SD/flash/virtual devices
    if (blockName.rfind("sd", 0) != 0 || blockName.size() < 3)
    {
        return false;
    }
    for (size_t i = 2; i < blockName.size(); i++)
    {
        if (!std::isalpha(static_cast<unsigned char>(blockName[i])))
        {
            return false;
        }
    }

    const std::string basePath = sysBlockRoot + "/" + blockName;
    if (ReadTrimmedFile(basePath + "/removable") != "1")
    {
        return false;
    }

    // Verify the backing device is on a USB bus
    const std::string devLink = basePath + "/device";
    char* resolved = realpath(devLink.c_str(), nullptr);
    if (resolved == nullptr)
    {
        return false;
    }

    std::string resolvedPath(resolved);
    free(resolved);
    if (resolvedPath.find("/usb") == std::string::npos)
    {
        return false;
    }

    return true;
}

USB_FS_TYPE UsbStorage::DetectFilesystemFromVbr(const uint8_t* sector, size_t length)
{
    if (sector == nullptr || length < 512)
    {
        return USB_FS_TYPE::UNKNOWN;
    }

    // exFAT: JumpBoot (EB 76 90) or FileSystemName "EXFAT   " at offset 3
    if (std::memcmp(sector + 3, "EXFAT   ", 8) == 0)
    {
        return USB_FS_TYPE::EXFAT;
    }

    // FAT boot sector signature 0x55 0xAA at offset 510..511
    if (sector[510] == 0x55 && sector[511] == 0xAA)
    {
        // FAT32 extended BPB string at offset 82
        if (std::memcmp(sector + 82, "FAT32   ", 8) == 0)
        {
            return USB_FS_TYPE::FAT32;
        }

        // FAT12/FAT16 BPB string at offset 54
        if (std::memcmp(sector + 54, "FAT16   ", 8) == 0 ||
            std::memcmp(sector + 54, "FAT12   ", 8) == 0 ||
            std::memcmp(sector + 54, "FAT     ", 8) == 0)
        {
            return USB_FS_TYPE::OTHER;
        }
    }

    return USB_FS_TYPE::UNKNOWN;
}

USB_FS_TYPE UsbStorage::ParseFsTypeString(const std::string& fsName)
{
    if (fsName == "vfat" || fsName == "fat32")
    {
        return USB_FS_TYPE::FAT32;
    }
    if (fsName == "exfat")
    {
        return USB_FS_TYPE::EXFAT;
    }
    if (!fsName.empty())
    {
        return USB_FS_TYPE::OTHER;
    }
    return USB_FS_TYPE::UNKNOWN;
}

std::string UsbStorage::FsTypeToString(USB_FS_TYPE fsType)
{
    switch (fsType)
    {
        case USB_FS_TYPE::FAT32:
            return "FAT32";
        case USB_FS_TYPE::EXFAT:
            return "exFAT";
        case USB_FS_TYPE::OTHER:
            return "Other";
        case USB_FS_TYPE::UNKNOWN:
        default:
            return "Unknown";
    }
}

std::string UsbStorage::SanitizeVolumeLabel(const std::string& label, USB_FORMAT_TARGET target)
{
    const size_t maxLen = (target == USB_FORMAT_TARGET::FAT32) ? 11 : 15;
    std::string out;
    out.reserve(maxLen);

    for (char c : label)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_' || c == '-')
        {
            if (target == USB_FORMAT_TARGET::FAT32)
            {
                out.push_back(static_cast<char>(std::toupper(uc)));
            }
            else
            {
                out.push_back(c);
            }
            if (out.size() >= maxLen)
            {
                break;
            }
        }
    }

    if (out.empty())
    {
        out = DEFAULT_VOLUME_LABEL;
    }
    return out;
}

std::array<uint8_t, 512> UsbStorage::BuildSinglePartitionMbr(uint64_t totalSectors, USB_FORMAT_TARGET target)
{
    std::array<uint8_t, 512> mbr{};
    constexpr uint32_t startLba = 2048; // 1 MiB alignment for flash erase blocks

    if (totalSectors > startLba)
    {
        uint64_t partSectors64 = totalSectors - startLba;
        // MBR partition sector count is a 32-bit field (max 2 TiB with 512-byte sectors)
        uint32_t partSectors = (partSectors64 > 0xFFFFFFFFULL) ? 0xFFFFFFFFu : static_cast<uint32_t>(partSectors64);

        uint8_t* entry = mbr.data() + 446;
        entry[0] = 0x00; // non-bootable
        entry[1] = 0xFE; // CHS start (LBA marker)
        entry[2] = 0xFF;
        entry[3] = 0xFF;
        // 0x0C = W95 FAT32 (LBA), 0x07 = HPFS/NTFS/exFAT
        entry[4] = (target == USB_FORMAT_TARGET::EXFAT) ? 0x07u : 0x0Cu;
        entry[5] = 0xFE; // CHS end (LBA marker)
        entry[6] = 0xFF;
        entry[7] = 0xFF;
        WriteLe32(entry + 8, startLba);
        WriteLe32(entry + 12, partSectors);
    }

    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    return mbr;
}

std::vector<UsbMountEntry> UsbStorage::ParseProcMountsContent(const std::string& content)
{
    std::vector<UsbMountEntry> entries;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line))
    {
        std::istringstream lineStream(line);
        UsbMountEntry entry;
        if (lineStream >> entry.device >> entry.mountPoint >> entry.fsType)
        {
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

bool UsbStorage::IsBootFilename(const std::string& filename)
{
    if (filename.empty() || filename == "." || filename == "..")
    {
        return false;
    }

    std::string lower;
    lower.reserve(filename.size());
    for (char c : filename)
    {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // X32 / OpenX32 DCP bootloader images (e.g. dcp_corefs_openx32.run, dcp_corefs_4.13.update)
    if (lower.rfind("dcp", 0) == 0)
    {
        if ((lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".run") == 0) ||
            (lower.size() > 7 && lower.compare(lower.size() - 7, 7, ".update") == 0))
        {
            return true;
        }
    }

    // Raw OpenX32 / Linux / U-Boot boot artifacts
    if (lower == "openx32.bin" ||
        lower == "uimage" ||
        lower == "zimage" ||
        lower == "u-boot.bin" ||
        lower == "uramdisk.bin" ||
        lower == "imx25-pdk.dtb")
    {
        return true;
    }

    return false;
}

bool UsbStorage::HasBootFilesInDirectory(const std::string& dirPath, std::string* matchedFile)
{
    if (dirPath.empty())
    {
        return false;
    }

    DIR* dir = opendir(dirPath.c_str());
    if (dir == nullptr)
    {
        return false;
    }

    bool found = false;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name(entry->d_name);
        if (IsBootFilename(name))
        {
            found = true;
            if (matchedFile != nullptr)
            {
                *matchedFile = name;
            }
            break;
        }
    }
    closedir(dir);
    return found;
}

bool UsbStorage::IsDeviceBootSource(const std::string& devicePath,
                                    const std::string& procMountsContent,
                                    const std::string& procCmdlineContent)
{
    if (devicePath.empty())
    {
        return false;
    }

    // 1. Check if this device or any of its partitions is mounted as an OS/boot filesystem
    std::vector<UsbMountEntry> mounts = ParseProcMountsContent(procMountsContent);
    for (const auto& m : mounts)
    {
        if (IsSameDeviceOrPartition(m.device, devicePath))
        {
            if (m.mountPoint == "/" || m.mountPoint == "/boot" || m.mountPoint == "/openx32")
            {
                return true;
            }
        }
    }

    // 2. Check kernel cmdline root= argument (e.g. "root=/dev/sda1")
    std::istringstream iss(procCmdlineContent);
    std::string token;
    while (iss >> token)
    {
        if (token.rfind("root=", 0) == 0)
        {
            std::string rootDev = token.substr(5);
            if (IsSameDeviceOrPartition(rootDev, devicePath))
            {
                return true;
            }
        }
    }

    return false;
}

bool UsbStorage::ProbeBootFilesReadOnly(const std::string& partitionPath, std::string* matchedFile) const
{
    if (partitionPath.empty())
    {
        return false;
    }

    char tmpDirTemplate[] = "/tmp/omc_usb_probe_XXXXXX";
    char* tmpDir = mkdtemp(tmpDirTemplate);
    if (tmpDir == nullptr)
    {
        return false;
    }

    const char* fsCandidates[] = {"vfat", "exfat"};
    bool mountedRo = false;
    for (const char* fs : fsCandidates)
    {
        if (mount(partitionPath.c_str(), tmpDir, fs, MS_RDONLY | MS_NOATIME, nullptr) == 0)
        {
            mountedRo = true;
            break;
        }
    }

    bool found = false;
    if (mountedRo)
    {
        found = HasBootFilesInDirectory(tmpDir, matchedFile);
        if (umount(tmpDir) != 0)
        {
            umount2(tmpDir, MNT_DETACH);
        }
    }

    rmdir(tmpDir);
    return found;
}

void UsbStorage::UpdateBootProtectionState(bool allowReadOnlyProbe)
{
    currentDevice.bootProtected = false;
    currentDevice.bootProtectionReason.clear();

    if (currentDevice.devicePath.empty() || currentDevice.state == USB_DRIVE_STATE::REMOVED)
    {
        return;
    }

    const std::string mountsContent = ReadWholeFile("/proc/mounts");
    const std::string cmdlineContent = ReadWholeFile("/proc/cmdline");
    if (IsDeviceBootSource(currentDevice.devicePath, mountsContent, cmdlineContent))
    {
        currentDevice.bootProtected = true;
        currentDevice.bootProtectionReason = "Active OS root/boot mount";
        return;
    }

    if (currentDevice.state == USB_DRIVE_STATE::MOUNTED && !currentDevice.mountPoint.empty())
    {
        std::string matchedFile;
        if (HasBootFilesInDirectory(currentDevice.mountPoint, &matchedFile))
        {
            currentDevice.bootProtected = true;
            currentDevice.bootProtectionReason = "Contains OS boot file: " + matchedFile;
        }
    }
    else if (allowReadOnlyProbe && !currentDevice.partitionPath.empty())
    {
        std::string matchedFile;
        if (ProbeBootFilesReadOnly(currentDevice.partitionPath, &matchedFile))
        {
            currentDevice.bootProtected = true;
            currentDevice.bootProtectionReason = "Contains OS boot file: " + matchedFile;
        }
    }
}

std::string UsbStorage::FindFirstUsbBlockDevice(const std::string& sysBlockRoot) const
{
    DIR* dir = opendir(sysBlockRoot.c_str());
    if (dir == nullptr)
    {
        return "";
    }

    std::vector<std::string> candidates;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name(entry->d_name);
        if (IsSafeRemovableUsbDevice(name, sysBlockRoot))
        {
            candidates.push_back(name);
        }
    }
    closedir(dir);

    if (candidates.empty())
    {
        return "";
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.front();
}

std::string UsbStorage::ResolvePartitionPath(const std::string& blockName) const
{
    std::string part1 = "/dev/" + blockName + "1";
    struct stat st{};
    if (stat(part1.c_str(), &st) == 0 && S_ISBLK(st.st_mode))
    {
        return part1;
    }
    return "/dev/" + blockName;
}

USB_FS_TYPE UsbStorage::ProbeDeviceFilesystem(const std::string& targetPath) const
{
    int fd = open(targetPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        return USB_FS_TYPE::UNKNOWN;
    }

    uint8_t sector[512];
    ssize_t bytesRead = pread(fd, sector, sizeof(sector), 0);
    close(fd);

    if (bytesRead < 512)
    {
        return USB_FS_TYPE::UNKNOWN;
    }

    return DetectFilesystemFromVbr(sector, sizeof(sector));
}

bool UsbStorage::RefreshDeviceState()
{
    std::lock_guard<std::mutex> lock(deviceMutex);

    std::string blockName = FindFirstUsbBlockDevice();
    if (blockName.empty())
    {
        formatErrorLatched.store(false);
        bool changed = (currentDevice.state != USB_DRIVE_STATE::REMOVED);
        currentDevice = UsbDeviceInfo{};
        currentDevice.mountPoint = mountPoint;
        currentDevice.state = USB_DRIVE_STATE::REMOVED;
        if (changed)
        {
            PublishStateToConfig();
        }
        return false;
    }

    // Keep format failure visible until the user unplugs the drive or retries formatting
    if (formatErrorLatched.load() && currentDevice.deviceName == blockName)
    {
        return true;
    }

    const std::string sysBase = "/sys/block/" + blockName;
    currentDevice.deviceName = blockName;
    currentDevice.devicePath = "/dev/" + blockName;
    currentDevice.partitionPath = ResolvePartitionPath(blockName);
    currentDevice.mountPoint = mountPoint;
    currentDevice.vendor = ReadTrimmedFile(sysBase + "/device/vendor");
    currentDevice.model = ReadTrimmedFile(sysBase + "/device/model");

    // Check if already mounted
    std::vector<UsbMountEntry> mounts = ParseProcMountsContent(ReadWholeFile("/proc/mounts"));
    bool mounted = false;
    for (const auto& m : mounts)
    {
        if (m.device == currentDevice.partitionPath || m.device == currentDevice.devicePath)
        {
            mounted = true;
            currentDevice.mountPoint = m.mountPoint;
            currentDevice.fsType = ParseFsTypeString(m.fsType);
            currentDevice.state = USB_DRIVE_STATE::MOUNTED;
            break;
        }
    }

    if (!mounted)
    {
        currentDevice.fsType = ProbeDeviceFilesystem(currentDevice.partitionPath);
        if (currentDevice.fsType == USB_FS_TYPE::FAT32 || currentDevice.fsType == USB_FS_TYPE::EXFAT)
        {
            if (Mount())
            {
                mounted = true;
            }
            else
            {
                currentDevice.state = USB_DRIVE_STATE::ERROR;
            }
        }
        else if (currentDevice.fsType == USB_FS_TYPE::OTHER)
        {
            currentDevice.state = USB_DRIVE_STATE::UNSUPPORTED_FS;
        }
        else
        {
            currentDevice.state = USB_DRIVE_STATE::UNMOUNTED;
        }
    }

    if (mounted)
    {
        UpdateSpaceUsage();
    }
    else
    {
        // Report raw block capacity in MB when unmounted
        std::string sectorsStr = ReadTrimmedFile(sysBase + "/size");
        uint64_t sectors = 0;
        if (!sectorsStr.empty())
        {
            sectors = std::strtoull(sectorsStr.c_str(), nullptr, 10);
        }
        currentDevice.totalSpaceMB = static_cast<uint32_t>((sectors * 512ULL) / (1024ULL * 1024ULL));
        currentDevice.usedSpaceMB = 0;
        currentDevice.freeSpaceMB = 0;
    }

    UpdateBootProtectionState();
    PublishStateToConfig();
    return true;
}

bool UsbStorage::Mount()
{
    if (currentDevice.partitionPath.empty())
    {
        return false;
    }

    mkdir(mountPoint.c_str(), 0755);

    const char* fsName = nullptr;
    const char* mountOpts = nullptr;

    if (currentDevice.fsType == USB_FS_TYPE::EXFAT)
    {
        fsName = "exfat";
        mountOpts = "iocharset=utf8";
    }
    else if (currentDevice.fsType == USB_FS_TYPE::FAT32)
    {
        fsName = "vfat";
        mountOpts = "utf8";
    }
    else
    {
        return false;
    }

    if (mount(currentDevice.partitionPath.c_str(), mountPoint.c_str(), fsName, MS_NOATIME, mountOpts) != 0)
    {
        // Retry without iocharset option in case kernel NLS UTF-8 is not enabled
        if (mount(currentDevice.partitionPath.c_str(), mountPoint.c_str(), fsName, MS_NOATIME, nullptr) != 0)
        {
            helper->Error("UsbStorage: failed to mount %s (%s) on %s: %s\n",
                          currentDevice.partitionPath.c_str(), fsName, mountPoint.c_str(), strerror(errno));
            return false;
        }
    }

    currentDevice.mountPoint = mountPoint;
    currentDevice.state = USB_DRIVE_STATE::MOUNTED;
    UpdateSpaceUsage();
    helper->Log("UsbStorage: mounted %s (%s) at %s\n",
                currentDevice.partitionPath.c_str(), fsName, mountPoint.c_str());
    return true;
}

bool UsbStorage::UnmountAllForDevice(const std::string& devicePath)
{
    std::vector<UsbMountEntry> mounts = ParseProcMountsContent(ReadWholeFile("/proc/mounts"));
    bool allOk = true;

    for (const auto& m : mounts)
    {
        if (IsSameDeviceOrPartition(m.device, devicePath))
        {
            if (umount(m.mountPoint.c_str()) != 0)
            {
                helper->Error("UsbStorage: failed to unmount %s (%s): %s\n",
                              m.mountPoint.c_str(), m.device.c_str(), strerror(errno));
                allOk = false;
            }
        }
    }

    return allOk;
}

bool UsbStorage::WriteSinglePartitionMbr(const std::string& devicePath, const std::string& blockName, USB_FORMAT_TARGET target)
{
    int fd = open(devicePath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        helper->Error("UsbStorage: cannot open %s for partitioning: %s\n", devicePath.c_str(), strerror(errno));
        return false;
    }

    uint64_t sizeBytes = 0;
    if (ioctl(fd, BLKGETSIZE64, &sizeBytes) != 0 || sizeBytes < (4ULL * 1024ULL * 1024ULL))
    {
        helper->Error("UsbStorage: invalid block device size on %s\n", devicePath.c_str());
        close(fd);
        return false;
    }

    uint64_t totalSectors = sizeBytes / 512ULL;
    std::array<uint8_t, 512> mbr = BuildSinglePartitionMbr(totalSectors, target);

    ssize_t written = pwrite(fd, mbr.data(), mbr.size(), 0);
    if (written != static_cast<ssize_t>(mbr.size()))
    {
        helper->Error("UsbStorage: failed to write MBR to %s: %s\n", devicePath.c_str(), strerror(errno));
        close(fd);
        return false;
    }

    // Wipe primary (sectors 1..33) and backup (last 33 sectors) GPT headers so CONFIG_EFI_PARTITION ignores stale GPT tables
    std::array<uint8_t, 512> zeroSector{};
    for (uint64_t s = 1; s <= 33 && s < totalSectors; s++)
    {
        pwrite(fd, zeroSector.data(), zeroSector.size(), static_cast<off_t>(s * 512ULL));
    }
    if (totalSectors > 66)
    {
        for (uint64_t s = totalSectors - 33; s < totalSectors; s++)
        {
            pwrite(fd, zeroSector.data(), zeroSector.size(), static_cast<off_t>(s * 512ULL));
        }
    }

    fsync(fd);
    ioctl(fd, BLKRRPART);
    close(fd);

    // Poll up to 2 seconds for kernel to create /dev/<blockName>1 rather than risking formatting raw /dev/<blockName>
    const std::string expectedPart = "/dev/" + blockName + "1";
    for (int attempt = 0; attempt < 20; attempt++)
    {
        struct stat st{};
        if (stat(expectedPart.c_str(), &st) == 0 && S_ISBLK(st.st_mode))
        {
            return true;
        }
        usleep(100000);
    }

    helper->Error("UsbStorage: partition node %s did not appear after BLKRRPART\n", expectedPart.c_str());
    return false;
}

bool UsbStorage::RunProcess(const std::vector<std::string>& args) const
{
    if (args.empty())
    {
        return false;
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args)
    {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0)
    {
        helper->Error("UsbStorage: fork() failed for %s: %s\n", args[0].c_str(), strerror(errno));
        return false;
    }

    if (pid == 0)
    {
        // Child process: execute formatter without invoking a shell
        execvp(argv[0], argv.data());
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return false;
    }

    return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
}

bool UsbStorage::StartFormatAsync(USB_FORMAT_TARGET target, const std::string& volumeLabel)
{
    if (formatInProgress.load())
    {
        helper->Error("UsbStorage: format already in progress\n");
        return false;
    }

    if (IsAudioBusy())
    {
        helper->Error("UsbStorage: refusing to format USB drive while audio recording/playback is active\n");
        return false;
    }

    if (state->bodyless)
    {
        helper->Log("UsbStorage: bodyless mode — skipping destructive format (%s)\n",
                    target == USB_FORMAT_TARGET::EXFAT ? "exFAT" : "FAT32");
        return false;
    }

    if (formatThread.joinable())
    {
        formatThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(deviceMutex);
        // Check boot protection while currentDevice.state is still MOUNTED/UNMOUNTED (before switching to FORMATTING)
        UpdateBootProtectionState(true);
        if (currentDevice.bootProtected)
        {
            helper->Error("UsbStorage: refusing to format boot USB device %s (%s)\n",
                          currentDevice.devicePath.c_str(), currentDevice.bootProtectionReason.c_str());
            PublishStateToConfig();
            return false;
        }

        formatErrorLatched.store(false);
        formatInProgress.store(true);
        currentDevice.state = USB_DRIVE_STATE::FORMATTING;
        PublishStateToConfig();
    }

    formatThread = std::thread([this, target, volumeLabel]() {
        bool ok = Format(target, volumeLabel);
        if (!ok)
        {
            formatErrorLatched.store(true);
        }
        formatInProgress.store(false);
        formatStateDirty.store(true);
    });

    return true;
}

bool UsbStorage::Format(USB_FORMAT_TARGET target, const std::string& volumeLabel)
{
    if (IsAudioBusy())
    {
        helper->Error("UsbStorage: refusing to format USB drive while audio recording/playback is active\n");
        return false;
    }

    if (state->bodyless)
    {
        helper->Log("UsbStorage: bodyless mode — skipping destructive format (%s)\n",
                    target == USB_FORMAT_TARGET::EXFAT ? "exFAT" : "FAT32");
        return false;
    }

    std::lock_guard<std::mutex> lock(deviceMutex);

    const USB_DRIVE_STATE previousState = currentDevice.state;
    std::string blockName = currentDevice.deviceName.empty() ? FindFirstUsbBlockDevice() : currentDevice.deviceName;
    if (!IsSafeRemovableUsbDevice(blockName))
    {
        helper->Error("UsbStorage: refusing to format unsafe or missing device '%s'\n", blockName.c_str());
        currentDevice.state = USB_DRIVE_STATE::ERROR;
        return false;
    }

    currentDevice.deviceName = blockName;
    currentDevice.devicePath = "/dev/" + blockName;
    if (currentDevice.partitionPath.empty())
    {
        currentDevice.partitionPath = ResolvePartitionPath(blockName);
    }

    // Defense-in-depth: re-verify boot USB protection (including read-only probe if unmounted) right before any destructive write
    UpdateBootProtectionState(true);
    if (currentDevice.bootProtected)
    {
        helper->Error("UsbStorage: refusing to format boot USB device %s (%s)\n",
                      currentDevice.devicePath.c_str(), currentDevice.bootProtectionReason.c_str());
        currentDevice.state = (previousState == USB_DRIVE_STATE::FORMATTING) ? USB_DRIVE_STATE::MOUNTED : previousState;
        return false;
    }

    currentDevice.state = USB_DRIVE_STATE::FORMATTING;

    sync();
    if (!UnmountAllForDevice(currentDevice.devicePath))
    {
        currentDevice.state = USB_DRIVE_STATE::ERROR;
        return false;
    }

    if (!WriteSinglePartitionMbr(currentDevice.devicePath, blockName, target))
    {
        currentDevice.state = USB_DRIVE_STATE::ERROR;
        return false;
    }

    const std::string expectedPartition = "/dev/" + blockName + "1";
    struct stat partStat{};
    if (stat(expectedPartition.c_str(), &partStat) != 0 || !S_ISBLK(partStat.st_mode))
    {
        helper->Error("UsbStorage: expected partition %s missing — aborting to protect MBR\n", expectedPartition.c_str());
        currentDevice.state = USB_DRIVE_STATE::ERROR;
        return false;
    }
    currentDevice.partitionPath = expectedPartition;
    const std::string cleanLabel = SanitizeVolumeLabel(volumeLabel, target);

    std::vector<std::string> cmd;
    if (target == USB_FORMAT_TARGET::EXFAT)
    {
        // 128 KB clusters reduce flash erase stalls during multitrack WAV recording
        cmd = {"mkfs.exfat", "-c", "128K", "-L", cleanLabel, currentDevice.partitionPath};
    }
    else
    {
        // FAT32 with 32 KB clusters (64 sectors * 512 B)
        cmd = {"mkfs.fat", "-F", "32", "-s", "64", "-n", cleanLabel, currentDevice.partitionPath};
    }

    helper->Log("UsbStorage: formatting %s as %s (label: %s)\n",
                currentDevice.partitionPath.c_str(),
                target == USB_FORMAT_TARGET::EXFAT ? "exFAT" : "FAT32",
                cleanLabel.c_str());

    if (!RunProcess(cmd))
    {
        helper->Error("UsbStorage: formatter command '%s' failed on %s\n",
                      cmd[0].c_str(), currentDevice.partitionPath.c_str());
        currentDevice.state = USB_DRIVE_STATE::ERROR;
        return false;
    }

    sync();
    currentDevice.fsType = (target == USB_FORMAT_TARGET::EXFAT) ? USB_FS_TYPE::EXFAT : USB_FS_TYPE::FAT32;
    currentDevice.label = cleanLabel;

    if (!Mount())
    {
        currentDevice.state = USB_DRIVE_STATE::ERROR;
        return false;
    }

    UpdateBootProtectionState(false);
    return true;
}

void UsbStorage::UpdateSpaceUsage()
{
    struct statvfs vfs{};
    if (statvfs(currentDevice.mountPoint.c_str(), &vfs) != 0)
    {
        return;
    }

    const uint64_t blockSize = static_cast<uint64_t>(vfs.f_frsize > 0 ? vfs.f_frsize : vfs.f_bsize);
    const uint64_t totalBytes = static_cast<uint64_t>(vfs.f_blocks) * blockSize;
    const uint64_t freeBytes = static_cast<uint64_t>(vfs.f_bavail) * blockSize;
    const uint64_t usedBytes = (totalBytes > freeBytes) ? (totalBytes - freeBytes) : 0ULL;

    currentDevice.totalSpaceMB = static_cast<uint32_t>(totalBytes / (1024ULL * 1024ULL));
    currentDevice.freeSpaceMB = static_cast<uint32_t>(freeBytes / (1024ULL * 1024ULL));
    currentDevice.usedSpaceMB = static_cast<uint32_t>(usedBytes / (1024ULL * 1024ULL));
}

void UsbStorage::PublishStateToConfig()
{
    config->Set(USB_STORAGE_STATE, static_cast<uint>(currentDevice.state));
    config->Set(USB_STORAGE_FS_TYPE, static_cast<uint>(currentDevice.fsType));
    config->Set(USB_STORAGE_TOTAL_SPACE, static_cast<uint>(currentDevice.totalSpaceMB));
    config->Set(USB_STORAGE_FREE_SPACE, static_cast<uint>(currentDevice.freeSpaceMB));
    config->Set(USB_STORAGE_BOOT_PROTECTED, currentDevice.bootProtected);

    std::string summary;
    if (currentDevice.state == USB_DRIVE_STATE::REMOVED)
    {
        summary = "No USB Drive";
    }
    else
    {
        std::string name = currentDevice.vendor;
        if (!currentDevice.model.empty())
        {
            if (!name.empty())
            {
                name += " ";
            }
            name += currentDevice.model;
        }
        if (name.empty())
        {
            name = currentDevice.deviceName;
        }

        summary = name + " (" + FsTypeToString(currentDevice.fsType);
        if (currentDevice.totalSpaceMB >= 1024)
        {
            summary += ", " + std::to_string(currentDevice.freeSpaceMB / 1024) + "/" +
                       std::to_string(currentDevice.totalSpaceMB / 1024) + " GB free";
        }
        else if (currentDevice.totalSpaceMB > 0)
        {
            summary += ", " + std::to_string(currentDevice.freeSpaceMB) + "/" +
                       std::to_string(currentDevice.totalSpaceMB) + " MB free";
        }
        summary += ")";
    }

    config->Set(USB_STORAGE_DEVICE_INFO, String(summary.c_str()));
}

// ######## ########  ######  ########  ######
//    ##    ##       ##    ##    ##    ##    ##
//    ##    ##       ##          ##    ##
//    ##    ######    ######     ##     ######
//    ##    ##             ##    ##          ##
//    ##    ##       ##    ##    ##    ##    ##
//    ##    ########  ######     ##     ######

TEST_CASE("UsbStorage - DetectFilesystemFromVbr")
{
    std::array<uint8_t, 512> sector{};

    CHECK(UsbStorage::DetectFilesystemFromVbr(sector.data(), sector.size()) == USB_FS_TYPE::UNKNOWN);
    CHECK(UsbStorage::DetectFilesystemFromVbr(nullptr, 512) == USB_FS_TYPE::UNKNOWN);
    CHECK(UsbStorage::DetectFilesystemFromVbr(sector.data(), 100) == USB_FS_TYPE::UNKNOWN);

    // exFAT VBR
    sector.fill(0);
    std::memcpy(sector.data() + 3, "EXFAT   ", 8);
    CHECK(UsbStorage::DetectFilesystemFromVbr(sector.data(), sector.size()) == USB_FS_TYPE::EXFAT);

    // FAT32 VBR
    sector.fill(0);
    std::memcpy(sector.data() + 82, "FAT32   ", 8);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    CHECK(UsbStorage::DetectFilesystemFromVbr(sector.data(), sector.size()) == USB_FS_TYPE::FAT32);

    // FAT16 VBR
    sector.fill(0);
    std::memcpy(sector.data() + 54, "FAT16   ", 8);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    CHECK(UsbStorage::DetectFilesystemFromVbr(sector.data(), sector.size()) == USB_FS_TYPE::OTHER);
}

TEST_CASE("UsbStorage - BuildSinglePartitionMbr")
{
    constexpr uint64_t sectors32GB = (32ULL * 1024ULL * 1024ULL * 1024ULL) / 512ULL;

    auto mbrFat32 = UsbStorage::BuildSinglePartitionMbr(sectors32GB, USB_FORMAT_TARGET::FAT32);
    CHECK(mbrFat32[510] == 0x55);
    CHECK(mbrFat32[511] == 0xAA);
    CHECK(mbrFat32[446 + 4] == 0x0C); // FAT32 LBA

    uint32_t startLba = static_cast<uint32_t>(mbrFat32[446 + 8]) |
                        (static_cast<uint32_t>(mbrFat32[446 + 9]) << 8) |
                        (static_cast<uint32_t>(mbrFat32[446 + 10]) << 16) |
                        (static_cast<uint32_t>(mbrFat32[446 + 11]) << 24);
    CHECK(startLba == 2048);

    auto mbrExfat = UsbStorage::BuildSinglePartitionMbr(sectors32GB, USB_FORMAT_TARGET::EXFAT);
    CHECK(mbrExfat[510] == 0x55);
    CHECK(mbrExfat[511] == 0xAA);
    CHECK(mbrExfat[446 + 4] == 0x07); // exFAT / HPFS / NTFS
}

TEST_CASE("UsbStorage - SanitizeVolumeLabel")
{
    CHECK(UsbStorage::SanitizeVolumeLabel("openx32 rec!", USB_FORMAT_TARGET::FAT32) == "OPENX32REC");
    CHECK(UsbStorage::SanitizeVolumeLabel("VeryLongVolumeName12345", USB_FORMAT_TARGET::FAT32) == "VERYLONGVOL");
    CHECK(UsbStorage::SanitizeVolumeLabel("OpenX32_Rec-01", USB_FORMAT_TARGET::EXFAT) == "OpenX32_Rec-01");
    CHECK(UsbStorage::SanitizeVolumeLabel("!!!", USB_FORMAT_TARGET::FAT32) == "OPENX32");
}

TEST_CASE("UsbStorage - ParseProcMountsContent")
{
    std::string sample =
        "proc /proc proc rw,relatime 0 0\n"
        "/dev/sda1 /mnt/usb exfat rw,noatime 0 0\n"
        "/dev/sdb1 /mnt/usb2 vfat rw,noatime 0 0\n";

    auto entries = UsbStorage::ParseProcMountsContent(sample);
    REQUIRE(entries.size() == 3);
    CHECK(entries[1].device == "/dev/sda1");
    CHECK(entries[1].mountPoint == "/mnt/usb");
    CHECK(UsbStorage::ParseFsTypeString(entries[1].fsType) == USB_FS_TYPE::EXFAT);
    CHECK(UsbStorage::ParseFsTypeString(entries[2].fsType) == USB_FS_TYPE::FAT32);
}

TEST_CASE("UsbStorage - IsSafeRemovableUsbDevice safety checks")
{
    // Reject internal SD card, loopback, traversal, and partition names immediately
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("mmcblk0"));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("mmcblk0p1"));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("loop0"));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("sda1"));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("../sda"));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice(""));

    // Create a temporary mock sysfs tree to validate removable + USB bus checks
    char tmpDirTemplate[] = "/tmp/omc_usb_test_XXXXXX";
    char* tmpDir = mkdtemp(tmpDirTemplate);
    REQUIRE(tmpDir != nullptr);

    std::string root(tmpDir);
    std::string usbTarget = root + "/devices/platform/usb1/1-1/host0/target";
    std::string nonUsbTarget = root + "/devices/platform/ahci0/host0/target";

    // Build nested dirs
    mkdir((root + "/devices").c_str(), 0755);
    mkdir((root + "/devices/platform").c_str(), 0755);
    mkdir((root + "/devices/platform/usb1").c_str(), 0755);
    mkdir((root + "/devices/platform/usb1/1-1").c_str(), 0755);
    mkdir((root + "/devices/platform/usb1/1-1/host0").c_str(), 0755);
    mkdir(usbTarget.c_str(), 0755);
    mkdir((root + "/devices/platform/ahci0").c_str(), 0755);
    mkdir((root + "/devices/platform/ahci0/host0").c_str(), 0755);
    mkdir(nonUsbTarget.c_str(), 0755);

    std::string sysBlock = root + "/sysblock";
    mkdir(sysBlock.c_str(), 0755);

    // Valid removable USB stick (sda)
    mkdir((sysBlock + "/sda").c_str(), 0755);
    {
        std::ofstream f(sysBlock + "/sda/removable");
        f << "1\n";
    }
    CHECK(symlink(usbTarget.c_str(), (sysBlock + "/sda/device").c_str()) == 0);

    // Non-removable USB device (sdb)
    mkdir((sysBlock + "/sdb").c_str(), 0755);
    {
        std::ofstream f(sysBlock + "/sdb/removable");
        f << "0\n";
    }
    CHECK(symlink(usbTarget.c_str(), (sysBlock + "/sdb/device").c_str()) == 0);

    // Removable non-USB device (sdc)
    mkdir((sysBlock + "/sdc").c_str(), 0755);
    {
        std::ofstream f(sysBlock + "/sdc/removable");
        f << "1\n";
    }
    CHECK(symlink(nonUsbTarget.c_str(), (sysBlock + "/sdc/device").c_str()) == 0);

    CHECK(UsbStorage::IsSafeRemovableUsbDevice("sda", sysBlock));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("sdb", sysBlock));
    CHECK_FALSE(UsbStorage::IsSafeRemovableUsbDevice("sdc", sysBlock));

    // Cleanup temp files
    unlink((sysBlock + "/sda/removable").c_str());
    unlink((sysBlock + "/sda/device").c_str());
    rmdir((sysBlock + "/sda").c_str());
    unlink((sysBlock + "/sdb/removable").c_str());
    unlink((sysBlock + "/sdb/device").c_str());
    rmdir((sysBlock + "/sdb").c_str());
    unlink((sysBlock + "/sdc/removable").c_str());
    unlink((sysBlock + "/sdc/device").c_str());
    rmdir((sysBlock + "/sdc").c_str());
    rmdir(sysBlock.c_str());
    rmdir(usbTarget.c_str());
    rmdir((root + "/devices/platform/usb1/1-1/host0").c_str());
    rmdir((root + "/devices/platform/usb1/1-1").c_str());
    rmdir((root + "/devices/platform/usb1").c_str());
    rmdir(nonUsbTarget.c_str());
    rmdir((root + "/devices/platform/ahci0/host0").c_str());
    rmdir((root + "/devices/platform/ahci0").c_str());
    rmdir((root + "/devices/platform").c_str());
    rmdir((root + "/devices").c_str());
    rmdir(root.c_str());
}

TEST_CASE("UsbStorage - Boot USB format protection")
{
    // Boot filename detection (case-insensitive)
    CHECK(UsbStorage::IsBootFilename("dcp_corefs_openx32.run"));
    CHECK(UsbStorage::IsBootFilename("DCP_COREFS_4.13.UPDATE"));
    CHECK(UsbStorage::IsBootFilename("dcp_x32.run"));
    CHECK(UsbStorage::IsBootFilename("openx32.bin"));
    CHECK(UsbStorage::IsBootFilename("uImage"));
    CHECK(UsbStorage::IsBootFilename("zImage"));
    CHECK(UsbStorage::IsBootFilename("u-boot.bin"));
    CHECK(UsbStorage::IsBootFilename("uramdisk.bin"));
    CHECK(UsbStorage::IsBootFilename("imx25-pdk.dtb"));

    CHECK_FALSE(UsbStorage::IsBootFilename("recording_01.wav"));
    CHECK_FALSE(UsbStorage::IsBootFilename("scene0.json"));
    CHECK_FALSE(UsbStorage::IsBootFilename("dcp_notes.txt"));
    CHECK_FALSE(UsbStorage::IsBootFilename("run.wav"));
    CHECK_FALSE(UsbStorage::IsBootFilename(".."));

    // Mountpoint & cmdline boot source detection
    const std::string normalMounts =
        "proc /proc proc rw 0 0\n"
        "/dev/sda1 /mnt/usb exfat rw,noatime 0 0\n";
    const std::string rootUsbMounts =
        "/dev/sda1 / ext4 rw 0 0\n";
    const std::string bootUsbMounts =
        "/dev/sda1 /boot vfat rw 0 0\n";

    CHECK_FALSE(UsbStorage::IsDeviceBootSource("/dev/sda", normalMounts, "console=ttymxc4 root=/dev/mmcblk0p2"));
    CHECK(UsbStorage::IsDeviceBootSource("/dev/sda", rootUsbMounts, "console=ttymxc4"));
    CHECK(UsbStorage::IsDeviceBootSource("/dev/sda", bootUsbMounts, "console=ttymxc4"));
    CHECK(UsbStorage::IsDeviceBootSource("/dev/sda", normalMounts, "console=ttymxc4 root=/dev/sda1 rw"));

    // Prefix collision regression tests: /dev/sda must NOT match /dev/sdaa or /dev/sdaa1
    CHECK(UsbStorage::IsSameDeviceOrPartition("/dev/sda", "/dev/sda"));
    CHECK(UsbStorage::IsSameDeviceOrPartition("/dev/sda1", "/dev/sda"));
    CHECK(UsbStorage::IsSameDeviceOrPartition("/dev/sda12", "/dev/sda"));
    CHECK_FALSE(UsbStorage::IsSameDeviceOrPartition("/dev/sdaa", "/dev/sda"));
    CHECK_FALSE(UsbStorage::IsSameDeviceOrPartition("/dev/sdaa1", "/dev/sda"));
    CHECK_FALSE(UsbStorage::IsDeviceBootSource("/dev/sda", "/dev/sdaa1 / ext4 rw 0 0\n", "root=/dev/sdaa1"));

    // Directory scan for OpenX32 DCP boot file
    char tmpDirTemplate[] = "/tmp/omc_boot_test_XXXXXX";
    char* tmpDir = mkdtemp(tmpDirTemplate);
    REQUIRE(tmpDir != nullptr);

    std::string dir(tmpDir);
    std::string matched;
    CHECK_FALSE(UsbStorage::HasBootFilesInDirectory(dir, &matched));

    {
        std::ofstream f(dir + "/dcp_corefs_openx32.run");
        f << "boot";
    }
    CHECK(UsbStorage::HasBootFilesInDirectory(dir, &matched));
    CHECK(matched == "dcp_corefs_openx32.run");

    unlink((dir + "/dcp_corefs_openx32.run").c_str());
    rmdir(dir.c_str());
}

TEST_CASE("UsbStorage - StartFormatAsync guards")
{
    Helper helper;
    State testState(&helper);
    testState.bodyless = true;
    Config testConfig("X32", &helper, false);
    X32BaseParameter basepar(nullptr, &testConfig, &testState, &helper);
    UsbStorage storage(&basepar);

    // Bodyless mode must refuse without launching a format thread
    CHECK_FALSE(storage.StartFormatAsync(USB_FORMAT_TARGET::EXFAT));
    CHECK_FALSE(storage.Format(USB_FORMAT_TARGET::FAT32));

    // Audio-busy guard must refuse even when bodyless is false
    testState.bodyless = false;
    storage.SetAudioBusy(true, false);
    CHECK_FALSE(storage.StartFormatAsync(USB_FORMAT_TARGET::EXFAT));
    CHECK_FALSE(storage.Format(USB_FORMAT_TARGET::FAT32));
    storage.SetAudioBusy(false, false);
}

}
