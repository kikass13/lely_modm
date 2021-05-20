/*
 * Copyright (c) 2016-2017, Niklas Hauser
 *
 * This file is part of the modm project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// ----------------------------------------------------------------------------
#include <modm/board.hpp>
using namespace Board;

/// kikass13:
#define LELY_NO_CO_OBJ_NAME 0
#define LELY_NO_CO_OBJ_DEFAULT 0
#define LELY_NO_CO_OBJ_UPLOAD 0
#define LELY_NO_CO_OBJ_LIMITS 0
/// include generated (convertEds2Hpp) slave definition
/// instead of loading the .eds file dynamically at runtime
#include "eds/eds-master.hpp"

/// kikass13:
/// include generated (convertEds2Hpp) slave definition
/// instead of loading the .eds file dynamically at runtime
#include "eds/eds-slave.hpp"


#include <lely/coapp/master.hpp>
#include <lely/io2/user/can.hpp>
#include <lely/io2/user/timer.hpp>
#include <lely/ev/loop.hpp>
#include <lely/io2/sys/io.hpp>
#include <lely/io2/sys/clock.hpp>

#include "timeWrapper.hpp"

/// kikass13:
/// include driver for slave which is used by the master to communicate with the slave
#include "SlaveDriver.hpp"
/// ... and also include generated (convertEds2Hpp) slave definition
/// instead of loading the .eds file dynamically at runtime
//#include "eds/eds-master.hpp"

/// kikass13:
/// include the slave class, which contains the behavior of the slave itself
#include "Slave.hpp"
/// ... and also include generated (convertEds2Hpp) slave definition
/// instead of loading the .eds file dynamically at runtime
//#include "eds/eds-slave.hpp"



// This function is invoked by a user-defined timer whenever the expiration time
// is updated. This information can be used to make the main loop more
// efficient, but nothing needs to be done to obtain a functioning tiomer.
void on_next(const timespec* tp, void* arg) {
  (void)tp;
  (void)arg;
}

// This function is invoked by a user-defined CAN channel whenever a CAN frame
// needs to be written. In this case we forward the frame to the other channel.
int on_write(const can_msg* msg, int timeout, void* arg) {
  // The user-defined argument is a pointer to a pointer to the other channel.
  auto& chan = **static_cast<lely::io::UserCanChannel**>(arg);
  // Post a task to process the CAN frame. This prevents nesting in the CAN
  // frame handlers. And since we're postponing the handling of the frame, we
  // need to create a copy.
  const can_msg msg_ = *msg;
  chan.get_executor().post([=, &chan]() {
    chan.on_read(&msg_, timeout);
  });
  return 0;
}

// Allocate giant array inside a NOLOAD heap section
// Play around with the array size and see effect it has on HeapTable!
const uint8_t *heap_begin{nullptr};
const uint8_t *heap_end{nullptr};
extern "C" void __modm_initialize_memory()
{
    bool success = HeapTable::find_largest(&heap_begin, &heap_end, modm::MemoryDefault);
	// MODM_LOG_DEBUG << "### heap_begin:" << (unsigned int) heap_begin << modm::endl;
	// MODM_LOG_DEBUG << "### heap_end:" << (unsigned int) heap_end << modm::endl;
	// MODM_LOG_DEBUG << "### Usable HeapSize: " << ( (unsigned long) *heap_end) - (unsigned long) *heap_begin) << modm::endl;
    modm_assert(success, "heap.init", "No default memory section found!");
}
const uint8_t *heap_top{heap_begin};
extern "C" void* _sbrk_r(struct _reent *,  ptrdiff_t size)
{
    const uint8_t *const heap = heap_top;
    heap_top += size;
    modm_assert(heap_top < heap_end, "heap.sbrk", "Heap overflowed!");
    return (void*) heap;
}

int main()
{
	Board::initialize();
	Leds::setOutput();

	/// #################
	/// ###### PRE
	/// kikass13:
	/// Initialize guards, context and loop for flow control and event handling
	/// The event loop and executor can be utilized multiple times 

	// Initialize the I/O library. This is required on Windows, but a no-op on Linux (for now).
	// lely::io::IoGuard io_guard;
	// Create an I/O context to synchronize I/O services during shutdown.

	// MODM_LOG_INFO << sizeof(io_ctx) << modm::endl;
	lely::io::Context ctx;

	// Create a non-polling event loop.
	lely::ev::Loop loop;
	auto exec = loop.get_executor();

	/// #################
	/// ###### TIMERS
	MODM_LOG_INFO << "TIMERS" << modm::endl;

	// Obtain the current time from the monotonic clock. This can be replaced by
	// any other type of clock.
	// auto now = lely::io::clock_monotonic.gettime();

	// Create a user-defined channel for exclusive use by the slave. We register
	// an on_next() callback to demonstrate the syntax, but the callback doesn't
	// actually (need to) do anything.
	lely::io::UserTimer stimer(ctx, exec, &on_next, nullptr);
	// // Create a user-defined channel for exclusive use by the master.
	lely::io::UserTimer mtimer(ctx, exec, &on_next, nullptr);

	// /// #################
	// /// ###### CHANNELS
	// /// kikass13:
	// /// create two channels which will refer to each other respectively by utilizing the same on_write() function
	// /// each channel will call that function if something has to be written, and write it into the receive buffer
	// /// of the other channel (master -> slave and vice versa)
	// /// this can of course be utilized to define or use our own can send / receive buffering handling on top
	// /// #################
	MODM_LOG_INFO << "CHANNELS" << modm::endl;

	// // The on_write() functions for the CAN channels of the master and slave need
	// // to refer to the other channel. This creates a chicken-and-egg problem. To
	// // solve it, we pass the callbacks a pointer to a pointer which will later be
	// // initialized with the address of the other channel.
	lely::io::UserCanChannel* tmpSchan{nullptr};
	lely::io::UserCanChannel* tmpMchan{nullptr};

	// // Create a user-defined CAN channel for exclusive use by the slave. The
	// // channel uses the default values for the flags, receive queue length and
	// // timeout. The on_write() callback will send the CAN frame to the channel of
	// // the master.
	lely::io::UserCanChannel schan(ctx, exec, lely::io::CanBusFlag::NONE, 0, 0, &on_write, &tmpMchan);
	// // Initialize a reference to the slave CAN channel for use by the master.
	tmpSchan = &schan;
	// // Create a user-defined CAN channel for exclusive use by the master.
	lely::io::UserCanChannel mchan(ctx, exec, lely::io::CanBusFlag::NONE, 0, 0, &on_write, &tmpSchan);
	// // Initialize a reference to the master CAN channel for use by the slave.
	tmpMchan = &mchan;

	// /// #################
	// /// ###### MASTER / SLAVE  
	// /// kikass13:
	// /// for Master:
	// ///   * create the master
	// ///   * create a driver for each slave to be handled
	// /// for slave:
	// ///   * create the slave(s)
	// // Create a CANopen master with node-ID 1. The master is asynchronous, which
	// // means every user-defined callback for a CANopen event will be posted as a
	// // task on the event loop, instead of being invoked during the event
	// // processing by the stack.
	MODM_LOG_INFO << "MASTER" << modm::endl;

	/// kikass13:
  	/// create Slave object using static generated eds device destription object 
  	co_dev_t* masterDeviceDescription = MyMaster_init();
  	lely::canopen::AsyncMaster master(mtimer, mchan, masterDeviceDescription, 1);

	MODM_LOG_INFO << " ... DRIVER" << modm::endl;

	// // Master:Create a driver for the slave with node-ID 2.
	MyDriver driver(exec, master, 2);

	MODM_LOG_INFO << "SLAVE" << modm::endl;
	// Create a CANopen slave with node-ID 2.
	/// create Slave object using static generated eds device destription object 
	co_dev_t* slaveDeviceDescription = MySlave1_init();
  	MySlave slave(stimer, schan, slaveDeviceDescription, 2);
	
	// /// #################
	// /// ###### START
	MODM_LOG_INFO << "START" << modm::endl;

	// // Start the NMT services of the slave and master by pretending to receive a 'reset node' command
	slave.Reset();
	master.Reset();
	
	MODM_LOG_INFO << "Lets goooo :)" << modm::endl;
  	// Busy loop without polling.
	while (true)
	{
		// Update the internal clocks of the timers.
		
		const auto now = modm::Clock::now();
		auto lelyNow = lely::io::Clock::time_point(now.time_since_epoch());
		stimer.get_clock().settime(lelyNow);
		mtimer.get_clock().settime(lelyNow);

		// 	// Execute all pending tasks without waiting.
		loop.poll();
	}
	return 0;
}
