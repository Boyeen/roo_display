# roo_display

This is work in progress. Please come back soon for updated content!

## Fundamentals

### Connecting your display

This step is necessary if your display and your microcontroller are separate devices. In this case, you need to connect them together.

SPI displays are common. For these, you need to connect at least 4 signals: SPI SCK, SPI MOSI, display CS (chip select), and display DC (data/command). With touch displays, you will need to connect at least two additional signals, say SPI MISO and touch CS in case of a SPI-based touch device. Additionally, you may need to connect a BL signal (backlit, used to control brightness), and a RST signal (hardware reset of the display). BL may usually be connected directly to VCC if you don't plan to adjust brightness programmatically.

For modern microcontrollers such as ESP32, you can use pretty much any digital I/O pins to connect these signals. If you can, use default SPI pins; it will make configuration a bit simpler.

### Basic setup

The minimalistic GUI application consists of the following basic steps:

* Instantiate a display driver for your hardware
* Instantiate the main display object
* Initialize the transport (e.g. SPI)
* Initialize the display: set parameters such like screen orientation, background color; clear the screen.

For example, a basic app that uses an ILI 9341 SPI display may look like this:

```cpp
#include "Arduino.h"

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"

using namespace roo_display;

Ili9341spi<5, 2, 4> device(Orientation().rotateLeft());
Display display(device);

void setup() {
  // Use default SPI pins, or specify your own.
  SPI.begin();

  // Set the background color, and clear the screen.
  display.init(color::White);
}

void loop() {}
```

This application configures an ILI 9341 display, using the default SPI interface with default pins (on ESP32, these are 18 for SCK, 23 for MOSI, and 19 for MISO), as well as GPIO 5 for CS, GPIO 2 for DC, and GPIO 4 for RST. The screen is left-rotated, i.e. in a 320x240 portrait mode.

We will now look at the steps in more detail.

#### Device drivers

All builtin drivers can be found in the ```roo_display/driver``` subdirectory. Look near the end of the header file to find the class name to use.

SPI drivers are generally class templates looking like this:

```cpp
template<pinCS, pinDC, pinRST>
class XXXspi { ... };
```

The template arguments allow you to specify pins that connect to your display's
CS, DC and RST signals. The driver may accept additional template parameters, e.g. overriding the default SPI settings. See the driver's comments for more details.

When you instantiate the SPI driver without any additional parameters, it will assume the default SPI interface. If you need to override that interface, you can do so by specifying one in the constructor:

```cpp
decltype(SPI) vspi(VSPI);

Ili9341spi<5, 2, 4> device(vspi);
Display display(device);

void setup() {
  vspi.begin();
}
```

#### Transport

As the examples above illustrate, you need to manually initialize the transport before doing anything else with the display. In this case, since we are using SPI, we need to explicitly call ```SPI.init()```.

By separating SPI initialization from display initialization, ```roo_display``` naturally supports complex configurations, e.g. connecting multiple displays, along with other SPI devices, to the same shared SPI bus.

#### Screen orientation

Display devices tend to support 8 different orientations. These result from a combination of 3 boolean flags: flipping along the horizontal axis, flipping along the vertical axis, and swapping the axes. Equivalently, the same 8 orientations can be considered to be a combination of 4 rotations (0, 90, 180, and 270 degrees) with a possible mirror flip along the X direction.

The ```Orientation``` class allows you to construct the desired orientation intuitively, e.g. as a combination of rotations and flips. In simple cases, a simple ```rotateLeft()```, ```rotateRight```, or ```rotateUpsideDown()``` will suffice, but the other methods may come handy in case you need to change the orientation dynamically (e.g. flip the display or rotate it).

Internally, the orientation object is represented by just a single byte, and it should be passed by value.

#### The display object

The display object encapsulates the driver, and it is the 'root' of your future interactions with the library. Post-initialization, you use it primarily to create drawing contexts, as described below. You may also use it to change the display orientation, and global settings such as the main clipping rectangle, a global background, or touch calibration settings. It is also used for retrieving touch input. These features will be described in later sections.

