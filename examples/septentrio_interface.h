// Copyright (C) Point One Navigation - All Rights Reserved

#include <chrono>

#include <boost/asio.hpp>
#include <ctime>
#include <fstream>
#include <string>

#include "glog/logging.h"

#include "sbf_framer.h"
#include "simple_asio_serial_port.h"

#include "third_party/septentrio/measepoch.h"
#include "third_party/septentrio/sbfread.h"

#pragma once
namespace point_one {
namespace gpsreceiver {

class SeptentrioReceiver {
 public:
  SeptentrioReceiver(const std::string &raw_log_path = "") {
    InitBinLog(raw_log_path);
  }
  virtual ~SeptentrioReceiver() {
    if (bin_log_.is_open()) {
      bin_log_.close();
    }
  }

  virtual bool Connect() = 0;

  void SetCallbackSBF(std::function<void(uint16_t, uint8_t *)> callback) {
    this->callbackSBF = callback;
    LOG(INFO) << "Set Septentrio Receiver Callback";
  }

 protected:
  std::ofstream bin_log_;
  std::function<void(uint16_t, uint8_t *)> callbackSBF;

  void InitBinLog(const std::string &raw_log_path) {
    if (!raw_log_path.empty()) {
      LOG(INFO) << " SEPT LOG PATH IS " << raw_log_path;

      char name[100];
      if (raw_log_path.find(".") == std::string::npos) {
        sprintf(name, "%s/septentrio-%d.bin", raw_log_path.c_str(),
                (int)std::time(nullptr));
      } else {
        sprintf(name, "%s", raw_log_path.c_str());
      }
      LOG(INFO) << " SEPT LOGGING TO " << name;
      bin_log_.open(name, std::ios::out | std::ios::app | std::ios::binary);
    }
  }

  void OnSBF(uint16_t len, uint8_t *data) {
    if (bin_log_.is_open()) {
      bin_log_.write((char *)data, len);
      bin_log_.flush();
    }
    if (callbackSBF != nullptr) {
      callbackSBF(len, data);
    }
  }
};

class SeptentrioSerialReceiver : public SeptentrioReceiver {
 public:
  SeptentrioSerialReceiver(boost::asio::io_service &io_service,
                           const std::string &device_path,
                           const std::string &raw_log_path = "")
      : SeptentrioReceiver(raw_log_path), serial_port(io_service), device_path_(device_path) {
    serial_port.SetCallback(std::bind(&SeptentrioSerialReceiver::OnData, this,
                                      std::placeholders::_1,
                                      std::placeholders::_2));
    framer.SetCallbackSBFFrame(std::bind(&SeptentrioSerialReceiver::OnSBF, this,
                                         std::placeholders::_1,
                                         std::placeholders::_2));
  }

  ~SeptentrioSerialReceiver() {}

  void Send(uint8_t *buf, size_t len) { serial_port.AsyncWrite(buf, len); }

  bool Connect() override { return serial_port.Open(device_path_, 115200); }

 private:
  point_one::utils::SimpleAsioSerialPort serial_port;
  std::string device_path_;
  SBFFramer framer;

  void OnData(uint8_t *data, uint16_t len) {
    for (int i = 0; i < len; i++) {
      framer.OnByte(data[i]);  // optimize...
    }
  }
};

}  // namespace gpsreceiver
}  // namespace point_one