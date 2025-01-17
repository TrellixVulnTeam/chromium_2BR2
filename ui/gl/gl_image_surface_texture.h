// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_SURFACE_TEXTURE_H_
#define UI_GL_GL_IMAGE_SURFACE_TEXTURE_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gfx {
class SurfaceTexture;

class GL_EXPORT GLImageSurfaceTexture : public GLImage {
 public:
  explicit GLImageSurfaceTexture(const Size& size);

  bool Initialize(SurfaceTexture* surface_texture);

  // Overridden from GLImage:
  void Destroy(bool have_context) override;
  Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const Point& offset,
                       const Rect& rect) override;
  bool ScheduleOverlayPlane(AcceleratedWidget widget,
                            int z_order,
                            OverlayTransform transform,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageSurfaceTexture() override;

 private:
  scoped_refptr<SurfaceTexture> surface_texture_;
  const Size size_;
  GLint texture_id_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GLImageSurfaceTexture);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_SURFACE_TEXTURE_H_