### DrawingContext and drawables

To draw an object, it must implement the ```Drawable``` interface:

```cpp
class Drawable {
 public:

  // Returns the bounding box encompassing all pixels that need to be drawn.
  virtual Box extents() const = 0;

  // Returns the boundaries to be used when aligning this drawable.
  virtual Box anchorExtents() const { return extents(); }

 private:
  virtual void drawTo(const Surface &s) const;
};
```

Later, you will see how to implement your own drawables. We will then dive into the Drawable interface (particularly, the ```drawTo()``` method) in more detail. For now, we will draw common, existing drawables. The point to take home, though, is the set of drawable primitives is open-ended. You are not restricted to a set of predefined primitives. You can roll your own drawables that will be rendered as efficiently as the built-ins, and they will work seamlessly with all the features of the library (clipping, transformations, fliters, etc.)

So, how do we actually draw a drawable?

To do so, we need to do one more step, which is to create a drawing context:

```cpp
DrawingContext dc(display);
dc.draw(drawable);
```

Drawing contexts further 'virtualize' the display, and allow tweaks that we will soon dive into. Drawing contexts keep SPI transactions alive, so they should be transient, i.e. destroyed quickly after you are done with the drawing.

### Drawing basic geometry

Let us put all of this together do draw some basic shapes:

```cpp
#include "Arduino.h"

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic_shapes.h"

using namespace roo_display;

Ili9341spi<5, 2, 4> device(Orientation().rotateLeft());
Display display(device);

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void loop() {
  DrawingContext dc(display);
  // Top-left corner at (20,20) and a diameter of 160.
  dc.draw(FilledCircle::ByExtents(20, 20, 160, color::Red));
  // Filled rectangle with corners at (10,15) and (80,120).
  dc.draw(FilledRect(10, 15, 80, 120, color::Orange));
  // Line from (10,10) to (120,80).
  dc.draw(Line(10, 10, 120, 80, color::Blue));

  delay(10000);
}
```

![img1](doc/images/img1.png)

### Color

The `roo_display` library internally uses 32-bit ARGB color. Most SPI displays use RGB565, with 16 bits per pixel. The conversion from the 32-bit ARGB to the device color is performed in the driver - that is, if your driver is capable of handling 24- or 32-bit color, you will get it without any loss of quality, but in most cases, your driver will downgrade to RGB565.

The 'A' in ARGB stands for 'alpha', which means 'translucency'. The `roo_display` library supports translucent color modes with alpha-blending. Most displays do not support transparency directly, so we will explore these color modes more in the section on offscreen drawing and overlays.

The color is represented by the ```Color``` class. It simply encapsulates the uint32_t integer, but provides convenience methods to simplify common operations.

You can specify regular, opaque RGB colors by calling the 3-argument constructor:

```cpp
Color c(0xF0, 0xC0, 0xC0);
```

You can also set each channel individually:

```cpp
c.set_r(0x30);
c.set_a(0x43);
```

You can also set a color directly from the uint32_t ARGB representation:

```cpp
c = Color(0xF0F0F0F0);
```

