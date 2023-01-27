#include <abmt/mutex.h>
#include "mbed.h"

using namespace abmt;

mutex::mutex(){
	mtx = new Mutex;
}
void mutex::lock(){
	 ((Mutex*)mtx)->lock();
}
void mutex::unlock(){
	((Mutex*)mtx)->unlock();
}

mutex::~mutex(){
	delete (Mutex*)mtx;
}

scope_lock mutex::get_scope_lock(){
	return scope_lock(*this);
}

scope_lock::scope_lock(mutex& m):m(m){
	m.lock();
}
scope_lock::~scope_lock(){
	m.unlock();
}
