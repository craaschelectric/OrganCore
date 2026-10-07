// OrganDiag.cpp  -  field diagnostics. See OrganDiag.h for what and why.

#include "OrganDiag.h"
#include "OrganCore.h"          // contract: DISPLAY_READBACK_ENABLED
#include "OrganStorage.h"       // organFS / organStorageMount()
#include "DisplayManager.h"     // displayReinit(), displayReady
#include <usb_dev.h>            // usb_configuration (Teensy core)
#include <stdio.h>
#include <string.h>

static const char*    DIAG_LOG_NAME  = "DIAG.LOG";
static const char*    DIAG_OLD_NAME  = "DIAG.OLD";
static const char*    DIAG_CNT_NAME  = "DIAG.CNT";
static const uint32_t DIAG_LOG_MAX_BYTES     = 4096;   // roll over to DIAG.OLD past this

bool     diagLinkLostNotice = false;
uint32_t diagBootNumber     = 0;

static bool     storageReady       = false;
static uint32_t resetCauseRegister = 0;     // SRC_SRSR as found at boot

static bool     usbMonitorArmed  = false;
static bool     usbWasConfigured = false;
static bool     usbEverConfigured = false;   // USB has completed enumeration at least once since boot
static uint32_t usbLostAtMs      = 0;

static bool     displayReinitPending  = false;
static bool     readbackNoticeLogged  = false; // DISPLAY_READBACK_ENABLED ignored: said so once

// ============================================================
// Logging
// ============================================================

void diagLog(const char* text) {
    uint32_t seconds = millis() / 1000;
    char line[96];
    snprintf(line, sizeof line, "B%lu +%02lu:%02lu:%02lu %s",
             (unsigned long)diagBootNumber,
             (unsigned long)(seconds / 3600), (unsigned long)((seconds / 60) % 60),
             (unsigned long)(seconds % 60), text);
    Serial.print("DIAG: ");
    Serial.println(line);

    if (!storageReady) return;

    // Roll over before the file gets big. Small files keep appends fast on QSPI
    // LittleFS, where seeking far into one large file is what made the old
    // single-file combination store so slow.
    if (organFS->exists(DIAG_LOG_NAME)) {
        File sizeCheck = organFS->open(DIAG_LOG_NAME, FILE_READ);
        uint32_t size = sizeCheck ? sizeCheck.size() : 0;
        if (sizeCheck) sizeCheck.close();
        if (size > DIAG_LOG_MAX_BYTES) {
            if (organFS->exists(DIAG_OLD_NAME)) organFS->remove(DIAG_OLD_NAME);
            organFS->rename(DIAG_LOG_NAME, DIAG_OLD_NAME);
        }
    }

    File logFile = organFS->open(DIAG_LOG_NAME, FILE_WRITE);   // FILE_WRITE appends
    if (!logFile) return;
    logFile.println(line);
    logFile.close();
}

// ============================================================
// Boot
// ============================================================

void diagInit() {
    // Read the reset cause first, before anything clears it, then clear it (the
    // register is write-1-to-clear and otherwise accumulates across resets).
    resetCauseRegister = SRC_SRSR;
    SRC_SRSR = resetCauseRegister;

    storageReady = organStorageMount();

    // Boot counter, in its own 4-byte file so it never depends on parsing the log.
    if (storageReady) {
        if (organFS->exists(DIAG_CNT_NAME)) {
            File countFile = organFS->open(DIAG_CNT_NAME, FILE_READ);
            if (countFile) {
                countFile.read((uint8_t*)&diagBootNumber, sizeof diagBootNumber);
                countFile.close();
            }
        }
        diagBootNumber++;
        File countFile = organFS->open(DIAG_CNT_NAME, FILE_WRITE_BEGIN);
        if (countFile) {
            countFile.seek(0);
            countFile.write((const uint8_t*)&diagBootNumber, sizeof diagBootNumber);
            countFile.close();
        }
    }

    // Reset cause, decoded. A brownout on the i.MX RT shows as a power-on reset;
    // there is no separate brownout flag. A firmware upload or software reboot
    // shows as lockup/software.
    char cause[80] = "";
    if (resetCauseRegister & (1u << 0)) strcat(cause, " power-on");
    if (resetCauseRegister & (1u << 1)) strcat(cause, " lockup/software");
    if (resetCauseRegister & (1u << 3)) strcat(cause, " user");
    if (resetCauseRegister & (1u << 4)) strcat(cause, " watchdog");
    if (resetCauseRegister & (1u << 7)) strcat(cause, " watchdog3");
    if (resetCauseRegister & (1u << 8)) strcat(cause, " over-temperature");
    if (resetCauseRegister & ((1u << 5) | (1u << 6))) strcat(cause, " JTAG");
    if (cause[0] == '\0') strcpy(cause, " unknown");

    char line[96];
    snprintf(line, sizeof line, "boot, reset:%s (SRSR 0x%03lX)", cause, (unsigned long)resetCauseRegister);
    diagLog(line);

    // If the previous run faulted, keep the full crash report in the log.
    if (CrashReport) {
        diagLog("previous run CRASHED - report follows");
        if (storageReady) {
            File logFile = organFS->open(DIAG_LOG_NAME, FILE_WRITE);
            if (logFile) {
                logFile.print(CrashReport);
                logFile.close();
            }
        }
    }

    if (!storageReady) Serial.println("DIAG: no storage -- logging to Serial only");
}

