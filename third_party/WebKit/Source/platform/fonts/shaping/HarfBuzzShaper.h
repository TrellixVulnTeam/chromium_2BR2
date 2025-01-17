/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HarfBuzzShaper_h
#define HarfBuzzShaper_h

#include "hb.h"
#include "platform/fonts/shaping/Shaper.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "wtf/Deque.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CharacterNames.h"

#include <unicode/uscript.h>

namespace blink {

class Font;
class GlyphBuffer;
class SimpleFontData;
class HarfBuzzShaper;

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
public:
    static PassRefPtr<ShapeResult> create(const Font* font,
        unsigned numCharacters, TextDirection direction)
    {
        return adoptRef(new ShapeResult(font, numCharacters, direction));
    }
    static PassRefPtr<ShapeResult> createForTabulationCharacters(const Font*,
        const TextRun&, float positionOffset, unsigned count);
    ~ShapeResult();

    float width() { return m_width; }
    FloatRect bounds() { return m_glyphBoundingBox; }
    unsigned numCharacters() const { return m_numCharacters; }
    void fallbackFonts(HashSet<const SimpleFontData*>*) const;

    static int offsetForPosition(Vector<RefPtr<ShapeResult>>&,
        const TextRun&, float targetX);
    static float fillGlyphBuffer(Vector<RefPtr<ShapeResult>>&,
        GlyphBuffer*, const TextRun&, unsigned from, unsigned to);
    static float fillGlyphBufferForTextEmphasis(Vector<RefPtr<ShapeResult>>&,
        GlyphBuffer*, const TextRun&, const GlyphData* emphasisData,
        unsigned from, unsigned to);
    static FloatRect selectionRect(Vector<RefPtr<ShapeResult>>&,
        TextDirection, float totalWidth, const FloatPoint&, int height,
        unsigned from, unsigned to);

    unsigned numberOfRunsForTesting() const;
    bool runInfoForTesting(unsigned runIndex, unsigned& startIndex,
        unsigned& numGlyphs, hb_script_t&);
    uint16_t glyphForTesting(unsigned runIndex, size_t glyphIndex);
    float advanceForTesting(unsigned runIndex, size_t glyphIndex);

private:
    struct RunInfo;

    ShapeResult(const Font*, unsigned numCharacters, TextDirection);

    int offsetForPosition(float targetX);
    template<TextDirection>
    float fillGlyphBufferForRun(GlyphBuffer*, const RunInfo*,
        float initialAdvance, unsigned from, unsigned to, unsigned runOffset);

    float fillGlyphBufferForTextEmphasisRun(GlyphBuffer*, const RunInfo*,
        const TextRun&, const GlyphData*, float initialAdvance,
        unsigned from, unsigned to, unsigned runOffset);

    float m_width;
    FloatRect m_glyphBoundingBox;
    Vector<OwnPtr<RunInfo>> m_runs;
    RefPtr<SimpleFontData> m_primaryFont;

    unsigned m_numCharacters;
    unsigned m_numGlyphs : 31;

    // Overall direction for the TextRun, dictates which order each individual
    // sub run (represented by RunInfo structs in the m_runs vector) can have a
    // different text direction.
    unsigned m_direction : 1;

    friend class HarfBuzzShaper;
};


