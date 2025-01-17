/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkRemote_DEFINED
#define SkRemote_DEFINED

#include "SkPaint.h"
#include "SkRegion.h"
#include "SkRemote_protocol.h"
#include "SkTypes.h"

class SkCanvas;
class SkMatrix;
class SkPath;
class SkShader;
class SkXfermode;

// TODO: document

namespace SkRemote {

    // General purpose identifier.  Holds a Type and a 56-bit value.
    class ID {
    public:
        ID() {}
        ID(Type type, uint64_t val) {
            fVal = (uint64_t)type << 56 | val;
            SkASSERT(this->type() == type && this->val() == val);
        }

        Type    type() const { return (Type)(fVal >> 56); }
        uint64_t val() const { return fVal & ~((uint64_t)0xFF << 56); }

        bool operator==(ID o) const { return fVal == o.fVal; }

    private:
        uint64_t fVal;
    };

    // Fields from SkPaint used by stroke, fill, and text draws.
    struct Misc {
        SkColor         fColor;
        SkFilterQuality fFilterQuality;
        bool fAntiAlias, fDither;

        static Misc CreateFrom(const SkPaint&);
        void applyTo(SkPaint*) const;
    };

    // Fields from SkPaint used by stroke draws only.
    struct Stroke {
        SkScalar fWidth, fMiter;
        SkPaint::Cap  fCap;
        SkPaint::Join fJoin;

        static Stroke CreateFrom(const SkPaint&);
        void applyTo(SkPaint*) const;
    };

    // TODO: document
    struct Encoder {
        virtual ~Encoder() {}

        virtual ID define(const SkMatrix&) = 0;
        virtual ID define(const Misc&)     = 0;
        virtual ID define(const SkPath&)   = 0;
        virtual ID define(const Stroke&)   = 0;
        virtual ID define(SkShader*)       = 0;
        virtual ID define(SkXfermode*)     = 0;

        virtual void undefine(ID) = 0;

        virtual void    save() = 0;
        virtual void restore() = 0;

        virtual void setMatrix(ID matrix) = 0;

        // TODO: struct CommonIDs { ID misc, shader, xfermode; ... }
        // for IDs that affect both fill + stroke?

        virtual void   clipPath(ID path, SkRegion::Op, bool aa)                      = 0;
        virtual void   fillPath(ID path, ID misc, ID shader, ID xfermode)            = 0;
        virtual void strokePath(ID path, ID misc, ID shader, ID xfermode, ID stroke) = 0;
    };

    // None of these factories take ownership of their arguments.

    // Returns a new SkCanvas that translates to the Encoder API.
    SkCanvas* NewCanvas(Encoder*);
    // Returns an Encoder that translates back to the SkCanvas API.
    Encoder* NewDecoder(SkCanvas*);
    // Wraps another Encoder with a cache.  TODO: parameterize
    Encoder* NewCachingEncoder(Encoder*);

}  // namespace SkRemote

#endif//SkRemote_DEFINED
