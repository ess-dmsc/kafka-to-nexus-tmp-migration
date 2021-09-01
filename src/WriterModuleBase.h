// SPDX-License-Identifier: BSD-2-Clause
//
// This code has been produced by the European Spallation Source
// and its partner institutes under the BSD 2 Clause License.
//
// See LICENSE.md at the top level for license information.
//
// Screaming Udder!                              https://esss.se

#pragma once

#include "FlatbufferMessage.h"
#include "JsonConfig/Field.h"
#include "JsonConfig/FieldHandler.h"
#include <h5cpp/hdf5.hpp>
#include <memory>
#include <string>
#include <string_view>
#include "MetaData/Tracker.h"

namespace WriterModule {

enum class InitResult { ERROR = -1, OK = 0 };

/// \brief Writes a given flatbuffer to HDF.
///
/// Base class for the writer modules which are responsible for actually
/// writing a flatbuffer message to the HDF file.  A writer module is
/// instantiated for each 'stream' which is configured in a file writer json
/// command.  The writer module class registers itself via a string id which
/// must be unique.  This id is used in the file writer json command.  The id
/// can be arbitrary but should as a convention contain the flatbuffer schema
/// id (`FBID`) like `FBID_<writer-module-name>`.
/// Example: Please see `src/schemas/ev42/ev42_rw.cpp`.
class Base {
public:
  Base(bool AcceptRepeatedTimestamps, std::string_view NX_class)
      : WriteRepeatedTimestamps(AcceptRepeatedTimestamps), NX_class(NX_class) {}
  virtual ~Base() = default;

  bool acceptsRepeatedTimestamps() const { return WriteRepeatedTimestamps; }

  auto defaultNeXusClass() const { return NX_class; }

  /// \brief Parses the configuration JSON structure for a stream.
  ///
  /// \note Should  NOT be called by the writer class itself. Is called by the
  /// application right after the constructor has been called.
  /// \param config_stream Configuration from the write file command for this
  /// stream.
  void parse_config(std::string const &ConfigurationStream) {
    ConfigHandler.processConfigData(ConfigurationStream);
    config_post_processing();
  }

  /// \brief For doing extra processing related to the configuration of the
  /// writer module.
  ///
  /// If extra processing of the configuration parameters is requried, for
  /// example: converting a string to an enum, it should
  /// be done in this function. This function is called by the application right
  /// after the constructor and parse_config().
  virtual void config_post_processing(){};


  /// \brief Override this function to register meta data values/fields.
  ///
  /// This function is called after Base::config_post_processing() and before Base::reopen() thus it is
  /// expected that the writer is properly configured. When it is called. Meta-data values that are to be stored to file
  /// will be written at the end of the write job just before closing the file for good.
  /// \param HDFGroup The (base/root) location in the HDF5 hierarchy for the meta-data.
  /// \param Tracker A unique pointer to an instance of the MetaData::Tracker class that is used to keep track of known meta-data fields.
  virtual void register_meta_data([[maybe_unused]] hdf5::node::Group const &HDFGroup, [[maybe_unused]] MetaData::TrackerPtr const &Tracker) {};

  /// \brief Initialise the writer instance for writing.
  ///
  /// Must be called before any data has arrived. Is used to initialise the HDF structure and (e.g. meta data) variables used in the writing process.
  ///
  /// \param HDFGroup The \p HDFGroup into which this module
  /// should write its data.
  /// \return The result.
  virtual InitResult init_hdf(hdf5::node::Group &HDFGroup) const = 0;

  /// \brief Reopen the HDF objects which are used by this writer module.
  ///
  /// \param InitParameters Contains most importantly the \p HDFGroup into
  /// which this module should write its data.
  ///
  /// \return The result.
  virtual InitResult reopen(hdf5::node::Group &HDFGroup) = 0;

  /// \brief Process the message in some way, for example write to the HDF file.
  ///
  /// \param msg The message to process
  virtual void write(FileWriter::FlatbufferMessage const &Message) = 0;

  void registerField(JsonConfig::FieldBase *Ptr) {
    ConfigHandler.registerField(Ptr);
  }

private:
  // Must appear before any config field object.
  JsonConfig::FieldHandler ConfigHandler;

protected:
  JsonConfig::Field<std::string> SourceName{this, "source", ""};
  JsonConfig::Field<std::string> Topic{this, "topic", ""};
  JsonConfig::Field<std::string> WriterModule{this, "writer_module", ""};

private:
  bool WriteRepeatedTimestamps;
  std::string_view NX_class;
};

class WriterException : public std::runtime_error {
public:
  explicit WriterException(const std::string &ErrorMessage)
      : std::runtime_error(ErrorMessage) {}
};

using ptr = std::unique_ptr<Base>;

} // namespace WriterModule
