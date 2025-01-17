// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../../../third_party/base/nonstd_unique_ptr.h"
#include "../../../../third_party/zlib_v128/zlib.h"
#include "../../../include/fxcodec/fx_codec.h"
#include "../../../include/fxcodec/fx_codec_flate.h"
#include "codec_int.h"

extern "C" {
static void* my_alloc_func(void* opaque,
                           unsigned int items,
                           unsigned int size) {
  return FX_Alloc2D(uint8_t, items, size);
}
static void my_free_func(void* opaque, void* address) {
  FX_Free(address);
}
static int FPDFAPI_FlateGetTotalOut(void* context) {
  return ((z_stream*)context)->total_out;
}
static int FPDFAPI_FlateGetTotalIn(void* context) {
  return ((z_stream*)context)->total_in;
}
static void FPDFAPI_FlateCompress(unsigned char* dest_buf,
                                  unsigned long* dest_size,
                                  const unsigned char* src_buf,
                                  unsigned long src_size) {
  compress(dest_buf, dest_size, src_buf, src_size);
}
void* FPDFAPI_FlateInit(void* (*alloc_func)(void*, unsigned int, unsigned int),
                        void (*free_func)(void*, void*)) {
  z_stream* p = (z_stream*)alloc_func(0, 1, sizeof(z_stream));
  if (p == NULL) {
    return NULL;
  }
  FXSYS_memset(p, 0, sizeof(z_stream));
  p->zalloc = alloc_func;
  p->zfree = free_func;
  inflateInit(p);
  return p;
}
void FPDFAPI_FlateInput(void* context,
                        const unsigned char* src_buf,
                        unsigned int src_size) {
  ((z_stream*)context)->next_in = (unsigned char*)src_buf;
  ((z_stream*)context)->avail_in = src_size;
}
int FPDFAPI_FlateOutput(void* context,
                        unsigned char* dest_buf,
                        unsigned int dest_size) {
  ((z_stream*)context)->next_out = dest_buf;
  ((z_stream*)context)->avail_out = dest_size;
  unsigned int pre_pos = (unsigned int)FPDFAPI_FlateGetTotalOut(context);
  int ret = inflate((z_stream*)context, Z_SYNC_FLUSH);
  unsigned int post_pos = (unsigned int)FPDFAPI_FlateGetTotalOut(context);
  unsigned int written = post_pos - pre_pos;
  if (written < dest_size) {
    FXSYS_memset(dest_buf + written, '\0', dest_size - written);
  }
  return ret;
}
int FPDFAPI_FlateGetAvailIn(void* context) {
  return ((z_stream*)context)->avail_in;
}
int FPDFAPI_FlateGetAvailOut(void* context) {
  return ((z_stream*)context)->avail_out;
}
void FPDFAPI_FlateEnd(void* context) {
  inflateEnd((z_stream*)context);
  ((z_stream*)context)->zfree(0, context);
}
}  // extern "C"

namespace {

class CLZWDecoder {
 public:
  int Decode(uint8_t* output,
             FX_DWORD& outlen,
             const uint8_t* input,
             FX_DWORD& size,
             FX_BOOL bEarlyChange);

 private:
  void AddCode(FX_DWORD prefix_code, uint8_t append_char);
  void DecodeString(FX_DWORD code);