// Shaping text runs is split into several stages: Run segmentation, shaping the
// initial segment, identify shaped and non-shaped sequences of the shaping
// result, and processing sub-runs by trying to shape them with a fallback font
// until the last resort font is reached.
//
// Going through one example to illustrate the process: The following is a run of
// vertical text to be shaped. After run segmentation in RunSegmenter it is split
// into 4 segments. The segments indicated by the segementation results showing
// the script, orientation information and small caps handling of the individual
// segment. The Japanese text at the beginning has script "Hiragana", does not
// need rotation when laid out vertically and does not need uppercasing when
// small caps is requested.
//
// 0 い
// 1 ろ
// 2 は USCRIPT_HIRAGANA,
//     OrientationIterator::OrientationKeep,
//     SmallCapsIterator::SmallCapsSameCase
//
// 3 a
// 4 ̄ (Combining Macron)
// 5 a
// 6 A USCRIPT_LATIN,
//     OrientationIterator::OrientationRotateSideways,
//     SmallCapsIterator::SmallCapsUppercaseNeeded
//
// 7 い
// 8 ろ
// 9 は USCRIPT_HIRAGANA,
//      OrientationIterator::OrientationKeep,
//      SmallCapsIterator::SmallCapsSameCase
//
//
// Let's assume the CSS for this text run is as follows:
//     font-family: "Heiti SC", Tinos, sans-serif;
// where Tinos is a web font, defined as a composite font, with two sub ranges,
// one for Latin U+00-U+FF and one unrestricted unicode-range.
//
// FontFallbackIterator provides the shaper with Heiti SC, then Tinos of the
// restricted unicode-range, then the unrestricted full unicode-range Tinos, then
// a system sans-serif.
//
// The initial segment 0-2 to the shaper, together with the segmentation
// properties and the initial Heiti SC font. Characters 0-2 are shaped
// successfully with Heiti SC. The next segment, 3-5 is passed to the shaper. The
// shaper attempts to shape it with Heiti SC, which fails for the Combining
// Macron. So the shaping result for this segment would look similar to this.
//
// Glyphpos: 0 1 2 3
// Cluster:  0 0 2 3
// Glyph:    a x a A (where x is .notdef)
//
// Now in the extractShapeResults() step we notice that there is more work to do,
// since Heiti SC does not have a glyph for the Combining Macron combined with an
// a. So, this cluster together with a Todo item for switching to the next font
// is put into HolesQueue.
//
// After shaping the initial segment, the remaining items in the HolesQueue are
// processed, picking them from the head of the queue. So, first, the next font
// is requested from the FontFallbackIterator. In this case, Tinos (for the range
// U+00-U+FF) comes back. Shaping using this font, assuming it is subsetted,
// fails again since there is no combining mark available. This triggers
// requesting yet another font. This time, the Tinos font for the full
// range. With this, shaping succeeds with the following HarfBuzz result:
//
//  Glyphpos 0 1 2 3
//  Cluster: 0 0 2 3
//  Glyph:   a ̄ a A (with glyph coordinates placing the ̄ above the first a)
//
// Now this sub run is successfully processed and can be appended to
// ShapeResult. A new ShapeResult::RunInfo is created. The logic in
// insertRunIntoShapeResult then takes care of merging the shape result into
// the right position the vector of RunInfos in ShapeResult.
//
// Shaping then continues analogously for the remaining Hiragana Japanese
// sub-run, and the result is inserted into ShapeResult as well.
class PLATFORM_EXPORT HarfBuzzShaper final : public Shaper {
public:
    HarfBuzzShaper(const Font*, const TextRun&);
    PassRefPtr<ShapeResult> shapeResult();
    ~HarfBuzzShaper() { }

    enum HolesQueueItemAction {
        HolesQueueNextFont,
        HolesQueueRange
    };

    struct HolesQueueItem {
        HolesQueueItemAction m_action;
        unsigned m_startIndex;
        unsigned m_numCharacters;
        HolesQueueItem(HolesQueueItemAction action, unsigned start, unsigned num)
            : m_action(action)
            , m_startIndex(start)
            , m_numCharacters(num) {};
    };

private:
    float nextExpansionPerOpportunity();
    void setExpansion(float);
    void setFontFeatures();

    void appendToHolesQueue(HolesQueueItemAction,
        unsigned startIndex,
        unsigned numCharacters);
    inline bool shapeRange(hb_buffer_t* harfBuzzBuffer,
        unsigned startIndex,
        unsigned numCharacters,
        const SimpleFontData* currentFont,
        unsigned currentFontRangeFrom,
        unsigned currentFontRangeTo,
        UScriptCode currentRunScript,
        hb_language_t);
    bool extractShapeResults(hb_buffer_t* harfBuzzBuffer,
        ShapeResult*,
        bool& fontCycleQueued,
        const HolesQueueItem& currentQueueItem,
        const SimpleFontData* currentFont,
        UScriptCode currentRunScript,
        bool isLastResort);
    bool collectFallbackHintChars(Vector<UChar32>& hint, bool needsList);

    void insertRunIntoShapeResult(ShapeResult*, PassOwnPtr<ShapeResult::RunInfo> runToInsert, unsigned startGlyph, unsigned numGlyphs, int, hb_buffer_t*);
    float adjustSpacing(ShapeResult::RunInfo*, size_t glyphIndex, unsigned currentCharacterIndex, float& offsetX, float& totalAdvance);

    OwnPtr<UChar[]> m_normalizedBuffer;
    unsigned m_normalizedBufferLength;

    float m_wordSpacingAdjustment; // Delta adjustment (pixels) for each word break.
    float m_letterSpacing; // Pixels to be added after each glyph.
    unsigned m_expansionOpportunityCount;

    Vector<hb_feature_t, 4> m_features;
    Deque<HolesQueueItem> m_holesQueue;
};


} // namespace blink

#endif // HarfBuzzShaper_h
