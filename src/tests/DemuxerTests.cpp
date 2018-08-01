#include <DemuxTopic.h>
#include <gtest/gtest.h>

namespace FileWriter {
  
using FileWriter::FlatbufferReaderRegistry::ReaderPtr;

class MessageTimeExtractionTest : public ::testing::Test {
public:
  void SetUp() override {
    std::map<std::string, ReaderPtr> &Readers =
    FlatbufferReaderRegistry::getReaders();
    Readers.clear();
  }
};

class DummyReader2 : public FileWriter::FlatbufferReader {
public:
  bool verify(Msg const &msg) const override { return true; }
  std::string source_name(Msg const &msg) const override {
    return "SomeSourceName";
  }
  std::uint64_t timestamp(Msg const &msg) const override { return 42; }
};
  
  class DummyWriter : public HDFWriterModule {
  public:
    void parse_config(std::string const &ConfigurationStream,
                      std::string const &ConfigurationModule) override {}
    InitResult init_hdf(hdf5::node::Group &HDFGroup,
                        std::string const &HDFAttributes) override {
      return InitResult::OK();
    }
    InitResult reopen(hdf5::node::Group &HDFGrup) override {
      return InitResult::OK();
    }
    WriteResult write(Msg const &msg) override { return WriteResult::OK(); }
    std::int32_t flush() override { return 0; }
    
    std::int32_t close() override { return 0; }
    
    void enable_cq(CollectiveQueue *cq, HDFIDStore *hdf_store,
                   int mpi_rank) override {}
  };

TEST_F(MessageTimeExtractionTest, Success) {
  std::string TestKey("temp");
  { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(TestKey); }
  char *TestData = new char[8];
  std::memcpy(TestData + 4, TestKey.c_str(), 4);
  FileWriter::Msg CurrentMessage = Msg::owned(TestData, 8);
  MessageTimestamp MessageInfo;
  EXPECT_NO_THROW(MessageInfo = getMessageTime(CurrentMessage));
  EXPECT_EQ(MessageInfo.Sourcename, "SomeSourceName");
  EXPECT_EQ(MessageInfo.Timestamp, 42ul);
}

  TEST_F(MessageTimeExtractionTest, WrongKey) {
    std::string TestKey("temp");
    std::string MessageKey("temr");
    { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(TestKey); }
    char *TestData = new char[8];
    std::memcpy(TestData + 4, MessageKey.c_str(), 4);
    FileWriter::Msg CurrentMessage = Msg::owned(TestData, 8);
    MessageTimestamp MessageInfo;
    EXPECT_THROW(getMessageTime(CurrentMessage), std::runtime_error);
  }

  TEST_F(MessageTimeExtractionTest, WrongMsgSize) {
    std::string TestKey("temp");
    { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(TestKey); }
    char *TestData = new char[8];
    std::memcpy(TestData + 4, TestKey.c_str(), 4);
    FileWriter::Msg CurrentMessage = Msg::owned(TestData, 7);
    MessageTimestamp MessageInfo;
    EXPECT_THROW(getMessageTime(CurrentMessage), std::runtime_error);
  }
  
  class DemuxerTest : public ::testing::Test {
  public:
    void SetUp() override {
      std::map<std::string, ReaderPtr> &Readers =
      FlatbufferReaderRegistry::getReaders();
      Readers.clear();
    }
  };
  
