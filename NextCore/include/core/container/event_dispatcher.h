#pragma once
#pragma

#include <functional>
#include "core/container/vector.h"

struct callback_handle {
    uint id = 0;
};

template<typename T>
struct Listener {
    callback_handle handle;
    std::function<void(const T&)> function;
    
    void operator()(const T& mesg) {
        function(mesg);
    }
};

template<class T>
struct EventDispatcher {
    uint handle_counter = 0;
	vector<Listener<T>> listeners;

	callback_handle listen(std::function<void(const T&)>&& func) {
        callback_handle handle{++handle_counter};
        listeners.append({handle, std::move(func)});
        
        return handle;
	}

	void remove(callback_handle handle) {
		int i = 0;
		for (; i < listeners.length; i++) {
			if (listeners[i].handle.id == handle.id) break;
		}

		listeners.data[i] = listeners.pop();
	}

	void broadcast(const T& mesg) {
		for (auto& func : listeners) {
			func(mesg);
		}
	}


};

