#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9486/ili9486spi.h"
#include "roo_testing/devices/microcontroller/esp32/fake_esp32.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9486Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(),
        flexViewport(viewport, 1),
        display(flexViewport),
        touch(flexViewport, FakeXpt2046Spi::Calibration(300, 100, 3100, 2900,
                                                        false, true, false)) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(2, display.dc());
    FakeEsp32().gpio.attachOutput(4, display.rst());

    FakeEsp32().attachSpiDevice(touch, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(15, touch.cs());
  }
} emulator;

#endif

#include "SPI.h"
#include "roo_display.h"
#include "roo_display/driver/ili9486.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_display/shape/basic.h"

using namespace roo_display;

Ili9486spi<5, 2, 4> device;
TouchXpt2046<15> touch;

Display display(device, touch);

void calibrate(Display& display);

int16_t x, y;
bool was_touched;

Point SwapXY(Point p) {
  return Point{.x = p.y, .y = p.x};
}

void setup() {
  // SPI.begin(18, 19, 23, -1);
  SPI.begin();
  Serial.begin(9600);
  digitalWrite(12, HIGH);
  display.init(color::DarkGray);
  x = -1;
  y = -1;
  was_touched = false;

  calibrate(display);
}

struct CalibrationInput {
  Point tl;
  Point tr;
  Point bl;
  Point br;

  void swapXY() {
    tl = SwapXY(tl);
    tr = SwapXY(tr);
    bl = SwapXY(bl);
    br = SwapXY(br);
  }

  void flipHorizontally() {
    std::swap(tl, tr);
    std::swap(bl, br);
  }

  void flipVertically() {
    std::swap(tl, bl);
    std::swap(tr, br);
  }
};

void readCalibrationData(Display& display, int16_t x, int16_t y, Point& point) {
  {
    DrawingContext dc(display);
    dc.draw(FilledCircle::ByRadius(x, y, 2, color::Blue));
  }
  TouchPoint tp;
  int32_t sum_x = 0, sum_y = 0;
  for (int i = 0; i < 128; ++i) {
    while (touch.getTouch(&tp, 1).touch_points == 0) {}
    Serial.println("Registered!");
    sum_x += tp.x;
    sum_y += tp.y;
  }
  point.x = sum_x / 128;
  point.y = sum_y / 128;
  {
    DrawingContext dc(display);
    dc.fill(color::Green);
    dc.fill(color::DarkGray);
  }
  while (touch.getTouch(&tp, 1).touch_points != 0) {}
  delay(200);
}

