#pragma once

#include <tuple>

template<typename ... T>
struct ref_tuple {};

template<typename T>
struct ref_tuple<T> {
	T& value;

	ref_tuple(T& value) : value(value) {}

	template<typename... Args>
	ref_tuple<T, Args...> operator+(ref_tuple<Args...> next) {
		return { value, next };
	}

	template<std::size_t N>
	T& get() { return value; }
};

template<typename T, typename... Args>
struct ref_tuple<T, Args...> {
	T& value;
	ref_tuple<Args...> next;

	ref_tuple(T& value, ref_tuple<Args...> next) : value(value), next(next) {};

	template<std::size_t N>
	auto& get() {
		if constexpr (N == 0) return value;
		else return next.template get<N - 1>();
	}
};

namespace std {
	template<typename... Args>
	struct tuple_size<ref_tuple<Args...>>
		: integral_constant<size_t, sizeof...(Args)> {};

	template<size_t N, typename... Args>
	struct tuple_element<N, ref_tuple<Args...>> {
		using type = decltype(declval<ref_tuple<Args...>>().template get<N>());
	};
}
