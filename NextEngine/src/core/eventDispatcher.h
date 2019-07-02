#pragma once
#include <functional>
#include "vector.h"

template<class T>
struct EventDispatcher {
	vector<std::function<void(T)> > listeners;

	void listen(std::function<void(T)> func) {
		listeners.append(func);
	}

	void broadcast(T mesg) {
		for (auto func : listeners) {
			func(mesg);
		}
	}
};

