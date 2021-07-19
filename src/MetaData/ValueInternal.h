#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "HDF5Storage.h"
#include <functional>

namespace MetaDataInternal {
class ValueBaseInternal {
public:
  ValueBaseInternal(std::string const &ValueKey) : Key(ValueKey) {}
  virtual ~ValueBaseInternal() = default;
  virtual nlohmann::json getAsJSON() const = 0;
  virtual void writeToHDF5File(hdf5::node::Group ) = 0;
  std::string getKey() const { return Key;}
private:
  std::string Key;
};

template <class DataType>
class ValueInternal : public ValueBaseInternal {
public:
  ValueInternal(std::string const &Key, std::function<void(hdf5::node::Group, DataType)> HDF5Writer) : ValueBaseInternal(Key), WriteToFile(HDF5Writer) {}
  void setValue(DataType NewValue) {MetaDataValue = NewValue;}
  DataType getValue() {return MetaDataValue;}
  virtual nlohmann::json getAsJSON() const override { nlohmann::json RetObj;
    RetObj[getKey()] = MetaDataValue;
    return RetObj;}
  virtual void writeToHDF5File(hdf5::node::Group ) override {};
private:
  DataType MetaDataValue;
  std::function<void(hdf5::node::Group, DataType)> WriteToFile;
};
}