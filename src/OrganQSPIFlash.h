// OrganQSPIFlash.h  -  LittleFS on the Teensy 4.1 QSPI NOR flash pads, with
// OrganCore's own chip table.
//
// Why this exists: the stock LittleFS_QSPIFlash only mounts a chip whose JEDEC
// ID is in LittleFS.cpp's private known_chips[] table, and that table cannot be
// extended from outside the file. The Boya BY25Q128ES fitted to several
// consoles (JEDEC 68 40 18) is not in it, so until 1.12.0 every build needed a
// hand-patched copy of LittleFS -- which a Teensyduino update, or a build on a
// different computer, silently undid, leaving the console with no combination
// storage. This class is the stock QSPI back-end copied into OrganCore with the
// Boya added to its table, so a stock Teensyduino on any machine works.
//
// It derives from the stock LittleFS base class, so the file system itself is
// still PJRC's, unmodified; only the QSPI hardware layer is ours. NOR only: the
// QSPI NAND parts that LittleFS_QSPI also probes are not supported.
//
// Use exactly like LittleFS_QSPIFlash: begin() reads the JEDEC ID, looks it up,
// mounts (formatting a blank chip), and returns false if the chip is absent or
// unknown. lastJedecId[] holds the three ID bytes from the last begin(), so a
// failure can name the part that answered (all 00 or all FF = nothing there).

#ifndef ORGANCORE_ORGANQSPIFLASH_H
#define ORGANCORE_ORGANQSPIFLASH_H

#include <Arduino.h>
#include <LittleFS.h>

#if defined(__IMXRT1062__)
class OrganQSPIFlash : public LittleFS
{
public:
	// Not constexpr: LittleFS's own constructor is not (Teensyduino 1.59), and a
	// constexpr constructor may not call a non-constexpr one.
	OrganQSPIFlash() { }
	bool begin();
	const char * getMediaName();
	const char * name() { return getMediaName(); }
	uint8_t lastJedecId[3] = {0, 0, 0};
private:
	int read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size);
	int prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size);
	int erase(lfs_block_t block);
	int wait(uint32_t microseconds);
	static int static_read(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, void *buffer, lfs_size_t size) {
		return ((OrganQSPIFlash *)(c->context))->read(block, offset, buffer, size);
	}
	static int static_prog(const struct lfs_config *c, lfs_block_t block,
	  lfs_off_t offset, const void *buffer, lfs_size_t size) {
		return ((OrganQSPIFlash *)(c->context))->prog(block, offset, buffer, size);
	}
	static int static_erase(const struct lfs_config *c, lfs_block_t block) {
		return ((OrganQSPIFlash *)(c->context))->erase(block);
	}
	static int static_sync(const struct lfs_config *c) {
		(void)c;
		return 0;
	}
	const void *hwinfo = nullptr;
};
#else
// Not a Teensy 4.x: there are no QSPI pads, so the mount always fails and the
// console reports no storage, exactly as the stock class does.
class OrganQSPIFlash : public LittleFS
{
public:
	OrganQSPIFlash() { }
	bool begin() { return false; }
	uint8_t lastJedecId[3] = {0, 0, 0};
};
#endif

#endif // ORGANCORE_ORGANQSPIFLASH_H
