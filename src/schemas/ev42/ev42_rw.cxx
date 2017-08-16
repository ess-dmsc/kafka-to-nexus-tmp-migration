#include "../../FlatbufferReader.h"
#include "../../HDFFile.h"
#include "../../HDFFile_h5.h"
#include "../../HDFWriterModule.h"
#include "../../SchemaRegistry.h"
#include "../../h5.h"
#include "../../helper.h"
#include "schemas/ev42_events_generated.h"
#include <limits>

namespace FileWriter {
namespace Schemas {
namespace ev42 {

using std::array;
using std::vector;
using std::string;
template <typename T> using uptr = std::unique_ptr<T>;

class reader : public FBSchemaReader {
  std::unique_ptr<FBSchemaWriter> create_writer_impl() override;
  bool verify_impl(Msg msg) override;
  std::string sourcename_impl(Msg msg) override;
  uint64_t ts_impl(Msg msg) override;
};

struct append_ret {
  int status;
  uint64_t written_bytes;
  uint64_t ix0;
  operator bool() const { return status == 0; }
};

class writer : public FBSchemaWriter {
  ~writer() override;
  void init_impl(std::string const &sourcename, hid_t hdf_group,
                 Msg msg) override;
  WriteResult write_impl(Msg msg) override;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_event_time_offset;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_event_id;
  uptr<h5::h5d_chunked_1d<uint64_t>> ds_event_time_zero;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_event_index;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_cue_index;
  uptr<h5::h5d_chunked_1d<uint64_t>> ds_cue_timestamp_zero;
  bool do_flush_always = false;
  uint64_t total_written_bytes = 0;
  uint64_t index_at_bytes = 0;
  uint64_t index_every_bytes = std::numeric_limits<uint64_t>::max();
  uint64_t ts_max = 0;
};

std::unique_ptr<FBSchemaWriter> reader::create_writer_impl() {
  return std::unique_ptr<FBSchemaWriter>(new writer);
}

static EventMessage const *get_fbuf(char const *data) {
  return GetEventMessage(data);
}

bool reader::verify_impl(Msg msg) {
  auto veri = flatbuffers::Verifier((uint8_t *)msg.data, msg.size);
  if (VerifyEventMessageBuffer(veri))
    return true;
  return false;
}

uint64_t reader::ts_impl(Msg msg) {
  auto fbuf = get_fbuf(msg.data);
  return fbuf->pulse_time();
}

std::string reader::sourcename_impl(Msg msg) {
  auto fbuf = get_fbuf(msg.data);
  auto v = fbuf->source_name();
  if (!v) {
    LOG(4, "WARNING message has no source name");
    return "";
  }
  return v->str();
}

writer::~writer() {}

void writer::init_impl(std::string const &sourcename, hid_t hdf_group,
                       Msg msg) {
  LOG(7, "ev42::init_impl");

  hsize_t chunk_n_elements = 1;

  if (config_file) {
    if (auto x = get_uint(config_file, "nexus.indices.index_every_kb")) {
      index_every_bytes = uint64_t(x) * 1024;
      LOG(7, "index_every_bytes: {}", index_every_bytes);
    } else if (auto x = get_uint(config_file, "nexus.indices.index_every_mb")) {
      index_every_bytes = uint64_t(x) * 1024 * 1024;
      LOG(7, "index_every_bytes: {}", index_every_bytes);
    }
    if (auto x = get_uint(config_file, "nexus.chunk.chunk_n_elements")) {
      chunk_n_elements = uint64_t(x);
      LOG(7, "chunk_n_elements: {}", chunk_n_elements);
    }
  }

  this->ds_event_time_offset = h5::h5d_chunked_1d<uint32_t>::create(
      hdf_group, "event_time_offset", chunk_n_elements * sizeof(uint32_t));
  this->ds_event_id = h5::h5d_chunked_1d<uint32_t>::create(
      hdf_group, "event_id", chunk_n_elements * sizeof(uint32_t));
  this->ds_event_time_zero = h5::h5d_chunked_1d<uint64_t>::create(
      hdf_group, "event_time_zero", chunk_n_elements * sizeof(uint64_t));
  this->ds_event_index = h5::h5d_chunked_1d<uint32_t>::create(
      hdf_group, "event_index", chunk_n_elements * sizeof(uint32_t));
  this->ds_cue_index = h5::h5d_chunked_1d<uint32_t>::create(
      hdf_group, "cue_index", chunk_n_elements * sizeof(uint32_t));
  this->ds_cue_timestamp_zero = h5::h5d_chunked_1d<uint64_t>::create(
      hdf_group, "cue_timestamp_zero", chunk_n_elements * sizeof(uint64_t));

  if (!ds_event_time_offset || !ds_event_id || !ds_event_time_zero ||
      !ds_event_index || !ds_cue_index || !ds_cue_timestamp_zero) {
    ds_event_time_offset.reset();
    ds_event_id.reset();
    ds_event_time_zero.reset();
    ds_event_index.reset();
    ds_cue_index.reset();
    ds_cue_timestamp_zero.reset();
  }
}

WriteResult writer::write_impl(Msg msg) {
  if (!ds_event_time_offset) {
    return {-1};
  }
  auto fbuf = get_fbuf(msg.data);
  int64_t ts = fbuf->pulse_time();
  auto w1ret = this->ds_event_time_offset->append_data_1d(
      fbuf->time_of_flight()->data(), fbuf->time_of_flight()->size());
  auto w2ret = this->ds_event_id->append_data_1d(fbuf->detector_id()->data(),
                                                 fbuf->detector_id()->size());
  if (w1ret.ix0 != w2ret.ix0) {
    LOG(3, "written data lengths differ");
  }
  auto pulse_time = fbuf->pulse_time();
  this->ds_event_time_zero->append_data_1d(&pulse_time, 1);
  uint32_t event_index = w1ret.ix0;
  this->ds_event_index->append_data_1d(&event_index, 1);
  total_written_bytes += w1ret.written_bytes;
  ts_max = std::max(pulse_time, ts_max);
  if (total_written_bytes > index_at_bytes + index_every_bytes) {
    this->ds_cue_timestamp_zero->append_data_1d(&ts_max, 1);
    this->ds_cue_index->append_data_1d(&event_index, 1);
    index_at_bytes = total_written_bytes;
  }
  if (do_flush_always) {
    auto file = hdf_file->h5file_detail().h5file();
    auto err = H5Fflush(file, H5F_SCOPE_LOCAL);
    if (err < 0) {
      LOG(4, "ERROR while flushing");
    }
  }
  return {ts};
}

class Info : public SchemaInfo {
public:
  FBSchemaReader::ptr create_reader() override;
};

FBSchemaReader::ptr Info::create_reader() {
  return FBSchemaReader::ptr(new reader);
}

SchemaRegistry::Registrar<Info> g_registrar(fbid_from_str("ev42"));

class FlatbufferReader : public FileWriter::FlatbufferReader {
  bool verify(Msg const &msg) const;
  std::string sourcename(Msg const &msg) const;
  uint64_t timestamp(Msg const &msg) const;
};

bool FlatbufferReader::verify(Msg const &msg) const {
  flatbuffers::Verifier veri((uint8_t *)msg.data, msg.size);
  if (VerifyEventMessageBuffer(veri)) {
    return true;
  }
  return false;
}

std::string FlatbufferReader::sourcename(Msg const &msg) const {
  auto fbuf = get_fbuf(msg.data);
  auto s1 = fbuf->source_name();
  if (!s1) {
    LOG(4, "message has no source name");
    return "";
  }
  return s1->str();
}

uint64_t FlatbufferReader::timestamp(Msg const &msg) const {
  auto fbuf = get_fbuf(msg.data);
  return fbuf->pulse_time();
}

FlatbufferReaderRegistry::Registrar<FlatbufferReader>
    g_registrar_FlatbufferReader(fbid_from_str("ev42"));

class HDFWriterModule : public FileWriter::HDFWriterModule {
public:
  static FileWriter::HDFWriterModule::ptr create();
  InitResult init_hdf(hid_t hid, rapidjson::Value const &config_stream,
                      rapidjson::Value const *config_module) override;
  WriteResult write(Msg const &msg) override;
  int32_t flush() override;
  int32_t close() override;