Additionally, the library defines 140 named [HTML color constants](https://www.w3schools.com/colors/colors_groups.asp):

```cpp
c = color::LightGoldenRodYellow;
```

as well as a convenience function to specify a shade of gray:

```cpp
c = Graylevel(0x40);  // Dark gray.
```

### Drawing simple text

To draw text, you need to pick a font, and use a ```TextLabel```:

```cpp
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSans_Regular/27.h"

/// ...

void loop() {
  DrawingContext dc(display);
  dc.draw(
    TextLabel(
        "Hello, World! μΩ ₿ ķ",
        font_NotoSans_Regular_27(),
        Color(0xFF404080)), 
    5, 30);
}
```

The 'origin' of the text label is at the baseline and to the left of the text. We need to shift it a little (applying the 5,30 offset) so that it fits on the screen with some margins.

![img2](doc/images/img2.png)

### Backgrounds and overwriting

By default, `roo_display` draws the minimum possible number of pixels:

```cpp
/// ...

void loop() {
  DrawingContext dc(display);
  dc.draw(
    TextLabel("12:00", font_NotoSans_Regular_27(), color::Blue),
    5, 30);
  dc.draw(
    TextLabel("21:41", font_NotoSans_Regular_27(), color::Red),
    5, 30);

  delay(10000);
}
```

![img3](doc/images/img3.png)

Often, you need to overwrite previous content. In order to do so without flicker (which would have occured if you simply cleared the background), you can tell the drawing context to fill the entire extents rectangle, returned by `object.extents()`, when drawing an `object`. You can do so by setting `fill mode` to `FILL_MODE_RECTANGLE` (replacing the default, `FILL_MODE_VISIBLE`):

```cpp
/// ...

void loop() {
  DrawingContext dc(display);
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(
    TextLabel("12:00", font_NotoSans_Regular_27(), color::Blue),
    5, 30);
  dc.draw(
    TextLabel("21:41", font_NotoSans_Regular_27(), color::Red),
    5, 30);

  delay(10000);
}
```

![img4](doc/images/img4.png)

So far, we have been using the default background color, which we had set in the call to `device.init()`. That default background may be overriden in the drawing context:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.setBackgroundColor(color::Pink);
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(
    TextLabel(
        "Hello, World!",
        font_NotoSans_Regular_27(),
        color::Black),
    15, 35);
  dc.setFillMode(FILL_MODE_VISIBLE);
  dc.draw(
    TextLabel(
        "Hello, World!",
        font_NotoSans_Regular_27(),
        color::Black),
    15, 75);
}
```

![img5](doc/images/img5.png)

Note that since the font is anti-aliased, setting the correct background color matters even with `FILL_MODE_VISIBLE`. Using background that does not match the underlying background results in 'colored' text edges, as seen in the example above.

### Alignment

The library allows drawing with specified horizontal and vertical alignment, so that you don't need to calculate and specify exact pixel offsets, ad we have been doing so far. Let's look at a simple example:

```cpp
void loop() {
  auto circle = FilledCircle::ByExtents(0, 0, 40, color::DarkGray);
  DrawingContext dc(display);
  dc.draw(circle, kLeft | kTop);
  dc.draw(circle, kCenter | kTop);
  dc.draw(circle, kRight | kTop);
  dc.draw(circle, kLeft | kMiddle);
  dc.draw(circle, kCenter | kMiddle);
  dc.draw(circle, kRight | kMiddle);
  dc.draw(circle, kLeft | kBottom);
  dc.draw(circle, kCenter | kBottom);
  dc.draw(circle, kRight | kBottom);
}
```

![img6](doc/images/img6.png)

You can adjust the alignment using specific offsets:

```cpp
void loop() {
  auto circle = FilledCircle::ByExtents(0, 0, 40, color::DarkGray);
  DrawingContext dc(display);
  dc.draw(circle, kLeft.shiftBy(10) | kTop.shiftBy(10));
  dc.draw(circle, kCenter | kTop.shiftBy(10));
  dc.draw(circle, kRight.shiftBy(-10) | kTop.shiftBy(10));
  dc.draw(circle, kLeft.shiftBy(10) | kMiddle);
  dc.draw(circle, kCenter | kMiddle);
  dc.draw(circle, kRight.shiftBy(-10) | kMiddle);
  dc.draw(circle, kLeft.shiftBy(10) | kBottom.shiftBy(-10));
  dc.draw(circle, kCenter | kBottom.shiftBy(-10));
  dc.draw(circle, kRight.shiftBy(-10) | kBottom.shiftBy(-10));
}
```

![img7](doc/images/img7.png)

What if you want to draw on the portion of the screen, and use that portion for alignment, rathen than the display boundaries? Not a problem - you can specify the desired bounds when creating the drawing context. The following example draws 4 shapes centered in 4 areas the screen is split into: 

```cpp
void loop() {
  int w = display.width();
  int h = display.height();
  auto centered = kMiddle | kCenter;
  {
    DrawingContext dc(display, Box(0, 0, w / 2 - 1, h / 2 - 1));
    dc.draw(
      FilledCircle::ByExtents(0, 0, 40, color::Purple),
      centered);
  }
  {
    DrawingContext dc(display, Box(w / 2, 0, w - 1, h / 2 - 1));
    dc.draw(
      FilledTriangle(0, 0, 10, 50, 80, 5, color::Crimson),
      centered);
  }
  {
    DrawingContext dc(display, Box(0, h / 2, w / 2 - 1, h - 1));
    dc.draw(
      FilledRoundRect(0, 0, 70, 30, 10, color::ForestGreen),
      centered);
  }
  {
    DrawingContext dc2(display, Box(w / 2, h / 2, w - 1, h - 1));
    dc2.draw(
      FilledRect(0, 0, 40, 80, color::DodgerBlue),
      centered);
  }
}
```

![img8](doc/images/img8.png)

For the purpose of alignment, drawables may declare 'anchor extents' that are different than the regular extents defining the bounding rectangle of their content. We will see the examples of that later.

As we saw in the examples, `kLeft` aligns left anchor of the object to the left side of the screen, `kCenter` aligns the center of the object to the center of the screen, and so on. You can override the 'destination' anchor for more specialized alignments. The following example draws the same text twice, first aligning the _top_ of the text to the middle of the sceen, and then aligning the _bottom_ of the text to the middle of the screen:

```cpp
#include "roo_smooth_fonts/NotoSerif_Italic/60.h"