  FX_DWORD m_InPos;
  FX_DWORD m_OutPos;
  uint8_t* m_pOutput;
  const uint8_t* m_pInput;
  FX_BOOL m_Early;
  FX_DWORD m_CodeArray[5021];
  FX_DWORD m_nCodes;
  uint8_t m_DecodeStack[4000];
  FX_DWORD m_StackLen;
  int m_CodeLen;
};
void CLZWDecoder::AddCode(FX_DWORD prefix_code, uint8_t append_char) {
  if (m_nCodes + m_Early == 4094) {
    return;
  }
  m_CodeArray[m_nCodes++] = (prefix_code << 16) | append_char;
  if (m_nCodes + m_Early == 512 - 258) {
    m_CodeLen = 10;
  } else if (m_nCodes + m_Early == 1024 - 258) {
    m_CodeLen = 11;
  } else if (m_nCodes + m_Early == 2048 - 258) {
    m_CodeLen = 12;
  }
}
void CLZWDecoder::DecodeString(FX_DWORD code) {
  while (1) {
    int index = code - 258;
    if (index < 0 || index >= (int)m_nCodes) {
      break;
    }
    FX_DWORD data = m_CodeArray[index];
    if (m_StackLen >= sizeof(m_DecodeStack)) {
      return;
    }
    m_DecodeStack[m_StackLen++] = (uint8_t)data;
    code = data >> 16;
  }
  if (m_StackLen >= sizeof(m_DecodeStack)) {
    return;
  }
  m_DecodeStack[m_StackLen++] = (uint8_t)code;
}
int CLZWDecoder::Decode(uint8_t* dest_buf,
                        FX_DWORD& dest_size,
                        const uint8_t* src_buf,
                        FX_DWORD& src_size,
                        FX_BOOL bEarlyChange) {
  m_CodeLen = 9;
  m_InPos = 0;
  m_OutPos = 0;
  m_pInput = src_buf;
  m_pOutput = dest_buf;
  m_Early = bEarlyChange ? 1 : 0;
  m_nCodes = 0;
  FX_DWORD old_code = (FX_DWORD)-1;
  uint8_t last_char;
  while (1) {
    if (m_InPos + m_CodeLen > src_size * 8) {
      break;
    }
    int byte_pos = m_InPos / 8;
    int bit_pos = m_InPos % 8, bit_left = m_CodeLen;
    FX_DWORD code = 0;
    if (bit_pos) {
      bit_left -= 8 - bit_pos;
      code = (m_pInput[byte_pos++] & ((1 << (8 - bit_pos)) - 1)) << bit_left;
    }
    if (bit_left < 8) {
      code |= m_pInput[byte_pos] >> (8 - bit_left);
    } else {
      bit_left -= 8;
      code |= m_pInput[byte_pos++] << bit_left;
      if (bit_left) {
        code |= m_pInput[byte_pos] >> (8 - bit_left);
      }
    }
    m_InPos += m_CodeLen;
    if (code < 256) {
      if (m_OutPos == dest_size) {
        return -5;
      }
      if (m_pOutput) {
        m_pOutput[m_OutPos] = (uint8_t)code;
      }
      m_OutPos++;
      last_char = (uint8_t)code;
      if (old_code != (FX_DWORD)-1) {
        AddCode(old_code, last_char);
      }
      old_code = code;
    } else if (code == 256) {
      m_CodeLen = 9;
      m_nCodes = 0;
      old_code = (FX_DWORD)-1;
    } else if (code == 257) {
      break;
    } else {
      if (old_code == (FX_DWORD)-1) {
        return 2;
      }
      m_StackLen = 0;
      if (code >= m_nCodes + 258) {
        if (m_StackLen < sizeof(m_DecodeStack)) {
          m_DecodeStack[m_StackLen++] = last_char;
        }
        DecodeString(old_code);
      } else {
        DecodeString(code);
      }
      if (m_OutPos + m_StackLen > dest_size) {
        return -5;
      }
      if (m_pOutput) {
        for (FX_DWORD i = 0; i < m_StackLen; i++) {
          m_pOutput[m_OutPos + i] = m_DecodeStack[m_StackLen - i - 1];
        }
      }
      m_OutPos += m_StackLen;
      last_char = m_DecodeStack[m_StackLen - 1];
      if (old_code < 256) {
        AddCode(old_code, last_char);
      } else if (old_code - 258 >= m_nCodes) {
        dest_size = m_OutPos;
        src_size = (m_InPos + 7) / 8;
        return 0;
      } else {
        AddCode(old_code, last_char);
      }
      old_code = code;
    }
  }
  dest_size = m_OutPos;
  src_size = (m_InPos + 7) / 8;
  return 0;
}

uint8_t PaethPredictor(int a, int b, int c) {
  int p = a + b - c;
  int pa = FXSYS_abs(p - a);
  int pb = FXSYS_abs(p - b);
  int pc = FXSYS_abs(p - c);
  if (pa <= pb && pa <= pc) {
    return (uint8_t)a;
  }
  if (pb <= pc) {
    return (uint8_t)b;
  }
  return (uint8_t)c;
}

FX_BOOL PNG_PredictorEncode(uint8_t*& data_buf,
                            FX_DWORD& data_size,
                            int predictor,
                            int Colors,
                            int BitsPerComponent,
                            int Columns) {
  const int BytesPerPixel = (Colors * BitsPerComponent + 7) / 8;
  const int row_size = (Colors * BitsPerComponent * Columns + 7) / 8;
  if (row_size <= 0)
    return FALSE;
  const int row_count = (data_size + row_size - 1) / row_size;
  const int last_row_size = data_size % row_size;
  uint8_t* dest_buf = FX_Alloc2D(uint8_t, row_size + 1, row_count);
  int byte_cnt = 0;
  uint8_t* pSrcData = data_buf;
  uint8_t* pDestData = dest_buf;
  for (int row = 0; row < row_count; row++) {
    if (predictor == 10) {
      pDestData[0] = 0;
      int move_size = row_size;
      if (move_size * (row + 1) > (int)data_size) {
        move_size = data_size - (move_size * row);
      }
      FXSYS_memmove(pDestData + 1, pSrcData, move_size);
      pDestData += (move_size + 1);
      pSrcData += move_size;
      byte_cnt += move_size;
      continue;
    }
    for (int byte = 0; byte < row_size && byte_cnt < (int)data_size; byte++) {
      switch (predictor) {
        case 11: {
          pDestData[0] = 1;
          uint8_t left = 0;
          if (byte >= BytesPerPixel) {
            left = pSrcData[byte - BytesPerPixel];
          }
          pDestData[byte + 1] = pSrcData[byte] - left;
        } break;
        case 12: {
          pDestData[0] = 2;
          uint8_t up = 0;
          if (row) {
            up = pSrcData[byte - row_size];
          }
          pDestData[byte + 1] = pSrcData[byte] - up;
        } break;
        case 13: {
          pDestData[0] = 3;
          uint8_t left = 0;
          if (byte >= BytesPerPixel) {
            left = pSrcData[byte - BytesPerPixel];
          }
          uint8_t up = 0;
          if (row) {
            up = pSrcData[byte - row_size];
          }
          pDestData[byte + 1] = pSrcData[byte] - (left + up) / 2;
        } break;
        case 14: {
          pDestData[0] = 4;
          uint8_t left = 0;
          if (byte >= BytesPerPixel) {
            left = pSrcData[byte - BytesPerPixel];
          }
          uint8_t up = 0;
          if (row) {
            up = pSrcData[byte - row_size];
          }
          uint8_t upper_left = 0;
          if (byte >= BytesPerPixel && row) {
            upper_left = pSrcData[byte - row_size - BytesPerPixel];
          }
          pDestData[byte + 1] =
              pSrcData[byte] - PaethPredictor(left, up, upper_left);
        } break;
        default: { pDestData[byte + 1] = pSrcData[byte]; } break;
      }
      byte_cnt++;
    }
    pDestData += (row_size + 1);
    pSrcData += row_size;
  }
  FX_Free(data_buf);
  data_buf = dest_buf;
  data_size = (row_size + 1) * row_count -
              (last_row_size > 0 ? (row_size - last_row_size) : 0);
  return TRUE;
}

void PNG_PredictLine(uint8_t* pDestData,
                     const uint8_t* pSrcData,
                     const uint8_t* pLastLine,
                     int bpc,
                     int nColors,
                     int nPixels) {
  int row_size = (nPixels * bpc * nColors + 7) / 8;
  int BytesPerPixel = (bpc * nColors + 7) / 8;
  uint8_t tag = pSrcData[0];
  if (tag == 0) {
    FXSYS_memmove(pDestData, pSrcData + 1, row_size);
    return;
  }
  for (int byte = 0; byte < row_size; byte++) {
    uint8_t raw_byte = pSrcData[byte + 1];
    switch (tag) {
      case 1: {
        uint8_t left = 0;
        if (byte >= BytesPerPixel) {
          left = pDestData[byte - BytesPerPixel];
        }
        pDestData[byte] = raw_byte + left;
        break;
      }
      case 2: {
        uint8_t up = 0;
        if (pLastLine) {
          up = pLastLine[byte];
        }
        pDestData[byte] = raw_byte + up;
        break;
      }
      case 3: {
        uint8_t left = 0;
        if (byte >= BytesPerPixel) {
          left = pDestData[byte - BytesPerPixel];
        }
        uint8_t up = 0;
        if (pLastLine) {
          up = pLastLine[byte];
        }
        pDestData[byte] = raw_byte + (up + left) / 2;
        break;
      }
      case 4: {
        uint8_t left = 0;
        if (byte >= BytesPerPixel) {
          left = pDestData[byte - BytesPerPixel];
        }
        uint8_t up = 0;
        if (pLastLine) {
          up = pLastLine[byte];
        }
        uint8_t upper_left = 0;
        if (byte >= BytesPerPixel && pLastLine) {
          upper_left = pLastLine[byte - BytesPerPixel];
        }
        pDestData[byte] = raw_byte + PaethPredictor(left, up, upper_left);
        break;
      }
      default:
        pDestData[byte] = raw_byte;
        break;
    }
  }
}

FX_BOOL PNG_Predictor(uint8_t*& data_buf,
                      FX_DWORD& data_size,
                      int Colors,
                      int BitsPerComponent,
                      int Columns) {
  const int BytesPerPixel = (Colors * BitsPerComponent + 7) / 8;
  const int row_size = (Colors * BitsPerComponent * Columns + 7) / 8;
  if (row_size <= 0)
    return FALSE;
  const int row_count = (data_size + row_size) / (row_size + 1);
  if (row_count <= 0)
    return FALSE;
  const int last_row_size = data_size % (row_size + 1);
  uint8_t* dest_buf = FX_Alloc2D(uint8_t, row_size, row_count);
  int byte_cnt = 0;
  uint8_t* pSrcData = data_buf;
  uint8_t* pDestData = dest_buf;
  for (int row = 0; row < row_count; row++) {
    uint8_t tag = pSrcData[0];
    byte_cnt++;
    if (tag == 0) {
      int move_size = row_size;
      if ((row + 1) * (move_size + 1) > (int)data_size) {
        move_size = last_row_size - 1;
      }
      FXSYS_memmove(pDestData, pSrcData + 1, move_size);
      pSrcData += move_size + 1;
      pDestData += move_size;
      byte_cnt += move_size;
      continue;
    }
    for (int byte = 0; byte < row_size && byte_cnt < (int)data_size; byte++) {
      uint8_t raw_byte = pSrcData[byte + 1];
      switch (tag) {
        case 1: {
          uint8_t left = 0;
          if (byte >= BytesPerPixel) {
            left = pDestData[byte - BytesPerPixel];
          }
          pDestData[byte] = raw_byte + left;
          break;
        }
        case 2: {
          uint8_t up = 0;
          if (row) {
            up = pDestData[byte - row_size];
          }
          pDestData[byte] = raw_byte + up;
          break;
        }
        case 3: {
          uint8_t left = 0;
          if (byte >= BytesPerPixel) {
            left = pDestData[byte - BytesPerPixel];
          }
          uint8_t up = 0;
          if (row) {
            up = pDestData[byte - row_size];
          }
          pDestData[byte] = raw_byte + (up + left) / 2;
          break;
        }
        case 4: {
          uint8_t left = 0;
          if (byte >= BytesPerPixel) {
            left = pDestData[byte - BytesPerPixel];
          }
          uint8_t up = 0;
          if (row) {
            up = pDestData[byte - row_size];
          }
          uint8_t upper_left = 0;
          if (byte >= BytesPerPixel && row) {
            upper_left = pDestData[byte - row_size - BytesPerPixel];
          }
          pDestData[byte] = raw_byte + PaethPredictor(left, up, upper_left);
          break;
        }
        default:
          pDestData[byte] = raw_byte;
          break;
      }
      byte_cnt++;
    }
    pSrcData += row_size + 1;
    pDestData += row_size;
  }
  FX_Free(data_buf);
  data_buf = dest_buf;
  data_size = row_size * row_count -
              (last_row_size > 0 ? (row_size + 1 - last_row_size) : 0);
  return TRUE;
}

void TIFF_PredictorEncodeLine(uint8_t* dest_buf,
                              int row_size,
                              int BitsPerComponent,
                              int Colors,
                              int Columns) {
  int BytesPerPixel = BitsPerComponent * Colors / 8;
  if (BitsPerComponent < 8) {
    uint8_t mask = 0x01;
    if (BitsPerComponent == 2) {
      mask = 0x03;
    } else if (BitsPerComponent == 4) {
      mask = 0x0F;
    }
    int row_bits = Colors * BitsPerComponent * Columns;
    for (int i = row_bits - BitsPerComponent; i >= BitsPerComponent;
         i -= BitsPerComponent) {
      int col = i % 8;
      int index = i / 8;
      int col_pre =
          (col == 0) ? (8 - BitsPerComponent) : (col - BitsPerComponent);
      int index_pre = (col == 0) ? (index - 1) : index;
      uint8_t cur = (dest_buf[index] >> (8 - col - BitsPerComponent)) & mask;
      uint8_t left =
          (dest_buf[index_pre] >> (8 - col_pre - BitsPerComponent)) & mask;
      cur -= left;
      cur &= mask;
      cur <<= (8 - col - BitsPerComponent);
      dest_buf[index] &= ~(mask << ((8 - col - BitsPerComponent)));
      dest_buf[index] |= cur;
    }
  } else if (BitsPerComponent == 8) {
    for (int i = row_size - 1; i >= BytesPerPixel; i--) {
      dest_buf[i] -= dest_buf[i - BytesPerPixel];
    }
  } else {
    for (int i = row_size - BytesPerPixel; i >= BytesPerPixel;
         i -= BytesPerPixel) {
      FX_WORD pixel = (dest_buf[i] << 8) | dest_buf[i + 1];
      pixel -=
          (dest_buf[i - BytesPerPixel] << 8) | dest_buf[i - BytesPerPixel + 1];
      dest_buf[i] = pixel >> 8;
      dest_buf[i + 1] = (uint8_t)pixel;
    }
  }
}

FX_BOOL TIFF_PredictorEncode(uint8_t*& data_buf,
                             FX_DWORD& data_size,
                             int Colors,
                             int BitsPerComponent,
                             int Columns) {
  int row_size = (Colors * BitsPerComponent * Columns + 7) / 8;
  if (row_size == 0)
    return FALSE;
  const int row_count = (data_size + row_size - 1) / row_size;
  const int last_row_size = data_size % row_size;
  for (int row = 0; row < row_count; row++) {
    uint8_t* scan_line = data_buf + row * row_size;
    if ((row + 1) * row_size > (int)data_size) {
      row_size = last_row_size;
    }
    TIFF_PredictorEncodeLine(scan_line, row_size, BitsPerComponent, Colors,
                             Columns);
  }
  return TRUE;
}

void TIFF_PredictLine(uint8_t* dest_buf,
                      int row_size,
                      int BitsPerComponent,
                      int Colors,
                      int Columns) {
  if (BitsPerComponent == 1) {
    int row_bits = FX_MIN(BitsPerComponent * Colors * Columns, row_size * 8);
    int index_pre = 0;
    int col_pre = 0;
    for (int i = 1; i < row_bits; i++) {
      int col = i % 8;
      int index = i / 8;
      if (((dest_buf[index] >> (7 - col)) & 1) ^
          ((dest_buf[index_pre] >> (7 - col_pre)) & 1)) {
        dest_buf[index] |= 1 << (7 - col);
      } else {
        dest_buf[index] &= ~(1 << (7 - col));
      }
      index_pre = index;
      col_pre = col;
    }
    return;
  }
  int BytesPerPixel = BitsPerComponent * Colors / 8;
  if (BitsPerComponent == 16) {
    for (int i = BytesPerPixel; i < row_size; i += 2) {
      FX_WORD pixel =
          (dest_buf[i - BytesPerPixel] << 8) | dest_buf[i - BytesPerPixel + 1];
      pixel += (dest_buf[i] << 8) | dest_buf[i + 1];
      dest_buf[i] = pixel >> 8;
      dest_buf[i + 1] = (uint8_t)pixel;
    }
  } else {
    for (int i = BytesPerPixel; i < row_size; i++) {
      dest_buf[i] += dest_buf[i - BytesPerPixel];
    }
  }
}

FX_BOOL TIFF_Predictor(uint8_t*& data_buf,
                       FX_DWORD& data_size,
                       int Colors,
                       int BitsPerComponent,
                       int Columns) {
  int row_size = (Colors * BitsPerComponent * Columns + 7) / 8;
  if (row_size == 0)
    return FALSE;
  const int row_count = (data_size + row_size - 1) / row_size;
  const int last_row_size = data_size % row_size;
  for (int row = 0; row < row_count; row++) {
    uint8_t* scan_line = data_buf + row * row_size;
    if ((row + 1) * row_size > (int)data_size) {
      row_size = last_row_size;
    }
    TIFF_PredictLine(scan_line, row_size, BitsPerComponent, Colors, Columns);
  }
  return TRUE;
}

void FlateUncompress(const uint8_t* src_buf,
                     FX_DWORD src_size,
                     FX_DWORD orig_size,
                     uint8_t*& dest_buf,
                     FX_DWORD& dest_size,
                     FX_DWORD& offset) {
  FX_DWORD guess_size = orig_size ? orig_size : src_size * 2;
  const FX_DWORD kStepSize = 10240;
  FX_DWORD alloc_step = orig_size ? kStepSize : std::min(src_size, kStepSize);
  static const FX_DWORD kMaxInitialAllocSize = 10000000;
  if (guess_size > kMaxInitialAllocSize) {
    guess_size = kMaxInitialAllocSize;
    alloc_step = kMaxInitialAllocSize;
  }
  FX_DWORD buf_size = guess_size;
  FX_DWORD last_buf_size = buf_size;

  dest_buf = nullptr;
  dest_size = 0;
  void* context = FPDFAPI_FlateInit(my_alloc_func, my_free_func);
  if (!context)
    return;

  nonstd::unique_ptr<uint8_t, FxFreeDeleter> guess_buf(
      FX_Alloc(uint8_t, guess_size + 1));
  guess_buf.get()[guess_size] = '\0';

  FPDFAPI_FlateInput(context, src_buf, src_size);

  if (src_size < kStepSize) {
    // This is the old implementation.
    uint8_t* cur_buf = guess_buf.get();
    while (1) {
      int32_t ret = FPDFAPI_FlateOutput(context, cur_buf, buf_size);
      if (ret != Z_OK)
        break;
      int32_t avail_buf_size = FPDFAPI_FlateGetAvailOut(context);
      if (avail_buf_size != 0)
        break;

      FX_DWORD old_size = guess_size;
      guess_size += alloc_step;
      if (guess_size < old_size || guess_size + 1 < guess_size) {
        FPDFAPI_FlateEnd(context);
        return;
      }

      {
        uint8_t* new_buf =
            FX_Realloc(uint8_t, guess_buf.release(), guess_size + 1);
        guess_buf.reset(new_buf);
      }
      guess_buf.get()[guess_size] = '\0';
      cur_buf = guess_buf.get() + old_size;
      buf_size = guess_size - old_size;
    }
    dest_size = FPDFAPI_FlateGetTotalOut(context);
    offset = FPDFAPI_FlateGetTotalIn(context);
    if (guess_size / 2 > dest_size) {
      {
        uint8_t* new_buf =
            FX_Realloc(uint8_t, guess_buf.release(), dest_size + 1);
        guess_buf.reset(new_buf);
      }
      guess_size = dest_size;
      guess_buf.get()[guess_size] = '\0';
    }
    dest_buf = guess_buf.release();
  } else {
    CFX_ArrayTemplate<uint8_t*> result_tmp_bufs;
    uint8_t* cur_buf = guess_buf.release();
    while (1) {
      int32_t ret = FPDFAPI_FlateOutput(context, cur_buf, buf_size);
      int32_t avail_buf_size = FPDFAPI_FlateGetAvailOut(context);
      if (ret != Z_OK) {
        last_buf_size = buf_size - avail_buf_size;
        result_tmp_bufs.Add(cur_buf);
        break;
      }
      if (avail_buf_size != 0) {
        last_buf_size = buf_size - avail_buf_size;
        result_tmp_bufs.Add(cur_buf);
        break;
      }

      result_tmp_bufs.Add(cur_buf);
      cur_buf = FX_Alloc(uint8_t, buf_size + 1);
      cur_buf[buf_size] = '\0';
    }
    dest_size = FPDFAPI_FlateGetTotalOut(context);
    offset = FPDFAPI_FlateGetTotalIn(context);
    if (result_tmp_bufs.GetSize() == 1) {
      dest_buf = result_tmp_bufs[0];
    } else {
      uint8_t* result_buf = FX_Alloc(uint8_t, dest_size);
      FX_DWORD result_pos = 0;
      for (int32_t i = 0; i < result_tmp_bufs.GetSize(); i++) {
        uint8_t* tmp_buf = result_tmp_bufs[i];
        FX_DWORD tmp_buf_size = buf_size;
        if (i == result_tmp_bufs.GetSize() - 1) {
          tmp_buf_size = last_buf_size;
        }
        FXSYS_memcpy(result_buf + result_pos, tmp_buf, tmp_buf_size);
        result_pos += tmp_buf_size;
        FX_Free(result_tmp_bufs[i]);
      }
      dest_buf = result_buf;
    }
  }
  FPDFAPI_FlateEnd(context);
}

}  // namespace

