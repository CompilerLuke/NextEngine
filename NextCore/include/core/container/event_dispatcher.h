#pragma once
#pragma

#include <functional>
#include "core/container/vector.h"

template<class T>
struct EventDispatcher {
	vector<std::function<void(T)> > listeners;

	void listen(std::function<void(T)> func) {
		listeners.append(func);
	}

	void remove(std::function<void(T)> func) {
		int i = 0;
		for (; i < listeners.length; i++) {
			if (listeners[i] == func) break;
		}

		listeners[i] = listeners.pop();
	}

	void broadcast(T mesg) {
		for (auto func : listeners) {
			func(mesg);
		}
	}


};