// ...

void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  DrawingContext dc(display);
  auto label = TextLabel("Hello!", font, color::Black);
  dc.draw(label, kCenter | kTop.toMiddle());
  dc.draw(label, kCenter | kBottom.toMiddle());

  delay(10000);
}
```

![img11](doc/images/img11.png)

Twere are two other alignment options, `kOrigin` and `kBaseline`. They correspond to the zero coordinate in the X and Y directions, respectively. If you use them without modifiers, the object will remain unaligned along the specific axis. For example:

```cpp
dc.draw(object, kOrigin | kBaseline);
```

is equivalent to

```cpp
dc.draw(object);
```

Using these options with `to` modifiers allows you to specify where to align the zero coordinate to. It is particularly useful for aligning text at baseline, as we will see in the later section.

Given the alignment, you can calculate the absolute offset that it will apply to a given object, and use it do draw other content:

```cpp
void loop() {
  Alignment a = kCenter | kMiddle;
  TextLabel label("Hello!", font_NotoSerif_Italic_60(), color::Black);
  DrawingContext dc(display);
  Offset offset = a.resolveOffset(dc.bounds(), label.anchorExtents());
  // Equivalent to dc.draw(label, a).
  dc.draw(label, offset.dx, offset.dy);
  // Draw rect border around the text, adding 5px padding.
  Box box = label.extents().translate(offset.dx, offset.dy);

  dc.draw(Rect(box.xMin() - 5, box.yMin() - 5, box.xMax() + 5,
               box.yMax() + 5, color::Red));

}
```

![img13](doc/images/img13.png)

### Tiles

Often, you need to draw variable-sized contents over some preallocated space. A simple example is to overwrite some previously drawn text with new text that might possibly be shorter. In this case, even if you set `FILL_MODE_RECTANGLE`, the original contents will not be completely erased, since the new contents has smaller extents.

You can solve this problem using the `Tile` utility class. A tile is a rectangular 'container' of a predefined size, which you can think of as padding of a specified background color around your drawable. The position of your drawable within the tile is given by an alignment.

Tiles redraw their contents without flicker, as long as the contents itself draws without flicker, which is true for `TextLabel` and most other builtin drawables.

Tile takes a pointer to the interior drawable when constructed:

```cpp
#include "roo_smooth_fonts/NotoSans_Regular/27.h"

