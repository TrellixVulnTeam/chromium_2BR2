// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that this file only tests the basic behavior of the cache counter, as in
// when it counts and when not, when result is nonzero and when not. It does not
// test whether the result of the counting is correct. This is the
// responsibility of a lower layer, and is tested in
// DiskCacheBackendTest.CalculateSizeOfAllEntries in net_unittests.

#include "chrome/browser/browsing_data/cache_counter.h"

#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/storage_partition_http_cache_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

class CacheCounterTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SetCacheDeletionPref(true);
    SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  }

  void SetCacheDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kDeleteCache, value);
  }

  void SetDeletionPeriodPref(BrowsingDataRemover::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  // One step in the process of creating a cache entry. Every step must be
  // executed on IO thread after the previous one has finished.
  void CreateCacheEntryStep(int return_value) {
    net::CompletionCallback callback =
        base::Bind(&CacheCounterTest::CreateCacheEntryStep,
                   base::Unretained(this));

    switch (next_step_) {
      case GET_CACHE: {
        next_step_ = CREATE_ENTRY;
        net::URLRequestContextGetter* context_getter =
            storage_partition_->GetURLRequestContext();
        net::HttpCache* http_cache = context_getter->GetURLRequestContext()->
            http_transaction_factory()->GetCache();
        return_value = http_cache->GetBackend(&backend_, callback);
        break;
      }

      case CREATE_ENTRY: {
        next_step_ = WRITE_DATA;
        return_value = backend_->CreateEntry("entry_key", &entry_, callback);
        break;
      }

      case WRITE_DATA: {
        next_step_ = DONE;
        std::string data = "entry data";
        scoped_refptr<net::StringIOBuffer> buffer =
            new net::StringIOBuffer(data);
        return_value =
            entry_->WriteData(0, 0, buffer.get(), data.size(), callback, true);
        break;
      }

      case DONE: {
        entry_->Close();
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&CacheCounterTest::Callback,
                       base::Unretained(this)));
        return;
      }
    }

    if (return_value >= 0) {
      // Success.
      CreateCacheEntryStep(net::OK);
    } else if (return_value == net::ERR_IO_PENDING) {
      // The callback will trigger the next step.
    } else {
      // Error.
      NOTREACHED();
    }
  }

  // Create a cache entry on the IO thread.
  void CreateCacheEntry() {
    storage_partition_ =
        BrowserContext::GetDefaultStoragePartition(browser()->profile());
    next_step_ = GET_CACHE;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CacheCounterTest::CreateCacheEntryStep,
                   base::Unretained(this),
                   net::OK));
    WaitForIOThread();
  }

  // Wait for IO thread operations, such as cache creation, counting, writing,
  // deletion etc.
  void WaitForIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  // General completion callback.
  void Callback() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (run_loop_)
      run_loop_->Quit();
  }

  // Callback from the counter.
  void CountingCallback(bool finished, uint32 count) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    finished_ = finished;
    result_ = count;
    if (run_loop_ && finished)
      run_loop_->Quit();
  }

  uint32 GetResult() {
    DCHECK(finished_);
    return result_;
  }

 private:
  enum CacheEntryCreationStep {
    GET_CACHE,
    CREATE_ENTRY,
    WRITE_DATA,
    DONE
  };
  CacheEntryCreationStep next_step_;
  content::StoragePartition* storage_partition_;
  disk_cache::Backend* backend_;
  disk_cache::Entry* entry_;

  scoped_ptr<base::RunLoop> run_loop_;

  bool finished_;
  uint32 result_;
};

// Tests that for the empty cache, the result is zero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, Empty) {
  CacheCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&CacheCounterTest::CountingCallback,
                          base::Unretained(this)));
  counter.Restart();

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that for a non-empty cache, the result is nonzero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, NonEmpty) {
  CreateCacheEntry();

  CacheCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&CacheCounterTest::CountingCallback,
                          base::Unretained(this)));
  counter.Restart();

  WaitForIOThread();

  EXPECT_NE(0u, GetResult());
}

// Tests that after dooming a nonempty cache, the result is zero.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, AfterDoom) {
  CreateCacheEntry();

  CacheCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&CacheCounterTest::CountingCallback,
                          base::Unretained(this)));

  browsing_data::StoragePartitionHttpCacheDataRemover::CreateForRange(
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile()),
      base::Time(),
      base::Time::Max())->Remove(
          base::Bind(&CacheCounter::Restart,
          base::Unretained(&counter)));

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, PrefChanged) {
  SetCacheDeletionPref(false);

  CacheCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&CacheCounterTest::CountingCallback,
                          base::Unretained(this)));
  SetCacheDeletionPref(true);

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counter does not count if the deletion preference is false.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, PrefIsFalse) {
  SetCacheDeletionPref(false);

  CacheCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&CacheCounterTest::CountingCallback,
                          base::Unretained(this)));
  counter.Restart();

  EXPECT_FALSE(counter.Pending());
}

// Tests that the counting is restarted when the time period changes. Currently,
// the results should be the same for every period. This is because the counter
// always counts the size of the entire cache, and it is up to the UI
// to interpret it as exact value or upper bound.
IN_PROC_BROWSER_TEST_F(CacheCounterTest, PeriodChanged) {
  CreateCacheEntry();

  CacheCounter counter;
  counter.Init(browser()->profile(),
               base::Bind(&CacheCounterTest::CountingCallback,
                          base::Unretained(this)));

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_HOUR);
  WaitForIOThread();
  uint32 result = GetResult();

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_DAY);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::LAST_WEEK);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::FOUR_WEEKS);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(BrowsingDataRemover::EVERYTHING);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());
}

}  // namespace
