/**
 * Author: Hendrik van Arragon, 2022
 * SPDX-License-Identifier: MIT
 */

#include "model_adapter_std.h"

#include <abmt/version.h>
#include <abmt/os.h>
#include <abmt/blob.h>

using namespace std;

void model_adatper_std::set_model(abmt::rt::model* m){
	abmt::rt::model_adatper::set_model(m);
	if(daq_lists != 0){
		delete[] daq_lists;
	}
	daq_lists = new daq_list_t[mdl->rasters.length];
}


void model_adatper_std::on_hello(uint16_t version){
	if(version != ABMT_VERSION){
		on_version_error();
	}
	connected = true;
	def_idx = 0;
};


void model_adatper_std::on_request_signal_def(){
	size_t siglal_def_size = 0;
	size_t signals_to_send = 0;
	size_t bytes_to_send = 0;
	for(size_t i = def_idx; i < mdl->signals.length; i++){
		auto s = mdl->signals[i];
		auto sz = s.type.get_def_size();
		siglal_def_size += sz;
		auto name_len = strlen(s.name);
		if(name_len > 255){
			name_len = 255;
		}
		siglal_def_size += name_len + 1; // 1 for len_byte
		if(siglal_def_size > max_def_size){
			break;
		}
		bytes_to_send = siglal_def_size;
		signals_to_send++;
	}

	auto blk = abmt::blob_shared(bytes_to_send);
	char* ptr = (char*)blk.data;
	size_t pos = 0;
	for(size_t i = def_idx; i <  def_idx + signals_to_send; i++){
		auto s = mdl->signals[i];
		auto name_len = strlen(s.name);
		if(name_len > 255){
			name_len = 255;
		}
		*(uint8_t*)ptr = (uint8_t)name_len;
		++ptr; ++pos;
		memcpy(ptr,s.name,name_len);
		pos += name_len;
		ptr += name_len;
		size_t def_size = s.type.get_def(ptr,blk.size - pos);
		pos += def_size;
		ptr += def_size;
	}
	def_idx += signals_to_send;
	send(abmt::rt::cmd::ack_signal_def,blk);
	if(bytes_to_send == 0){
		def_idx = 0; // reset counter
	}
};

void model_adatper_std::on_request_parameter_def(){
	size_t param_def_size = 0;
	size_t params_to_send = 0;
	size_t bytes_to_send = 0;
	//for(auto p: mdl->parameters){
	for(size_t i = def_idx; i < mdl->parameters.length; i++){
		auto p = mdl->parameters[i];
		auto sz = p.parameter->get_type().get_def_size();
		param_def_size += sz;
		auto name_len = strlen(p.name);
		if(name_len > 255){
			name_len = 255;
		}
		param_def_size += name_len + 1; // 1 for len_byte
		if(param_def_size > max_def_size){
			break;
		}
		bytes_to_send = param_def_size;
		params_to_send++;
	}

	auto blk = abmt::blob_shared(bytes_to_send);
	char* ptr = (char*)blk.data;
	size_t pos = 0;
	for(size_t i = def_idx; i < def_idx + params_to_send ; i++){
		auto p = mdl->parameters[i];
		auto name_len = strlen(p.name);
		if(name_len > 255){
			name_len = 255;
		}
		*(uint8_t*)ptr = (uint8_t)name_len;
		++ptr; ++pos;
		memcpy(ptr,p.name,name_len);
		pos += name_len;
		ptr += name_len;
		size_t def_size = p.parameter->get_type().get_def(ptr,blk.size - pos);
		pos += def_size;
		ptr += def_size;
	}
	def_idx += params_to_send;
	send(abmt::rt::cmd::ack_parameter_def,blk);
	if(bytes_to_send == 0){
		def_idx = 0; // reset counter
	}
};

void model_adatper_std::on_set_daq_list(abmt::blob& lst){
	send_mtx.lock();

	for(size_t i = 0; i < mdl->rasters.length; ++i){
		daq_lists[i].clear();
	}

	auto num_elements = lst.size/sizeof(uint32_t);
	for(uint32_t i = 0; i < num_elements; ++i){
		auto idx = lst.get<uint32_t>(i*sizeof(uint32_t));
		if(idx < mdl->signals.length){
			auto& signal = mdl->signals[idx];
			daq_lists[signal.raster_index].push_back({idx,i});
		}
	}

	send_mtx.unlock();

	send(abmt::rt::cmd::ack_set_daq_list);
};

