/**
 * Author: Hendrik van Arragon, 2022
 * SPDX-License-Identifier: MIT
 */
#include <iostream>
#include <filesystem>
#include <abmt/io/eio.h>
#include <abmt/util/bt_exception.h>
#include <unistd.h>

using namespace abmt;
using namespace abmt::io;
using namespace std;

void abmt::log(std::string s){
	cout << s << endl;
}
void abmt::log_err(std::string s){
	cout << s << endl;
}
void abmt::die(std::string s){
	throw abmt::util::bt_exception(s);
}

void abmt::die_if(bool condition, std::string msg){
	if(condition){
		abmt::die(msg);
	}
}

int main(int argc, char* argv[]){
	bool quit = false;
	if(argc != 3){
		cout << "usage:uart_to_abmt <serial_port> <baudrate>" << endl;
		return -1;
	}
	string port = argv[1];
	int baud_rate = stol(argv[2]);
	usleep(150*1000); // controller reset delay
	for(int i = 0; i < 60; i++){
		if(std::filesystem::exists(port)){
			break;
		}
		usleep(100*1000);
	}

	try{
		usleep(150*1000); // premissions might not been setup
		cout << "Open serial port "<< port <<  " with baud rate: " << baud_rate << "..." << endl;
		abmt::io::event_list e;
		abmt::io::serial_ptr uart_con(new abmt::io::serial(e,port,baud_rate, true));

		usleep(250*1000);
		uint16_t port = 15101;
		const char* port_str = std::getenv("ABMT_MODEL_PORT");
		if(port_str != nullptr){
			port = std::stoul(port_str);
		}
		abmt::io::tcp_ptr abmt_con = abmt::io::tcp::connect_to(e,{"127.0.0.1",port});
		abmt_con->on_new_data = [&](abmt::blob& b)->size_t{
			uart_con->send(b.data, b.size);
			return b.size;
		};

		abmt_con->on_close = [&]{
			cout << "Abmt closed connection..." << endl;
			quit = true;
		};

		uart_con->on_new_data = [&](abmt::blob& b)->size_t{
			abmt_con->send(b);
			return b.size;
		};

		while(quit == false){
				e.wait(100);
			}
	}catch(abmt::util::bt_exception& e){
		cout << "An exception occured: " << e.what() << endl;
		//e.print_backtrace();
	}catch(std::exception& e){
		cout << "An exception occured: " << e.what() << endl;
	}catch(const char* c ){
		cout << "An exception occured: " << c << endl;
	}
	catch(...){
		cout << "An unknown exception occured... " << endl;
	}
}
