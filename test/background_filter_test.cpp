
#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/filter/background.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

static const char mask[] =
    "                "
    "   1234321      "
    "  123454321     "
    " 12345654321    "
    "  345676543     "
    "   5678765      "
    "                ";

class SimpleRoundBg {
 public:
  SimpleRoundBg(Box extents) {}
  template <typename ColorMode>
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    Color bgcolor = color::Transparent;
    if (Box(1, 2, 16, 8).contains(x, y)) {
      const char bg = mask[x - 1 + (y - 2) * 16];
      if (bg != ' ') {
        uint8_t grey = (bg - '0') * 0x11;
        bgcolor = Color(grey, grey, grey);
      }
    }
    offscreen->writePixel(mode, x, y, AlphaBlend(bgcolor, color));
  }

  static DisplayOutput* Create(DisplayOutput& output, Box extents) {
    Box bounds(1, 2, 16, 8);
    static auto raster = MakeRasterizable(
        bounds,
        [bounds](int16_t x, int16_t y) -> Color {
          EXPECT_TRUE(bounds.contains(x, y))
              << "Out-of-bounds read: (" << x << ", " << y
              << "), while bounds = " << bounds;
          const char bg = mask[x - 1 + (y - 2) * 16];
          if (bg == ' ') return color::Transparent;
          uint8_t grey = (bg - '0') * 0x11;
          return Color(grey, grey, grey);
        },
        TRANSPARENCY_BINARY);
    return new BackgroundFilter(output, &raster);
  }
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleRoundBg> RefDeviceSimple;
typedef FilteredOutput<Grayscale4, SimpleRoundBg> TestDeviceSimple;

TEST(Background, SimpleTests) {
  TestFillRects<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                   Orientation());
  TestFillHLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());
  TestFillVLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());
  TestFillDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
  TestFillPixels<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());

  TestWriteRects<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());
  TestWriteHLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                     Orientation());
  TestWriteVLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                     Orientation());
  TestWriteDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
  TestWritePixels<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                     Orientation());
  TestWritePixelsSnake<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                          Orientation());
  TestWriteRectWindowSimple<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
}

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                           Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
}

}  // namespace roo_display