void model_adatper_std::on_set_paq_list(abmt::blob& lst){
	paq_list.clear();

	auto num_elements = lst.size/sizeof(uint32_t);
	for(uint32_t i = 0; i < num_elements; ++i){
		auto idx = lst.get<uint32_t>(i*sizeof(uint32_t));
		if(idx < mdl->parameters.length){
			paq_list.push_back(idx);
		}
	}
	send(abmt::rt::cmd::ack_set_paq_list);

	if(paq_list.size() > 0){
		send_all_parameters();
	}
};

void model_adatper_std::on_set_parameter(abmt::blob& data){
	if(data.size < 8){
		return;
	}
	auto idx = data.get<uint32_t>(0);
	//auto len = data.as<uint32_t>(4);
	if(mdl->parameters.length > idx){
		auto param =  mdl->parameters[idx].parameter;

		param->lock();
		// write data to parameter
		auto exp =param->get_type();
		abmt::blob data_blk = data.sub_blob(8);
		exp.deserialize(data_blk.data,data_blk.size);

		// send parameter
		uint32_t size = exp.get_size();
		auto blk = abmt::blob_shared(size + sizeof(abmt::rt::paq_data_hdr));

		abmt::rt::paq_data_hdr hdr;
		hdr.size = size;
		auto data_area = blk.sub_blob(sizeof(hdr),size);
		exp.serialize(data_area.data,data_area.size);

		param->unlock();

		// find list_idx
		bool found = false;
		for(uint32_t i = 0; i < paq_list.size(); ++i){
			if(paq_list[i] == idx){
				hdr.list_idx = i;
				found = true;
				break;
			}
		}
		blk.set(hdr, 0);
		if(found){
			send(abmt::rt::cmd::paq_data,blk);
		}
		save_parameters();
	}else{
		abmt::os::log_err("on_set_parameter: index out of range!");
	}
};

void model_adatper_std::on_command(string cmd){
	if(cmd == "quit"){
		connected = false;
		clear_daq_lists();
		on_quit();
	}
}

void model_adatper_std::save_parameters(){
	on_save_parameters();
}

void model_adatper_std::clear_daq_lists(){
	send_mtx.lock();
	for(size_t i = 0; i < mdl->rasters.length; ++i){
		daq_lists[i].clear();
	}
	send_mtx.unlock();
}

void model_adatper_std::send_daq(size_t raster){
	auto now = abmt::time::now();
	auto& daq_list = daq_lists[raster];
	if(daq_list.size() == 0){
		return;
	}

	size_t size_to_send = 0;
	for(auto& d:daq_list){
		size_to_send += sizeof(abmt::rt::daq_data_hdr) + mdl->signals[d.signal_index].type.get_size();
	}

	auto blk = abmt::blob_shared(size_to_send);

	size_t mem_pos = 0;
	for(auto& d:daq_list){
		uint32_t signal_size =  mdl->signals[d.signal_index].type.get_size();

		abmt::rt::daq_data_hdr hdr = {d.daq_index, signal_size, now.ns_since_1970};
		blk.set(hdr, mem_pos);
		mem_pos += sizeof(hdr);

		auto data_area = blk.sub_blob(mem_pos,signal_size);
		mdl->signals[d.signal_index].type.serialize(data_area.data,data_area.size);
		mem_pos += signal_size;
	}

	send(abmt::rt::cmd::daq_data,blk);
}

void model_adatper_std::send_all_parameters(){
	size_t size_to_send = 0;
	for(auto& idx: paq_list){
		auto param = mdl->parameters[idx].parameter;
		param->lock();
		size_to_send += sizeof(abmt::rt::paq_data_hdr) + param->get_type().get_size();
		param->unlock();
	}


	auto blk = abmt::blob_shared(size_to_send);
	size_t mem_pos = 0;

	uint32_t entry_num = 0;
	for(auto& idx: paq_list){
		auto param = mdl->parameters[idx].parameter;
		param->lock();

		auto exp = mdl->parameters[idx].parameter->get_type();
		uint32_t size = exp.get_size();

		abmt::rt::paq_data_hdr hdr = {entry_num, size};
		blk.set(hdr, mem_pos);
		mem_pos += sizeof(hdr);

		auto data_area = blk.sub_blob(mem_pos,size);
		exp.serialize(data_area.data,data_area.size);
		mem_pos += size;

		param->unlock();
		entry_num++;
	}

	send(abmt::rt::cmd::paq_data,blk);
}

model_adatper_std::~model_adatper_std(){
	if(daq_lists != 0){
		delete[] daq_lists;
	}
}
