// Copyright (c) 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder_test.h"

#include <limits.h>

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "../public/fpdf_text.h"
#include "../public/fpdfview.h"
#include "test_support.h"
#include "testing/gmock/include/gmock/gmock.h"

#ifdef PDF_ENABLE_V8
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"
#endif  // PDF_ENABLE_V8

namespace {
const char* g_exe_path_ = nullptr;
}  // namespace

FPDF_BOOL Is_Data_Avail(FX_FILEAVAIL* pThis, size_t offset, size_t size) {
  return true;
}

void Add_Segment(FX_DOWNLOADHINTS* pThis, size_t offset, size_t size) {}

EmbedderTest::EmbedderTest()
    : default_delegate_(new EmbedderTest::Delegate()),
      document_(nullptr),
      form_handle_(nullptr),
      avail_(nullptr),
      external_isolate_(nullptr),
      loader_(nullptr),
      file_length_(0),
      file_contents_(nullptr) {
  memset(&hints_, 0, sizeof(hints_));
  memset(&file_access_, 0, sizeof(file_access_));
  memset(&file_avail_, 0, sizeof(file_avail_));
  delegate_ = default_delegate_.get();
}

EmbedderTest::~EmbedderTest() {
}

void EmbedderTest::SetUp() {
#ifdef PDF_ENABLE_V8
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  InitializeV8ForPDFium(g_exe_path_, std::string(), &natives_, &snapshot_,
                        &platform_);
#else
  InitializeV8ForPDFium(&platform_);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
#endif  // FPDF_ENABLE_V8

  FPDF_LIBRARY_CONFIG config;
  config.version = 2;
  config.m_pUserFontPaths = nullptr;
  config.m_v8EmbedderSlot = 0;
  config.m_pIsolate = external_isolate_;
  FPDF_InitLibraryWithConfig(&config);

  UNSUPPORT_INFO* info = static_cast<UNSUPPORT_INFO*>(this);
  memset(info, 0, sizeof(UNSUPPORT_INFO));
  info->version = 1;
  info->FSDK_UnSupport_Handler = UnsupportedHandlerTrampoline;
  FSDK_SetUnSpObjProcessHandler(info);
}

void EmbedderTest::TearDown() {
  if (document_) {
    FORM_DoDocumentAAction(form_handle_, FPDFDOC_AACTION_WC);
    FPDFDOC_ExitFormFillEnvironment(form_handle_);
    FPDF_CloseDocument(document_);
  }
  FPDFAvail_Destroy(avail_);
  FPDF_DestroyLibrary();

#ifdef PDF_ENABLE_V8
  v8::V8::ShutdownPlatform();
  delete platform_;
#endif  // PDF_ENABLE_V8

  delete loader_;
  free(file_contents_);
}

bool EmbedderTest::OpenDocument(const std::string& filename) {
  file_contents_ = GetFileContents(filename.c_str(), &file_length_);
  if (!file_contents_) {
    return false;
  }

  loader_ = new TestLoader(file_contents_, file_length_);
  file_access_.m_FileLen = static_cast<unsigned long>(file_length_);
  file_access_.m_GetBlock = TestLoader::GetBlock;
  file_access_.m_Param = loader_;

  file_avail_.version = 1;
  file_avail_.IsDataAvail = Is_Data_Avail;

  hints_.version = 1;
  hints_.AddSegment = Add_Segment;

  avail_ = FPDFAvail_Create(&file_avail_, &file_access_);
  (void)FPDFAvail_IsDocAvail(avail_, &hints_);

  if (!FPDFAvail_IsLinearized(avail_)) {
    document_ = FPDF_LoadCustomDocument(&file_access_, nullptr);
  } else {
    document_ = FPDFAvail_GetDocument(avail_, nullptr);
  }

  (void)FPDF_GetDocPermissions(document_);
  (void)FPDFAvail_IsFormAvail(avail_, &hints_);

  IPDF_JSPLATFORM* platform = static_cast<IPDF_JSPLATFORM*>(this);
  memset(platform, 0, sizeof(IPDF_JSPLATFORM));
  platform->version = 2;
  platform->app_alert = AlertTrampoline;

  FPDF_FORMFILLINFO* formfillinfo = static_cast<FPDF_FORMFILLINFO*>(this);
  memset(formfillinfo, 0, sizeof(FPDF_FORMFILLINFO));
  formfillinfo->version = 1;
  formfillinfo->FFI_SetTimer = SetTimerTrampoline;
  formfillinfo->FFI_KillTimer = KillTimerTrampoline;
  formfillinfo->FFI_GetPage = GetPageTrampoline;
  formfillinfo->m_pJsPlatform = platform;

  form_handle_ = FPDFDOC_InitFormFillEnvironment(document_, formfillinfo);
  FPDF_SetFormFieldHighlightColor(form_handle_, 0, 0xFFE4DD);
  FPDF_SetFormFieldHighlightAlpha(form_handle_, 100);

  return true;
}

