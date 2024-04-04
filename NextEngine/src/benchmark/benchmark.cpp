#include "core/memory/allocator.h"
#include <iostream>
#include <chrono>
#include "core/container/vector.h"
#include <vector>
#include <string>

int bench() {
	std::cout << ("APPENDING\n") << std::endl;

	auto bench_max = 10;

	for (int i = 0; i < 6; i++) {
		std::cout << "NUM ITERATIONS " << std::to_string(bench_max) << std::endl;

		{
			auto start = std::chrono::high_resolution_clock::now();
			vector<int> arr;

			for (int i = 0; i < bench_max; i++) {
				arr.append(i);
			}
			auto end = std::chrono::high_resolution_clock::now();

			std::chrono::duration<double, std::milli> diff = end - start;

			std::cout << "VECTOR took " << std::to_string(diff.count()) << "ms" << std::endl;
		}

		{
			auto start = std::chrono::high_resolution_clock::now();
			std::vector<int> arr;

			for (int i = 0; i < bench_max; i++) {
				arr.push_back(i);
			}
			auto end = std::chrono::high_resolution_clock::now();

			std::chrono::duration<double, std::milli> diff = end - start;

			std::cout << "std::VECTOR took " << std::to_string(diff.count()) << "ms" << std::endl;
		}

		bench_max *= 10;
	}


	return 0;
}