// ...
void loop() {
  const auto& font = font_NotoSans_Regular_27();
  TextLabel label1("Hello, hello, world!", font, color::Black);
  TextLabel label2("Hello, world!", font, color::Black);
  Box extents(10, 10, 309, 49);
  Alignment a = kLeft.shiftBy(5) | kMiddle;
  DrawingContext dc(display);
  Tile tile1(&label1, extents, a, color::Gainsboro);
  dc.draw(tile1);
  delay(2000);
  Tile tile2(&label2, extents, a, color::Gainsboro);
  dc.draw(tile2);
  delay(2000);
}
```

You can inline some of the objects as temporaries by using `MakeTileOf`:

```cpp
void loop() {
  const auto& font = font_NotoSans_Regular_27();
  Box extents(10, 10, 309, 49);
  Alignment a = kLeft.shiftBy(5) | kMiddle;
  DrawingContext dc(display);
  dc.draw(MakeTileOf(
      TextLabel("Hello, hello, world!", font, color::Black),
      extents, a, color::Gainsboro));
  delay(2000);
  dc.draw(MakeTileOf(
      TextLabel("Hello, world!", font, color::Black),
      extents, a, color::Gainsboro));
  delay(2000);
}
```

![img16](doc/images/img16.png)

You can omit the tile's background color. It will then default to `color::Background`, inheriting the drawing context's background, but still forcing `FILL_MODE_RECTANGLE`. That is, such a tile will draw its entire extents, regardless of the fill mode settings. It is most likely the the most commonly needed behavior: the background is 'invisible' yet the contents get redrawn when needed without extra hassle.

If you need to, however, you can force the tile to respect the fill mode settings, by explicitly specifying `color::Transparent` as the background.

You can also specify a translucent background color. In this case, the actual background will be the alpha-blend of the tile's background over the drawing context's background:

```cpp
void setup() {
  // ...
  int w = display.width();
  int h = display.height();
  DrawingContext dc(display);
  dc.draw(FilledRect(0, h / 2, w - 1, h - 1, color::Khaki));
}

void loop() {
  auto tile = MakeTileOf(
      TextLabel(
          "Hello, world!",
          font_NotoSans_Regular_27(),
          color::Black),
      Box(10, 10, 309, 49),
      kLeft.shiftBy(5) | kMiddle,
      color::Red.withA(0x30));
  DrawingContext dc(display);
  dc.draw(tile);
  dc.setBackgroundColor(color::Khaki);
  dc.draw(tile, 0, display.height() / 2);
  delay(10000);
}
```

![img17](doc/images/img17.png)

### Clipping

You can set a _clip box_ on a drawing context in order to constrain the drawn area to a specific rectangle.

```cpp
void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  int w = display.width();
  int h = display.height();
  DrawingContext dc(display);
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  dc.setClipBox(0, 0, w - 1, h / 2 - 1);
  label.setColor(color::Maroon);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipBox(0, h / 2, w - 1, h - 1);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);

  delay(10000);
}
```

![img14](doc/images/img14.png)

You can also set a _clip mask_, which is a bit mask allowing to define arbitrary clipping shapes.

```cpp
#include "Arduino.h"

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic_shapes.h"

#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_Italic/60.h"

using namespace roo_display;

Ili9341spi<5, 2, 4> device(Orientation().rotateLeft());
Display display(device);

// 320x60 pixels.
uint8_t mask_data[320 * 60 / 8];

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
  for (int i = 0; i < 30; i++) {
    // Alternate rows: bits set, bits cleared.
    memset(mask_data + 2 * i * 320 / 8, 0xFF, 320 / 8);
    memset(mask_data + (2 * i + 1) * 320 / 8, 0x00, 320 / 8);
  }
}

