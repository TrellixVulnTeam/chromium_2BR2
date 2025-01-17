// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/property_type_converters.h"

namespace mojo {

// static
const std::vector<uint8_t>
    TypeConverter<const std::vector<uint8_t>, Rect>::Convert(
        const Rect& input) {
  std::vector<uint8_t> vec(16);
  vec[0] = (input.x >> 24) & 0xFF;
  vec[1] = (input.x >> 16) & 0xFF;
  vec[2] = (input.x >> 8) & 0xFF;
  vec[3] = input.x & 0xFF;
  vec[4] = (input.y >> 24) & 0xFF;
  vec[5] = (input.y >> 16) & 0xFF;
  vec[6] = (input.y >> 8) & 0xFF;
  vec[7] = input.y & 0xFF;
  vec[8] = (input.width >> 24) & 0xFF;
  vec[9] = (input.width >> 16) & 0xFF;
  vec[10] = (input.width >> 8) & 0xFF;
  vec[11] = input.width & 0xFF;
  vec[12] = (input.height >> 24) & 0xFF;
  vec[13] = (input.height >> 16) & 0xFF;
  vec[14] = (input.height >> 8) & 0xFF;
  vec[15] = input.height & 0xFF;
  return vec;
}

// static
Rect TypeConverter<Rect, const std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  Rect rect;
  rect.x = input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3];
  rect.y = input[4] << 24 | input[5] << 16 | input[6] << 8 | input[7];
  rect.width = input[8] << 24 | input[9] << 16 | input[10] << 8 | input[11];
  rect.height = input[12] << 24 | input[13] << 16 | input[14] << 8 | input[15];
  return rect;
}

// static
const std::vector<uint8_t>
    TypeConverter<const std::vector<uint8_t>, Size>::Convert(
        const Size& input) {
  std::vector<uint8_t> vec(8);
  vec[0] = (input.width >> 24) & 0xFF;
  vec[1] = (input.width >> 16) & 0xFF;
  vec[2] = (input.width >> 8) & 0xFF;
  vec[3] = input.width & 0xFF;
  vec[4] = (input.height >> 24) & 0xFF;
  vec[5] = (input.height >> 16) & 0xFF;
  vec[6] = (input.height >> 8) & 0xFF;
  vec[7] = input.height & 0xFF;
  return vec;
}

// static
Size TypeConverter<Size, const std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  Size size;
  size.width = input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3];
  size.height = input[4] << 24 | input[5] << 16 | input[6] << 8 | input[7];
  return size;
}

// static
const std::vector<uint8_t>
    TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
        const int32_t& input) {
  std::vector<uint8_t> vec(8);
  vec[0] = (input >> 24) & 0xFF;
  vec[1] = (input >> 16) & 0xFF;
  vec[2] = (input >> 8) & 0xFF;
  vec[3] = input & 0xFF;
  return vec;
}

// static
int32_t TypeConverter<int32_t, const std::vector<uint8_t>>::Convert(
    const std::vector<uint8_t>& input) {
  return input[0] << 24 | input[1] << 16 | input[2] << 8 | input[3];
}

}  // namespace mojo

