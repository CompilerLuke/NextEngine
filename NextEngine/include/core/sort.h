#pragma once
#include "allocator.h"
#include <iostream>
#include "temporary.h"

//ripped from https://www.geeksforgeeks.org/radix-sort/

template<typename T, typename F>
long long getMax(T arr[], int n, F func)
{
	long long mx = func(arr[0]);
	for (int i = 1; i < n; i++)
		if (func(arr[i]) > mx)
			mx = func(arr[i]);
	return mx;
}

template<typename T, typename F>
void countSort(T arr[], int n, long long exp, F func) //todo allow different allocators
{
	T* output = (T*)temporary_allocator.allocate(sizeof(T) * n);

	int i, count[10] = { 0 };

	// Store count of occurrences in count[] 
	for (i = 0; i < n; i++)
		count[(func(arr[i]) / exp) % 10]++;

	// Change count[i] so that count[i] now contains actual 
	//  position of this digit in output[] 
	for (i = 1; i < 10; i++)
		count[i] += count[i - 1];

	// Build the output array 
	for (i = n - 1; i >= 0; i--)
	{
		auto key = func(arr[i]);
		output[count[(key / exp) % 10] - 1] = arr[i]; 
		count[(key / exp) % 10]--;
	}

	// Copy the output array to arr[], so that arr[] now 
	// contains sorted numbers according to current digit 
	for (i = 0; i < n; i++) {
		arr[i] = output[i];
	}
}

// The main function to that sorts arr[] of size n using  
// Radix Sort 
template<typename T, typename F>
void radixsort(T arr[], int n, F f)
{
	// Find the maximum number to know number of digits 
	long long m = getMax(arr, n, f);

	// Do counting sort for every digit. Note that instead 
	// of passing digit number, exp is passed. exp is 10^i 
	// where i is current digit number 

	for (long long exp = 1; m / exp > 0; exp *= 10)
		countSort(arr, n, exp, f);
}