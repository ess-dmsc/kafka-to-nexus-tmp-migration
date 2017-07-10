#include "../HDFFile.h"
#include "../CommandHandler.h"
#include "../HDFFile_impl.h"
#include "../KafkaW.h"
#include "../SchemaRegistry.h"
#include "../helper.h"
#include "../schemas/ev42/ev42_synth.h"
#include "../schemas/f141/synth.h"
#include <gtest/gtest.h>
#include <random>
#include <rapidjson/document.h>
#include <unistd.h>

// Verify
TEST(HDFFile, create) {
  auto fname = "tmp-test.h5";
  unlink(fname);
  using namespace BrightnESS::FileWriter;
  HDFFile f1;
  f1.init("tmp-test.h5", rapidjson::Value());
}

class T_HDFFile : public testing::Test {
public:
  static void write_f141() {
    auto fname = "tmp-test.h5";
    auto source_name = "some-sourcename";
    unlink(fname);
    using namespace BrightnESS;
    using namespace BrightnESS::FileWriter;
    FileWriter::HDFFile f1;
    f1.init(fname, rapidjson::Value());
    auto &reg = BrightnESS::FileWriter::Schemas::SchemaRegistry::items();
    std::array<char, 4> fbid{{'f', '1', '4', '1'}};
    auto writer = reg.at(fbid)->create_reader()->create_writer();
    BrightnESS::FileWriter::Msg msg;
    BrightnESS::FlatBufs::f141_epics_nt::synth synth(
        source_name,
        BrightnESS::FlatBufs::f141_epics_nt::PV::NTScalarArrayDouble, 1, 1);
    auto fb = synth.next<double>(0);
    msg = BrightnESS::FileWriter::Msg{(char *)fb.builder->GetBufferPointer(),
                                      (int32_t)fb.builder->GetSize()};
    // f1.impl->h5file
    writer->init(&f1, "/", source_name, msg, nullptr, nullptr);
  }
};

TEST_F(T_HDFFile, write_f141) { T_HDFFile::write_f141(); }

class T_CommandHandler : public testing::Test {
public:
  static void new_03() {
    using namespace BrightnESS;
    using namespace BrightnESS::FileWriter;
    auto cmd = gulp("tests/msg-cmd-new-03.json");
    LOG(7, "cmd: {:.{}}", cmd.data(), cmd.size());
    rapidjson::Document d;
    d.Parse(cmd.data(), cmd.size());
    char const *fname = d["file_attributes"]["file_name"].GetString();
    // char const * source_name = d["streams"][0]["source"].GetString();
    unlink(fname);
    FileWriter::CommandHandler ch(nullptr, nullptr);
    ch.handle({cmd.data(), (int32_t)cmd.size()});
  }

  static void new_03_data() {
    using namespace BrightnESS;
    using namespace BrightnESS::FileWriter;
    auto cmd = gulp("tests/msg-cmd-new-03.json");
    LOG(7, "cmd: {:.{}}", cmd.data(), cmd.size());
    rapidjson::Document d;
    d.Parse(cmd.data(), cmd.size());
    char const *fname = d["file_attributes"]["file_name"].GetString();
    char const *source_name = d["streams"][0]["source"].GetString();
    unlink(fname);
    FileWriter::CommandHandler ch(nullptr, nullptr);
    ch.handle({cmd.data(), (int32_t)cmd.size()});

    ASSERT_EQ(ch.file_writer_tasks.size(), (size_t)1);

    auto &fwt = ch.file_writer_tasks.at(0);
    ASSERT_EQ(fwt->demuxers().size(), (size_t)1);

    // TODO
    // Make demuxer process the test message.

    // From here, I need the file writer task instance
    return;

    auto &reg = BrightnESS::FileWriter::Schemas::SchemaRegistry::items();
    std::array<char, 4> fbid{{'f', '1', '4', '1'}};
    auto writer = reg.at(fbid)->create_reader()->create_writer();
    BrightnESS::FileWriter::Msg msg;
    BrightnESS::FlatBufs::f141_epics_nt::synth synth(
        source_name,
        BrightnESS::FlatBufs::f141_epics_nt::PV::NTScalarArrayDouble, 1, 1);
    auto fb = synth.next<double>(0);
    msg = BrightnESS::FileWriter::Msg{(char *)fb.builder->GetBufferPointer(),
                                      (int32_t)fb.builder->GetSize()};
  }

