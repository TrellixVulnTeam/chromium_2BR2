// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_HANDLER_MAC_EXCEPTION_HANDLER_SERVER_H_
#define CRASHPAD_HANDLER_MAC_EXCEPTION_HANDLER_SERVER_H_

#include "base/basictypes.h"

#include <mach/mach.h>

#include "base/mac/scoped_mach_port.h"
#include "util/mach/exc_server_variants.h"

namespace crashpad {

//! \brief Runs the main exception-handling server in Crashpad’s handler
//!     process.
class ExceptionHandlerServer {
 public:
  ExceptionHandlerServer();
  ~ExceptionHandlerServer();

  //! \brief Runs the exception-handling server.
  //!
  //! \param[in] exception_interface An object to send exception messages to.
  //!
  //! This method monitors receive_port() for exception messages and no-senders
  //! notifications. It continues running until it has no more clients,
  //! indicated by the receipt of a no-senders notification. It is important to
  //! assure that a send right has been transferred to a client (or queued by
  //! `mach_msg()` to be sent to a client) prior to calling this method, or it
  //! will detect that it is sender-less and return immediately.
  //!
  //! All exception messages will be passed to \a exception_interface.
  //!
  //! This method must only be called once on an ExceptionHandlerServer object.
  //!
  //! If an unexpected condition that prevents this method from functioning is
  //! encountered, it will log a message and terminate execution. Receipt of an
  //! invalid message on receive_port() will cause a message to be logged, but
  //! this method will continue running normally.
  void Run(UniversalMachExcServer::Interface* exception_interface);

  //! \brief Returns the receive right that will be monitored for exception
  //!     messages.
  //!
  //! The caller does not take ownership of this port. The caller must not use
  //! this port for any purpose other than to make send rights for clients.
  mach_port_t receive_port() const { return receive_port_.get(); }

 private:
  base::mac::ScopedMachReceiveRight receive_port_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServer);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_MAC_EXCEPTION_HANDLER_SERVER_H_