void loop() {
  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  int w = display.width();
  int h = display.height();
  DrawingContext dc(display);
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  ClipMask mask1(mask_data, Box(0, 90, 319, 149));
  dc.setClipMask(&mask1);
  label.setColor(color::DarkSalmon);
  dc.draw(label, kCenter | kMiddle);
  // Same as before, but shifted by one row.
  ClipMask mask2(mask_data, Box(0, 91, 319, 150));
  dc.setClipMask(&mask2);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipMask(nullptr);  // To be on the safe side.

  delay(10000);
}
```

![img15](doc/images/img15.png)

You will see an easier way to initialize a clip mask in the section on offscreen drawing.

> Note: drawing with a clip mask is noticeably slower. Use wisely.

### Transformations

You can apply basic affine transformations (integer scaling, right-angle rotation, translation) to anything you draw. 

A transformation is represented by the `Transformation` object. Calling its default constructor creates an 'identity' transformation, i.e. one that does not change anything. You can then use it as a starting point to compose your transformation, by calling methods that add specific modifications, such as: translation, scaling, rotation, coordinate swapping, and others. You can also use a more specialized constructor to initialize your transformation in one step.

Likely the most important application of transformations is rotated or scaled text:

```cpp
#include "roo_display/filter/transformation.h"

// ...

void loop() {
  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(10, 3));
  dc.draw(
      TextLabel("F", font_NotoSerif_Italic_27(), color::Black),
      kCenter | kMiddle);
  dc.setTransformation(Transformation().rotateLeft());
  dc.draw(
      TextLabel("Hello, World!", font_NotoSerif_Italic_27(), color::Black),
      kLeft | kMiddle);
  dc.setTransformation(Transformation().rotateRight().flipX());
  dc.draw(
      TextLabel("Hello, World!", font_NotoSerif_Italic_27(), color::Black),
      kRight | kMiddle);
  delay(10000);
}
```

![img18](doc/images/img18.png)

### Drawing individual pixels

There is no 'Pixel' drawable. Drawing pixels as individual drawables would be slow.

Instead, you can draw pixels in batches, using the following idiom:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.drawPixels([](PixelWriter& writer) {
    for (int i = 0; i < 319; ++i) {
      writer.writePixel(i, 100 + (int)(20 * sin(i / 20.0)), color::Black);
    }
    for (int i = 0; i < 319; ++i) {
      writer.writePixel(i, 180 + (int)(20 * cos(i / 20.0)), color::Red);
    }
  });
  delay(10000);
}
```

![img19](doc/images/img19.png)

Pixel drawing respects settings of the drawing context, such as the clip region, and transformation:

```cpp
void loop() {
  DrawingContext dc(display);
  dc.setClipBox(20, 20, 299, 219);
  dc.setTransformation(Transformation().scale(3, 3));
  dc.drawPixels([](PixelWriter& writer) {
    for (int i = 0; i < 100; ++i) {
      writer.writePixel(i, 30 + (int)(10 * sin(i / 10.0)), color::Black);
    }
    for (int i = 0; i < 100; ++i) {
      writer.writePixel(i, 50 + (int)(10 * cos(i / 10.0)), color::Red);
    }
  });
  delay(10000);
}
```

![img20](doc/images/img20.png)

> Note: `PixelWriter` buffers the pixels and flushes them in batches, in order to optimize the underlying write operations. Therefore, the pixels may not appear until `drawPixels()` returns.

## Working with text

### Fonts

The library comes with a collection of free Noto UTF fonts of various sizes and faces (Sans, Sans condensed, and Serif; with bold and italic variants). They are anti-aliased and support kerning pairs. The default charset is reasonably rich; covering latin-1 and latin-2, and a number of symbols. Later, you 

### Alignment

Let's see what these 'anchor extents' look like for a text label:

