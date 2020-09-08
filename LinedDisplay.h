
#pragma once

#include "SSD1306.h"                    // OLED display library - Latest Git commit for clockspeed fixes
//                                        https://github.com/squix78/esp8266-oled-ssd1306
#include "OLEDDisplayFonts.h"             // additional fonts in separate file in lib

class LinedDisplay {

private:
  class Line {
  public:
    String text;
    OLEDDISPLAY_TEXT_ALIGNMENT alignment;
    const uint8_t* font;
    int font_height;

    Line() : text(""), alignment(TEXT_ALIGN_CENTER) {
      setFont(ArialMT_Plain_16);
    }

    void setFont(const uint8_t *newFont) {
      if (newFont != NULL) {
        font = newFont;
        font_height = font[HEIGHT_POS]; // Arial 10pt==13px, 16pt==19px, 24pt==28px
      }
    }
  };

  SSD1306 screen;
  const int16_t screen_rows = 64;
  const int16_t screen_columns = 128;
  Line *lines;

public:
  enum LineNumber { BANNER_LINE = 0, PHASE_LINE, MESSAGE_LINE, DATA_LINE, LINE_COUNT };

  LinedDisplay(int sdaPin, int sclPin) : screen(0x3c, sdaPin, sclPin, GEOMETRY_128_64, I2C_ONE, 100000) {
    lines = new Line[LINE_COUNT];
    // BANNER and PHASE are Arial 16, the Line default
    lines[MESSAGE_LINE].setFont(ArialMT_Plain_10);
    lines[DATA_LINE].setFont(ArialMT_Plain_10);
    // 19+19+13+13=64 leaves 64-64=0 rows remaining
  }

  ~LinedDisplay() {
    screen.end();
    delete[](lines);
  }

  void begin() {
    screen.init();
    screen.flipScreenVertically();       // display pins on top
    screen.setContrast(255);
  }

  void splash() {
    lines[BANNER_LINE].text = "GK-WiFi vMine";
    lines[PHASE_LINE].text = "Freshly booted!";
    lines[DATA_LINE].text = "Built: " __TIME__ " " __DATE__;
    paint();
  }

  void writeLine(LineNumber lineIdx, String& s) {
    if (lineIdx < 0 || LINE_COUNT <= lineIdx) {
      return;
    }

    lines[lineIdx].text = s;

    paint();
  }

  void paint() {
    screen.clear();
    int targetRow = 0;
    for (int lineIdx = 0; lineIdx < LINE_COUNT; lineIdx++) {
      Line *line = &lines[lineIdx];
      screen.setFont(line->font);
      screen.setTextAlignment(line->alignment);
      // TODO: handle different alignments
      screen.drawString(64, targetRow, line->text.c_str());
      targetRow += line->font_height;
    }
    screen.display();
  }

  void testPattern() {
    screen.setColor(WHITE);
    screen.clear();
    for (int16_t colIdx = 0; colIdx < screen_columns; colIdx++) {
      for (int16_t rowIdx = 0; rowIdx < screen_rows; rowIdx++) {
        if (colIdx % 2 == 0 && rowIdx % 2 == 0) {
          screen.setPixel(colIdx, rowIdx);
        }
      }
    }
    screen.display();
  }
};
