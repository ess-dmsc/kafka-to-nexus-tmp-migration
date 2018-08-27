#include "FlatbufferReader.h"

namespace FileWriter {
namespace Schemas {
namespace f142 {

FBUF const *get_fbuf(char const *data) { return GetLogData(data); }

bool FlatbufferReader::verify(FlatbufferMessage const &Message) const {
  auto Verifier = flatbuffers::Verifier(
      reinterpret_cast<const uint8_t *>(Message.data()), Message.size());
  return VerifyLogDataBuffer(Verifier);
}

std::string
FlatbufferReader::source_name(FlatbufferMessage const &Message) const {
  auto fbuf = get_fbuf(Message.data());
  auto s1 = fbuf->source_name();
  if (!s1) {
    LOG(Sev::Warning, "message has no source name");
    return "";
  }
  return s1->str();
}

uint64_t FlatbufferReader::timestamp(FlatbufferMessage const &Message) const {
  auto fbuf = get_fbuf(Message.data());
  return fbuf->timestamp();
}

static FlatbufferReaderRegistry::Registrar<FlatbufferReader>
    RegisterReader("f142");
}
}
}