void calibrate(Display& display) {
  //  touch_raw.setCalibration(TouchCalibration());
  int width = display.width();
  int height = display.height();
  CalibrationInput input;
  // Generally, the resolutions will almost certainly be divisible by 8, so
  // it is a good default choice for margins.
  Box calib_rect(width / 8, height / 8, width * 7 / 8 - 1, height * 7 / 8 - 1);
  Orientation orientation;
  bool swap_xy = false;
  do {
    readCalibrationData(display, calib_rect.xMin(), calib_rect.yMin(),
                        input.tl);
    readCalibrationData(display, calib_rect.xMax(), calib_rect.yMin(),
                        input.tr);
    readCalibrationData(display, calib_rect.xMax(), calib_rect.yMax(),
                        input.br);
    readCalibrationData(display, calib_rect.xMin(), calib_rect.yMax(),
                        input.bl);
    if (abs(input.tr.x - input.tl.x) > abs(input.tr.y - input.tl.y) &&
        abs(input.br.x - input.bl.x) > abs(input.br.y - input.bl.y) &&
        abs(input.bl.x - input.tl.x) < abs(input.bl.y - input.tl.y) &&
        abs(input.br.x - input.tr.x) < abs(input.br.y - input.tr.y)) {
      // Non-swapped orientation.
    } else if (abs(input.tr.x - input.tl.x) < abs(input.tr.y - input.tl.y) &&
               abs(input.br.x - input.bl.x) < abs(input.br.y - input.bl.y) &&
               abs(input.bl.x - input.tl.x) > abs(input.bl.y - input.tl.y) &&
               abs(input.br.x - input.tr.x) > abs(input.br.y - input.tr.y)) {
      input.swapXY();
      orientation = orientation.swapXY();
    } else {
      Serial.println("X vs Y readout is inconclusive.");
      continue;
    }

    if (input.tr.x > input.tl.x && input.br.x > input.bl.x) {
      // No flip.
    } else if (input.tr.x < input.tl.x && input.br.x < input.bl.x) {
      input.flipHorizontally();
      orientation = orientation.flipHorizontally();
    } else {
      Serial.println("Horizontal direction is inconclusive.");
      continue;
    }

    if (input.bl.y > input.tl.y && input.br.y > input.tr.y) {
      // No flip.
    } else if (input.bl.y < input.tl.y && input.br.y < input.tr.y) {
      input.flipVertically();
      orientation = orientation.flipVertically();
    } else {
      Serial.println("Vertical direction is inconclusive.");
      continue;
    }
  } while (false);

  int16_t xMin = (input.tl.x + input.bl.x) / 2;
  int16_t yMin = (input.tl.y + input.tr.y) / 2;
  int16_t xMax = (input.tr.x + input.br.x) / 2;
  int16_t yMax = (input.bl.y + input.br.y) / 2;

  // Now, these correspond to the calib_rect, which is 3/4 of the size
  // in each direction. We now need to compensate to the full screen.
  int16_t xborder = (xMax - xMin) / 6;
  int16_t yborder = (yMax - yMin) / 6;
  Serial.printf("Calibrated successfully: %d %d %d %d; direction: %s\n",
                xMin - xborder, yMin - yborder, xMax + xborder, yMax + yborder,
                orientation.asString());
  display.setTouchCalibration(TouchCalibration(xMin - xborder, yMin - yborder,
                                               xMax + xborder, yMax + yborder,
                                               orientation));

  // int16_t x_span =
  //     (abs(input.tr.x - input.tl.x) + abs(input.br.x - input.bl.x)) / 2;
  // int16_t y_span =
  //     (abs(input.bl.y - input.tl.y) + abs(input.br.y - input.tr.y)) / 2;
}

// int16_t old_x;

void loop(void) {
  int16_t old_x = x;
  int16_t old_y = y;
  unsigned long start = millis();
  bool touched = display.getTouch(x, y);
  if (touched) {
    was_touched = true;
    DrawingContext dc(display);
    dc.draw(Line(0, y, display.width() - 1, y, color::Red));
    dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
    if (x != old_x) {
      dc.draw(Line(old_x, 0, old_x, display.height() - 1, color::DarkGray));
    }
    if (y != old_y) {
      dc.draw(Line(0, old_y, display.width() - 1, old_y, color::DarkGray));
    }
    dc.draw(Line(0, y, display.width() - 1, y, color::Red));
    dc.draw(Line(x, 0, x, display.height() - 1, color::Red));

    Serial.println(String("Touched: ") + x + ", " + y);
    // tft.fillCircle(((uint32_t)(4096 - point.y) * 240) / 4096,
    //                ((uint32_t)point.x * 320) / 4096, point.z / 1000,
    // color);
    // tft.fillCircle(x, y, 4, color);
    // Serial.println(String("IRQ Touched: ") + point.x + ", " + point.y + ", "
    // + point.z);
  } else {
    if (was_touched) {
      Serial.println("Released!");
      was_touched = false;
      DrawingContext dc(display);
      dc.draw(Line(0, old_y, display.width() - 1, old_y, color::DarkGray));
      dc.draw(Line(old_x, 0, old_x, display.height() - 1, color::DarkGray));
    }
    // color = rand() & 0xFFFF;
    // color &= ~0x8410;
  }
}
