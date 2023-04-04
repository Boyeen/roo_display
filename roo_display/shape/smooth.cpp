#include "roo_display/shape/smooth.h"

#include <Arduino.h>
#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

namespace {

inline uint8_t getAlpha(float r, float xpax, float ypay, float bax, float bay,
                        float dr, uint8_t max_alpha) {
  float h = fmaxf(
      fminf((xpax * bax + ypay * bay) / (bax * bax + bay * bay), 1.0f), 0.0f);
  float dx = xpax - bax * h;
  float dy = ypay - bay * h;
  float d = r - sqrtf(dx * dx + dy * dy) - h * dr;
  return (d >= 1.0 ? max_alpha : d <= 0.0 ? 0 : (uint8_t)(d * max_alpha));
}

}  // namespace

SmoothWedgeShape::SmoothWedgeShape(FpPoint a, float a_width, FpPoint b,
                                   float b_width, Color color)
    : ax_(a.x),
      ay_(a.y),
      bx_(b.x),
      by_(b.y),
      aw_(a_width),
      bw_(b_width),
      color_(color) {
  int16_t x0 = (int32_t)floorf(fminf(a.x - a_width, b.x - b_width));
  int16_t y0 = (int32_t)floorf(fminf(a.y - a_width, b.y - b_width));
  int16_t x1 = (int32_t)ceilf(fmaxf(a.x + a_width, b.x + b_width));
  int16_t y1 = (int32_t)ceilf(fmaxf(a.y + a_width, b.y + b_width));
  extents_ = Box(x0, y0, x1, y1);
}

void SmoothWedgeShape::drawInteriorTo(const Surface& s) const {
  float ar = aw_;
  float br = bw_;
  float ax = ax_;
  float ay = ay_;
  float bx = bx_;
  float by = by_;
  uint8_t max_alpha = color_.a();

  // Establish x start and y start.
  int32_t ys = ay;
  if ((ax - ar) > (bx - br)) ys = by;

  float rdt = ar - br;  // Radius delta.
  uint8_t alpha = max_alpha;
  ar += 0.5f;

  float xpax, ypay, bax = bx - ax, bay = by - ay;

  Box box = Box::Intersect(extents_, s.clip_box().translate(-s.dx(), -s.dy()));
  if (box.empty()) {
    return;
  }
  if (ys < box.yMin()) ys = box.yMin();
  int32_t xs = box.xMin();
  int16_t dx = s.dx();
  int16_t dy = s.dy();
  Color preblended = AlphaBlend(s.bgcolor(), color_);

  BufferedPixelWriter writer(s.out(), s.paint_mode());

  // Scan bounding box from ys down, calculate pixel intensity from distance to
  // line.
  for (int32_t yp = ys; yp <= box.yMax(); yp++) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0) break;  // Skip right side.
      xpax = xp - ax;
      alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
      if (alpha == 0) continue;
      // Track the edge to minimize calculations.
      if (!endX) {
        endX = true;
        xs = xp;
      }
      writer.writePixel(xp + dx, yp + dy,
                        alpha == max_alpha
                            ? preblended
                            : AlphaBlend(s.bgcolor(), color_.withA(alpha)));
    }
  }

  // Reset x start to left side of box.
  xs = box.xMin();
  // Scan bounding box from ys-1 up, calculate pixel intensity from distance to
  // line.
  for (int32_t yp = ys - 1; yp >= box.yMin(); yp--) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0) break;  // Skip right side of drawn line.
      xpax = xp - ax;
      alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
      if (alpha == 0) continue;
      // Track line boundary.
      if (!endX) {
        endX = true;
        xs = xp;
      }
      writer.writePixel(xp + dx, yp + dy,
                        alpha == max_alpha
                            ? preblended
                            : AlphaBlend(s.bgcolor(), color_.withA(alpha)));
    }
  }
}

