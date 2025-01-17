/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkBitmapRegionDecoderInterface.h"
#include "SkAndroidCodec.h"

/*
 * This class implements SkBitmapRegionDecoder using an SkAndroidCodec.
 */
class SkBitmapRegionCodec : public SkBitmapRegionDecoderInterface {
public:

    /*
     * Takes ownership of pointer to codec
     */
    SkBitmapRegionCodec(SkAndroidCodec* codec);

    bool decodeRegion(SkBitmap* bitmap, SkBitmap::Allocator* allocator,
                      const SkIRect& desiredSubset, int sampleSize,
                      SkColorType colorType, bool requireUnpremul) override;

    bool conversionSupported(SkColorType colorType) override;

    SkEncodedFormat getEncodedFormat() override { return fCodec->getEncodedFormat(); }

private:

    SkAutoTDelete<SkAndroidCodec> fCodec;

    typedef SkBitmapRegionDecoderInterface INHERITED;

};
