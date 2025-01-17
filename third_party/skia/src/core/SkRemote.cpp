/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCanvas.h"
#include "SkPath.h"
#include "SkRect.h"
#include "SkRemote.h"
#include "SkShader.h"
#include "SkTHash.h"

namespace SkRemote {

    Misc Misc::CreateFrom(const SkPaint& paint) {
        Misc misc = {
            paint.getColor(),
            paint.getFilterQuality(),
            paint.isAntiAlias(),
            paint.isDither(),
        };
        return misc;
    }

    void Misc::applyTo(SkPaint* paint) const {
        paint->setColor        (fColor);
        paint->setFilterQuality(fFilterQuality);
        paint->setAntiAlias    (fAntiAlias);
        paint->setDither       (fDither);
    }

    static bool operator==(const Misc& a, const Misc& b) {
        return a.fColor         == b.fColor
            && a.fFilterQuality == b.fFilterQuality
            && a.fAntiAlias     == b.fAntiAlias
            && a.fDither        == b.fDither;
    }

    // Misc carries 10 bytes of data in a 12 byte struct, so we need a custom hash.
    static_assert(sizeof(Misc) > offsetof(Misc, fDither) + sizeof(Misc().fDither), "");
    struct MiscHash {
        uint32_t operator()(const Misc& misc) {
            return SkChecksum::Murmur3(&misc, offsetof(Misc, fDither) + sizeof(Misc().fDither));
        }
    };

    Stroke Stroke::CreateFrom(const SkPaint& paint) {
        Stroke stroke = {
            paint.getStrokeWidth(),
            paint.getStrokeMiter(),
            paint.getStrokeCap(),
            paint.getStrokeJoin(),
        };
        return stroke;
    }

    void Stroke::applyTo(SkPaint* paint) const {
        paint->setStrokeWidth(fWidth);
        paint->setStrokeMiter(fMiter);
        paint->setStrokeCap  (fCap);
        paint->setStrokeJoin (fJoin);
    }

    static bool operator==(const Stroke& a, const Stroke& b) {
        return a.fWidth == b.fWidth
            && a.fMiter == b.fMiter
            && a.fCap   == b.fCap
            && a.fJoin  == b.fJoin;
    }

    // The default SkGoodHash works fine for Stroke, as it's dense.
    static_assert(sizeof(Stroke) == offsetof(Stroke, fJoin) + sizeof(Stroke().fJoin), "");

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

    class Canvas final : public SkCanvas {
    public:
        explicit Canvas(Encoder* encoder)
            : SkCanvas(1,1)
            , fEncoder(encoder) {}

    private:
        // Calls Encoder::define() when created, Encoder::undefine() when destroyed.
        class AutoID : ::SkNoncopyable {
        public:
            template <typename T>
            explicit AutoID(Encoder* encoder, const T& val)
                : fEncoder(encoder)
                , fID(encoder->define(val)) {}
            ~AutoID() { if (fEncoder) fEncoder->undefine(fID); }

            AutoID(AutoID&& o) : fEncoder(o.fEncoder), fID(o.fID) {
                o.fEncoder = nullptr;
            }
            AutoID& operator=(AutoID&&) = delete;

            operator ID () const { return fID; }

        private:
            Encoder* fEncoder;
            const ID fID;
        };

        template <typename T>
        AutoID id(const T& val) { return AutoID(fEncoder, val); }

        void   willSave() override { fEncoder->   save(); }
        void didRestore() override { fEncoder->restore(); }

        void    didConcat(const SkMatrix&) override { this->didSetMatrix(this->getTotalMatrix()); }
        void didSetMatrix(const SkMatrix& matrix) override {
            fEncoder->setMatrix(this->id(matrix));
        }

        void onDrawOval(const SkRect& oval, const SkPaint& paint) override {
            SkPath path;
            path.addOval(oval);
            this->onDrawPath(path, paint);
        }

        void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
            SkPath path;
            path.addRect(rect);
            this->onDrawPath(path, paint);
        }

        void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override {
            SkPath path;
            path.addRRect(rrect);
            this->onDrawPath(path, paint);
        }

        void onDrawDRRect(const SkRRect& outside, const SkRRect& inside,
                          const SkPaint& paint) override {
            SkPath path;
            path.addRRect(outside);
            path.addRRect(inside, SkPath::kCCW_Direction);
            this->onDrawPath(path, paint);
        }

        void onDrawPath(const SkPath& path, const SkPaint& paint) override {
            auto p = this->id(path),
                 m = this->id(Misc::CreateFrom(paint)),
                 s = this->id(paint.getShader()),
                 x = this->id(paint.getXfermode());

            if (paint.getStyle() == SkPaint::kFill_Style) {
                fEncoder->fillPath(p, m, s, x);
            } else {
                // TODO: handle kStrokeAndFill_Style
                fEncoder->strokePath(p, m, s, x, this->id(Stroke::CreateFrom(paint)));
            }
        }

        void onDrawPaint(const SkPaint& paint) override {
            SkPath path;
            path.setFillType(SkPath::kInverseWinding_FillType);  // Either inverse FillType is fine.
            this->onDrawPath(path, paint);
        }

