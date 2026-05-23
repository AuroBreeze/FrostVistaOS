#ifndef __KERNEL_EXT4_HELPER_H__
#define __KERNEL_EXT4_HELPER_H__

#include "kernel/types.h"
#include "kernel/defs.h"

// EXT4 on-disk fields are little-endian. Read byte-by-byte to avoid relying on
// host endianness or aligned integer loads.
static inline uint16 ext4_read_le16(const uint8 *p)
{
	return (uint16) p[0] | ((uint16) p[1] << 8);
}

static inline uint32 ext4_read_le32(const uint8 *p)
{
	return (uint32) p[0] | ((uint32) p[1] << 8) | ((uint32) p[2] << 16) |
	       ((uint32) p[3] << 24);
}

// Some 64-bit EXT4 fields are stored as separate low/high 32-bit values for
// compatibility with older descriptor layouts.
static inline uint64 ext4_read_le64_split(const uint8 *lo, const uint8 *hi)
{
	return ((uint64) ext4_read_le32(hi) << 32) | ext4_read_le32(lo);
}

static inline int ext4_name_eq(const uint8 *raw_name, uint8 raw_len,
			       const char *name)
{
	if (strlen(name) != raw_len) {
		return 0;
	}
	return strncmp((const char *) raw_name, name, raw_len) == 0;
}

#endif
