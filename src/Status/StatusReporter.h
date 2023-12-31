// SPDX-License-Identifier: BSD-2-Clause
//
// This code has been produced by the European Spallation Source
// and its partner institutes under the BSD 2 Clause License.
//
// See LICENSE.md at the top level for license information.
//
// Screaming Udder!                              https://esss.se

#pragma once

#include "SetThreadName.h"
#include "StatusReporterBase.h"
#include <asio.hpp>
#include <chrono>

namespace Status {

/// \brief Implements the thread for reporting the filewriter status to log and
/// Kafka.
class StatusReporter : public StatusReporterBase {
public:
  StatusReporter(Kafka::BrokerSettings const &Settings,
                 std::string const &StatusTopic,
                 ApplicationStatusInfo const &StatusInformation)
      : StatusReporterBase(Settings, StatusTopic, StatusInformation), IO(),
        AsioTimer(IO, StatusInformation.UpdateInterval) {
    this->start();
  }
  StatusReporter(std::shared_ptr<Kafka::Producer> Producer,
                 std::unique_ptr<Kafka::ProducerTopic> &StatusProducerTopic,
                 ApplicationStatusInfo const &StatusInformation)
      : StatusReporterBase(Producer, std::move(StatusProducerTopic),
                           StatusInformation),
        IO(), AsioTimer(IO, StatusInformation.UpdateInterval) {
    this->start();
  }

  ~StatusReporter() override;

private:
  void start();
  void run() {
    setThreadName("status_update");
    IO.run();
  }
  asio::io_context IO;
  asio::steady_timer AsioTimer;
  std::thread StatusThread;
  /// Blocks until the timer thread has stopped
  void waitForStop();
  void postReportStatusActions() override;
};

} // namespace Status
