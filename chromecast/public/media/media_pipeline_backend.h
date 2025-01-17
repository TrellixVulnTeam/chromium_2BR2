// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_

#include "decoder_config.h"

namespace chromecast {
struct Size;

namespace media {
class CastDecoderBuffer;
class DecryptContext;

// Interface for platform-specific output of media.
// A new MediaPipelineBackend will be instantiated for each media player
// instance and raw audio stream.  If a backend has both video and audio
// decoders, they must be synchronized.
// If more backends are requested than the platform supports, the unsupported
// extra backends may return nullptr for CreateAudioDecoder/CreateVideoDecoder.
// The basic usage pattern is:
//   * Decoder objects created, then Initialize called
//   * Start/Stop/Pause/Resume used to manage playback state
//   * Decoder objects are used to pass actual stream data buffers
//   * Backend must make appropriate callbacks on the provided Delegate
// All functions will be called on the media thread.  Delegate callbacks
// must be made on this thread also (using provided TaskRunner if necessary).
class MediaPipelineBackend {
 public:
  // Return code for PushBuffer
  enum BufferStatus {
    kBufferSuccess,
    kBufferFailed,
    kBufferPending,
  };

  class Decoder {
   public:
    typedef MediaPipelineBackend::BufferStatus BufferStatus;

    // Statistics (computed since pipeline last started playing).
    // For video, a sample is defined as a frame.
    struct Statistics {
      uint64_t decoded_bytes;
      uint64_t decoded_samples;
      uint64_t dropped_samples;
    };

    // Pushes a buffer of data for decoding and output.  If the implementation
    // cannot push the buffer now, it must store the buffer, return
    // |kBufferPending| and execute the push at a later time when it becomes
    // possible to do so.  The implementation must then invoke
    // Client::OnPushComplete.  Pushing a pending buffer should be aborted if
    // Stop is called; OnPushAudioComplete need not be invoked in this case.
    // If |kBufferPending| is returned, the pipeline will stop pushing any
    // further buffers until OnPushComplete is invoked.
    // OnPushComplete should be only be invoked to indicate completion of a
    // pending buffer push - not for the immediate |kBufferSuccess| return case.
    // The buffer's lifetime is managed by the caller code - it MUST NOT be
    // deleted by the MediaPipelineBackend implementation, and MUST NOT be
    // dereferenced after completion of buffer push (i.e.
    // kBufferSuccess/kBufferFailure for synchronous completion, OnPushComplete
    // for kBufferPending case).
    virtual BufferStatus PushBuffer(CastDecoderBuffer* buffer) = 0;

    // Returns the playback statistics since this decoder's creation.  Only
    // called when playing or paused.
    virtual void GetStatistics(Statistics* statistics) = 0;

   protected:
    virtual ~Decoder() {}
  };

  class AudioDecoder : public Decoder {
   public:
    // Info on pipeline latency: amount of data in pipeline not rendered yet,
    // and timestamp of system clock (must be CLOCK_MONOTONIC_RAW) at which
    // delay measurement was taken. Both times in microseconds.
    struct RenderingDelay {
      RenderingDelay()
          : delay_microseconds(INT64_MIN), timestamp_microseconds(INT64_MIN) {}
      RenderingDelay(int64_t delay_microseconds_in,
                     int64_t timestamp_microseconds_in)
          : delay_microseconds(delay_microseconds_in),
            timestamp_microseconds(timestamp_microseconds_in) {}
      int64_t delay_microseconds;
      int64_t timestamp_microseconds;
    };

    // Provides the audio configuration.  Called once before the backend is
    // initialized, and again any time the configuration changes (in any state).
    // Returns true if the configuration is a supported configuration.
    virtual bool SetConfig(const AudioConfig& config) = 0;

    // Sets the volume multiplier for this audio stream.
    // The multiplier is in the range [0.0, 1.0].  If not called, a default
    // multiplier of 1.0 is assumed. Returns true if successful.
    // Only called after the backend has been initialized.
    virtual bool SetVolume(float multiplier) = 0;

    // Returns the pipeline latency: i.e. the amount of data
    // in the pipeline that have not been rendered yet, in microseconds.
    // Returns delay = INT64_MIN if the latency is not available.
    // Only called when the backend is playing.
    virtual RenderingDelay GetRenderingDelay() = 0;

   protected:
    ~AudioDecoder() override {}
  };

  class VideoDecoder : public Decoder {
   public:
    // Provides the video configuration.  Called once before the backend is
    // initialized, and again any time the configuration changes (in any state).
    // Returns true if the configuration is a supported configuration.
    virtual bool SetConfig(const VideoConfig& config) = 0;

   protected:
    ~VideoDecoder() override {}
  };

  // Delegate methods must be called on the main CMA thread.
  class Delegate {
   public:
    // Must be called when video resolution change is detected by decoder.
    virtual void OnVideoResolutionChanged(VideoDecoder* decoder,
                                          const Size& size) = 0;

    // See comments on PushBuffer.  Must not be called with kBufferPending.
    virtual void OnPushBufferComplete(Decoder* decoder,
                                      BufferStatus status) = 0;

    // Must be called after an end-of-stream buffer has been rendered (ie, the
    // last real buffer has been sent to the output hardware).
    virtual void OnEndOfStream(Decoder* decoder) = 0;

    // May be called if a decoder error occurs. No more calls to PushBuffer()
    // will be made after this is called.
    virtual void OnDecoderError(Decoder* decoder) = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~MediaPipelineBackend() {}

  // Creates a new AudioDecoder attached to this pipeline.  MediaPipelineBackend
  // maintains ownership of the decoder object (and must not delete before it's
  // destroyed).  Will be called zero or more times, all calls made before
  // Initialize. May return nullptr if the platform implementation cannot
  // support any additional simultaneous playback at this time.
  virtual AudioDecoder* CreateAudioDecoder() = 0;

  // Creates a new VideoDecoder attached to this pipeline.  MediaPipelineBackend
  // maintains ownership of the decoder object (and must not delete before it's
  // destroyed).  Will be called zero or more times, all calls made before
  // Initialize. Note: Even if your backend only supports audio, you must
  // provide a default implementation of VideoDecoder; one way to do this is to
  // inherit from MediaPipelineBackendDefault. May return nullptr if the
  // platform implementation cannot support any additional simultaneous playback
  // at this time.
  virtual VideoDecoder* CreateVideoDecoder() = 0;

  // Initializes the backend.  This will be called once, after Decoder creation
  // but before all other functions.  Hardware resources for all decoders should
  // be acquired here.  Backend is then considered in Initialized state.
  // Returns false for failure.
  virtual bool Initialize(Delegate* delegate) = 0;

  // Places pipeline into playing state.  Playback will start at given time once
  // buffers are pushed.  Called only when in Initialized state. |start_pts| is
  // the start playback timestamp in microseconds.
  virtual bool Start(int64_t start_pts) = 0;

  // Returns pipeline to 'Initialized' state.  May be called while playing or
  // paused.  Buffers cannot be pushed in Initialized state.
  virtual bool Stop() = 0;

  // Pauses media playback.  Called only when in playing state.
  virtual bool Pause() = 0;

  // Resumes media playback.  Called only when in paused state.
  virtual bool Resume() = 0;

  // Gets the current playback timestamp in microseconds.
  virtual int64_t GetCurrentPts() = 0;

  // Sets the playback rate.  |rate| > 0.  If this is not called, a default rate
  // of 1.0 is assumed. Returns true if successful.
  virtual bool SetPlaybackRate(float rate) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_MEDIA_PIPELINE_BACKEND_H_
