// SysExParser.h
// Parses Hauptwerk SysEx messages and extracts text for display lines.
// Each line is independently configured with its own LCD number and byte offset.

#ifndef SYSEX_PARSER_H
#define SYSEX_PARSER_H

#include "OrganCore.h"

// Extracted display text (null-terminated), updated on each matching SysEx
extern char displayLineText[MAX_DISPLAY_LINES][32];

// Set true when any line changes (cleared by display after rendering)
extern bool displayDirty;

// Parse a SysEx message. Returns true if it was a recognized Hauptwerk message.
bool parseSysEx(const uint8_t *data, uint16_t length);

#endif // SYSEX_PARSER_H
