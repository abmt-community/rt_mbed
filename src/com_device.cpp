/**
 * Author: Hendrik van Arragon, 2022
 * SPDX-License-Identifier: MIT
 */
#include "com_device.h"
#include "mbed.h"
#include "../main.h"

#if defined DEVICE_USBDEVICE && COM_USE_USB

#include "USBCDC.h"

class USBCDC_ABMT: public USBCDC, public com_device{
public:
	USBCDC_ABMT():USBCDC(false){
		// when waiting in USBCDC constructor the overloaded
		// virtual functions are not called.
		connect();
		//wait_ready();
	}
protected:
	virtual const uint8_t* string_iproduct_desc(){
		static const uint8_t string_iproduct_descriptor[] = {
			0x0c,                                                       //bLength
			STRING_DESCRIPTOR,                                          //bDescriptorType 0x03
			'A', 0, 'B', 0, 'M', 0, 'T', 0, USB_DEVICE_NUM, 0           //bString iProduct - USB DEVICE
		};
		return string_iproduct_descriptor;
	};

	virtual const uint8_t* string_imanufacturer_desc(){
	    static const uint8_t string_imanufacturer_descriptor[] = {
	        0x10,                                                    //bLength
	        STRING_DESCRIPTOR,                                       //bDescriptorType 0x03
	        'a', 0, 'b', 0, 'm', 0, 't', 0, ' ', 0, 'v', 0, '1', 0   //bString iManufacturer - mbed.org
	    };
	    return string_imanufacturer_descriptor;
	}

	virtual bool ready(){
		return USBCDC::ready();
	}
	
	virtual void rcv(void* buff, uint32_t buff_size, uint32_t* bytes_received){
		receive_nb((uint8_t*)buff,buff_size,bytes_received);
	}
	
	virtual void snd(void* buff, uint32_t len, uint32_t* bytes_send){
		send_nb((uint8_t*)buff,len,bytes_send);
	}

};

com_device* get_com_device(){
	static USBCDC_ABMT cdc; // constructor called at first call. Important for USB.
	return &cdc;
}

#elif not defined DISABLE_ABMT_SERIAL_COM

struct uart_com: public BufferedSerial, public com_device{
	uart_com():BufferedSerial(SERIAL_PIN_TX, SERIAL_PIN_RX, SERIAL_BAUDRATE){
		set_blocking(false);
	}
	
	virtual bool ready(){
		return true;
	}
	
	virtual void rcv(void* buff, uint32_t buff_size, uint32_t* bytes_received){
		auto ret = read((uint8_t*)buff,buff_size);
		*bytes_received = 0;
		if(ret > 0){
			*bytes_received = ret;
		}
		
	}
	
	virtual void snd(void* buff, uint32_t len, uint32_t* bytes_send){
		auto ret = write((uint8_t*)buff,len);
		*bytes_send = 0;
		if(ret > 0){
			*bytes_send = ret;
		}
	}
};

com_device* get_com_device(){
	static uart_com device; // constructor called at first call. Important for USB.
	return &device;
}

#else

struct dummy_com: public com_device{
	dummy_com(){

	}

	virtual bool ready(){
		return false;
	}

	virtual void rcv(void* buff, uint32_t buff_size, uint32_t* bytes_received){
		*bytes_received = 0;
	}

	virtual void snd(void* buff, uint32_t len, uint32_t* bytes_send){
		*bytes_send = len;
	}
};

com_device* get_com_device(){
	static dummy_com device; // constructor called at first call. Important for USB.
	return &device;
}

#endif
