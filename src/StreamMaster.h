//===-- src/StreamMaster.h - Streamers manager class definition -------*- C++
//-*-===//
//
//
//===----------------------------------------------------------------------===//
///
/// \file Header only implementation of the StreamMaster, that
/// coordinates the execution of the Streamers
///
//===----------------------------------------------------------------------===//

#pragma once

#include "FileWriterTask.h"
#include "Report.hpp"

#include <atomic>

namespace FileWriter {
class StreamerOptions;
}

namespace FileWriter {

/// The StreamMaster task is coordinate the different Streamers. When
/// constructed creates a unique Streamer per topic and waits for a
/// start command. When this command is issued the StreamMaster calls
/// the write command sequentially on each Streamer. When the stop
/// command is issued, or when the Steamer reaches a predetermined
/// point in time the Streamer is stopped and removed.  The
/// StreamMaster can regularly send report on the status of the Streamers,
/// the amount of data written and other information as Kafka messages on
/// the ``status`` topic.
template <typename Streamer> class StreamMaster {
  using SEC = Status::StreamerErrorCode;
  using SMEC = Status::StreamMasterErrorCode;
  friend class CommandHandler;

public:
  StreamMaster() {}

  StreamMaster(const std::string &broker,
               std::unique_ptr<FileWriterTask> file_writer_task,
               const StreamerOptions &options)
      : Demuxers(file_writer_task->demuxers()),
        WriterTask(std::move(file_writer_task)) {

    for (auto &d : Demuxers) {
      Streamers.emplace(std::piecewise_construct,
                        std::forward_as_tuple(d.topic()),
                        std::forward_as_tuple(broker, d.topic(), options));
      Streamers[d.topic()].setSources(d.sources());
    }
    NumStreamers = Streamers.size();
  }

  StreamMaster(const StreamMaster &) = delete;
  StreamMaster(StreamMaster &&) = default;

  ~StreamMaster() {
    Stop = true;
    if (loop.joinable()) {
      loop.join();
    }
    if (ReportThread.joinable()) {
      ReportThread.join();
    }
  }

  StreamMaster &operator=(const StreamMaster &) = delete;

  /// Set the timepoint (in milliseconds) that triggers the
  /// termination of the run. When the timestamp of a Source in the
  /// Streamer reaches this time the source is removed. When all the
  /// Sources in a Steramer are removed the Streamer connection is
  /// closed and the Streamer marked as
  /// StreamerErrorCode::has_finished
  /// \param StopTime timestamp of the
  /// last message to be written in nanoseconds
  bool setStopTime(const milliseconds &StopTime) {
    for (auto &s : Streamers) {
      s.second.getOptions().StopTimestamp = StopTime;
    }
    // ForceStopThread = std::thread([&] { this->forceStop(); });
    return true;
  }

  /// Start the streams writing. Return true if successiful, false
  /// in case of failure
  bool start() {
    LOG(Sev::Info, "StreamMaster: start");
    Write = true;
    Stop = false;

    if (!loop.joinable()) {
      loop = std::thread([&] { this->run(); });
      std::this_thread::sleep_for(milliseconds(100));
    }
    return loop.joinable();
  }

  /// Stop the streams writing. Return true if successiful, false in case
  /// of failure
  bool stop() {
    Write = false;
    Stop = true;
    try {
      std::call_once(StopGuard,
                     &FileWriter::StreamMaster<Streamer>::stopImplemented,
                     this);
    }
    catch (std::exception &e) {
      LOG(Sev::Warning, "Error while stopping: {}", e.what());
    }
    if (loop.joinable()) {
      loop.join();
    }
    if (ReportThread.joinable()) {
      ReportThread.join();
    }
    return !(loop.joinable() || ReportThread.joinable());
  }

  void report(std::shared_ptr<KafkaW::ProducerTopic> p,
              const milliseconds &report_ms = milliseconds{1000}) {
    if (!ReportThread.joinable()) {
      ReportPtr.reset(new Report(p, report_ms));
      ReportThread =
          std::thread([&] { ReportPtr->report(Streamers, Stop, RunStatus); });
    } else {
      LOG(Sev::Debug, "Status report already started, nothing to do");
    }
  }

  /// Return a reference to the FileWriterTask assiciated with the
  /// current file
  FileWriterTask const &getFileWriterTask() const { return *WriterTask; }

  const SMEC status() {
    for (auto &s : Streamers) {
      if (int(s.second.runStatus()) < 0) {
        RunStatus = SMEC::streamer_error;
      }
    }
    return RunStatus.load();
  }

  /// Return the unique job id assiciated with the streamer (and hence
  /// with the NeXus file)
  std::string getJobId() const { return WriterTask->job_id(); }

private:
  bool processStreamResult(Streamer &Stream, DemuxTopic &Demux) {
    auto ProcessStartTime = std::chrono::system_clock::now();
    while (Write && ((std::chrono::system_clock::now() - ProcessStartTime) <
                     TopicWriteDuration)) {
      FileWriter::ProcessMessageResult ProcessResult = Stream.write(Demux);
      if (ProcessResult.is_OK()) {
        return false;
      }
      if (ProcessResult.is_STOP() && (Stream.numSources() == 0)) {
        closeStream(Stream, Demux.topic());
        break;
      }
      // handle errors
    }
    return true;
  }

  void run() {
    using namespace std::chrono;
    RunStatus = SMEC::running;

    while (!Stop && Demuxers.size() > 0) {
      for (auto &Demux : Demuxers) {
        auto &s = Streamers[Demux.topic()];

        // If the stream is active process the messages
        if (s.runStatus() == SEC::writing) {
          bool ProcessStreamResult(processStreamResult(s, Demux));
          if (ProcessStreamResult) {
            break;
          }
          continue;
        }
        // If the stream has finished skip to the next stream
        if (s.runStatus() == SEC::has_finished) {
          break;
        }
        // If the kafka connection is not ready skip to the next stream.
        // Nevertheless if there's only one stream wait half a second in order
        // to prevent spinning
        if (s.runStatus() == SEC::not_initialized) {
          if (Streamers.size() == 1) {
            std::this_thread::sleep_for(milliseconds(500));
          }
          continue;
        }
        if (int(s.runStatus()) < 0) {
          LOG(Sev::Error, "Error in topic {} : {}", Demux.topic(), int(s.runStatus()));
          continue;
        }
      }
    }

    RunStatus = SMEC::has_finished;
  }

  SMEC closeStream(Streamer &Stream, const std::string &TopicName) {
    LOG(Sev::Debug, "All sources in Stream have expired, close connection");
    Stream.runStatus() = SEC::has_finished;
    Stream.closeStream();
    NumStreamers--;
    if (NumStreamers != 0) {
      return SMEC::running;
    }
    Stop = true;
    return SMEC::has_finished;
  }

  /// Implementation of the stop command. Make sure that the Streamers
  /// are not polled for messages, the status report is stopped and
  /// closes all the connections to the Kafka streams.
  void stopImplemented() {
    LOG(Sev::Info, "StreamMaster: stop");
    if (loop.joinable()) {
      loop.join();
    }
    if (ReportThread.joinable()) {
      ReportThread.join();
    }
    for (auto &s : Streamers) {
      LOG(Sev::Info, "Shut down {} : {}", s.first);
      auto v = s.second.closeStream();
      if (v != SEC::has_finished) {
        LOG(Sev::Warning, "Error while stopping {} : {}", s.first,
            Status::Err2Str(v));
      } else {
        LOG(Sev::Info, "\t...done");
      }
    }
    Streamers.clear();
  }

  /// Make sure that if the stop command has been issued with a specific time
  /// point the file writer stops at most within 5 seconds since the timepoint
  /// has been reached on the system, independently on the message timestamp
  void forceStop() {
    using namespace std::chrono;
    std::this_thread::sleep_for(milliseconds(5000));
    while (!Stop) {
      milliseconds CurrentSystemTime(
          duration_cast<milliseconds>(system_clock::now().time_since_epoch()));
      if ((CurrentSystemTime > Demuxers[0].stop_time() + milliseconds(5000)) &&
          !Stop) {
        stop();
      }
    }
  }

  std::map<std::string, Streamer> Streamers;
  std::vector<DemuxTopic> &Demuxers;
  std::thread loop;
  std::thread ReportThread;
  std::thread ForceStopThread;
  std::atomic<SMEC> RunStatus{SMEC::not_started};
  std::atomic<bool> Write{false};
  std::atomic<bool> Stop{false};
  std::unique_ptr<FileWriterTask> WriterTask{nullptr};
  std::once_flag StopGuard;
  std::unique_ptr<Report> ReportPtr{nullptr};

  milliseconds TopicWriteDuration{1000};
  size_t NumStreamers{0};
};

} // namespace FileWriter
