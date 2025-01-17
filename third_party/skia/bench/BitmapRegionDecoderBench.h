/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef BitmapRegionDecoderBench_DEFINED
#define BitmapRegionDecoderBench_DEFINED

#include "Benchmark.h"
#include "SkBitmapRegionDecoderInterface.h"
#include "SkData.h"
#include "SkImageInfo.h"
#include "SkRefCnt.h"
#include "SkString.h"

/**
 *  Benchmark Android's BitmapRegionDecoder for a particular colorType, sampleSize, and subset.
 *
 *  fStrategy determines which of various implementations is to be used.
 *
 *  nanobench.cpp handles creating benchmarks for interesting scaled subsets.  We strive to test
 *  on real use cases.
 */
class BitmapRegionDecoderBench : public Benchmark {
public:
    // Calls encoded->ref()
    BitmapRegionDecoderBench(const char* basename, SkData* encoded,
            SkBitmapRegionDecoderInterface::Strategy strategy, SkColorType colorType,
            uint32_t sampleSize, const SkIRect& subset);

protected:
    const char* onGetName() override;
    bool isSuitableFor(Backend backend) override;
    void onDraw(int n, SkCanvas* canvas) override;
    void onDelayedSetup() override;

private:
    SkString                                       fName;
    SkAutoTDelete<SkBitmapRegionDecoderInterface>  fBRD;
    SkAutoTUnref<SkData>                           fData;
    const SkBitmapRegionDecoderInterface::Strategy fStrategy;
    const SkColorType                              fColorType;
    const uint32_t                                 fSampleSize;
    const SkIRect                                  fSubset;
    typedef Benchmark INHERITED;
};
#endif // BitmapRegionDecoderBench_DEFINED