  static bool check_cue(std::vector<uint64_t> const &event_time_zero,
                        std::vector<uint32_t> const &event_index,
                        uint64_t cue_timestamp_zero, uint32_t cue_index) {
    bool found = false;
    size_t i2 = 0;
    for (auto &evt : event_time_zero) {
      if (evt == cue_timestamp_zero) {
        found = true;
        break;
      }
      ++i2;
    }
    if (!found)
      return false;
    if (event_index[i2] != cue_index)
      return false;
    return true;
  }

  static void data_ev42() {
    using namespace BrightnESS;
    using namespace BrightnESS::FileWriter;
    using std::array;
    using std::vector;
    using std::string;
    auto cmd = gulp("tests/msg-cmd-new-03.json");
    LOG(7, "cmd: {:.{}}", cmd.data(), cmd.size());
    rapidjson::Document d;
    d.Parse(cmd.data(), cmd.size());
    auto fname = get_string(&d, "file_attributes.file_name");
    auto source_name = get_string(&d, "streams.0.source");
    // char const * fname = d["file_attributes"]["file_name"].GetString();
    // char const * source_name = d["streams"][0]["source"].GetString();
    unlink(string(fname).c_str());
    rapidjson::Document config_file;
    config_file.Parse("{\"nexus\":{\"indices\":{\"index_every_kb\":1000}}}");
    ASSERT_FALSE(config_file.HasParseError());
    FileWriter::CommandHandler ch(nullptr, &config_file);
    ch.handle({cmd.data(), (int32_t)cmd.size()});

    ASSERT_EQ(ch.file_writer_tasks.size(), (size_t)1);

    auto &fwt = ch.file_writer_tasks.at(0);
    ASSERT_EQ(fwt->demuxers().size(), (size_t)1);

    std::vector<FlatBufs::ev42::fb> fbs;
    std::vector<BrightnESS::FileWriter::Msg> msgs;

    using DT = uint32_t;
    int const SP = 256 * 1024;
    int seed = 1;
    std::mt19937 rnd_nn;
    rnd_nn.seed(0);
    auto &reg = BrightnESS::FileWriter::Schemas::SchemaRegistry::items();
    std::array<char, 4> fbid{{'e', 'v', '4', '2'}};
    auto writer = reg.at(fbid)->create_reader()->create_writer();
    BrightnESS::FlatBufs::ev42::synth synth(source_name, seed);
    LOG(7, "generating...");
    for (int i1 = 0; i1 < SP; ++i1) {
      fbs.push_back(synth.next(rnd_nn() >> 24));
      auto &fb = fbs.back();
      msgs.push_back(
          BrightnESS::FileWriter::Msg{(char *)fb.builder->GetBufferPointer(),
                                      (int32_t)fb.builder->GetSize()});
    }
    LOG(7, "processing...");
    using CLK = std::chrono::steady_clock;
    using MS = std::chrono::milliseconds;
    auto t1 = CLK::now();
    for (auto &msg : msgs) {
      if (false) {
        auto v = binary_to_hex(msg.data, msg.size);
        LOG(7, "msg:\n{:.{}}", v.data(), v.size());
      }
      fwt->demuxers().at(0).process_message(msg.data, msg.size);
    }
    auto t2 = CLK::now();
    LOG(7, "processing done in {}",
        std::chrono::duration_cast<MS>(t2 - t1).count());
    LOG(7, "finishing...");
    fwt.reset();
    LOG(7, "done.");

    herr_t err;

    auto fid = H5Fopen(string(fname).c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    ASSERT_GE(fid, 0);

    auto g0 = H5Gopen(fid, "/entry-01/instrument-01/events-01", H5P_DEFAULT);
    ASSERT_GE(g0, 0);

    auto ds = H5Dopen2(fid, "/entry-01/instrument-01/events-01/event_id",
                       H5P_DEFAULT);
    ASSERT_GE(ds, 0);

    auto dt = H5Dget_type(ds);
    ASSERT_GE(dt, 0);
    ASSERT_EQ(H5Tget_size(dt), sizeof(DT));

    auto dsp = H5Dget_space(ds);
    ASSERT_GE(dsp, 0);
    ASSERT_EQ(H5Sis_simple(dsp), 1);
    auto nn = H5Sget_simple_extent_npoints(dsp);

    auto mem = H5Screate(H5S_SIMPLE);
    using A = array<hsize_t, 1>;
    A sini = {{(uint32_t)nn}};
    A smax = {{(uint32_t)nn}};
    H5Sset_extent_simple(mem, sini.size(), sini.data(), smax.data());

    vector<DT> data(nn);

    err =
        H5Dread(ds, H5T_NATIVE_UINT32, dsp, H5S_ALL, H5P_DEFAULT, data.data());
    ASSERT_GE(err, 0);

    {
      rnd_nn.seed(0);
      size_t n1 = 0;
      BrightnESS::FlatBufs::ev42::synth synth(source_name, seed);
      for (int i1 = 0; i1 < SP; ++i1) {
        auto fb_ = synth.next(rnd_nn() >> 24);
        auto fb = fb_.root();
        auto a = fb->detector_id();
        for (size_t i2 = 0; i2 < a->size(); ++i2) {
          // LOG(3, "{}, {}, {}", i2, data.at(i2), a->Get(i2));
          ASSERT_EQ(a->Get(i2), data.at(n1));
          ++n1;
        }
      }
    }

    {
      auto ds_cue_timestamp_zero =
          H5Dopen2(fid, "/entry-01/instrument-01/events-01/cue_timestamp_zero",
                   H5P_DEFAULT);
      ASSERT_GE(ds_cue_timestamp_zero, 0);
      vector<uint64_t> cue_timestamp_zero(
          H5Sget_simple_extent_npoints(H5Dget_space(ds_cue_timestamp_zero)));
      err = H5Dread(ds_cue_timestamp_zero, H5T_NATIVE_UINT64, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, cue_timestamp_zero.data());
      ASSERT_EQ(err, 0);
      H5Dclose(ds_cue_timestamp_zero);

      auto ds_cue_index = H5Dopen2(
          fid, "/entry-01/instrument-01/events-01/cue_index", H5P_DEFAULT);
      ASSERT_GE(ds_cue_index, 0);
      vector<uint32_t> cue_index(
          H5Sget_simple_extent_npoints(H5Dget_space(ds_cue_index)));
      err = H5Dread(ds_cue_index, H5T_NATIVE_UINT32, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, cue_index.data());
      ASSERT_EQ(err, 0);
      H5Dclose(ds_cue_index);

      ASSERT_GT(cue_timestamp_zero.size(), 10);
      ASSERT_EQ(cue_timestamp_zero.size(), cue_index.size());

      auto ds_event_time_zero =
          H5Dopen2(fid, "/entry-01/instrument-01/events-01/event_time_zero",
                   H5P_DEFAULT);
      ASSERT_GE(ds_event_time_zero, 0);
      vector<uint64_t> event_time_zero(
          H5Sget_simple_extent_npoints(H5Dget_space(ds_event_time_zero)));
      err = H5Dread(ds_event_time_zero, H5T_NATIVE_UINT64, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, event_time_zero.data());
      ASSERT_EQ(err, 0);
      H5Dclose(ds_event_time_zero);

      auto ds_event_index = H5Dopen2(
          fid, "/entry-01/instrument-01/events-01/event_index", H5P_DEFAULT);
      ASSERT_GE(ds_event_index, 0);
      vector<uint32_t> event_index(
          H5Sget_simple_extent_npoints(H5Dget_space(ds_event_index)));
      err = H5Dread(ds_event_index, H5T_NATIVE_UINT32, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, event_index.data());
      ASSERT_EQ(err, 0);
      H5Dclose(ds_event_index);

      ASSERT_GT(event_time_zero.size(), 0);
      ASSERT_EQ(event_time_zero.size(), event_index.size());

      for (int i1 = 0; i1 < cue_timestamp_zero.size(); ++i1) {
        auto ok = check_cue(event_time_zero, event_index,
                            cue_timestamp_zero[i1], cue_index[i1]);
        ASSERT_TRUE(ok);
      }
    }

    H5Tclose(dt);
    H5Dclose(ds);
    H5Gclose(g0);
    H5Fclose(fid);
  }
};

TEST_F(T_CommandHandler, new_03) { T_CommandHandler::new_03(); }

TEST_F(T_CommandHandler, new_03_data) { T_CommandHandler::new_03_data(); }

TEST_F(T_CommandHandler, data_ev42) { T_CommandHandler::data_ev42(); }