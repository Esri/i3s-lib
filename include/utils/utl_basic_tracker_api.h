/*
Copyright 2020 Esri

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of
the License at http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.

For additional information, contact:
Environmental Systems Research Institute, Inc.
Attn: Contracts Dept
380 New York Street
Redlands, California, USA 92373
email: contracts@esri.com
*/

#pragma once
#include "utils/utl_string.h" //TBD: too much of a dependency ?
#include "utils/utl_declptr.h"
#include <array>

namespace i3slib
{

namespace utl
{

//! API to get task callbacks.
//! WARNING: xxxEvent() functions may be called by any threads so implementation should be prepared for that.
//! NOTE: logging a "critical" message will "fail" the progress. cancel() will set the status to cancel too.
enum class Log_level  { Debug = 0, Info, Warning, Critical, _Count };
class Basic_tracker
{
public:
  DECL_PTR(Basic_tracker);
  enum class Status { Not_set = 0, Success, Failure, Canceled };
  virtual ~Basic_tracker(){}
  //--- Basic_tracker: ---
  virtual void    set_progress_steps(int64_t steps)=0;
  virtual void    set_progress( int64_t current_step ) = 0;    //! implementation MUST be thread-safe
  virtual void    set_finished(Status s) = 0;         //! implementation MUST be thread-safe
  virtual Status  get_status() const = 0;
  virtual void    cancel() { set_finished(Status::Canceled); }
  virtual bool    is_canceled() const { return get_status() == Status::Canceled; }
  virtual bool    is_keep_going() const { return  get_status() == Status::Not_set;}

  template<typename... Args>
  static bool log(Basic_tracker* tracker, Log_level level, int code, Args&&... args)
  {
    if (tracker)
    {
      const std::array<std::string, sizeof...(Args)> arr = { to_string(std::forward<Args>(args))... };
      tracker->log_it_(level, code, sizeof...(Args), arr.data());
    }
    
    return false;
  }

protected:

  virtual void log_it_(Log_level lev, int code, size_t count, const std::string* args) = 0;

  static void log_it_(Basic_tracker& tracker, Log_level level, int code, size_t count, const std::string* args)
  {
    tracker.log_it_(level, code, count, args);
  }
};

template<typename... Args>
bool log_error(Basic_tracker* tracker, int code, Args&&... args)
{
  return Basic_tracker::log(tracker, Log_level::Critical, code, std::forward<Args>(args)...);
}

template<typename... Args>
bool log_warning(Basic_tracker* tracker, int code, Args&&... args)
{
  return Basic_tracker::log(tracker, Log_level::Warning, code, std::forward<Args>(args)...);
}

template<typename... Args>
bool log_info(Basic_tracker* tracker, int code, Args&&... args)
{
  return Basic_tracker::log(tracker, Log_level::Info, code, std::forward<Args>(args)...);
}

template<typename... Args>
bool log_debug(Basic_tracker* tracker, int code, Args&&... args)
{
  return Basic_tracker::log(tracker, Log_level::Debug, code, std::forward<Args>(args)...);
}

} // namespace utl

} // namespace i3slib