```cpp
#include "roo_smooth_fonts/NotoSerif_Italic/90.h"

// ...

void loop() {
  const auto& font = font_NotoSerif_Italic_90();
  int dx = 40;
  int dy = 160;
  DrawingContext dc(display);
  auto label = TextLabel("yeah.", font, color::Black);
  dc.setBackgroundColor(color::LightGray);
  dc.draw(FilledRect(label.extents(), color::LightGray), dx, dy);
  dc.draw(Line(dx, 0, dx, display.height() - 1, color::Red));
  dc.draw(Line(0, dy, display.width() - 1, dy, color::Red));
  dc.draw(Rect(label.anchorExtents(), color::Blue), dx, dy);

  dc.draw(label, dx, dy);

  delay(10000);
}
```

![img9](doc/images/img9.png)

In this case, anchor extents are dictated by the rules of typography, rather than what the minimum bounding box looks like. In particular, the anchors leave a bit of space after the period and above the ascent of the text. On the left side, the tail of 'y' sticks outside the anchor box.

Let's see how the aligned text looks like:

```cpp
#include "roo_smooth_fonts/NotoSerif_Italic/27.h"

// ...

void loop() {
  const auto& font = font_NotoSerif_Italic_27();
  auto label = TextLabel("yeah.", font, color::Black);
  int dx = 10;
  int dy = 10;
  int w = display.width();
  int h = display.height();
  DrawingContext dc(display);
  dc.draw(Line(0, dy, w - 1, dy, color::Red));
  dc.draw(Line(0, h - dy - 1, w - 1, h - dy - 1, color::Red));
  dc.draw(Line(dx, 0, dx, h - 1, color::Red));
  dc.draw(Line(w - dx - 1, 0, w - dx - 1, h - 1, color::Red));
  dc.draw(label, kLeft.shiftBy(dx) | kTop.shiftBy(dy));
  dc.draw(label, kCenter | kTop.shiftBy(dy));
  dc.draw(label, kRight.shiftBy(-dx) | kTop.shiftBy(dy));
  dc.draw(label, kLeft.shiftBy(dx) | kMiddle);
  dc.draw(label, kCenter | kMiddle);
  dc.draw(label, kRight.shiftBy(-dx) | kMiddle);
  dc.draw(label, kLeft.shiftBy(dx) | kBottom.shiftBy(-dy));
  dc.draw(label, kCenter | kBottom.shiftBy(-dy));
  dc.draw(label, kRight.shiftBy(-dx) | kBottom.shiftBy(-dy));

  delay(10000);
}
```

![img10](doc/images/img10.png)

```cpp
#include "roo_smooth_fonts/NotoSerif_Italic/15.h"
#include "roo_smooth_fonts/NotoSerif_Italic/18.h"
#include "roo_smooth_fonts/NotoSerif_Italic/27.h"
#include "roo_smooth_fonts/NotoSerif_Italic/40.h"

// ...

void loop() {
  int dx = 5;
  auto label1 = TextLabel(
    "Hello ", font_NotoSerif_Italic_15(), color::Black);
  auto label2 = TextLabel(
    "Hello ", font_NotoSerif_Italic_18(), color::Black);
  auto label3 = TextLabel(
    "Hello ", font_NotoSerif_Italic_27(), color::Black);
  auto label4 = TextLabel(
    "Hello ", font_NotoSerif_Italic_40(), color::Black);
  DrawingContext dc(display);
  dc.draw(label1, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label1.anchorExtents().width();
  dc.draw(label2, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label2.anchorExtents().width();
  dc.draw(label3, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label3.anchorExtents().width();
  dc.draw(label4, kLeft.shiftBy(dx) | kBaseline.toMiddle());
  dx += label4.anchorExtents().width();

  delay(10000);
}
```

![img12](doc/images/img12.png)


## Drawing images and icons

## Using off-screen buffers

## Overlays, backgrounds, filters

## Touch

## Flicker-less composition

## Extending the library

### Implementing custom drawables

### Adding new device drivers

### Supporting non-SPI transports

## UI utilities

### Tile
