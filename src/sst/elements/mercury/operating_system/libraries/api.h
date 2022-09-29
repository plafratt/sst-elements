/**
Copyright 2009-2021 National Technology and Engineering Solutions of Sandia, 
LLC (NTESS).  Under the terms of Contract DE-NA-0003525, the U.S.  Government 
retains certain rights in this software.

Sandia National Laboratories is a multimission laboratory managed and operated
by National Technology and Engineering Solutions of Sandia, LLC., a wholly 
owned subsidiary of Honeywell International, Inc., for the U.S. Department of 
Energy's National Nuclear Security Administration under contract DE-NA0003525.

Copyright (c) 2009-2021, NTESS

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Questions? Contact sst-macro-help@sandia.gov
*/

#pragma once

#include <sst/core/event.h>
#include <sst/core/subcomponent.h>
#include <common/factory.h>
#include <common/component.h>
#include <common/events.h>
#include <operating_system/process/software_id.h>
#include <operating_system/process/app_fwd.h>
#include <operating_system/process/thread_fwd.h>
#include <common/timestamp.h>
#include <common/node_address.h>
#include <sys/time.h>

namespace SST {
namespace Hg {

class API
{
 public:
  SST_ELI_DECLARE_BASE(API)
  SST_ELI_DECLARE_DEFAULT_INFO()
  SST_ELI_DECLARE_CTOR(SST::Params&,App*,SST::Component*)

  virtual ~API();

  SoftwareId sid() const;

  NodeId addr() const;

  App* parent() const {
    return parent_;
  }

  Thread* activeThread();

  virtual void init(){}

  virtual void finish(){}

  Timestamp now() const;

  void schedule(Timestamp t, ExecutionEvent* ev);

  void scheduleDelay(TimeDelta t, ExecutionEvent* ev);

  /**
   * @brief start_api_call
   * Enter a call such as MPI_Send. Any perf counters or time counters
   * collected since the last API call can then advance time or
   * increment statistics.
   */
  void startAPICall();

  /**
   * @brief end_api_call
   * Exit a call such as MPI_Send. Perf counters or time counters
   * collected since the last API call can then clear counters for
   * the next time window.
   */
  void endAPICall();

 protected:
  API(SST::Params& params, App* parent, SST::Component* comp);

  App* parent_;

};

void apiLock();
void apiUnlock();

} // end namespace Hg
} // end namespace SST
