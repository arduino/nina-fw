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

#ifndef SHA_H
#define SHA_H

#include <Arduino.h>

class SHAClass : public Stream {

public:
  SHAClass(int blockSize, int digestSize);
  virtual ~SHAClass();

  int beginHash();
  int endHash();

  int beginHmac(const String& secret);
  int beginHmac(const char* secret);
  int beginHmac(const byte secret[], int length);
  int endHmac();

  // Stream
  virtual int available();
  virtual int read();
  virtual int peek();
  virtual void flush();

  // Print
  virtual size_t write(uint8_t data);
  virtual size_t write(const uint8_t *buffer, size_t size);

  size_t readBytes(char* buffer, size_t length); // read chars from stream into buffer
  size_t readBytes(uint8_t* buffer, size_t length) { return readBytes((char *)buffer, length); }

protected:
  virtual int begin() = 0;
  virtual int update(const uint8_t *buffer, size_t size) = 0;
  virtual int end(uint8_t *digest) = 0;

private:
  int _blockSize;
  int _digestSize;

  uint8_t* _digest;
  int _digestIndex;
  uint8_t* _secret;
  int _secretLength;
};

#endif
