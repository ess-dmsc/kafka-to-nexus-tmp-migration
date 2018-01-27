#include "FileWriterTask.h"
#include "HDFFile.h"
#include "Source.h"
#include "helper.h"
#include "logger.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace FileWriter {

using std::string;
using std::vector;

std::atomic<uint32_t> n_FileWriterTask_created{0};

std::vector<DemuxTopic> &FileWriterTask::demuxers() { return _demuxers; }

FileWriterTask::FileWriterTask() {
  using namespace std::chrono;
  _id = static_cast<uint64_t>(
      duration_cast<nanoseconds>(system_clock::now().time_since_epoch())
          .count());
  _id = (_id & uint64_t(-1) << 16) | (n_FileWriterTask_created & 0xffff);
  ++n_FileWriterTask_created;
}

FileWriterTask::~FileWriterTask() {
  LOG(Sev::Debug, "~FileWriterTask");
  _demuxers.clear();
}

FileWriterTask &FileWriterTask::set_hdf_filename(std::string hdf_output_prefix,
                                                 std::string hdf_filename) {
  this->hdf_output_prefix = hdf_output_prefix;
  this->hdf_filename = hdf_filename;
  return *this;
}

void FileWriterTask::add_source(Source &&source) {
  bool found = false;
  for (auto &d : _demuxers) {
    if (d.topic() == source.topic()) {
      d.add_source(std::move(source));
      found = true;
    }
  }
  if (!found) {
    _demuxers.emplace_back(source.topic());
    auto &d = _demuxers.back();
    d.add_source(std::move(source));
  }
}

int FileWriterTask::hdf_init(rapidjson::Value const &nexus_structure,
                             rapidjson::Value const &config_file,
                             std::vector<StreamHDFInfo> &stream_hdf_info,
                             std::vector<hdf5::node::Group> &groups) {
  filename_full = hdf_filename;
  if (!hdf_output_prefix.empty()) {
    filename_full = hdf_output_prefix + "/" + filename_full;
  }

  try {
    hdf_file.init(filename_full, nexus_structure, config_file,
                  stream_hdf_info, groups);
  }
  catch (...)
  {
    LOG(Sev::Warning,
        "can not initialize hdf file  hdf_output_prefix: {}  hdf_filename: {}",
        hdf_output_prefix, hdf_filename);
    return -1;
  }

  return 0;
}

void FileWriterTask::hdf_close()
{
  // nest another exception?
  hdf_file.close();
}

int FileWriterTask::hdf_reopen() {
  try {
    hdf_file.reopen(filename_full, rapidjson::Value());
  }
  catch (...)
  {
    return -1;
  }
  return 0;
}

uint64_t FileWriterTask::id() const { return _id; }
std::string FileWriterTask::job_id() const { return _job_id; }

void FileWriterTask::job_id_init(const std::string &s) { _job_id = s; }

rapidjson::Value FileWriterTask::stats(
    rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator> &a) const {
  using namespace rapidjson;
  Value js_topics;
  js_topics.SetObject();
  for (auto &d : _demuxers) {
    js_topics.AddMember(Value(d.topic().c_str(), a), Value(0), a);
  }
  Value js_fwt;
  js_fwt.SetObject();
  js_fwt.AddMember("filename", Value(hdf_filename.c_str(), a), a);
  js_fwt.AddMember("topics", js_topics, a);
  return js_fwt;
}

} // namespace FileWriter
