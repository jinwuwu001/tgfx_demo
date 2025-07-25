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

#include "tgfx/core/Buffer.h"
#include <cstring>
#include "core/utils/Log.h"

namespace tgfx {
Buffer::Buffer(size_t size) {
  alloc(size);
}

Buffer::Buffer(const void* data, size_t size) {
  if (data == nullptr || size == 0) {
    return;
  }
  alloc(size);
  if (_data != nullptr) {
    memcpy(_data, data, size);
  }
}

Buffer::Buffer(std::shared_ptr<Data> data) : Buffer(data->data(), data->size()) {
}

Buffer::~Buffer() {
  delete[] _data;
}

bool Buffer::alloc(size_t size) {
  if (_data != nullptr) {
    delete[] _data;
    _data = nullptr;
    _size = 0;
  }
  _data = size > 0 ? new (std::nothrow) uint8_t[size] : nullptr;
  if (_data != nullptr) {
    _size = size;
  }
  return _data != nullptr;
}

std::shared_ptr<Data> Buffer::release() {
  if (isEmpty()) {
    return nullptr;
  }
  auto data = Data::MakeAdopted(_data, _size, Data::DeleteProc);
  _data = nullptr;
  _size = 0;
  return data;
}

void Buffer::reset() {
  if (isEmpty()) {
    return;
  }
  delete[] _data;
  _data = nullptr;
  _size = 0;
}

void Buffer::clear() {
  if (isEmpty()) {
    return;
  }
  memset(_data, 0, _size);
}

std::shared_ptr<Data> Buffer::copyRange(size_t offset, size_t length) {
  length = getClampedLength(offset, length);
  if (length == 0) {
    return nullptr;
  }
  return Data::MakeWithCopy(_data + offset, length);
}

void Buffer::writeRange(size_t offset, size_t length, const void* bytes) {
  length = getClampedLength(offset, length);
  if (length == 0) {
    return;
  }
  memcpy(_data + offset, bytes, length);
}

uint8_t Buffer::operator[](size_t index) const {
  DEBUG_ASSERT(index >= 0 && index < _size);
  return _data[index];
}

uint8_t& Buffer::operator[](size_t index) {
  DEBUG_ASSERT(index >= 0 && index < _size);
  return _data[index];
}

size_t Buffer::getClampedLength(size_t offset, size_t length) const {
  size_t available = _size;
  if (offset >= available || length == 0) {
    return 0;
  }
  available -= offset;
  if (length > available) {
    length = available;
  }
  return length;
}
}  // namespace tgfx
