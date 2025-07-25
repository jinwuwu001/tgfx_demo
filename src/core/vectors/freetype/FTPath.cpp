/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "FTPath.h"

namespace tgfx {
bool FTPath::isEmpty() const {
  return points.empty();
}

void FTPath::moveTo(const Point& point) {
  if (!points.empty()) {
    contours.push_back(points.size() - 1);
  }
  verbs.push_back(PathVerb::Move);
  points.push_back({FloatToFDot6(point.x), FloatToFDot6(point.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::lineTo(const Point& point) {
  verbs.push_back(PathVerb::Line);
  points.push_back({FloatToFDot6(point.x), FloatToFDot6(point.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::quadTo(const Point& control, const Point& end) {
  verbs.push_back(PathVerb::Quad);
  points.push_back({FloatToFDot6(control.x), FloatToFDot6(control.y)});
  tags.push_back(FT_CURVE_TAG_CONIC);
  points.push_back({FloatToFDot6(end.x), FloatToFDot6(end.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::cubicTo(const Point& control1, const Point& control2, const Point& end) {
  verbs.push_back(PathVerb::Cubic);
  points.push_back({FloatToFDot6(control1.x), FloatToFDot6(control1.y)});
  tags.push_back(FT_CURVE_TAG_CUBIC);
  points.push_back({FloatToFDot6(control2.x), FloatToFDot6(control2.y)});
  tags.push_back(FT_CURVE_TAG_CUBIC);
  points.push_back({FloatToFDot6(end.x), FloatToFDot6(end.y)});
  tags.push_back(FT_CURVE_TAG_ON);
}

void FTPath::close() {
  if (!verbs.empty() && verbs[verbs.size() - 1] == PathVerb::Close) {
    return;
  }
  auto lastContourPointIndex = contours.empty() ? 0 : contours[contours.size() - 1] + 1;
  if (points.size() - lastContourPointIndex < 1) {
    return;
  }
  auto startPoint = points[lastContourPointIndex];
  auto endPoint = points[points.size() - 1];
  if (startPoint.x != endPoint.x || startPoint.y != endPoint.y) {
    verbs.push_back(PathVerb::Line);
    points.push_back(startPoint);
    tags.push_back(FT_CURVE_TAG_ON);
  }
  verbs.push_back(PathVerb::Close);
}

std::vector<std::shared_ptr<FreetypeOutline>> FTPath::getOutlines() const {
  if (points.empty()) {
    return {};
  }
  std::vector<std::shared_ptr<FreetypeOutline>> outlines = {};
  auto outline = std::make_shared<FreetypeOutline>();
  auto contourCount = contours.size() + 1;
  size_t startPointIndex = 0;
  for (size_t i = 0; i < contourCount; i++) {
    auto contourPointIndex = contours.size() > i ? contours[i] : points.size() - 1;
    if (contourPointIndex - startPointIndex >= FT_OUTLINE_POINTS_MAX) {
      if (!finalizeOutline(outline.get(), startPointIndex)) {
        return {};
      }
      outlines.push_back(outline);
      outline = std::make_shared<FreetypeOutline>();
      startPointIndex = contours[i - 1] + 1;
    }
    outline->contours.push_back(static_cast<int16_t>(contourPointIndex - startPointIndex));
  }
  if (finalizeOutline(outline.get(), startPointIndex)) {
    outlines.push_back(outline);
  }
  return outlines;
}

bool FTPath::finalizeOutline(FreetypeOutline* outline, size_t startPointIndex) const {
  if (outline->contours.empty()) {
    return false;
  }
  auto endPointIndex = outline->contours[outline->contours.size() - 1];
  outline->outline.points = const_cast<FT_Vector*>(&(points[startPointIndex]));
  outline->outline.tags =
      reinterpret_cast<unsigned char*>(const_cast<char*>(&(tags[startPointIndex])));
  outline->outline.contours = reinterpret_cast<unsigned short*>(outline->contours.data());
  outline->outline.n_points = static_cast<unsigned short>(endPointIndex + 1);
  outline->outline.n_contours = static_cast<unsigned short>(outline->contours.size());
  outline->outline.flags = evenOdd ? FT_OUTLINE_EVEN_ODD_FILL : FT_OUTLINE_NONE;
  return true;
}

}  // namespace tgfx