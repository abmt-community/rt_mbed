/**
 * Author: Hendrik van Arragon, 2022
 * SPDX-License-Identifier: MIT
 */


#include "main.h"
#include "mbed.h"

#include <abmt/os.h>
#include <abmt/io/buffer.h>
#include <abmt/rt/model.h>

#include "src/model_adapter_std.h"
#include "src/com_device.h"

model_adatper_std adapter;

void abmt::log(std::string s){
	adapter.log(s);
}
void abmt::die(std::string s){
	abmt::log("Fatal Error:");
	abmt::log(s);
	abmt::log("Reset!");
	ThisThread::sleep_for(std::chrono::milliseconds(5));
	NVIC_SystemReset();
}

void abmt::die_if(bool condition, std::string msg){
	if(condition){
		abmt::die(msg);
	}
}

auto start_time = Kernel::Clock::now();
abmt::time abmt::time::now(){
	return {std::chrono::duration_cast<std::chrono::nanoseconds>(Kernel::Clock::now() - start_time).count()};
}

uint64_t now_ms(){
	return std::chrono::duration_cast<std::chrono::milliseconds>(Kernel::Clock::now() - start_time).count();
}

struct mbed_task{
	Thread thread;
	abmt::rt::raster* raster;
	uint32_t raster_index;
	uint32_t interval_ms;
	uint64_t start_ms;

	mbed_task(abmt::rt::raster* r, uint32_t r_idx):thread(osPriorityNormal,RASTER_STACK_SIZE, nullptr, r->name), raster(r), raster_index(r_idx){
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
		new mbed_task(mdl_ptr->rasters[raster_index], raster_index);
	}

	bool buffer_error_send = false;

	auto last_online_send = now_ms();
	osThreadSetPriority(osThreadGetId(), osPriorityLow);

	mbed_file_handle(STDIN_FILENO)->enable_input(false);

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
				abmt::log("Error: Data in outbuffer can't be send in 0.1s.");
				abmt::log("Datatransmission stopped. Reduce viewed signals...");
			}
		}
    }
}
