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

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "base.h"
#include "defines.h"
#include "enum.h"

namespace OMC
{

struct UsbDeviceInfo
{
    std::string deviceName;     // e.g. "sda"
    std::string devicePath;     // e.g. "/dev/sda"
    std::string partitionPath;  // e.g. "/dev/sda1"
    std::string mountPoint;     // e.g. "/mnt/usb"
    std::string vendor;
    std::string model;
    std::string label;
    USB_FS_TYPE fsType = USB_FS_TYPE::UNKNOWN;
    USB_DRIVE_STATE state = USB_DRIVE_STATE::REMOVED;
    uint32_t totalSpaceMB = 0;
    uint32_t usedSpaceMB = 0;
    uint32_t freeSpaceMB = 0;
    bool bootProtected = false;
    std::string bootProtectionReason;
};

struct UsbMountEntry
{
    std::string device;
    std::string mountPoint;
    std::string fsType;
};

class UsbStorage : public X32Base
{
    using enum MP_ID;

    public:
        static constexpr const char* DEFAULT_MOUNT_POINT = "/mnt/usb";
        static constexpr const char* DEFAULT_VOLUME_LABEL = "OPENX32";

        explicit UsbStorage(X32BaseParameter* basepar);
        ~UsbStorage();

        void Init();
        void Sync();
        void Tick1000ms();

        // shortcut: USB audio recorder is not yet implemented; call this once playback/recording is wired in
        void SetAudioBusy(bool recording, bool playback)
        {
            recordingActive.store(recording);
            playbackActive.store(playback);
        }
        bool IsAudioBusy() const { return recordingActive.load() || playbackActive.load(); }

        bool RefreshDeviceState();
        bool Mount();
        bool StartFormatAsync(USB_FORMAT_TARGET target, const std::string& volumeLabel = DEFAULT_VOLUME_LABEL);
        bool Format(USB_FORMAT_TARGET target, const std::string& volumeLabel = DEFAULT_VOLUME_LABEL);

        // Pure / testable helpers
        static bool IsSafeRemovableUsbDevice(const std::string& blockName, const std::string& sysBlockRoot = "/sys/block");
        static bool IsSameDeviceOrPartition(const std::string& candidate, const std::string& baseDevice);
        static USB_FS_TYPE DetectFilesystemFromVbr(const uint8_t* sector, size_t length);
        static USB_FS_TYPE ParseFsTypeString(const std::string& fsName);
        static std::string FsTypeToString(USB_FS_TYPE fsType);
        static std::string SanitizeVolumeLabel(const std::string& label, USB_FORMAT_TARGET target);
        static std::array<uint8_t, 512> BuildSinglePartitionMbr(uint64_t totalSectors, USB_FORMAT_TARGET target);
        static std::vector<UsbMountEntry> ParseProcMountsContent(const std::string& content);
        static bool IsBootFilename(const std::string& filename);
        static bool HasBootFilesInDirectory(const std::string& dirPath, std::string* matchedFile = nullptr);
        static bool IsDeviceBootSource(const std::string& devicePath,
                                       const std::string& procMountsContent,
                                       const std::string& procCmdlineContent);

    private:
        UsbDeviceInfo currentDevice;
        std::atomic<bool> recordingActive{false};
        std::atomic<bool> playbackActive{false};
        std::atomic<bool> formatInProgress{false};
        std::atomic<bool> formatStateDirty{false};
        std::atomic<bool> formatErrorLatched{false};
        std::mutex deviceMutex;
        std::thread formatThread;
        std::string mountPoint = DEFAULT_MOUNT_POINT;

        std::string FindFirstUsbBlockDevice(const std::string& sysBlockRoot = "/sys/block") const;
        std::string ResolvePartitionPath(const std::string& blockName) const;
        USB_FS_TYPE ProbeDeviceFilesystem(const std::string& targetPath) const;
        bool ProbeBootFilesReadOnly(const std::string& partitionPath, std::string* matchedFile) const;
        bool WriteSinglePartitionMbr(const std::string& devicePath, const std::string& blockName, USB_FORMAT_TARGET target);
        bool RunProcess(const std::vector<std::string>& args) const;
        bool UnmountAllForDevice(const std::string& devicePath);
        void UpdateSpaceUsage();
        void UpdateBootProtectionState(bool allowReadOnlyProbe = false);
        void PublishStateToConfig();
};

}
