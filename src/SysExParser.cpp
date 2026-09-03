// SysExParser.cpp

#include "SysExParser.h"
#include "MidiOut.h"
#include "Debug.h"
#include "TuningConfig.h"
#include <string.h>
#include "PitchManager.h"
#include <stdlib.h>   // atof
#include <math.h>     // lround

char displayLineText[MAX_DISPLAY_LINES][32];
bool displayDirty = false;

bool parseSysEx(const uint8_t *data, uint16_t length) {
    // Hauptwerk format: F0 7D 01 <lcd#> 00 <ascii...> F7
    // Teensy passes full message including F0 and F7.
    // Skip F0 prefix if present.
    if (length > 0 && data[0] == 0xF0) {
        data++;
        length--;
    }
    // Strip F7 suffix if present
    if (length > 0 && data[length - 1] == 0xF7) {
        length--;
    }

    // Hauptwerk format after stripping F0/F7:
    //   data[0]=7D  data[1]=01  data[2]=00  data[3]=<lcd#>  data[4]=00  data[5+]=<ascii>

    if (length < 6) return false;
    if (data[0] != HW_SYSEX_MFG_ID) return false;
    if (data[1] != HW_SYSEX_MSG_TYPE) return false;

    uint8_t lcdNum = data[3];
    bool matched = false;

    // GrandOrgue pitch report (pulse-feedback mode): the ASCII payload on
    // PITCH_SYSEX_LCD_NUM is a cents value like "1.0 cent" or "-2.0 cent". Parse
    // it and feed the pitch manager. PITCH_SYSEX_LCD_NUM must be distinct from
    // the displayLineLCD[] values so this isn't taken for a status line.
    if (ORGAN_TUNING_PRESENT && PITCH_PULSE_ENABLED && lcdNum == PITCH_SYSEX_LCD_NUM) {
        const uint8_t off = 5;                   // ASCII start (after 7D 01 00 lcd 00)
        if (off < length) {
            uint16_t n = length - off;
            if (n > 31) n = 31;
            char text[32];
            memcpy(text, &data[off], n);
            text[n] = '\0';

            int cents = (int)lround(atof(text));
            pitchManagerOnReportedOffset(cents);
            matched = true;

            Serial.print("DBG: SysEx pitch report \"");
            Serial.print(text);
            Serial.print("\" -> ");
            Serial.print(cents);
            Serial.println(" cents");
        }
        return matched;   // pitch LCD is not a display line; done
    }

    // Check each display line config to see if this LCD number matches
    for (uint8_t line = 0; line < NUM_DISPLAY_LINES; line++) {
        if (displayLineLCD[line] == 0xFF) continue;  // Line not configured
        if (lcdNum != displayLineLCD[line]) continue;
        
        // Extract text at the configured offset
        uint8_t start = displayLineOffset[line];
        uint8_t len = displayLineLen[line];
        
        if (start >= length) continue;
        if (start + len > length) len = length - start;
        if (len > 31) len = 31;
        
        // Copy and null-terminate
        char newText[32];
        memcpy(newText, &data[start], len);
        newText[len] = '\0';
        
        // Trim trailing spaces
        while (len > 0 && newText[len - 1] == ' ') {
            newText[--len] = '\0';
        }

        // Ignore empty messages entirely — don't store or trigger redraw
        if (len == 0) continue;
        
        // Only mark dirty if text actually changed
        if (strcmp(newText, displayLineText[line]) != 0) {
            memcpy(displayLineText[line], newText, len + 1);

            Serial.print("DBG: SysEx line ");
            Serial.print(line);
            Serial.print(" lcd=0x");
            if (lcdNum < 0x10) Serial.print('0');
            Serial.print(lcdNum, HEX);
            Serial.print(" text=\"");
            Serial.print(displayLineText[line]);
            Serial.print("\" len=");
            Serial.println(len);

            // Only trigger redraw if at least one line has content
            displayDirty = true;

            // Auto-save trigger: if the SAVE line starts with the trigger prefix,
            // send a SAVE button MIDI pulse
            if (line == SYSEX_SAVE_LINE_INDEX &&
                len >= strlen(SYSEX_SAVE_TRIGGER) &&
                memcmp(newText, SYSEX_SAVE_TRIGGER, strlen(SYSEX_SAVE_TRIGGER)) == 0) {
                midiOutNoteOn(SAVE_BUTTON_MIDI_NOTE, 127, SAVE_BUTTON_MIDI_CHANNEL + 1);
                midiOutNoteOff(SAVE_BUTTON_MIDI_NOTE, 0, SAVE_BUTTON_MIDI_CHANNEL + 1);
                Serial.println("DBG: SysEx SAVE trigger fired");
            }
        }
        
        matched = true;
    }
    
    return matched;
}