class CCodec_FlateScanlineDecoder : public CCodec_ScanlineDecoder {
 public:
  CCodec_FlateScanlineDecoder();
  ~CCodec_FlateScanlineDecoder() override;

  void Create(const uint8_t* src_buf,
              FX_DWORD src_size,
              int width,
              int height,
              int nComps,
              int bpc,
              int predictor,
              int Colors,
              int BitsPerComponent,
              int Columns);
  void Destroy() { delete this; }

  // CCodec_ScanlineDecoder
  void v_DownScale(int dest_width, int dest_height) override {}
  FX_BOOL v_Rewind() override;
  uint8_t* v_GetNextLine() override;
  FX_DWORD GetSrcOffset() override;

  void* m_pFlate;
  const uint8_t* m_SrcBuf;
  FX_DWORD m_SrcSize;
  uint8_t* m_pScanline;
  uint8_t* m_pLastLine;
  uint8_t* m_pPredictBuffer;
  uint8_t* m_pPredictRaw;
  int m_Predictor;
  int m_Colors, m_BitsPerComponent, m_Columns, m_PredictPitch, m_LeftOver;
};

CCodec_FlateScanlineDecoder::CCodec_FlateScanlineDecoder() {
  m_pFlate = NULL;
  m_pScanline = NULL;
  m_pLastLine = NULL;
  m_pPredictBuffer = NULL;
  m_pPredictRaw = NULL;
  m_LeftOver = 0;
}
CCodec_FlateScanlineDecoder::~CCodec_FlateScanlineDecoder() {
  FX_Free(m_pScanline);
  FX_Free(m_pLastLine);
  FX_Free(m_pPredictBuffer);
  FX_Free(m_pPredictRaw);
  if (m_pFlate) {
    FPDFAPI_FlateEnd(m_pFlate);
  }
}
void CCodec_FlateScanlineDecoder::Create(const uint8_t* src_buf,
                                         FX_DWORD src_size,
                                         int width,
                                         int height,
                                         int nComps,
                                         int bpc,
                                         int predictor,
                                         int Colors,
                                         int BitsPerComponent,
                                         int Columns) {
  m_SrcBuf = src_buf;
  m_SrcSize = src_size;
  m_OutputWidth = m_OrigWidth = width;
  m_OutputHeight = m_OrigHeight = height;
  m_nComps = nComps;
  m_bpc = bpc;
  m_bColorTransformed = FALSE;
  m_Pitch = (width * nComps * bpc + 7) / 8;
  m_pScanline = FX_Alloc(uint8_t, m_Pitch);
  m_Predictor = 0;
  if (predictor) {
    if (predictor >= 10) {
      m_Predictor = 2;
    } else if (predictor == 2) {
      m_Predictor = 1;
    }
    if (m_Predictor) {
      if (BitsPerComponent * Colors * Columns == 0) {
        BitsPerComponent = m_bpc;
        Colors = m_nComps;
        Columns = m_OrigWidth;
      }
      m_Colors = Colors;
      m_BitsPerComponent = BitsPerComponent;
      m_Columns = Columns;
      m_PredictPitch = (m_BitsPerComponent * m_Colors * m_Columns + 7) / 8;
      m_pLastLine = FX_Alloc(uint8_t, m_PredictPitch);
      m_pPredictRaw = FX_Alloc(uint8_t, m_PredictPitch + 1);
      m_pPredictBuffer = FX_Alloc(uint8_t, m_PredictPitch);
    }
  }
}
FX_BOOL CCodec_FlateScanlineDecoder::v_Rewind() {
  if (m_pFlate) {
    FPDFAPI_FlateEnd(m_pFlate);
  }
  m_pFlate = FPDFAPI_FlateInit(my_alloc_func, my_free_func);
  if (m_pFlate == NULL) {
    return FALSE;
  }
  FPDFAPI_FlateInput(m_pFlate, m_SrcBuf, m_SrcSize);
  m_LeftOver = 0;
  return TRUE;
}
uint8_t* CCodec_FlateScanlineDecoder::v_GetNextLine() {
  if (m_Predictor) {
    if (m_Pitch == m_PredictPitch) {
      if (m_Predictor == 2) {
        FPDFAPI_FlateOutput(m_pFlate, m_pPredictRaw, m_PredictPitch + 1);
        PNG_PredictLine(m_pScanline, m_pPredictRaw, m_pLastLine,
                        m_BitsPerComponent, m_Colors, m_Columns);
        FXSYS_memcpy(m_pLastLine, m_pScanline, m_PredictPitch);
      } else {
        FPDFAPI_FlateOutput(m_pFlate, m_pScanline, m_Pitch);
        TIFF_PredictLine(m_pScanline, m_PredictPitch, m_bpc, m_nComps,
                         m_OutputWidth);
      }
    } else {
      int bytes_to_go = m_Pitch;
      int read_leftover = m_LeftOver > bytes_to_go ? bytes_to_go : m_LeftOver;
      if (read_leftover) {
        FXSYS_memcpy(m_pScanline,
                     m_pPredictBuffer + m_PredictPitch - m_LeftOver,
                     read_leftover);
        m_LeftOver -= read_leftover;
        bytes_to_go -= read_leftover;
      }
      while (bytes_to_go) {
        if (m_Predictor == 2) {
          FPDFAPI_FlateOutput(m_pFlate, m_pPredictRaw, m_PredictPitch + 1);
          PNG_PredictLine(m_pPredictBuffer, m_pPredictRaw, m_pLastLine,
                          m_BitsPerComponent, m_Colors, m_Columns);
          FXSYS_memcpy(m_pLastLine, m_pPredictBuffer, m_PredictPitch);
        } else {
          FPDFAPI_FlateOutput(m_pFlate, m_pPredictBuffer, m_PredictPitch);
          TIFF_PredictLine(m_pPredictBuffer, m_PredictPitch, m_BitsPerComponent,
                           m_Colors, m_Columns);
        }
        int read_bytes =
            m_PredictPitch > bytes_to_go ? bytes_to_go : m_PredictPitch;
        FXSYS_memcpy(m_pScanline + m_Pitch - bytes_to_go, m_pPredictBuffer,
                     read_bytes);
        m_LeftOver += m_PredictPitch - read_bytes;
        bytes_to_go -= read_bytes;
      }
    }
  } else {
    FPDFAPI_FlateOutput(m_pFlate, m_pScanline, m_Pitch);
  }
  return m_pScanline;
}
FX_DWORD CCodec_FlateScanlineDecoder::GetSrcOffset() {
  return FPDFAPI_FlateGetTotalIn(m_pFlate);
}

