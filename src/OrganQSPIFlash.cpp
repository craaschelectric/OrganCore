// OrganQSPIFlash.cpp  -  OrganCore's own LittleFS back-end for the Teensy 4.1
// QSPI NOR flash pads. See OrganQSPIFlash.h for why this exists.
//
// Everything below the PJRC notice is copied from LittleFS.cpp in Paul
// Stoffregen's LittleFS for Teensy (github.com/PaulStoffregen/LittleFS, main
// branch, October 2026): the known-chips table, the FlexSPI2 IP-command helpers,
// blockIsBlank(), and LittleFS_QSPIFlash's begin/read/prog/erase/wait/
// getMediaName, renamed to OrganQSPIFlash. Changes from the original:
//   - one row added to the table, the Boya BY25Q128ES (JEDEC 68 40 18). Its
//     timing values are the datasheet's worst case rounded up: page program
//     2.4 ms max -> 3000 us, 64 KB block erase 3 s max on the slower
//     temperature grade -> 3000000 us. Command set (9Fh ID, 50h/31h status
//     write, 6Bh quad read, 32h quad program, D8h 64 KB erase, 05h status) is
//     the same as the Winbond W25Q128JV the stock code was written for.
//   - the table and lookup are file-static here, separate from LittleFS's own.
// Upstream fixes to this code will NOT arrive here automatically; that is the
// accepted price of not depending on a patched copy of LittleFS.
//
// Original notice, retained as its licence requires:
/* LittleFS for Teensy
 * Copyright (c) 2020, Paul Stoffregen, paul@pjrc.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "OrganQSPIFlash.h"

#if defined(__IMXRT1062__)

PROGMEM static const struct qspiChipInfo {
	uint8_t id[3];
	uint8_t addrbits;	// number of address bits, 24 or 32
	uint16_t progsize;	// page size for programming, in bytes
	uint32_t erasesize;	// sector size for erasing, in bytes
	uint8_t  erasecmd;	// command to use for sector erase
	uint32_t chipsize;	// total number of bytes in the chip
	uint32_t progtime;	// maximum microseconds to wait for page programming
	uint32_t erasetime;	// maximum microseconds to wait for sector erase
	const char pn[22];		//flash name
} organKnownChips[] = {
{{0x68, 0x40, 0x18}, 24, 256, 65536, 0xD8, 16777216, 3000, 3000000, "BY25Q128ES"}, // Boya BY25Q128ES (OrganCore addition)
{{0xEF, 0x40, 0x15}, 24, 256, 32768, 0x52, 2097152, 3000, 1600000, "W25Q16JV-Q"},  // Winbond W25Q16JV*Q/W25Q16FV
{{0xEF, 0x40, 0x16}, 24, 256, 32768, 0x52, 4194304, 3000, 1600000, "W25Q32JV-Q"},  // Winbond W25Q32JV*Q/W25Q32FV
{{0xEF, 0x40, 0x17}, 24, 256, 65536, 0xD8, 8388608, 3000, 2000000, "W25Q64JV-Q"},  // Winbond W25Q64JV*Q/W25Q64FV
{{0xEF, 0x40, 0x18}, 24, 256, 65536, 0xD8, 16777216, 3000, 2000000, "W25Q128JV-Q"}, // Winbond W25Q128JV*Q/W25Q128FV
{{0xEF, 0x40, 0x19}, 32, 256, 65536, 0xDC, 33554432, 3000, 2000000, "W25Q256JV-Q"}, // Winbond W25Q256JV*Q
{{0xEF, 0x40, 0x20}, 32, 256, 65536, 0xDC, 67108864, 3500, 2000000, "W25Q512JV-Q"}, // Winbond W25Q512JV*Q
{{0xEF, 0x40, 0x21}, 32, 256, 65536, 0xDC, 134217728, 3500, 2000000, "W25Q01JV-Q"},// Winbond W25Q01JV*Q
{{0x62, 0x06, 0x13}, 24, 256,  4096, 0x20, 524288, 5000, 300000, "SST25PF040C"},  // Microchip SST25PF040C
//{{0xEF, 0x40, 0x14}, 24, 256,  4096, 0x20, 1048576, 5000, 300000, "W25Q80DV"},  // Winbond W25Q80DV  not tested
{{0xEF, 0x70, 0x17}, 24, 256, 65536, 0xD8, 8388608, 3000, 2000000, "W25Q64JV-M"},  // Winbond W25Q64JV*M (DTR)
{{0xEF, 0x70, 0x18}, 24, 256, 65536, 0xD8, 16777216, 3000, 2000000, "W25Q128JV-M"}, // Winbond W25Q128JV*M (DTR)
{{0xEF, 0x70, 0x19}, 32, 256, 65536, 0xDC, 33554432, 3000, 2000000, "W25Q256JV-M"}, // Winbond W25Q256JV*M (DTR)
{{0xEF, 0x80, 0x19}, 32, 256, 65536, 0xDC, 33554432, 3000, 2000000, "W25Q256JW-M"}, // Winbond (W25Q256JW*M)
{{0xEF, 0x70, 0x20}, 32, 256, 65536, 0xDC, 67108864, 3500, 2000000, "W25Q512JV-M"}, // Winbond W25Q512JV*M (DTR)
{{0x1F, 0x84, 0x01}, 24, 256,  4096, 0x20, 524288, 2500, 300000, "AT25SF041"},    // Adesto/Atmel AT25SF041
{{0x01, 0x40, 0x14}, 24, 256,  4096, 0x20, 1048576, 5000, 300000, "S25FL208K"},   // Spansion S25FL208K
{{0xC8, 0x40, 0x13}, 24, 256,  4096, 0x20,  524288, 2400, 300000, "GD25Q40C"},   // GigaDevice GD25Q40C
{{0xC8, 0x40, 0x14}, 24, 256,  4096, 0x20, 1048576, 2400, 300000, "GD25Q80C"},   // GigaDevice GD25Q80C
{{0xC8, 0x40, 0x15}, 24, 256, 32768, 0x52, 2097152, 4000, 1600000, "GD25Q16E"},  // GigaDevice GD25Q16E
{{0xC8, 0x40, 0x16}, 24, 256, 32768, 0x52, 4194304, 4000, 1600000, "GD25Q32E"},  // GigaDevice GD25Q32E
{{0xC8, 0x40, 0x17}, 24, 256, 65536, 0xD8, 8388608, 4000, 3000000, "GD25Q64E"},  // GigaDevice GD25Q64E
{{0xC8, 0x40, 0x18}, 24, 256, 65536, 0xD8, 16777216, 4000, 3000000, "GD25Q128E"},  // GigaDevice GD25Q128E
{{0xC8, 0x40, 0x19}, 32, 256, 65536, 0xDC, 33554432, 2000, 1600000, "GD25Q256E"},  // GigaDevice GD25Q256E
//FRAM
{{0x03, 0x2E, 0xC2}, 24, 64, 128, 0, 1048576, 250, 1200, "CY15B108QN"}, //Cypress 8Mb FRAM, CY15B108QN
{{0xC2, 0x24, 0x00}, 24, 64, 128, 0, 131072, 250, 1200, "FM25V10-G"},  //Cypress 1Mb FRAM, FM25V10-G
{{0xC2, 0x24, 0x01}, 24, 64, 128, 0, 131072, 250, 1200, "FM25V10-G (rev 1)"},  //Cypress 1Mb FRAM, rev1
{{0xAE, 0x83, 0x09}, 24, 64, 128, 0, 131072, 250, 1200, "MR45V100A"},  //ROHM MR45V100A 1 Mbit FeRAM Memory
{{0xC2, 0x26, 0x08}, 24, 64, 128, 0, 524288, 250, 1200, "CY15B104Q"},  //Cypress 4Mb FRAM, CY15B104Q
{{0x60, 0x2A, 0xC2}, 24, 64, 128, 0, 262144, 250, 1200, "CY15B102Q"},  //Cypress 2Mb FRAM, CY15B102Q
{{0x60, 0x2A, 0xC2}, 24, 64, 128, 0, 262144, 250, 1200, "CY15B102Q"},  //Cypress 2Mb FRAM, CY15B102Q
{{0x04, 0x7F, 0x48}, 24, 64, 128, 0, 262144, 250, 1200, "MB85RS2MTAPNF"},  //Fujitsu 2Mb FRAM, MB85RS2MTAPNF
{{0x04, 0x7F, 0x49}, 24, 64, 128, 0, 524288, 250, 1200, "MB85RS4MT"},  //Fujitsu 4Mb FRAM, MB85RS2MT
};

static const struct qspiChipInfo * qspiChipLookup(const uint8_t *id)
{
	const unsigned int numchips = sizeof(organKnownChips) / sizeof(struct qspiChipInfo);
	for (unsigned int i=0; i < numchips; i++) {
		const uint8_t *chip = organKnownChips[i].id;
		if (id[0] == chip[0] && id[1] == chip[1] && id[2] == chip[2]) {
			return organKnownChips + i;
		}
	}
	return nullptr;
}

static bool blockIsBlank(struct lfs_config *config, lfs_block_t block, void *readBuf, bool full = true)
{
	if (!readBuf) return false;
	for (lfs_off_t offset=0; offset < config->block_size; offset += config->read_size) {
		memset(readBuf, 0, config->read_size);
		config->read(config, block, offset, readBuf, config->read_size);
		const uint8_t *buf = (uint8_t *)readBuf;
		for (unsigned int i=0; i < config->read_size; i++) {
			if (buf[i] != 0xFF) return false;
		}
		if ( !full )
			return true; // first bytes read as 0xFF
	}
	return true; // all bytes read as 0xFF
}

#define LUT0(opcode, pads, operand) (FLEXSPI_LUT_INSTRUCTION((opcode), (pads), (operand)))
#define LUT1(opcode, pads, operand) (FLEXSPI_LUT_INSTRUCTION((opcode), (pads), (operand)) << 16)
#define CMD_SDR         FLEXSPI_LUT_OPCODE_CMD_SDR
#define ADDR_SDR        FLEXSPI_LUT_OPCODE_RADDR_SDR
#define READ_SDR        FLEXSPI_LUT_OPCODE_READ_SDR
#define WRITE_SDR       FLEXSPI_LUT_OPCODE_WRITE_SDR
#define DUMMY_SDR       FLEXSPI_LUT_OPCODE_DUMMY_SDR
#define PINS1           FLEXSPI_LUT_NUM_PADS_1
#define PINS4           FLEXSPI_LUT_NUM_PADS_4

static void flexspi2_ip_command(uint32_t index, uint32_t addr)
{
	uint32_t n;
	const uint32_t addr_offset = (FLEXSPI2_FLSHA1CR0 & 0x7FFFFF) << 10;
	FLEXSPI2_IPCR0 = addr + addr_offset;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index);
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
	while (!((n = FLEXSPI2_INTR) & FLEXSPI_INTR_IPCMDDONE)); // wait
	if (n & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

static void flexspi2_ip_read(uint32_t index, uint32_t addr, void *data, uint32_t length)
{
	uint8_t *p = (uint8_t *)data;

	FLEXSPI2_INTR = FLEXSPI_INTR_IPRXWA;
	// Clear RX FIFO and set watermark to 16 bytes
	FLEXSPI2_IPRXFCR = FLEXSPI_IPRXFCR_CLRIPRXF | FLEXSPI_IPRXFCR_RXWMRK(1);
	const uint32_t addr_offset = (FLEXSPI2_FLSHA1CR0 & 0x7FFFFF) << 10;
	FLEXSPI2_IPCR0 = addr + addr_offset;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index) | FLEXSPI_IPCR1_IDATSZ(length);
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
// page 1649 : Reading Data from IP RX FIFO
// page 1706 : Interrupt Register (INTR)
// page 1723 : IP RX FIFO Control Register (IPRXFCR)
// page 1732 : IP RX FIFO Status Register (IPRXFSTS)

	while (1) {
		if (length >= 16) {
			if (FLEXSPI2_INTR & FLEXSPI_INTR_IPRXWA) {
				volatile uint32_t *fifo = &FLEXSPI2_RFDR0;
				uint32_t a = *fifo++;
				uint32_t b = *fifo++;
				uint32_t c = *fifo++;
				uint32_t d = *fifo++;
				*(uint32_t *)(p+0) = a;
				*(uint32_t *)(p+4) = b;
				*(uint32_t *)(p+8) = c;
				*(uint32_t *)(p+12) = d;
				p += 16;
				length -= 16;
				FLEXSPI2_INTR = FLEXSPI_INTR_IPRXWA;
			}
		} else if (length > 0) {
			if ((FLEXSPI2_IPRXFSTS & 0xFF) >= ((length + 7) >> 3)) {
				volatile uint32_t *fifo = &FLEXSPI2_RFDR0;
				while (length >= 4) {
					*(uint32_t *)(p) = *fifo++;
					p += 4;
					length -= 4;
				}
				uint32_t a = *fifo;
				if (length >= 1) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				if (length >= 2) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				if (length >= 3) {
					*p++ = a & 0xFF;
					a = a >> 8;
				}
				length = 0;
			}
		} else {
			if (FLEXSPI2_INTR & FLEXSPI_INTR_IPCMDDONE) break;
		}
		// TODO: timeout...
	}
	if (FLEXSPI2_INTR & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

static void flexspi2_ip_write(uint32_t index, uint32_t addr, const void *data, uint32_t length)
{
	const uint8_t *src;
	uint32_t n, wrlen;

	const uint32_t addr_offset = (FLEXSPI2_FLSHA1CR0 & 0x7FFFFF) << 10;
	FLEXSPI2_IPCR0 = addr + addr_offset;
	FLEXSPI2_IPCR1 = FLEXSPI_IPCR1_ISEQID(index) | FLEXSPI_IPCR1_IDATSZ(length);
	src = (const uint8_t *)data;
	FLEXSPI2_IPCMD = FLEXSPI_IPCMD_TRG;
	while (!((n = FLEXSPI2_INTR) & FLEXSPI_INTR_IPCMDDONE)) {
		if (n & FLEXSPI_INTR_IPTXWE) {
			wrlen = length;
			if (wrlen > 8) wrlen = 8;
			if (wrlen > 0) {
				//Serial.print("%");
				memcpy((void *)&FLEXSPI2_TFDR0, src, wrlen);
				src += wrlen;
				length -= wrlen;
				FLEXSPI2_INTR = FLEXSPI_INTR_IPTXWE;
			}
		}
	}
	if (n & FLEXSPI_INTR_IPCMDERR) {
		FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDERR;
		//Serial.printf("Error: FLEXSPI2_IPRXFSTS=%08lX\r\n", FLEXSPI2_IPRXFSTS);
	}
	FLEXSPI2_INTR = FLEXSPI_INTR_IPCMDDONE;
}

FLASHMEM
bool OrganQSPIFlash::begin()
{
	// Workaround for strange compatibility problem with Wire (and likely other libs)
	// https://github.com/PaulStoffregen/LittleFS/issues/63
	if (Serial) ;

	configured = false;

	uint8_t buf[4] = {0, 0, 0, 0};

	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;
	// cmd index 8 = read ID bytes
	FLEXSPI2_LUT32 = LUT0(CMD_SDR, PINS1, 0x9F) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT33 = 0;

	flexspi2_ip_read(8, 0, buf, 3);


	//Serial.printf("Flash ID: %02X %02X %02X\n", buf[0], buf[1], buf[2]);
	const struct qspiChipInfo *info = qspiChipLookup(buf);
	if (!info) {
		lastJedecId[0] = buf[0]; lastJedecId[1] = buf[1]; lastJedecId[2] = buf[2];
		return false;
	}
	lastJedecId[0] = buf[0]; lastJedecId[1] = buf[1]; lastJedecId[2] = buf[2];
	hwinfo = info;
	//Serial.printf("Flash size is %.2f Mbyte\n", (float)info->chipsize / 1048576.0f);

	memset(&lfs, 0, sizeof(lfs));
	memset(&config, 0, sizeof(config));
	config.context = (void *)this;
	config.read = &static_read;
	config.prog = &static_prog;
	config.erase = &static_erase;
	config.sync = &static_sync;
	config.read_size = info->progsize;
	config.prog_size = info->progsize;
	config.block_size = info->erasesize;
	config.block_count = info->chipsize / info->erasesize;
	config.block_cycles = 400;
	config.cache_size = info->progsize;
	config.lookahead_size = info->progsize;
	//config.lookahead_size = config.block_count/8;
	config.name_max = LFS_NAME_MAX;
	configured = true;

	// configure FlexSPI2 for chip's size
	FLEXSPI2_FLSHA2CR0 = info->chipsize / 1024;

	FLEXSPI2_LUTKEY = FLEXSPI_LUTKEY_VALUE;
	FLEXSPI2_LUTCR = FLEXSPI_LUTCR_UNLOCK;

	// TODO: is this Winbond specific?  Diable for non-Winbond chips...
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x50);
	flexspi2_ip_command(10, 0); // volatile write status enable
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x31) | LUT1(CMD_SDR, PINS1, 0x02);
	FLEXSPI2_LUT41 = 0;
	flexspi2_ip_command(10, 0); // enable quad mode

	if (info->addrbits == 24) {
		// cmd index 9 = read QSPI (1-1-4)
		FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x6B) | LUT1(ADDR_SDR, PINS1, 24);
		FLEXSPI2_LUT37 = LUT0(DUMMY_SDR, PINS4, 8) |  LUT1(READ_SDR, PINS4, 1);
		FLEXSPI2_LUT38 = 0;
		// cmd index 11 = program QSPI (1-1-4)
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x32) | LUT1(ADDR_SDR, PINS1, 24);
		FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS4, 1);
		// cmd index 12 = sector erase
		FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, info->erasecmd) | LUT1(ADDR_SDR, PINS1, 24);
		FLEXSPI2_LUT49 = 0;
	} else {
		// cmd index 9 = read QSPI (1-1-4)
		FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x6C) | LUT1(ADDR_SDR, PINS1, 32);
		FLEXSPI2_LUT37 = LUT0(DUMMY_SDR, PINS4, 8) |  LUT1(READ_SDR, PINS4, 1);
		FLEXSPI2_LUT38 = 0;
		// cmd index 11 = program QSPI (1-1-4)
		FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x34) | LUT1(ADDR_SDR, PINS1, 32);
		FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS4, 1);
		// cmd index 12 = sector erase
		FLEXSPI2_LUT48 = LUT0(CMD_SDR, PINS1, info->erasecmd) | LUT1(ADDR_SDR, PINS1, 32);
		FLEXSPI2_LUT49 = 0;
		// cmd index 9 = read SPI (1-1-1)
		//FLEXSPI2_LUT36 = LUT0(CMD_SDR, PINS1, 0x13) | LUT1(ADDR_SDR, PINS1, 32);
		//FLEXSPI2_LUT37 = LUT0(READ_SDR, PINS1, 1);
		// cmd index 11 = program SPI (1-1-1)
		//FLEXSPI2_LUT44 = LUT0(CMD_SDR, PINS1, 0x12) | LUT1(ADDR_SDR, PINS1, 32);
		//FLEXSPI2_LUT45 = LUT0(WRITE_SDR, PINS1, 1);
	}
	// cmd index 10 = write enable
	FLEXSPI2_LUT40 = LUT0(CMD_SDR, PINS1, 0x06);
	// cmd index 13 = get status
	FLEXSPI2_LUT52 = LUT0(CMD_SDR, PINS1, 0x05) | LUT1(READ_SDR, PINS1, 1);
	FLEXSPI2_LUT53 = 0;


	//Serial.println("attempting to mount existing media");
	if (lfs_mount(&lfs, &config) < 0) {
		//Serial.println("couldn't mount media, attemping to format");
		if (lfs_format(&lfs, &config) < 0) {
			//Serial.println("format failed :(");
			return false;
		}
		//Serial.println("attempting to mount freshly formatted media");
		if (lfs_mount(&lfs, &config) < 0) {
			//Serial.println("mount after format failed :(");
			return false;
		}
	}
	mounted = true;
	//Serial.println("success");
	return true;
}

int OrganQSPIFlash::read(lfs_block_t block, lfs_off_t offset, void *buf, lfs_size_t size)
{
	const uint32_t addr = block * config.block_size + offset;
	flexspi2_ip_read(9, addr, buf, size);
	// TODO: detect errors, return LFS_ERR_IO
	//printtbuf(buf, 20);
	return 0;
}

int OrganQSPIFlash::prog(lfs_block_t block, lfs_off_t offset, const void *buf, lfs_size_t size)
{
	flexspi2_ip_command(10, 0);
	const uint32_t addr = block * config.block_size + offset;
	//printtbuf(buf, 20);
	flexspi2_ip_write(11, addr, buf, size);
	// TODO: detect errors, return LFS_ERR_IO
	const uint32_t progtime = ((const struct qspiChipInfo *)hwinfo)->progtime;
	return wait(progtime);
}

int OrganQSPIFlash::erase(lfs_block_t block)
{
	void *buffer = malloc(config.read_size);
	if ( buffer != nullptr) {
		if ( blockIsBlank(&config, block, buffer)) {
			free(buffer);
			return 0; // Already formatted exit no wait
		}
		free(buffer);
	}
	flexspi2_ip_command(10, 0);
	const uint32_t addr = block * config.block_size;
	flexspi2_ip_command(12, addr);
	// TODO: detect errors, return LFS_ERR_IO
	const uint32_t erasetime = ((const struct qspiChipInfo *)hwinfo)->erasetime;
	return wait(erasetime);
}

int OrganQSPIFlash::wait(uint32_t microseconds)
{
	elapsedMicros usec = 0;
	while (1) {
		uint8_t status;
		flexspi2_ip_read(13, 0, &status, 1);
		if (!(status & 1)) break;
		if (usec > microseconds) return LFS_ERR_IO; // timeout
		yield();
	}
	//Serial.printf("  waited %u us\n", (unsigned int)usec);
	return 0; // success
}


FLASHMEM
const char * OrganQSPIFlash::getMediaName(){
	if (!hwinfo) return nullptr;
	return ((const struct qspiChipInfo *)hwinfo)->pn;
}

#endif // __IMXRT1062__
