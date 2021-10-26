// SPDX-License-Identifier: BSD-2-Clause
//
// This code has been produced by the European Spallation Source
// and its partner institutes under the BSD 2 Clause License.
//
// See LICENSE.md at the top level for license information.
//
// Screaming Udder!                              https://esss.se

#pragma once

#include "LinkAndStreamSettings.h"
#include "MetaData/Tracker.h"
#include "StreamHDFInfo.h"
#include "json.h"
#include "logger.h"
#include <H5Ipublic.h>
#include <chrono>
#include <deque>
#include <h5cpp/hdf5.hpp>
#include <memory>
#include <string>
#include <vector>

namespace FileWriter {

class HDFFileBase {
public:
  virtual ~HDFFileBase() = default;
  virtual void flush();

  auto hdfGroup() const { return H5File.root(); }

protected:
  auto &hdfFile() { return H5File; }
  void init(const std::string &NexusStructure,
            std::vector<StreamHDFInfo> &StreamHDFInfo);

  void init(const nlohmann::json &NexusStructure,
            std::vector<StreamHDFInfo> &StreamHDFInfo);

private:
  hdf5::file::File H5File;
};

class HDFFile : public HDFFileBase {
public:
  HDFFile(std::string const &FileName, nlohmann::json const &NexusStructure,
          std::vector<StreamHDFInfo> &StreamHDFInfo,
          MetaData::TrackerPtr &TrackerPtr);
  void addLinksAndMetaData(std::vector<LinkSettings> const &LinkSettingsList);
  void openInSWMRMode();
  void openInRegularMode();
  bool isSWMRMode() const;
  bool isRegularMode() const;
  ~HDFFile() = default;

private:
  bool SWMRMode{false};
  void createFileInRegularMode();
  void openFileInRegularMode();
  void openFileInSWMRMode();
  void closeFile();

  hdf5::property::FileAccessList FileAccessList;
  hdf5::property::FileCreationList FileCreationList;

  std::string H5FileName;
  nlohmann::json StoredNexusStructure;
  MetaData::TrackerPtr const MetaDataTracker;
};

} // namespace FileWriter
