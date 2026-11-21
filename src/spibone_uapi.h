/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
/*
 * Userspace ABI of the LiteX SPIBone bus driver: /dev/spiboneN.
 *
 * Wishbone access (bus state "online" only, else ENODEV):
 *   SPIBONE_IOC_PEEK / POKE           one word at a byte address (LiteX csr.csv)
 *   SPIBONE_IOC_BURST_READ / WRITE    count consecutive words (word-incrementing)
 *
 * Bus state (also /sys/bus/spi/devices/<dev>/state: "offline", "config", "online"):
 *   SPIBONE_STATE_OFFLINE  no gateware with the bridge: no bus access, no children
 *   SPIBONE_STATE_CONFIG   the FPGA is being configured: SPIBONE_IOC_RAW_MESSAGE only
 *   SPIBONE_STATE_ONLINE   the bridge runs: Wishbone access, the client devices of
 *                          the DT child nodes exist
 *   Leaving "online" removes the client devices first (e.g. the AES67 netdev).
 *
 * FPGA configuration (state "config" only):
 *   SPIBONE_IOC_RAW_MESSAGE  one SPI message (struct spi_ioc_transfer as for
 *                            spidev, linux/spi/spidev.h). The chip select stays
 *                            asserted for the whole message (cs_change as for
 *                            spidev), so e.g. a configuration command plus the
 *                            bitstream go out in one chip-select frame.
 *
 * The Rust KernelTransport of config_tool mirrors these definitions: keep in sync.
 */
#ifndef _UAPI_SPIBONE_H
#define _UAPI_SPIBONE_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct spibone_wb_xfer {
	__u32 addr;		/* byte address */
	__u32 val;		/* PEEK: value read */
};

struct spibone_wb_burst {
	__u32 addr;		/* byte address of the first word */
	__u32 count;		/* number of words (1 .. 65535) */
	__u64 words;		/* user pointer to count __u32 */
};

struct spibone_raw_message {
	__u64 xfers;		/* user pointer to n_xfers struct spi_ioc_transfer */
	__u32 n_xfers;		/* 1 .. SPIBONE_RAW_MAX_XFERS */
	__u32 mode;		/* SPI mode bits for this message: SPI_CPHA | SPI_CPOL (0 .. 3) */
};

#define SPIBONE_STATE_OFFLINE	0
#define SPIBONE_STATE_CONFIG	1
#define SPIBONE_STATE_ONLINE	2

#define SPIBONE_RAW_MAX_XFERS	4096
#define SPIBONE_RAW_MAX_BYTES	(8 << 20)

#define SPIBONE_IOC_MAGIC	0xB5
#define SPIBONE_IOC_PEEK	_IOWR(SPIBONE_IOC_MAGIC, 1, struct spibone_wb_xfer)
#define SPIBONE_IOC_POKE	_IOW(SPIBONE_IOC_MAGIC, 2, struct spibone_wb_xfer)
#define SPIBONE_IOC_BURST_READ	_IOW(SPIBONE_IOC_MAGIC, 3, struct spibone_wb_burst)
#define SPIBONE_IOC_BURST_WRITE	_IOW(SPIBONE_IOC_MAGIC, 4, struct spibone_wb_burst)
#define SPIBONE_IOC_GET_STATE	_IOR(SPIBONE_IOC_MAGIC, 5, __u32)
#define SPIBONE_IOC_SET_STATE	_IOW(SPIBONE_IOC_MAGIC, 6, __u32)
#define SPIBONE_IOC_RAW_MESSAGE	_IOW(SPIBONE_IOC_MAGIC, 7, struct spibone_raw_message)

#endif /* _UAPI_SPIBONE_H */