  TEST_F(DemuxerTest, Success) {
    std::string TestKey("temp");
    std::string SourceName("SomeSourceName");
    { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(TestKey); }
    char *TestData = new char[8];
    std::memcpy(TestData + 4, TestKey.c_str(), 4);
    FileWriter::Msg CurrentMessage = Msg::owned(TestData, 8);
    ProcessMessageResult Result;
    DemuxTopic TestDemuxer("SomeTopicName");
    DummyWriter::ptr Writer(new DummyWriter);
    Source DummySource(SourceName, std::move(Writer));
    TestDemuxer.add_source(std::move(DummySource));
    ASSERT_EQ(TestDemuxer.sources().size(), size_t(1));
    EXPECT_NO_THROW(TestDemuxer.sources().at(SourceName));
    EXPECT_NO_THROW(Result = TestDemuxer.process_message(std::move(CurrentMessage)));
    EXPECT_EQ(Result, ProcessMessageResult::OK);
    EXPECT_EQ(TestDemuxer.messages_processed.load(), size_t(1));
    EXPECT_EQ(TestDemuxer.error_message_too_small.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_no_flatbuffer_reader.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_no_source_instance.load(), size_t(0));
  }
  
  TEST_F(DemuxerTest, WrongSourceName) {
    std::string TestKey("temp");
    std::string SourceName("WrongSourceName");
    { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(TestKey); }
    char *TestData = new char[8];
    std::memcpy(TestData + 4, TestKey.c_str(), 4);
    FileWriter::Msg CurrentMessage = Msg::owned(TestData, 8);
    ProcessMessageResult Result;
    DemuxTopic TestDemuxer("SomeTopicName");
    DummyWriter::ptr Writer(new DummyWriter);
    Source DummySource(SourceName, std::move(Writer));
    TestDemuxer.add_source(std::move(DummySource));
    EXPECT_NO_THROW(Result = TestDemuxer.process_message(std::move(CurrentMessage)));
    EXPECT_EQ(Result, ProcessMessageResult::ERR);
    EXPECT_EQ(TestDemuxer.messages_processed.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_message_too_small.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_no_flatbuffer_reader.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_no_source_instance.load(), size_t(1));
  }
  
  TEST_F(DemuxerTest, WrongKey) {
    std::string TestKey("temp");
    std::string AltKey("temo");
    std::string SourceName("SomeSourceName");
    { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(AltKey); }
    char *TestData = new char[8];
    std::memcpy(TestData + 4, TestKey.c_str(), 4);
    FileWriter::Msg CurrentMessage = Msg::owned(TestData, 8);
    ProcessMessageResult Result;
    DemuxTopic TestDemuxer("SomeTopicName");
    DummyWriter::ptr Writer(new DummyWriter);
    Source DummySource(SourceName, std::move(Writer));
    TestDemuxer.add_source(std::move(DummySource));
    EXPECT_NO_THROW(Result = TestDemuxer.process_message(std::move(CurrentMessage)));
    EXPECT_EQ(Result, ProcessMessageResult::ERR);
    EXPECT_EQ(TestDemuxer.messages_processed.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_message_too_small.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_no_flatbuffer_reader.load(), size_t(1));
    EXPECT_EQ(TestDemuxer.error_no_source_instance.load(), size_t(0));
  }
  
  TEST_F(DemuxerTest, MessageTooSmall) {
    std::string TestKey("temp");
    std::string SourceName("SomeSourceName");
    { FlatbufferReaderRegistry::Registrar<DummyReader2> RegisterIt(TestKey); }
    char *TestData = new char[8];
    std::memcpy(TestData + 4, TestKey.c_str(), 4);
    FileWriter::Msg CurrentMessage = Msg::owned(TestData, 7);
    ProcessMessageResult Result;
    DemuxTopic TestDemuxer("SomeTopicName");
    DummyWriter::ptr Writer(new DummyWriter);
    Source DummySource(SourceName, std::move(Writer));
    TestDemuxer.add_source(std::move(DummySource));
    EXPECT_NO_THROW(Result = TestDemuxer.process_message(std::move(CurrentMessage)));
    EXPECT_EQ(Result, ProcessMessageResult::ERR);
    EXPECT_EQ(TestDemuxer.messages_processed.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_message_too_small.load(), size_t(1));
    EXPECT_EQ(TestDemuxer.error_no_flatbuffer_reader.load(), size_t(0));
    EXPECT_EQ(TestDemuxer.error_no_source_instance.load(), size_t(0));
  }

} // namespace FileWriter
