/*
 * Copyright (c) 2019 Arduino SA. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "SHA.h"

SHAClass::SHAClass(int blockSize, int digestSize) :
  _blockSize(blockSize),
  _digestSize(digestSize),
  _digestIndex(digestSize)
{
  _digest = (uint8_t*)malloc(_digestSize);
  _secret = (uint8_t*)malloc(_blockSize);
}

SHAClass::~SHAClass()
{
  if (_secret) {
    free(_secret);
    _secret = NULL;
  }

  if (_digest) {
    free(_digest);
    _digest = NULL;
  }
}

int SHAClass::beginHash()
{
  _digestIndex = _digestSize;

  return begin();
}

int SHAClass::endHash()
{
  if (end(_digest) == 0) {
    _digestIndex = _digestSize;
    return 0;
  } else {
    _digestIndex = 0;
    return 1;
  }
}

int SHAClass::beginHmac(const String& secret)
{
  return beginHmac((const uint8_t*)secret.c_str(), secret.length());
}

int SHAClass::beginHmac(const char* secret)
{
  return beginHmac((uint8_t*)secret, strlen(secret));
}

int SHAClass::beginHmac(const byte secret[], int length)
{
  if (length > _blockSize) {
    if (beginHash() == 0) {
      return 0;
    } else if (write(secret, length) != (size_t)length) {
      return 0;
    } else if (endHash() == 0) {
      return 0;
    }
    secret = _digest;
    _secretLength = _digestSize;;
  } else {
    _secretLength = length;
  }

  memset(_secret, 0x00, _blockSize);
  memcpy(_secret, secret, _secretLength);

  if (beginHash() == 0) {
    return 0;
  }

  uint8_t ipad[_blockSize];
  memcpy(ipad, _secret, _blockSize);

  for (int i = 0; i < _blockSize; i++) {
    ipad[i] ^= 0x36;
  }

  return (write(ipad, _blockSize) == (size_t)_blockSize);
}

int SHAClass::endHmac()
{
  if (endHash() == 0) {
    return 0;
  }

  uint8_t opad[_blockSize];
  memcpy(opad, _secret, _blockSize);

  for (int i = 0; i < _blockSize; i++) {
    opad[i] ^= 0x5c;
  }

  if (beginHash() == 0) {
    return 0;
  }

  if (write(opad, _blockSize) != (size_t)_blockSize) {
    return 0;
  }

  if (write(_digest, _digestSize) != (size_t)_digestSize) {
    return 0;
  }

  return endHash();
}

int SHAClass::available()
{
  return (_digestSize - _digestIndex);
}

int SHAClass::read()
{
  if (!available()) {
    return -1;
  }

  return _digest[_digestIndex++];
}

size_t SHAClass::readBytes(char *buffer, size_t length)
{
  int toCopy = available();

  if (toCopy > (int)length) {
    toCopy = length;
  }

  memcpy(buffer, _digest + _digestIndex, toCopy);
  _digestIndex += toCopy;

  return toCopy;
}

int SHAClass::peek()
{
  if (!available()) {
    return -1;
  }

  return _digest[_digestIndex];
}

void SHAClass::flush()
{
  // no-op
}

size_t SHAClass::write(uint8_t data)
{
  return write(&data, sizeof(data));
}

size_t SHAClass::write(const uint8_t *buffer, size_t size)
{
  if (update(buffer, size) == 0) {
    setWriteError();
    return 0;
  }

  return size;
}
