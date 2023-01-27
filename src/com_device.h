#ifndef COM_DEVICE_H_
#define COM_DEVICE_H_

#include <cstdint>

struct com_device{
	virtual bool ready() = 0;
	virtual void rcv(void* buff, uint32_t buff_size, uint32_t* bytes_received) = 0;
	virtual void snd(void* buff, uint32_t len, uint32_t* bytes_send) = 0;
};

com_device* get_com_device();

#endif
