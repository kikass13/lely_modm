#pragma once

#include <modm/board.hpp>
#include <chrono>
using namespace std::chrono_literals;

#include <lely/coapp/fiber_driver.hpp>


class MyDriver : public lely::canopen::FiberDriver {
 public:
  using FiberDriver::FiberDriver;

 private:

 void OnState(lely::canopen::NmtState st) noexcept override {

 }
 void OnCanState(lely::io::CanState new_state, lely::io::CanState old_state) noexcept override {
                    
 }


  // This function gets called when the boot-up process of the slave completes.
  // The 'st' parameter contains the last known NMT state of the slave
  // (typically pre-operational), 'es' the error code (0 on success), and 'what'
  // a description of the error, if any.
  void OnBoot(lely::canopen::NmtState /*st*/, char es,
         const std::string& what) noexcept override {
    if (!es || es == 'L') {
      MODM_LOG_INFO << "slave " << static_cast<int>(id()) << " booted sucessfully"
                << modm::endl;
    } else {
      MODM_LOG_INFO << "slave " << static_cast<int>(id())
                << " failed to boot: " << what.c_str() << modm::endl;
    }
  }

  // This function gets called during the boot-up process for the slave. The
  // 'res' parameter is the function that MUST be invoked when the configuration
  // is complete. Because this function runs as a task inside a coroutine, it
  // can suspend itself and wait for an asynchronous function, such as an SDO
  // request, to complete.
  void OnConfig(std::function<void(std::error_code ec)> res) noexcept override {
    try {
      // Perform a few SDO write requests to configure the slave. The
      // AsyncWrite() function returns a future which becomes ready once the
      // request completes, and the Wait() function suspends the coroutine for
      // this task until the future is ready.

      // Configure the slave to monitor the heartbeat of the master (node-ID 1)
      // with a timeout of 2000 ms.
      Wait(AsyncWrite<uint32_t>(0x1016, 1, (1 << 16) | 2000));
      // Configure the slave to produce a heartbeat every 1000 ms.
      Wait(AsyncWrite<uint16_t>(0x1017, 0, 1000));
      // Configure the heartbeat consumer on the master.
      ConfigHeartbeat(2000ms);

      // Reset object 4000:00 and 4001:00 on the slave to 0.
      Wait(AsyncWrite<uint32_t>(0x4000, 0, 0));
      Wait(AsyncWrite<uint32_t>(0x4001, 0, 0));

      // Report success (empty error code).
      res({});
    } catch (lely::canopen::SdoError& e) {
      // If one of the SDO requests resulted in an error, abort the
      // configuration and report the error code.
      res(e.code());
    }
  }

  // This function is similar to OnConfg(), but it gets called by the
  // AsyncDeconfig() method of the master.
  void OnDeconfig(std::function<void(std::error_code ec)> res) noexcept override {
    try {
      // Disable the heartbeat consumer on the master.
      ConfigHeartbeat(0ms);
      // Disable the heartbeat producer on the slave.
      Wait(AsyncWrite<uint16_t>(0x1017, 0, 0));
      // Disable the heartbeat consumer on the slave.
      Wait(AsyncWrite<uint32_t>(0x1016, 1, 0));
      res({});
    } catch (lely::canopen::SdoError& e) {
      res(e.code());
    }
  }

  // This function gets called every time a value is written to the local object
  // dictionary of the master by an RPDO (or SDO, but that is unlikely for a
  // master), *and* the object has a known mapping to an object on the slave for
  // which this class is the driver. The 'idx' and 'subidx' parameters are the
  // object index and sub-index of the object on the slave, not the local object
  // dictionary of the master.
  void OnRpdoWrite(uint16_t idx, uint8_t subidx) noexcept override {
    if (idx == 0x4001 && subidx == 0) {
      // Obtain the value sent by PDO from object 4001:00 on the slave.
      uint32_t val = rpdo_mapped[0x4001][0];
      // Increment the value and store it to an object in the local object
      // dictionary that will be sent by TPDO to object 4000:00 on the slave.
      tpdo_mapped[0x4000][0] = ++val;

      MODM_LOG_INFO << "[Master] OnRpdoWrite() " << val << modm::endl;
    }
  }
};