void EmbedderTest::DoOpenActions() {
  FORM_DoDocumentJSAction(form_handle_);
  FORM_DoDocumentOpenAction(form_handle_);
}

int EmbedderTest::GetFirstPageNum() {
  int first_page = FPDFAvail_GetFirstPageNum(document_);
  (void)FPDFAvail_IsPageAvail(avail_, first_page, &hints_);
  return first_page;
}

int EmbedderTest::GetPageCount() {
  int page_count = FPDF_GetPageCount(document_);
  for (int i = 0; i < page_count; ++i) {
    (void)FPDFAvail_IsPageAvail(avail_, i, &hints_);
  }
  return page_count;
}

FPDF_PAGE EmbedderTest::LoadPage(int page_number) {
  FPDF_PAGE page = FPDF_LoadPage(document_, page_number);
  if (!page) {
    return nullptr;
  }
  FORM_OnAfterLoadPage(page, form_handle_);
  FORM_DoPageAAction(page, form_handle_, FPDFPAGE_AACTION_OPEN);
  return page;
}

FPDF_PAGE EmbedderTest::LoadAndCachePage(int page_number) {
  FPDF_PAGE page = delegate_->GetPage(form_handle_, document_, page_number);
  if (!page) {
    return nullptr;
  }
  FORM_DoPageAAction(page, form_handle_, FPDFPAGE_AACTION_OPEN);
  return page;
}

FPDF_BITMAP EmbedderTest::RenderPage(FPDF_PAGE page) {
  int width = static_cast<int>(FPDF_GetPageWidth(page));
  int height = static_cast<int>(FPDF_GetPageHeight(page));
  FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, 0);
  FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
  FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, 0);
  FPDF_FFLDraw(form_handle_, bitmap, page, 0, 0, width, height, 0, 0);
  return bitmap;
}

void EmbedderTest::UnloadPage(FPDF_PAGE page) {
  FORM_DoPageAAction(page, form_handle_, FPDFPAGE_AACTION_CLOSE);
  FORM_OnBeforeClosePage(page, form_handle_);
  FPDF_ClosePage(page);
}

FPDF_PAGE EmbedderTest::Delegate::GetPage(FPDF_FORMHANDLE form_handle,
                                          FPDF_DOCUMENT document,
                                          int page_index) {
  auto it = m_pageMap.find(page_index);
  if (it != m_pageMap.end()) {
    return it->second;
  }
  FPDF_PAGE page = FPDF_LoadPage(document, page_index);
  if (!page) {
    return nullptr;
  }
  m_pageMap[page_index] = page;
  FORM_OnAfterLoadPage(page, form_handle);
  return page;
}

// static
void EmbedderTest::UnsupportedHandlerTrampoline(UNSUPPORT_INFO* info,
                                                int type) {
  EmbedderTest* test = static_cast<EmbedderTest*>(info);
  test->delegate_->UnsupportedHandler(type);
}

// static
int EmbedderTest::AlertTrampoline(IPDF_JSPLATFORM* platform,
                                  FPDF_WIDESTRING message,
                                  FPDF_WIDESTRING title,
                                  int type,
                                  int icon) {
  EmbedderTest* test = static_cast<EmbedderTest*>(platform);
  return test->delegate_->Alert(message, title, type, icon);
}

// static
int EmbedderTest::SetTimerTrampoline(FPDF_FORMFILLINFO* info,
                                     int msecs,
                                     TimerCallback fn) {
  EmbedderTest* test = static_cast<EmbedderTest*>(info);
  return test->delegate_->SetTimer(msecs, fn);
}

// static
void EmbedderTest::KillTimerTrampoline(FPDF_FORMFILLINFO* info, int id) {
  EmbedderTest* test = static_cast<EmbedderTest*>(info);
  return test->delegate_->KillTimer(id);
}

// static
FPDF_PAGE EmbedderTest::GetPageTrampoline(FPDF_FORMFILLINFO* info,
                                          FPDF_DOCUMENT document,
                                          int page_index) {
  EmbedderTest* test = static_cast<EmbedderTest*>(info);
  return test->delegate_->GetPage(test->form_handle(), document, page_index);
}

// Can't use gtest-provided main since we need to stash the path to the
// executable in order to find the external V8 binary data files.
int main(int argc, char** argv) {
  g_exe_path_ = argv[0];
  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
