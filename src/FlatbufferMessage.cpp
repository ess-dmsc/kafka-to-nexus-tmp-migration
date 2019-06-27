#include "FlatbufferMessage.h"
#include "FlatbufferReader.h"
#include <functional>

namespace FileWriter {

FlatbufferMessage::FlatbufferMessage(char const *const BufferPtr,
                                     size_t const Size)
    : DataPtr(BufferPtr), DataSize(Size) {
  extractPacketInfo();
}

FlatbufferMessage::SrcHash calcSourceHash(std::string ID, std::string Name) {
  return std::hash<std::string>{}(ID + Name);
}

void FlatbufferMessage::extractPacketInfo() {
  if (DataSize < 8) {
    Valid = false;
    throw BufferTooSmallError(fmt::format(
        "Flatbuffer was only {} bytes. Expected ≥ 8 bytes.", DataSize));
  }
  std::string FlatbufferID(data() + 4, 4);
  try {
    auto &Reader = FlatbufferReaderRegistry::find(FlatbufferID);
    if (not Reader->verify(*this)) {
      throw NotValidFlatbuffer(
          fmt::format("Buffer which has flatbuffer ID \"{}\" is not a valid "
                      "flatbuffer of this type.",
                      FlatbufferID));
    }
    Sourcename = Reader->source_name(*this);
    Timestamp = Reader->timestamp(*this);
    SourceNameIDHash = calcSourceHash(FlatbufferID, Sourcename);
    ID = FlatbufferID;
  } catch (std::out_of_range &E) {
    Valid = false;
    throw UnknownFlatbufferID(fmt::format(
        "Unable to locate reader with the ID \"{}\" in the registry.",
        FlatbufferID));
  }
  Valid = true;
}
} // namespace FileWriter
