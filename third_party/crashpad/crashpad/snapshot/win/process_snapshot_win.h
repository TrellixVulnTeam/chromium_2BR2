// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_SNAPSHOT_WIN_PROCESS_SNAPSHOT_WIN_H_
#define CRASHPAD_SNAPSHOT_WIN_PROCESS_SNAPSHOT_WIN_H_

#include <windows.h>
#include <sys/time.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "client/crashpad_info.h"
#include "snapshot/crashpad_info_client_options.h"
#include "snapshot/exception_snapshot.h"
#include "snapshot/memory_map_region_snapshot.h"
#include "snapshot/memory_snapshot.h"
#include "snapshot/module_snapshot.h"
#include "snapshot/process_snapshot.h"
#include "snapshot/system_snapshot.h"
#include "snapshot/thread_snapshot.h"
#include "snapshot/win/exception_snapshot_win.h"
#include "snapshot/win/memory_map_region_snapshot_win.h"
#include "snapshot/win/memory_snapshot_win.h"
#include "snapshot/win/module_snapshot_win.h"
#include "snapshot/win/system_snapshot_win.h"
#include "snapshot/win/thread_snapshot_win.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/misc/uuid.h"
#include "util/win/address_types.h"
#include "util/win/process_structs.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

//! \brief A ProcessSnapshot of a running (or crashed) process running on a
//!     Windows system.
class ProcessSnapshotWin final : public ProcessSnapshot {
 public:
  ProcessSnapshotWin();
  ~ProcessSnapshotWin() override;

  //! \brief Initializes the object.
  //!
  //! \param[in] process The handle to create a snapshot from.
  //! \param[in] suspension_state Whether \a process has been suspended by the
  //!     caller.
  //! \param[in] debug_critical_section_address The address in the target
  //!     process's address space of a `CRITICAL_SECTION` allocated with valid
  //!     `.DebugInfo`. Used as a starting point to walk the process's locks.
  //!     May be `0`.
  //!
  //! \return `true` if the snapshot could be created, `false` otherwise with
  //!     an appropriate message logged.
  //!
  //! \sa ScopedProcessSuspend
  bool Initialize(HANDLE process,
                  ProcessSuspensionState suspension_state,
                  WinVMAddress debug_critical_section_address);

  //! \brief Initializes the object's exception.
  //!
  //! This populates the data to be returned by Exception().
  //!
  //! This method must not be called until after a successful call to
  //! Initialize().
  //!
  //! \param[in] exception_information_address The address in the client
  //!     process's address space of an ExceptionInformation structure.
  //!
  //! \return `true` if the exception information could be initialized, `false`
  //!     otherwise with an appropriate message logged. When this method returns
  //!     `false`, the ProcessSnapshotWin object's validity remains unchanged.
  bool InitializeException(WinVMAddress exception_information_address);

  //! \brief Sets the value to be returned by ReportID().
  //!
  //! The crash report ID is under the control of the snapshot producer, which
  //! may call this method to set the report ID. If this is not done, ReportID()
  //! will return an identifier consisting entirely of zeroes.
  void SetReportID(const UUID& report_id) { report_id_ = report_id; }

  //! \brief Sets the value to be returned by ClientID().
  //!
  //! The client ID is under the control of the snapshot producer, which may
  //! call this method to set the client ID. If this is not done, ClientID()
  //! will return an identifier consisting entirely of zeroes.
  void SetClientID(const UUID& client_id) { client_id_ = client_id; }

  //! \brief Sets the value to be returned by AnnotationsSimpleMap().
  //!
  //! All process annotations are under the control of the snapshot producer,
  //! which may call this method to establish these annotations. Contrast this
  //! with module annotations, which are under the control of the process being
  //! snapshotted.
  void SetAnnotationsSimpleMap(
      const std::map<std::string, std::string>& annotations_simple_map) {
    annotations_simple_map_ = annotations_simple_map;
  }

  //! \brief Returns options from CrashpadInfo structures found in modules in
  //!     the process.
  //!
  //! \param[out] options Options set in CrashpadInfo structures in modules in
  //!     the process.
  void GetCrashpadOptions(CrashpadInfoClientOptions* options);

  // ProcessSnapshot:

  pid_t ProcessID() const override;
  pid_t ParentProcessID() const override;
  void SnapshotTime(timeval* snapshot_time) const override;
  void ProcessStartTime(timeval* start_time) const override;
  void ProcessCPUTimes(timeval* user_time, timeval* system_time) const override;
  void ReportID(UUID* report_id) const override;
  void ClientID(UUID* client_id) const override;
  const std::map<std::string, std::string>& AnnotationsSimpleMap()
      const override;
  const SystemSnapshot* System() const override;
  std::vector<const ThreadSnapshot*> Threads() const override;
  std::vector<const ModuleSnapshot*> Modules() const override;
  const ExceptionSnapshot* Exception() const override;
  std::vector<const MemoryMapRegionSnapshot*> MemoryMap() const override;
  std::vector<HandleSnapshot> Handles() const override;
  std::vector<const MemorySnapshot*> ExtraMemory() const override;

 private:
  // Initializes threads_ on behalf of Initialize().
  void InitializeThreads();

  // Initializes modules_ on behalf of Initialize().
  void InitializeModules();

  // Initializes various memory blocks reachable from the PEB on behalf of
  // Initialize().
  template <class Traits>
  void InitializePebData(WinVMAddress debug_critical_section_address);

  void AddMemorySnapshot(WinVMAddress address,
                         WinVMSize size,
                         PointerVector<internal::MemorySnapshotWin>* into);

  template <class Traits>
  void AddMemorySnapshotForUNICODE_STRING(
      const process_types::UNICODE_STRING<Traits>& us,
      PointerVector<internal::MemorySnapshotWin>* into);

  template <class Traits>
  void AddMemorySnapshotForLdrLIST_ENTRY(
      const process_types::LIST_ENTRY<Traits>& le,
      size_t offset_of_member,
      PointerVector<internal::MemorySnapshotWin>* into);

  WinVMSize DetermineSizeOfEnvironmentBlock(
      WinVMAddress start_of_environment_block);

  // Starting from the address of a CRITICAL_SECTION, walks the doubly-linked
  // list stored in RTL_CRITICAL_SECTION.DebugInfo.ProcessLocksList adding both
  // the RTL_CRITICAL_SECTION and the RTL_CRITICAL_SECTION_DEBUG memory blocks
  // to the snapshot.
  template <class Traits>
  void ReadLocks(WinVMAddress start,
                 PointerVector<internal::MemorySnapshotWin>* into);

  internal::SystemSnapshotWin system_;
  PointerVector<internal::MemorySnapshotWin> extra_memory_;
  PointerVector<internal::ThreadSnapshotWin> threads_;
  PointerVector<internal::ModuleSnapshotWin> modules_;
  scoped_ptr<internal::ExceptionSnapshotWin> exception_;
  PointerVector<internal::MemoryMapRegionSnapshotWin> memory_map_;
  ProcessReaderWin process_reader_;
  UUID report_id_;
  UUID client_id_;
  std::map<std::string, std::string> annotations_simple_map_;
  timeval snapshot_time_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSnapshotWin);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_WIN_PROCESS_SNAPSHOT_WIN_H_
