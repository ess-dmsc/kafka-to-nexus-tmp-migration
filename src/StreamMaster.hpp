#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>

#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>

#include "DemuxTopic.h"


struct Streamer;
struct FileWriterCommand;

constexpr std::chrono::milliseconds operator "" _ms(const unsigned long long int value) {
  return std::chrono::milliseconds(value);
}

using namespace BrightnESS::FileWriter;


template<typename Streamer, typename Demux>
struct StreamMaster {
  
  StreamMaster() : keep_writing(false) { };
  
  StreamMaster(std::string& broker, std::vector<DemuxTopic>& _demux) : demux(_demux) {
    for( auto& d: demux) {
      streamer[d.topic()] = Streamer(broker,d.topic());
    }
  };

  bool start() {
    keep_writing = true;
    loop = std::thread( [&] { this->run(); } );
    return loop.joinable();
  }

  std::vector< std::pair<std::string,int> > stop() {
    keep_writing = false;
    loop.join();
    std::vector< std::pair<std::string,int> > stream_status;
    for (auto it=streamer.begin(); it!=streamer.end(); ++it) {
      stream_status.push_back(std::pair<std::string,int>(it->first,it->second.closeStream()));
    }
    return stream_status;
  }

private:
  
  void run() {
    while( keep_writing ) {
      std::for_each(demux.begin(),demux.end(), [&](Demux & item) {
          run_on_demux(item); });
    }
  }

  
  int run_on_demux(Demux& dem) {
    start_source_time = std::chrono::system_clock::now();
    int value;
    do {
      value = streamer[dem.topic()].write(dem);
    } while( value == 0 && ((std::chrono::system_clock::now() - start_source_time) < duration) );
    return value;
  }
  
   
  static std::chrono::milliseconds duration;
  std::chrono::system_clock::time_point start_source_time;
  std::map<std::string,Streamer> streamer;
  std::vector<Demux>& demux;  
  std::atomic<int> index;
  std::map< std::string,std::vector< std::pair<std::string,int64_t> > > timestamp_list;
  std::atomic<bool> keep_writing;
  std::thread loop;
};

template<typename S,typename D>
std::chrono::milliseconds StreamMaster<S,D>::duration=1_ms;
