// AboutScreen.cpp  -  see AboutScreen.h.

#include "AboutScreen.h"
#include "Display.h"        // ui, fonts

#include <stdio.h>
#include <string.h>

void aboutScreenPaint() {
    const int textLeft         = ui.displaySpaceLeftX + 6;
    const int versionValueLeft = textLeft + 90;
    const int versionLineHeight     = 22;
    const int descriptionLineHeight = 18;
    const int maxDescriptionLines   = 4;
    const size_t maxCharsPerDescriptionLine = 44;   // what fits across the panel at Arial 9

    ui.drawTitleBar("About");
    ui.clearDisplaySpace();

    int lineY = ui.displaySpaceTopY + 8;

    ui.lcdSetFont(Arial_10_Bold);

    ui.lcdSetFontColor(LCD_LIGHTGREY);
    ui.lcdSetCursorXY(textLeft, lineY);
    ui.lcdPrint("OrganCore");
    ui.lcdSetFontColor(LCD_WHITE);
    ui.lcdSetCursorXY(versionValueLeft, lineY);
    ui.lcdPrint(ORGANCORE_VERSION);
    lineY += versionLineHeight;

    ui.lcdSetFontColor(LCD_LIGHTGREY);
    ui.lcdSetCursorXY(textLeft, lineY);
    ui.lcdPrint("Firmware");
    ui.lcdSetFontColor(LCD_WHITE);
    ui.lcdSetCursorXY(versionValueLeft, lineY);
    ui.lcdPrint(FIRMWARE_VERSION);
    lineY += versionLineHeight + 14;   // a gap before the description

    // The description: '\n' starts a new line, and a line longer than the panel
    // wraps by character at the screen edge. Anything past the fourth line is
    // not shown.
    ui.lcdSetFont(Arial_9_Bold);
    ui.lcdSetFontColor(LCD_WHITE);

    char descriptionLine[maxCharsPerDescriptionLine + 1];
    const char* descriptionCursor = ABOUT_DESCRIPTION;
    int descriptionLinesShown = 0;

    while (*descriptionCursor != '\0' && descriptionLinesShown < maxDescriptionLines) {
        const char* newlinePosition = strchr(descriptionCursor, '\n');
        size_t lineLength = (newlinePosition != NULL) ? (size_t)(newlinePosition - descriptionCursor)
                                                      : strlen(descriptionCursor);
        if (lineLength > maxCharsPerDescriptionLine) lineLength = maxCharsPerDescriptionLine;

        memcpy(descriptionLine, descriptionCursor, lineLength);
        descriptionLine[lineLength] = '\0';

        ui.lcdSetCursorXY(textLeft, lineY);
        ui.lcdPrint(descriptionLine);
        lineY += descriptionLineHeight;
        descriptionLinesShown++;

        descriptionCursor += lineLength;
        if (*descriptionCursor == '\n') descriptionCursor++;   // consume the break that ended this line
    }
}
