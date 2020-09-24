// SPDX-License-Identifier: BSD-2-Clause
//
// This code has been produced by the European Spallation Source
// and its partner institutes under the BSD 2 Clause License.
//
// See LICENSE.md at the top level for license information.
//
// Screaming Udder!                              https://esss.se

/** Copyright (C) 2018 European Spallation Source ERIC */

/// \file
/// \brief Define classes required for chopper time stamp writing.

#pragma once

#include "FlatbufferMessage.h"
#include "NeXusDataset/NeXusDataset.h"
#include "WriterModuleBase.h"

namespace WriterModule {
namespace tdct {
using FlatbufferMessage = FileWriter::FlatbufferMessage;

/// See parent class for documentation.
class tdct_Writer : public WriterModule::Base {
public:
  tdct_Writer() : WriterModule::Base(false) {}
  ~tdct_Writer() override = default;

  void parse_config(std::string const &) override;

  InitResult init_hdf(hdf5::node::Group &HDFGroup,
                      std::string const &HDFAttributes) override;

  InitResult reopen(hdf5::node::Group &HDFGroup) override;

  void write(FlatbufferMessage const &Message) override;

protected:
  NeXusDataset::Time Timestamp;
  NeXusDataset::CueIndex CueTimestampIndex;
  NeXusDataset::CueTimestampZero CueTimestamp;
  SharedLogger Logger = spdlog::get("filewriterlogger");
};
} // namespace tdct
} // namespace WriterModule