  uptr<h5::h5d_chunked_1d<uint32_t>> ds_event_time_offset;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_event_id;
  uptr<h5::h5d_chunked_1d<uint64_t>> ds_event_time_zero;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_event_index;
  uptr<h5::h5d_chunked_1d<uint32_t>> ds_cue_index;
  uptr<h5::h5d_chunked_1d<uint64_t>> ds_cue_timestamp_zero;
  bool do_flush_always = false;
  uint64_t total_written_bytes = 0;
  uint64_t index_at_bytes = 0;
  uint64_t index_every_bytes = std::numeric_limits<uint64_t>::max();
  uint64_t ts_max = 0;
};

FileWriter::HDFWriterModule::ptr HDFWriterModule::create() {
  return FileWriter::HDFWriterModule::ptr(new HDFWriterModule);
}

HDFWriterModule::InitResult
HDFWriterModule::init_hdf(hid_t hid, rapidjson::Value const &config_stream,
                          rapidjson::Value const *config_module) {
  auto str = get_string(&config_stream, "source");
  if (!str) {
    return HDFWriterModule::InitResult::ERROR_INCOMPLETE_CONFIGURATION();
  }
  auto sourcename = str.v;
  str = get_string(&config_stream, "type");
  if (!str) {
    return HDFWriterModule::InitResult::ERROR_INCOMPLETE_CONFIGURATION();
  }
  auto type = str.v;
  LOG(3, "sourcename: {}", sourcename);
  LOG(3, "type: {}", type);

  LOG(7, "ev42::init_impl");

  hsize_t chunk_n_elements = 1;

  if (auto x = get_int(&config_stream, "nexus.indices.index_every_kb")) {
    index_every_bytes = (int)x * 1024;
    LOG(7, "index_every_bytes: {}", index_every_bytes);
  } else if (auto x = get_int(&config_stream, "nexus.indices.index_every_mb")) {
    index_every_bytes = (int)x * 1024 * 1024;
    LOG(7, "index_every_bytes: {}", index_every_bytes);
  }
  if (auto x = get_int(&config_stream, "nexus.chunk.chunk_n_elements")) {
    chunk_n_elements = x.v;
    LOG(7, "chunk_n_elements: {}", chunk_n_elements);
  }

  this->ds_event_time_offset = h5::h5d_chunked_1d<uint32_t>::create(
      hid, "event_time_offset", chunk_n_elements * sizeof(uint32_t));
  this->ds_event_id = h5::h5d_chunked_1d<uint32_t>::create(
      hid, "event_id", chunk_n_elements * sizeof(uint32_t));
  this->ds_event_time_zero = h5::h5d_chunked_1d<uint64_t>::create(
      hid, "event_time_zero", chunk_n_elements * sizeof(uint64_t));
  this->ds_event_index = h5::h5d_chunked_1d<uint32_t>::create(
      hid, "event_index", chunk_n_elements * sizeof(uint32_t));
  this->ds_cue_index = h5::h5d_chunked_1d<uint32_t>::create(
      hid, "cue_index", chunk_n_elements * sizeof(uint32_t));
  this->ds_cue_timestamp_zero = h5::h5d_chunked_1d<uint64_t>::create(
      hid, "cue_timestamp_zero", chunk_n_elements * sizeof(uint64_t));

  if (!ds_event_time_offset || !ds_event_id || !ds_event_time_zero ||
      !ds_event_index || !ds_cue_index || !ds_cue_timestamp_zero) {
    ds_event_time_offset.reset();
    ds_event_id.reset();
    ds_event_time_zero.reset();
    ds_event_index.reset();
    ds_cue_index.reset();
    ds_cue_timestamp_zero.reset();
  }

  return HDFWriterModule::InitResult::OK();
}

HDFWriterModule::WriteResult HDFWriterModule::write(Msg const &msg) {
  if (!ds_event_time_offset) {
    return HDFWriterModule::WriteResult::ERROR_IO();
  }
  auto fbuf = get_fbuf(msg.data);
  // int64_t ts = fbuf->pulse_time();
  auto w1ret = this->ds_event_time_offset->append_data_1d(
      fbuf->time_of_flight()->data(), fbuf->time_of_flight()->size());
  auto w2ret = this->ds_event_id->append_data_1d(fbuf->detector_id()->data(),
                                                 fbuf->detector_id()->size());
  if (w1ret.ix0 != w2ret.ix0) {
    LOG(3, "written data lengths differ");
  }
  auto pulse_time = fbuf->pulse_time();
  this->ds_event_time_zero->append_data_1d(&pulse_time, 1);
  uint32_t event_index = w1ret.ix0;
  this->ds_event_index->append_data_1d(&event_index, 1);
  total_written_bytes += w1ret.written_bytes;
  ts_max = std::max(pulse_time, ts_max);
  if (total_written_bytes > index_at_bytes + index_every_bytes) {
    this->ds_cue_timestamp_zero->append_data_1d(&ts_max, 1);
    this->ds_cue_index->append_data_1d(&event_index, 1);
    index_at_bytes = total_written_bytes;
  }
  return HDFWriterModule::WriteResult::OK();
}

int32_t HDFWriterModule::flush() { return 0; }

int32_t HDFWriterModule::close() { return 0; }

HDFWriterModuleRegistry::Registrar
    g_registrar_HDFWriterModule("ev42", HDFWriterModule::create);

} // namespace ev42
} // namespace Schemas
} // namespace FileWriter