void diagBootComplete() {
    usbWasConfigured  = (usb_configuration != 0);
    usbEverConfigured = usbWasConfigured;
    usbMonitorArmed   = true;
}

// ============================================================
// Serial dump
// ============================================================

static void dumpFile(const char* name) {
    Serial.print("==== "); Serial.print(name); Serial.println(" ====");
    if (!storageReady || !organFS->exists(name)) { Serial.println("(none)"); return; }
    File f = organFS->open(name, FILE_READ);
    if (!f) { Serial.println("(unreadable)"); return; }
    uint8_t chunk[64];
    int n;
    while ((n = f.read(chunk, sizeof chunk)) > 0) Serial.write(chunk, n);
    f.close();
}

// ============================================================
// Poll
// ============================================================

void diagPoll(bool atRunScreen) {
    // ---- USB link monitor ----
    bool usbConfigured = (usb_configuration != 0);
    // 1.13.4: the first enumeration is not a recovery. setup() usually finishes
    // before the host has enumerated the Teensy, so diagBootComplete() records
    // "not configured"; the host completing enumeration a few seconds later was
    // then reported as a link restored after a loss that never happened (and the
    // "restored after" time was just the time since power-on). A loss or a
    // recovery now only counts once USB has been configured at least once.
    if (usbMonitorArmed && usbConfigured != usbWasConfigured) {
        if (!usbConfigured) {
            usbLostAtMs = millis();
            diagLog("USB link lost");
        } else if (usbEverConfigured) {
            char line[64];
            snprintf(line, sizeof line, "USB link restored after %lu ms",
                     (unsigned long)(millis() - usbLostAtMs));
            diagLog(line);
            diagLinkLostNotice   = true;   // the engine stopped listening; say so
            displayReinitPending = true;   // same dip that drops USB browns out the panel
        }
    }
    if (usbMonitorArmed && usbConfigured) usbEverConfigured = true;
    usbWasConfigured = usbConfigured;

    // ---- Serial dump on 'D' ----
    if (Serial.available()) {
        int c = Serial.read();
        if (c == 'D' || c == 'd') {
            dumpFile(DIAG_OLD_NAME);
            dumpFile(DIAG_LOG_NAME);
            Serial.println("==== end ====");
        }
    }

    if (!atRunScreen || !displayReady) return;

    // ---- Re-init requested by the USB monitor ----
    if (displayReinitPending) {
        displayReinitPending = false;
        displayReinit();
        diagLog("display re-initialized after USB link recovery");
        return;
    }

    // ---- Panel health readback: not supported ----
    // 1.13.0 asked the ILI9341 for its power-mode register every two seconds and
    // re-initialized a panel that reported itself asleep. That needs the ILI9341_t3
    // object, which TeensyUserInterface keeps private (the call was `ui.lcd->`,
    // and TeensyUserInterface has no such member), so it did not compile. Reading
    // the register any other way would mean driving the SPI bus underneath
    // TeensyUserInterface, so the periodic check is gone. DISPLAY_READBACK_ENABLED
    // stays in the contract so existing ConfigData files still link; if it is set
    // true it is ignored, and that is logged once. The re-initialization after a
    // USB link recovery (above) is unchanged.
    if (DISPLAY_READBACK_ENABLED && !readbackNoticeLogged) {
        readbackNoticeLogged = true;
        diagLog("DISPLAY_READBACK_ENABLED ignored: panel readback is not supported");
    }
}

// ============================================================
// Diagnostics screen support
// ============================================================

uint16_t diagReadLog(char* buffer, uint16_t bufferSize, char** lines, uint16_t maxLines) {
    buffer[0] = '\0';
    if (!storageReady || !organFS->exists(DIAG_LOG_NAME)) return 0;
    File f = organFS->open(DIAG_LOG_NAME, FILE_READ);
    if (!f) return 0;

    // Keep the tail: if the file is bigger than the buffer, skip its start.
    uint32_t size = f.size();
    if (size >= bufferSize) f.seek(size - (bufferSize - 1));
    int n = f.read((uint8_t*)buffer, bufferSize - 1);
    f.close();
    if (n < 0) n = 0;
    buffer[n] = '\0';

    // Split into lines; then keep only the last maxLines of them.
    char*    allLines[160];
    uint16_t total = 0;
    char*    start = buffer;
    for (char* p = buffer; *p; p++) {
        if (*p == '\r') *p = '\0';
        if (*p == '\n') {
            *p = '\0';
            if (start[0] != '\0' && total < 160) allLines[total++] = start;
            start = p + 1;
        }
    }
    if (start[0] != '\0' && total < 160) allLines[total++] = start;

    uint16_t first = (total > maxLines) ? (total - maxLines) : 0;
    uint16_t count = 0;
    for (uint16_t i = first; i < total; i++) lines[count++] = allLines[i];
    return count;
}