SmoothRoundRectShape::SmoothRoundRectShape(float x0, float y0, float x1,
                                           float y1, float radius,
                                           float interior_radius,
                                           Color outline_color,
                                           Color interior_color)
    : x0_(x0),
      y0_(y0),
      x1_(x1),
      y1_(y1),
      r_(radius),
      ri_(interior_radius),
      outline_color_(outline_color),
      interior_color_(interior_color) {
  {
    int16_t xMin = (int16_t)roundf(x0_ - r_);
    int16_t yMin = (int16_t)roundf(y0_ - r_);
    int16_t xMax = (int16_t)roundf(x1_ + r_);
    int16_t yMax = (int16_t)roundf(y1_ + r_);
    extents_ = Box(xMin, yMin, xMax, yMax);
  }
  {
    int16_t xMin = (int16_t)ceilf(x0_ - ri_ + 0.5f);
    int16_t xMax = (int16_t)floorf(x1_ + ri_ - 0.5f);
    int16_t yMin = (int16_t)ceilf(y0_ + 0.5f);
    int16_t yMax = (int16_t)floorf(y1_ - 0.5f);
    inner_wide_ = Box(xMin, yMin, xMax, yMax);
  }
  {
    int16_t xMin = (int16_t)ceilf(x0_ + 0.5f);
    int16_t xMax = (int16_t)floorf(x1_ - 0.5f);
    int16_t yMin = (int16_t)ceilf(y0_ - ri_ + 0.5f);
    int16_t yMax = (int16_t)floorf(y1_ + ri_ - 0.5f);
    inner_tall_ = Box(xMin, yMin, xMax, yMax);
  }
  {
    float d = sqrtf(0.5f) * ri_;
    int16_t xMin = (int16_t)ceilf(x0_ - d + 0.5f);
    int16_t xMax = (int16_t)floorf(x1_ + d - 0.5f);
    int16_t yMin = (int16_t)ceilf(y0_ - d + 0.5f);
    int16_t yMax = (int16_t)floorf(y1_ + d - 0.5f);
    inner_mid_ = Box(xMin, yMin, xMax, yMax);
  }
}

void SmoothRoundRectShape::readColors(const int16_t* x, const int16_t* y,
                                      uint32_t count, Color* result) const {
  while (count-- > 0) {
    *result++ = getColor(*x++, *y++);
  }
}

inline float SmoothRoundRectShape::calcDistSq(int16_t x, int16_t y) const {
  float ref_x = x;
  if (ref_x < x0_) ref_x = x0_;
  if (ref_x > x1_) ref_x = x1_;
  float ref_y = y;
  if (ref_y < y0_) ref_y = y0_;
  if (ref_y > y1_) ref_y = y1_;
  float dx = x - ref_x;
  float dy = y - ref_y;
  return dx * dx + dy * dy;
}

bool SmoothRoundRectShape::readColorRect(int16_t xMin, int16_t yMin,
                                         int16_t xMax, int16_t yMax,
                                         Color* result) const {
  Box box(xMin, yMin, xMax, yMax);
  // Check if the rect happens to fall within the known inner rectangles.
  if (inner_mid_.contains(box) || inner_wide_.contains(box) ||
      inner_tall_.contains(box)) {
    *result = interior_color_;
    return true;
  }
  float dtl = calcDistSq(xMin, yMin);
  float dtr = calcDistSq(xMax, yMin);
  float dbl = calcDistSq(xMin, yMax);
  float dbr = calcDistSq(xMax, yMax);
  float r_min_sq = (ri_ - 0.5) * (ri_ - 0.5);
  // Check if the rect falls entirely inside the interior boundary.
  if (dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq && dbr < r_min_sq) {
    *result = interior_color_;
    return true;
  }

  float r_max_sq = (r_ + 0.5) * (r_ + 0.5);
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < x0_) {
    if (yMax < y0_) {
      if (dbr >= r_max_sq) {
        *result = color::Transparent;
        return true;
      }
    } else if (yMax > y1_) {
      if (dtr >= r_max_sq) {
        *result = color::Transparent;
        return true;
      }
    }
  } else if (xMax > x1_) {
    if (yMax < y0_) {
      if (dbl >= r_max_sq) {
        *result = color::Transparent;
        return true;
      }
    } else if (yMax > y1_) {
      if (dtl >= r_max_sq) {
        *result = color::Transparent;
        return true;
      }
    }
  }

  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = getColorPreChecked(x, y);
    }
  }
  // This is now very unlikely to be true, or we would have caught it above.
  // uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
  // Color c = result[0];
  // for (uint32_t i = 1; i < pixel_count; i++) {
  //   if (result[i] != c) return false;
  // }
  // return true;
  return false;
}

Color SmoothRoundRectShape::getColor(int16_t x, int16_t y) const {
  if (inner_mid_.contains(x, y) || inner_wide_.contains(x, y) ||
      inner_tall_.contains(x, y)) {
    return interior_color_;
  }
  return getColorPreChecked(x, y);
}