        void onDrawText(const void* text, size_t byteLength,
                        SkScalar x, SkScalar y, const SkPaint& paint) override {
            // Text-as-paths is a temporary hack.
            // TODO: send SkTextBlobs and SkTypefaces
            SkPath path;
            paint.getTextPath(text, byteLength, x, y, &path);
            this->onDrawPath(path, paint);
        }

        void onDrawPosText(const void* text, size_t byteLength,
                           const SkPoint pos[], const SkPaint& paint) override {
            // Text-as-paths is a temporary hack.
            // TODO: send SkTextBlobs and SkTypefaces
            SkPath path;
            paint.getPosTextPath(text, byteLength, pos, &path);
            this->onDrawPath(path, paint);
        }

        void onDrawPosTextH(const void* text, size_t byteLength,
                            const SkScalar xpos[], SkScalar constY, const SkPaint& paint) override {
            size_t length = paint.countText(text, byteLength);
            SkAutoTArray<SkPoint> pos(length);
            for(size_t i = 0; i < length; ++i) {
                pos[i].set(xpos[i], constY);
            }
            this->onDrawPosText(text, byteLength, &pos[0], paint);
        }

        void onClipRect(const SkRect& rect, SkRegion::Op op, ClipEdgeStyle edgeStyle) override {
            SkPath path;
            path.addRect(rect);
            this->onClipPath(path, op, edgeStyle);
        }

        void onClipRRect(const SkRRect& rrect, SkRegion::Op op, ClipEdgeStyle edgeStyle) override {
            SkPath path;
            path.addRRect(rrect);
            this->onClipPath(path, op, edgeStyle);
        }

        void onClipPath(const SkPath& path, SkRegion::Op op, ClipEdgeStyle edgeStyle) override {
            fEncoder->clipPath(this->id(path), op, edgeStyle == kSoft_ClipEdgeStyle);
        }

