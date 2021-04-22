#pragma once

#include <modm/board.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include <lely/coapp/slave.hpp>

class MySlave : public lely::canopen::BasicSlave {
 public:
  using BasicSlave::BasicSlave;

 protected:
  // This function gets called every time a value is written to the local object
  // dictionary by an SDO or RPDO.
  void OnWrite(uint16_t idx, uint8_t subidx) noexcept override {
    if (idx == 0x4000 && subidx == 0) {
      // Read the value just written to object 4000:00, probably by RPDO 1.
      uint32_t val = (*this)[0x4000][0];
      MODM_LOG_INFO << "slave: received " << val << modm::endl;
      // Copy it to object 4001:00, so that it will be sent by the next TPDO.
      (*this)[0x4001][0] = val;
    }
  }
};