ICodec_ScanlineDecoder* CCodec_FlateModule::CreateDecoder(
    const uint8_t* src_buf,
    FX_DWORD src_size,
    int width,
    int height,
    int nComps,
    int bpc,
    int predictor,
    int Colors,
    int BitsPerComponent,
    int Columns) {
  CCodec_FlateScanlineDecoder* pDecoder = new CCodec_FlateScanlineDecoder;
  pDecoder->Create(src_buf, src_size, width, height, nComps, bpc, predictor,
                   Colors, BitsPerComponent, Columns);
  return pDecoder;
}
FX_DWORD CCodec_FlateModule::FlateOrLZWDecode(FX_BOOL bLZW,
                                              const uint8_t* src_buf,
                                              FX_DWORD src_size,
                                              FX_BOOL bEarlyChange,
                                              int predictor,
                                              int Colors,
                                              int BitsPerComponent,
                                              int Columns,
                                              FX_DWORD estimated_size,
                                              uint8_t*& dest_buf,
                                              FX_DWORD& dest_size) {
  dest_buf = NULL;
  FX_DWORD offset = 0;
  int predictor_type = 0;
  if (predictor) {
    if (predictor >= 10) {
      predictor_type = 2;
    } else if (predictor == 2) {
      predictor_type = 1;
    }
  }
  if (bLZW) {
    {
      nonstd::unique_ptr<CLZWDecoder> decoder(new CLZWDecoder);
      dest_size = (FX_DWORD)-1;
      offset = src_size;
      int err = decoder->Decode(NULL, dest_size, src_buf, offset, bEarlyChange);
      if (err || dest_size == 0 || dest_size + 1 < dest_size) {
        return -1;
      }
    }
    {
      nonstd::unique_ptr<CLZWDecoder> decoder(new CLZWDecoder);
      dest_buf = FX_Alloc(uint8_t, dest_size + 1);
      dest_buf[dest_size] = '\0';
      decoder->Decode(dest_buf, dest_size, src_buf, offset, bEarlyChange);
    }
  } else {
    FlateUncompress(src_buf, src_size, estimated_size, dest_buf, dest_size,
                    offset);
  }
  if (predictor_type == 0) {
    return offset;
  }
  FX_BOOL ret = TRUE;
  if (predictor_type == 2) {
    ret = PNG_Predictor(dest_buf, dest_size, Colors, BitsPerComponent, Columns);
  } else if (predictor_type == 1) {
    ret =
        TIFF_Predictor(dest_buf, dest_size, Colors, BitsPerComponent, Columns);
  }
  return ret ? offset : -1;
}
FX_BOOL CCodec_FlateModule::Encode(const uint8_t* src_buf,
                                   FX_DWORD src_size,
                                   int predictor,
                                   int Colors,
                                   int BitsPerComponent,
                                   int Columns,
                                   uint8_t*& dest_buf,
                                   FX_DWORD& dest_size) {
  if (predictor != 2 && predictor < 10) {
    return Encode(src_buf, src_size, dest_buf, dest_size);
  }
  uint8_t* pSrcBuf = NULL;
  pSrcBuf = FX_Alloc(uint8_t, src_size);
  FXSYS_memcpy(pSrcBuf, src_buf, src_size);
  FX_BOOL ret = TRUE;
  if (predictor == 2) {
    ret = TIFF_PredictorEncode(pSrcBuf, src_size, Colors, BitsPerComponent,
                               Columns);
  } else if (predictor >= 10) {
    ret = PNG_PredictorEncode(pSrcBuf, src_size, predictor, Colors,
                              BitsPerComponent, Columns);
  }
  if (ret)
    ret = Encode(pSrcBuf, src_size, dest_buf, dest_size);
  FX_Free(pSrcBuf);
  return ret;
}
FX_BOOL CCodec_FlateModule::Encode(const uint8_t* src_buf,
                                   FX_DWORD src_size,
                                   uint8_t*& dest_buf,
                                   FX_DWORD& dest_size) {
  dest_size = src_size + src_size / 1000 + 12;
  dest_buf = FX_Alloc(uint8_t, dest_size);
  unsigned long temp_size = dest_size;
  FPDFAPI_FlateCompress(dest_buf, &temp_size, src_buf, src_size);
  dest_size = (FX_DWORD)temp_size;
  return TRUE;
}