        Encoder* fEncoder;
    };

    SkCanvas* NewCanvas(Encoder* encoder) { return new Canvas(encoder); }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

    class Decoder final : public Encoder {
    public:
        explicit Decoder(SkCanvas* canvas) : fCanvas(canvas) {}

    private:
        template <typename Map, typename T>
        ID define(Type type, Map* map, const T& val) {
            ID id(type, fNextID++);
            map->set(id, val);
            return id;
        }

        ID define(const SkMatrix& v) override {return this->define(Type::kMatrix,   &fMatrix,   v);}
        ID define(const Misc&     v) override {return this->define(Type::kMisc,     &fMisc,     v);}
        ID define(const SkPath&   v) override {return this->define(Type::kPath,     &fPath,     v);}
        ID define(const Stroke&   v) override {return this->define(Type::kStroke,   &fStroke,   v);}
        ID define(SkShader*       v) override {return this->define(Type::kShader,   &fShader,   v);}
        ID define(SkXfermode*     v) override {return this->define(Type::kXfermode, &fXfermode, v);}

        void undefine(ID id) override {
            switch(id.type()) {
                case Type::kMatrix:   return fMatrix  .remove(id);
                case Type::kMisc:     return fMisc    .remove(id);
                case Type::kPath:     return fPath    .remove(id);
                case Type::kStroke:   return fStroke  .remove(id);
                case Type::kShader:   return fShader  .remove(id);
                case Type::kXfermode: return fXfermode.remove(id);
            };
        }

        void    save() override { fCanvas->save(); }
        void restore() override { fCanvas->restore(); }

        void setMatrix(ID matrix) override { fCanvas->setMatrix(fMatrix.find(matrix)); }

        void clipPath(ID path, SkRegion::Op op, bool aa) override {
            fCanvas->clipPath(fPath.find(path), op, aa);
        }
        void fillPath(ID path, ID misc, ID shader, ID xfermode) override {
            SkPaint paint;
            paint.setStyle(SkPaint::kFill_Style);
            fMisc.find(misc).applyTo(&paint);
            paint.setShader  (fShader  .find(shader));
            paint.setXfermode(fXfermode.find(xfermode));
            fCanvas->drawPath(fPath.find(path), paint);
        }
        void strokePath(ID path, ID misc, ID shader, ID xfermode, ID stroke) override {
            SkPaint paint;
            paint.setStyle(SkPaint::kStroke_Style);
            fMisc  .find(misc  ).applyTo(&paint);
            fStroke.find(stroke).applyTo(&paint);
            paint.setShader  (fShader  .find(shader));
            paint.setXfermode(fXfermode.find(xfermode));
            fCanvas->drawPath(fPath.find(path), paint);
        }

        // Maps ID -> T.
        template <typename T, Type kType>
        class IDMap {
        public:
            ~IDMap() {
                // A well-behaved client always cleans up its definitions.
                SkASSERT(fMap.count() == 0);
            }

            void set(const ID& id, const T& val) {
                SkASSERT(id.type() == kType);
                fMap.set(id, val);
            }

            void remove(const ID& id) {
                SkASSERT(id.type() == kType);
                fMap.remove(id);
            }

            const T& find(const ID& id) const {
                SkASSERT(id.type() == kType);
                T* val = fMap.find(id);
                SkASSERT(val != nullptr);
                return *val;
            }

        private:
            SkTHashMap<ID, T> fMap;
        };

        // Maps ID -> T*, and keeps the T alive by reffing it.
        template <typename T, Type kType>
        class ReffedIDMap {
        public:
            ReffedIDMap() {}
            ~ReffedIDMap() {
                // A well-behaved client always cleans up its definitions.
                SkASSERT(fMap.count() == 0);
            }

            void set(const ID& id, T* val) {
                SkASSERT(id.type() == kType);
                fMap.set(id, SkSafeRef(val));
            }

            void remove(const ID& id) {
                SkASSERT(id.type() == kType);
                T** val = fMap.find(id);
                SkASSERT(val);
                SkSafeUnref(*val);
                fMap.remove(id);
            }

            T* find(const ID& id) const {
                SkASSERT(id.type() == kType);
                T** val = fMap.find(id);
                SkASSERT(val);
                return *val;
            }

        private:
            SkTHashMap<ID, T*> fMap;
        };


        IDMap<SkMatrix        , Type::kMatrix>   fMatrix;
        IDMap<Misc            , Type::kMisc  >   fMisc;
        IDMap<SkPath          , Type::kPath  >   fPath;
        IDMap<Stroke          , Type::kStroke>   fStroke;
        ReffedIDMap<SkShader  , Type::kShader>   fShader;
        ReffedIDMap<SkXfermode, Type::kXfermode> fXfermode;

        SkCanvas* fCanvas;
        uint64_t fNextID = 0;
    };

    Encoder* NewDecoder(SkCanvas* canvas) { return new Decoder(canvas); }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

    class CachingEncoder final : public Encoder {
    public:
        explicit CachingEncoder(Encoder* wrapped) : fWrapped(wrapped) {}

    private:
        struct Undef {
            Encoder* fEncoder;
            template <typename T>
            void operator()(const T&, ID* id) const { fEncoder->undefine(*id); }
        };

        ~CachingEncoder() override {
            Undef undef{fWrapped};
            fMatrix  .foreach(undef);
            fMisc    .foreach(undef);
            fPath    .foreach(undef);
            fStroke  .foreach(undef);
            fShader  .foreach(undef);
            fXfermode.foreach(undef);
        }

        template <typename Map, typename T>
        ID define(Map* map, const T& v) {
            if (const ID* id = map->find(v)) {
                return *id;
            }
            ID id = fWrapped->define(v);
            map->set(v, id);
            return id;
        }

        ID define(const SkMatrix& v) override { return this->define(&fMatrix,   v); }
        ID define(const Misc&     v) override { return this->define(&fMisc,     v); }
        ID define(const SkPath&   v) override { return this->define(&fPath,     v); }
        ID define(const Stroke&   v) override { return this->define(&fStroke,   v); }
        ID define(SkShader*       v) override { return this->define(&fShader,   v); }
        ID define(SkXfermode*     v) override { return this->define(&fXfermode, v); }

        void undefine(ID) override {}

        void    save() override { fWrapped->   save(); }
        void restore() override { fWrapped->restore(); }

        void setMatrix(ID matrix) override { fWrapped->setMatrix(matrix); }

        void clipPath(ID path, SkRegion::Op op, bool aa) override {
            fWrapped->clipPath(path, op, aa);
        }
        void fillPath(ID path, ID misc, ID shader, ID xfermode) override {
            fWrapped->fillPath(path, misc, shader, xfermode);
        }
        void strokePath(ID path, ID misc, ID shader, ID xfermode, ID stroke) override {
            fWrapped->strokePath(path, misc, shader, xfermode, stroke);
        }

        // Maps const T* -> ID, and refs the key.
        template <typename T, Type kType>
        class RefKeyMap {
        public:
            RefKeyMap() {}
            ~RefKeyMap() { fMap.foreach([](const T* key, ID*) { SkSafeUnref(key); }); }

            void set(const T* key, ID id) {
                SkASSERT(id.type() == kType);
                fMap.set(SkSafeRef(key), id);
            }

            void remove(const T* key) {
                fMap.remove(key);
                SkSafeUnref(key);
            }

            const ID* find(const T* key) const {
                return fMap.find(key);
            }

            template <typename Fn>
            void foreach(const Fn& fn) {
                fMap.foreach(fn);
            }
        private:
            SkTHashMap<const T*, ID> fMap;
        };

        SkTHashMap<SkMatrix, ID>               fMatrix;
        SkTHashMap<Misc, ID, MiscHash>         fMisc;
        SkTHashMap<SkPath, ID>                 fPath;
        SkTHashMap<Stroke, ID>                 fStroke;
        RefKeyMap<SkShader, Type::kShader>     fShader;
        RefKeyMap<SkXfermode, Type::kXfermode> fXfermode;

        Encoder* fWrapped;
    };

    Encoder* NewCachingEncoder(Encoder* wrapped) { return new CachingEncoder(wrapped); }

} // namespace SkRemote
