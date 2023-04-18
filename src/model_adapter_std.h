/**
 * Author: Hendrik van Arragon, 2022
 * SPDX-License-Identifier: MIT
 */

#ifndef MODEL_ADAPTER_STD_H_
#define MODEL_ADAPTER_STD_H_

#include <abmt/rt/model_adapter.h>
#include <vector>
#include <functional>
#include <cstdint>

struct daq_element{
	uint32_t signal_index;
	uint32_t daq_index;
};

using daq_list_t = std::vector<daq_element>;

class model_adatper_std: public abmt::rt::model_adatper{
public:

	daq_list_t* daq_lists = 0;
	std::vector<uint32_t>   paq_list;
	std::function<void()>   on_quit = []{};
	std::function<void()>   on_save_parameters = []{};
	std::function<void()>   on_version_error = []{};

	size_t max_def_size = 63*1024;
	size_t def_idx = 0;
	bool connected = false;

	void set_model(abmt::rt::model* m);

	virtual void on_hello(uint16_t version);

	virtual void on_request_signal_def();

	virtual void on_request_parameter_def();

	virtual void on_set_daq_list(abmt::blob& lst);

	virtual void on_set_paq_list(abmt::blob& lst);

	virtual void on_set_parameter(abmt::blob& data);

	virtual void on_command(std::string cmd);

	void save_parameters();
	void clear_daq_lists();

	virtual ~model_adatper_std();
	void send_daq(size_t raster);

	void send_all_parameters();

};




#endif /* MODEL_ADAPTER_LINUX_H_ */
