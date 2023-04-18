/**
 * Author: Hendrik van Arragon, 2022
 * SPDX-License-Identifier: MIT
 */


#include "main.h"
#include "bare_metall_loop.h"
#include "mbed.h"

#include <abmt/os.h>
#include <abmt/io/buffer.h>
#include <abmt/rt/model.h>

#include "src/model_adapter_std.h"
#include "src/com_device.h"

model_adatper_std adapter;

void abmt::os::log(std::string s){
	adapter.log(s);
}
void abmt::os::log_err(std::string s){
	adapter.log_err(s);
}
void abmt::os::die(std::string s){
	abmt::os::log_err("Fatal Error:");
	abmt::os::log_err(s);
	abmt::os::log_err("Reset!");
	ThisThread::sleep_for(std::chrono::milliseconds(5));
	NVIC_SystemReset();
}

void abmt::os::die_if(bool condition, std::string msg){
	if(condition){
		abmt::os::die(msg);
	}
}

void bare_metall_trigger_main_loop(){
	// Noting to do here.
	// Single threaded runtimes can use this function
	// to trigger their main loop. Interrupts can call
	// this function to signal that new async rasters 
	// are ready.
}


auto start_time = Kernel::Clock::now();
abmt::time abmt::time::now(){
	return {std::chrono::duration_cast<std::chrono::nanoseconds>(Kernel::Clock::now() - start_time).count()};
}

uint64_t now_ms(){
	return std::chrono::duration_cast<std::chrono::milliseconds>(Kernel::Clock::now() - start_time).count();
}

struct mbed_raster{
	Thread thread;
	abmt::rt::raster* raster;
	uint32_t raster_index;
	uint32_t interval_ms;
	uint64_t start_ms;

	mbed_raster(abmt::rt::raster* r, uint32_t r_idx):thread(osPriorityNormal,RASTER_STACK_SIZE, nullptr, r->name), raster(r), raster_index(r_idx){
		interval_ms = raster->interval.ms();
		start_ms = now_ms();
		thread.start([this]{
			run();
		});
	}

	void run(){
		raster->init();
		if(raster->is_sync == false){
			while(true){
				if(raster->poll() == false){
					ThisThread::yield();
				}else{
					break;
				}
			}
		}
		raster->init_tick();
		adapter.send_daq(raster_index);
		raster->n_ticks++;
		while(true){
			if(raster->is_sync){
				int32_t sleep_time = (start_ms + raster->n_ticks*interval_ms) - now_ms();
				if(sleep_time >= 0){
					ThisThread::sleep_for(std::chrono::milliseconds(sleep_time));
				}else{
					raster->n_ticks = (now_ms()-start_ms)/interval_ms;
				}
			}else{
				if(raster->poll() == false){
					ThisThread::yield();
					continue;
				}
			}
			raster->tick();
			adapter.send_daq(raster_index);
			raster->n_ticks++;
		}

	}

};

abmt::io::buffer in(128);
abmt::io::buffer out(280);
uint32_t bytes_read = 0;
uint32_t bytes_send = 0;

int main() 
{  
	ThisThread::sleep_for(std::chrono::milliseconds(5));
	com_device* com = get_com_device();

	auto mdl_ptr = abmt::rt::get_model();

	// feel free to change this, when you have large signalnames or huge object definitions
	adapter.max_def_size = 265;
	adapter.set_model(mdl_ptr);
	adapter.connection.set_source(&in);
	adapter.connection.set_sink(&out);

	for(size_t raster_index = 0; raster_index < mdl_ptr->rasters.length; raster_index++){
		new mbed_raster(mdl_ptr->rasters[raster_index], raster_index);
	}

	bool buffer_error_send = false;

	auto last_online_send = now_ms();
	osThreadSetPriority(osThreadGetId(), osPriorityLow);

	bool nothing_done = false;
	while(1) {
		if(nothing_done){
			if(adapter.connected){
				ThisThread::sleep_for(std::chrono::milliseconds(1));
			}else{
				ThisThread::sleep_for(std::chrono::seconds(1));
			}
		}else{
			ThisThread::yield();
		}
		nothing_done = true;

		if(com->ready() == false){
			out.flush();
			continue;
		}

		com->rcv(in.data + in.bytes_used, in.size - in.bytes_used, &bytes_read);
		if(bytes_read > 0){
			in.bytes_used += bytes_read;
			in.send();
			nothing_done = false;
		}

		if( ( now_ms() - last_online_send) > 1000 ){
			adapter.send_model_online();
			last_online_send = now_ms();
			buffer_error_send = false;
		}


		if(out.bytes_used > 0){
			adapter.send_mtx.lock();
			com->snd(out.data,out.bytes_used,&bytes_send);
			out.pop_front(bytes_send);
			nothing_done = false;
			adapter.send_mtx.unlock();
		}


		if(out.bytes_used > SERIAL_BAUDRATE/8/10){
			if(buffer_error_send == false){
				buffer_error_send = true;
				out.flush();
				adapter.clear_daq_lists();
				abmt::os::log_err("Error: Data in outbuffer can't be send in 0.1s.");
				abmt::os::log_err("Datatransmission stopped. Reduce viewed signals...");
			}
		}
    }
}
