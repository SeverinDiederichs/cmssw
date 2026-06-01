#include "SimG4Core/Application/interface/EventWindowTimer.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <string>

namespace {
  using Clock = std::chrono::steady_clock;

  std::mutex timerMutex;
  bool sawFirstEvent = false;
  bool sawLastEvent = false;
  Clock::time_point firstEventBegin;
  Clock::time_point lastEventEnd;
  unsigned long long completedEvents = 0;

  std::string timingLabel() {
    if (auto const* label = std::getenv("CMS_TIMING_LABEL")) {
      if (label[0] != '\0') {
        return label;
      }
    }
    return "cmsRun";
  }
}  // namespace

void simg4::eventWindowTimerBeginEvent() {
  auto const now = Clock::now();
  std::lock_guard<std::mutex> lock(timerMutex);
  if (!sawFirstEvent) {
    sawFirstEvent = true;
    firstEventBegin = now;
  }
}

void simg4::eventWindowTimerEndEvent() {
  auto const now = Clock::now();
  std::lock_guard<std::mutex> lock(timerMutex);
  ++completedEvents;
  if (!sawLastEvent || now > lastEventEnd) {
    sawLastEvent = true;
    lastEventEnd = now;
  }
}

void simg4::eventWindowTimerPrintSummary() {
  unsigned long long events = 0;
  bool hasMeasurement = false;
  double elapsedSeconds = 0.0;

  {
    std::lock_guard<std::mutex> lock(timerMutex);
    events = completedEvents;
    hasMeasurement = sawFirstEvent && sawLastEvent && completedEvents > 0;
    if (hasMeasurement) {
      elapsedSeconds = std::chrono::duration<double>(lastEventEnd - firstEventBegin).count();
    }
  }

  if (!hasMeasurement || elapsedSeconds <= 0.0) {
    edm::LogWarning("SimG4EventWindowTimer") << "No completed events available for event-window timing";
    return;
  }

  auto const throughput = static_cast<double>(events) / elapsedSeconds;
  edm::LogPrint("SimG4EventWindowTimer")
      << "SimG4EventWindowTimer> label=" << timingLabel() << " events=" << events
      << " event-window-wall=" << std::fixed << std::setprecision(6) << elapsedSeconds
      << " s throughput=" << throughput << " ev/s\n"
      << "SimG4EventWindowTimer> definition: first G4 BeginOfEventAction to last G4 EndOfEventAction; no warmup "
         "skip, no fit, no outlier rejection";
}