Color SmoothRoundRectShape::getColorPreChecked(int16_t x, int16_t y) const {
  float ref_x = x;
  if (ref_x < x0_) ref_x = x0_;
  if (ref_x > x1_) ref_x = x1_;
  float ref_y = y;
  if (ref_y < y0_) ref_y = y0_;
  if (ref_y > y1_) ref_y = y1_;
  float dx = x - ref_x;
  float dy = y - ref_y;
  // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < r_ - 0.5) {
  //   *result++ = color_;
  //   continue;
  // }
  float d_squared = dx * dx + dy * dy;
  float ri_squared_adjusted = ri_ * ri_ + 0.25f;
  if (d_squared <= ri_squared_adjusted - ri_ - 1) {
    return interior_color_;
  }
  float r_squared_adjusted = r_ * r_ + 0.25f;
  if (d_squared >= r_squared_adjusted + r_) {
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= r_squared_adjusted - r_;
  bool fully_outside_inner =
      r_ == ri_ || d_squared >= ri_squared_adjusted + ri_;
  if (fully_within_outer && fully_outside_inner) {
    return outline_color_;
  }
  float d = sqrtf(d_squared);
  if (fully_outside_inner) {
    return outline_color_.withA(
        (uint8_t)(outline_color_.a() * (r_ - d + 0.5f)));
  }
  if (fully_within_outer) {
    return AlphaBlend(interior_color_,
                      outline_color_.withA((uint8_t)(outline_color_.a() *
                                                     (1 - (ri_ - d + 0.5f)))));
  }
  return AlphaBlend(
      interior_color_,
      outline_color_.withA(
          (uint8_t)(outline_color_.a() *
                    std::max(0.0f, (r_ - d + 0.5f) - (ri_ - d + 0.5f)))));
}

namespace {

struct SmoothRoundRectSpec {
  DisplayOutput* out;
  FillMode fill_mode;
  PaintMode paint_mode;
  float x0;
  float y0;
  float x1;
  float y1;
  float r;
  float r_squared_adjusted;
  float ri;
  float ri_squared_adjusted;
  Color color;
  Color bgcolor;
  Color interior;
  Color pre_blended_outline;
  Color pre_blended_interior;
  Box inner_wide;
  Box inner_mid;
  Box inner_tall;
};

inline Color GetSmoothRoundRectShapeColor(const SmoothRoundRectSpec& spec,
                                          int16_t x, int16_t y) {
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (spec.inner_mid.contains(x, y) || spec.inner_wide.contains(x, y) ||
  //     spec.inner_tall.contains(x, y)) {
  //   return spec.interior;
  // }
  float ref_x = x;
  if (ref_x < spec.x0) ref_x = spec.x0;
  if (ref_x > spec.x1) ref_x = spec.x1;
  float ref_y = y;
  if (ref_y < spec.y0) ref_y = spec.y0;
  if (ref_y > spec.y1) ref_y = spec.y1;
  float dx = x - ref_x;
  float dy = y - ref_y;
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < spec.ri - 0.5) {
  //   return spec.interior;
  // }

  float d_squared = dx * dx + dy * dy;
  if (d_squared <= spec.ri_squared_adjusted - spec.ri - 1) {
    return spec.interior;
  }
  if (d_squared >= spec.r_squared_adjusted + spec.r) {
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= spec.r_squared_adjusted - spec.r;
  bool fully_outside_inner =
      spec.r == spec.ri || d_squared >= spec.ri_squared_adjusted + spec.ri;
  if (fully_within_outer && fully_outside_inner) {
    return spec.color;
  }
  // Note: replacing with integer sqrt (iterative, loop-unrolled, 24-bit) slows
  // things down. At 64-bit not loop-unrolled, does so dramatically.
  float d = sqrtf(d_squared);
  if (fully_outside_inner) {
    return spec.color.withA((uint8_t)(spec.color.a() * (spec.r - d + 0.5f)));
  }
  if (fully_within_outer) {
    return AlphaBlend(spec.interior,
                      spec.color.withA((uint8_t)(spec.color.a() *
                                                 (1 - (spec.ri - d + 0.5f)))));
  }
  return AlphaBlend(
      spec.interior,
      spec.color.withA((uint8_t)(spec.color.a() *
                                 std::max(0.0f, (spec.r - d + 0.5f) -
                                                    (spec.ri - d + 0.5f)))));
}

namespace {

float CalcSmoothRoundRectDistSq(SmoothRoundRectSpec& spec, int16_t x,
                                int16_t y) {
  float ref_x = x;
  if (ref_x < spec.x0) ref_x = spec.x0;
  if (ref_x > spec.x1) ref_x = spec.x1;
  float ref_y = y;
  if (ref_y < spec.y0) ref_y = spec.y0;
  if (ref_y > spec.y1) ref_y = spec.y1;
  float dx = x - ref_x;
  float dy = y - ref_y;
  return dx * dx + dy * dy;
}

}  // namespace

// Called for rectangles with area <= 64 pixels.
void FillSmoothRoundRectRectInternal(SmoothRoundRectSpec& spec, int16_t xMin,
                                     int16_t yMin, int16_t xMax, int16_t yMax) {
  Box box(xMin, yMin, xMax, yMax);
  if (spec.inner_mid.contains(box) || spec.inner_wide.contains(box) ||
      spec.inner_tall.contains(box)) {
    spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_interior);
    return;
  }
  float dtl = CalcSmoothRoundRectDistSq(spec, xMin, yMin);
  float dtr = CalcSmoothRoundRectDistSq(spec, xMax, yMin);
  float dbl = CalcSmoothRoundRectDistSq(spec, xMin, yMax);
  float dbr = CalcSmoothRoundRectDistSq(spec, xMax, yMax);
  float r_min_sq = (spec.ri - 0.5) * (spec.ri - 0.5);
  // Check if the rect falls entirely inside the interior boundary.
  if (dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq && dbr < r_min_sq) {
    if (spec.fill_mode == FILL_MODE_RECTANGLE || spec.interior.a() > 0) {
      spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_interior);
    }
    return;
  }

  float r_max_sq = (spec.r + 0.5) * (spec.r + 0.5);
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < spec.x0) {
    if (yMax < spec.y0) {
      if (dbr >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    } else if (yMin > spec.y1) {
      if (dtr >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    }
  } else if (xMin > spec.x1) {
    if (yMax < spec.y0) {
      if (dbl >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    } else if (yMin > spec.y1) {
      if (dtl >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    }
  }
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
    BufferedPixelWriter writer(*spec.out, spec.paint_mode);
    for (int16_t y = yMin; y <= yMax; ++y) {
      for (int16_t x = xMin; x <= xMax; ++x) {
        Color c = GetSmoothRoundRectShapeColor(spec, x, y);
        if (c.a() == 0) continue;
        // writer.writePixel(x, y, c);
        writer.writePixel(x, y,
                          c == spec.interior ? spec.pre_blended_interior
                          : c == spec.color  ? spec.pre_blended_outline
                                             : AlphaBlend(spec.bgcolor, c));
      }
    }
  } else {
    Color color[64];
    int cnt = 0;
    for (int16_t y = yMin; y <= yMax; ++y) {
      for (int16_t x = xMin; x <= xMax; ++x) {
        Color c = GetSmoothRoundRectShapeColor(spec, x, y);
        color[cnt++] = c.a() == 0           ? spec.bgcolor
                       : c == spec.interior ? spec.pre_blended_interior
                       : c == spec.color    ? spec.pre_blended_outline
                                            : AlphaBlend(spec.bgcolor, c);
      }
    }
    spec.out->setAddress(Box(xMin, yMin, xMax, yMax), spec.paint_mode);
    spec.out->write(color, cnt);
  }
}

}  // namespace

void SmoothRoundRectShape::drawTo(const Surface& s) const {
  Box box = Box::Intersect(extents_.translate(s.dx(), s.dy()), s.clip_box());
  if (box.empty()) return;
  SmoothRoundRectSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .paint_mode = s.paint_mode(),
      .x0 = x0_,
      .y0 = y0_,
      .x1 = x1_,
      .y1 = y1_,
      .r = r_,
      .r_squared_adjusted = r_ * r_ + 0.25f,
      .ri = ri_,
      .ri_squared_adjusted = ri_ * ri_ + 0.25f,
      .color = outline_color_,
      .bgcolor = s.bgcolor(),
      .interior = interior_color_,
      .pre_blended_outline =
          AlphaBlend(AlphaBlend(s.bgcolor(), interior_color_), outline_color_),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), interior_color_),
      .inner_wide = inner_wide_,
      .inner_mid = inner_mid_,
      .inner_tall = inner_tall_,
  };
  if (s.dx() != 0 || s.dy() != 0) {
    spec.x0 += s.dx();
    spec.y0 += s.dy();
    spec.x1 += s.dx();
    spec.y1 += s.dy();
    spec.inner_wide = spec.inner_wide.translate(s.dx(), s.dy());
    spec.inner_mid = spec.inner_mid.translate(s.dx(), s.dy());
    spec.inner_tall = spec.inner_tall.translate(s.dx(), s.dy());
  }
  int16_t xMin = box.xMin();
  int16_t xMax = box.xMax();
  int16_t yMin = box.yMin();
  int16_t yMax = box.yMax();
  {
    uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
    if (pixel_count <= 64) {
      FillSmoothRoundRectRectInternal(spec, xMin, yMin, xMax, yMax);
      return;
    }
  }
  const int16_t xMinOuter = (xMin / 8) * 8;
  const int16_t yMinOuter = (yMin / 8) * 8;
  const int16_t xMaxOuter = (xMax / 8) * 8 + 7;
  const int16_t yMaxOuter = (yMax / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSmoothRoundRectRectInternal(
          spec, std::max(x, xMin), std::max(y, yMin),
          std::min((int16_t)(x + 7), xMax), std::min((int16_t)(y + 7), yMax));
    }
  }
}

}  // namespace roo_display
