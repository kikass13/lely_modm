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

#include <lely/coapp/master.hpp>
#include <lely/io2/user/can.hpp>
#include <lely/io2/user/timer.hpp>
#include <lely/ev/loop.hpp>
#include <lely/io2/sys/io.hpp>
//#include <lely/io2/sys/clock.hpp>

/// kikass13:
/// include driver for slave which is used by the master to communicate with the slave
#include "SlaveDriver.hpp"
/// ... and also include generated (convertEds2Hpp) slave definition
/// instead of loading the .eds file dynamically at runtime
#include "eds/eds-master.hpp"

/// kikass13:
/// include the slave class, which contains teh behavior of the slave itself
#include "Slave.hpp"
/// ... and also include generated (convertEds2Hpp) slave definition
/// instead of loading the .eds file dynamically at runtime
#include "eds/eds-slave.hpp"



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



int main()
{
	Board::initialize();
	Leds::setOutput();

	uint32_t counter(0);

	/// #################
	/// ###### PRE
	/// kikass13:
	/// Initialize gurads, context and loop for flow control and event handling
	/// The event loop and executor can be utilized multiple times 

	// Initialize the I/O library. This is required on Windows, but a no-op on
	// Linux (for now).
	lely::io::IoGuard io_guard;
	// Create an I/O context to synchronize I/O services during shutdown.
	lely::io::Context ctx;
	// Create a non-polling event loop.
	lely::ev::Loop loop;
	auto exec = loop.get_executor();

	/// #################
	/// ###### TIMERS

	// Obtain the current time from the monotonic clock. This can be replaced by
	// any other type of clock.
	// auto now = lely::io::clock_monotonic.gettime();

	// Create a user-defined channel for exclusive use by the slave. We register
	// an on_next() callback to demonstrate the syntax, but the callback doesn't
	// actually (need to) do anything.
	lely::io::UserTimer stimer(ctx, exec, &on_next, nullptr);
	// // Create a user-defined channel for exclusive use by the master.
	lely::io::UserTimer mtimer(ctx, exec, &on_next, nullptr);
	// // Initialize the internal clock of the timer.
	// stimer.get_clock().settime(now);
	// mtimer.get_clock().settime(now);

	// /// #################
	// /// ###### CHANNELS
	// /// kikass13:
	// /// create two channels which will refer to each other respectively by utilizing the same on_write() function
	// /// each channel will call that function if something has to be written, and write it into the receive buffer
	// /// of the other channel (master -> slave and vice versa)
	// /// this can of course be utilized to define or use our own can send / receive buffering handling on top
	// /// #################

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
	/// kikass13:
  	/// create Slave object using static generated eds device destription object 
  	lely::canopen::AsyncMaster master(mtimer, mchan, &MyMaster, 1);

	// // Master:Create a driver for the slave with node-ID 2.
	MyDriver driver(exec, master, 2);

	
	// Create a CANopen slave with node-ID 2.
	/// create Slave object using static generated eds device destription object 
  	MySlave slave(stimer, schan, &MySlave1, 2);
	// /// #################
	// /// ###### START

	// // Start the NMT services of the slave and master by pretending to receive a 'reset node' command
	// slave.Reset();
	// master.Reset();

  	// // Busy loop without polling.
	while (true)
	{
		// 	// Update the internal clocks of the timers.
		// 	auto now = io::clock_monotonic.gettime();
		// 	stimer.get_clock().settime(now);
		// 	mtimer.get_clock().settime(now);

		// 	// Execute all pending tasks without waiting.
		loop.poll();

		MODM_LOG_INFO << "loop: " << counter++ << modm::endl;
	}

	return 0;
}
