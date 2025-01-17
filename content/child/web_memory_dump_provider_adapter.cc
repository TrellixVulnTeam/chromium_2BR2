// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_memory_dump_provider_adapter.h"

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_profiler_allocation_context.h"
#include "base/trace_event/memory_profiler_allocation_register.h"
#include "content/child/web_process_memory_dump_impl.h"
#include "third_party/WebKit/public/platform/WebMemoryDumpProvider.h"

using namespace base;
using namespace base::trace_event;

namespace {

AllocationRegister* g_allocation_register = nullptr;
LazyInstance<Lock>::Leaky g_allocation_register_lock =
    LAZY_INSTANCE_INITIALIZER;

void ReportAllocation(void* address, size_t size) {
  AllocationContext context = AllocationContextTracker::GetContextSnapshot();
  AutoLock lock(g_allocation_register_lock.Get());

  if (g_allocation_register)
    g_allocation_register->Insert(address, size, context);
}

void ReportFree(void* address) {
  AutoLock lock(g_allocation_register_lock.Get());

  if (g_allocation_register)
    g_allocation_register->Remove(address);
}

}  // namespace

namespace content {

WebMemoryDumpProviderAdapter::WebMemoryDumpProviderAdapter(
    blink::WebMemoryDumpProvider* wmdp)
    : web_memory_dump_provider_(wmdp), is_registered_(false) {
}

WebMemoryDumpProviderAdapter::~WebMemoryDumpProviderAdapter() {
  DCHECK(!is_registered_);
}

bool WebMemoryDumpProviderAdapter::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  blink::WebMemoryDumpLevelOfDetail level;
  switch (args.level_of_detail) {
    case base::trace_event::MemoryDumpLevelOfDetail::LIGHT:
      level = blink::WebMemoryDumpLevelOfDetail::Light;
      break;
    case base::trace_event::MemoryDumpLevelOfDetail::DETAILED:
      level = blink::WebMemoryDumpLevelOfDetail::Detailed;
      break;
    default:
      NOTREACHED();
      return false;
  }
  WebProcessMemoryDumpImpl web_pmd_impl(args.level_of_detail, pmd);

  if (web_memory_dump_provider_->supportsHeapProfiling()) {
    // TODO(ruuda): Dump |g_allocation_register| into the |ProcessMemoryDump|.
  }

  return web_memory_dump_provider_->onMemoryDump(level, &web_pmd_impl);
}

void WebMemoryDumpProviderAdapter::OnHeapProfilingEnabled(bool enabled) {
  if (!web_memory_dump_provider_->supportsHeapProfiling())
    return;

  if (enabled) {
    {
      AutoLock lock(g_allocation_register_lock.Get());
      if (!g_allocation_register)
        g_allocation_register = new AllocationRegister();
    }

    // Make this dump provider call the global hooks on every allocation / free.
    // TODO(ruuda): Because bookkeeping is done here in the adapter, and not in
    // the dump providers themselves, all dump providers in Blink share the
    // same global allocation register. At the moment this is not a problem,
    // because the only dump provider that supports heap profiling is the
    // PartitionAlloc dump provider. When Blink can depend on base and this
    // glue layer is removed, dump providers can have their own instance of the
    // allocation register.
    web_memory_dump_provider_->onHeapProfilingEnabled(ReportAllocation,
                                                      ReportFree);
  } else {
    web_memory_dump_provider_->onHeapProfilingEnabled(nullptr, nullptr);
  }
}

}  // namespace content
