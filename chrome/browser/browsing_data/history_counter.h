// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_HISTORY_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_HISTORY_COUNTER_H_

#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "components/history/core/browser/history_service.h"

namespace history {
class QueryResults;
}

class HistoryCounter: public BrowsingDataCounter {
 public:
  HistoryCounter();
  ~HistoryCounter() override;

  const std::string& GetPrefName() const override;

  // Whether there are counting tasks in progress. Only used for testing.
  bool HasTrackedTasks();

 private:
  const std::string pref_name_;
  base::CancelableTaskTracker cancelable_task_tracker_;

  void Count() override;

  void OnGetLocalHistoryCount(history::HistoryCountResult result);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_HISTORY_COUNTER_H_
