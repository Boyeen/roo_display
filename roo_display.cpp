#include "roo_display.h"

#include <Arduino.h>

#include <cmath>

#include "roo_display/filter/color_filter.h"

namespace roo_display {

bool TouchDisplay::getTouch(int16_t& x, int16_t& y) {
  int16_t raw_x, raw_y, raw_z;
  bool touched = touch_device_.getTouch(raw_x, raw_y, raw_z);
  if (!touched) {
    return false;
  }
  touch_calibration_.Calibrate(raw_x, raw_y, raw_z);
  Orientation orientation = display_device_.orientation();
  if (orientation.isRightToLeft()) {
    raw_x = 4095 - raw_x;
  }
  if (orientation.isBottomToTop()) {
    raw_y = 4095 - raw_y;
  }
  if (orientation.isXYswapped()) {
    std::swap(raw_x, raw_y);
  }

  x = ((int32_t)raw_x * (display_device_.effective_width() - 1)) / 4095;
  y = ((int32_t)raw_y * (display_device_.effective_height() - 1)) / 4095;
  return true;
}

class DummyTouch : public TouchDevice {
 public:
  bool getTouch(int16_t& x, int16_t& y, int16_t& z) override { return false; }
};

static DummyTouch dummy_touch;

Display::Display(DisplayDevice& display_device, TouchDevice* touch_device,
                 TouchCalibration touch_calibration)
    : display_device_(display_device),
      touch_(display_device,
             touch_device == nullptr ? dummy_touch : *touch_device,
             touch_calibration),
      nest_level_(0),
      orientation_(display_device.orientation()),
      extents_(Box::MaximumBox()),
      bgcolor_(Color(0)),
      background_(nullptr) {
  resetExtents();
}

void Display::setOrientation(Orientation orientation) {
  if (orientation_ == orientation) return;
  orientation_ = orientation;
  nest();
  display_device_.setOrientation(orientation);
  unnest();
  // Update, since the width and height might have gotten swapped.
  resetExtents();
}

void Display::init(Color bgcolor) {
  display_device_.init();
  setBackground(bgcolor);
  clear();
}

void Display::clear() {
  DrawingContext dc(*this);
  dc.clear();
}

void DrawingContext::fill(Color color) { draw(Fill(color)); }
void DrawingContext::clear() { draw(Clear()); }

namespace {

class Pixels : public Drawable {
 public:
  Pixels(const std::function<void(ClippingBufferedPixelWriter&)>& fn,
         Box extents, PaintMode paint_mode)
      : fn_(fn), extents_(extents), paint_mode_(paint_mode) {}

  Box extents() const override { return extents_; }

 private:
  void drawTo(const Surface& s) const override {
    if (s.dx() == 0 && s.dy() == 0) {
      ClippingBufferedPixelWriter writer(s.out(), extents_, paint_mode_);
      fn_(writer);
    } else {
      TransformedDisplayOutput out(s.out(),
                                   Transformation().translate(s.dx(), s.dy()));
      ClippingBufferedPixelWriter writer(out, extents_, paint_mode_);
      fn_(writer);
    }
  }

  const std::function<void(ClippingBufferedPixelWriter&)>& fn_;
  Box extents_;
  PaintMode paint_mode_;
};

}  // namespace

DrawingContext::~DrawingContext() { unnest_(); }

void DrawingContext::setWriteOnce() {
  if (write_once_) return;
  write_once_ = true;
  front_to_back_writer_.reset(
      new FrontToBackWriter(output(), bounds().translate(dx_, dy_)));
}

void DrawingContext::drawPixels(
    const std::function<void(ClippingBufferedPixelWriter&)>& fn,
    PaintMode paint_mode) {
  draw(
      Pixels(fn, transformation_.smallestEnclosingRect(clip_box_), paint_mode));
}

void DrawingContext::drawInternal(const Drawable& object, int16_t dx,
                                  int16_t dy, Color bgcolor) {
  DisplayOutput& out = front_to_back_writer_.get() != nullptr
                           ? *front_to_back_writer_
                           : output();
  Surface s(out, dx + dx_, dy + dy_, clip_box_.translate(dx_, dy_), write_once_,
            bgcolor, fill_mode_, paint_mode_);

  if (clip_mask_ == nullptr) {
    drawInternalWithBackground(s, object);
  } else {
    ClipMaskFilter filter(out, clip_mask_);
    s.set_out(&filter);
    drawInternalWithBackground(s, object);
  }
}

void DrawingContext::drawInternalWithBackground(Surface& s,
                                                const Drawable& object) {
  if (background_ == nullptr) {
    drawInternalTransformed(s, object);
  } else {
    BackgroundFilter filter(s.out(), background_);
    s.set_out(&filter);
    drawInternalTransformed(s, object);
  }
}

void DrawingContext::drawInternalTransformed(Surface& s,
                                             const Drawable& object) {
  if (!transformed_) {
    s.drawObject(object);
  } else if (!transformation_.is_rescaled() && !transformation_.xy_swap()) {
    // Translation only.
    s.set_dx(s.dx() + transformation_.x_offset());
    s.set_dy(s.dy() + transformation_.y_offset());
    s.drawObject(object);
  } else {
    s.drawObject(TransformedDrawable(transformation_, &object));
  }
}

namespace {

class ErasedDrawable : public Drawable {
 public:
  ErasedDrawable(const Drawable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }
  Box anchorExtents() const override { return delegate_->anchorExtents(); }

 private:
  void drawTo(const Surface& s) const override {
    Surface news = s;
    ColorFilter<Erasure> filter(s.out());
    news.set_out(&filter);
    news.set_paint_mode(PAINT_MODE_REPLACE);
    news.drawObject(*delegate_);
  }

  const Drawable* delegate_;
};

}  // namespace

void DrawingContext::erase(const Drawable& object) {
  draw(ErasedDrawable(&object));
}

void DrawingContext::erase(const Drawable& object, int16_t dx, int16_t dy) {
  draw(ErasedDrawable(&object), dx, dy);
}

void DrawingContext::erase(const Drawable& object, Alignment alignment) {
  draw(ErasedDrawable(&object), alignment);
}

}  // namespace roo_display
