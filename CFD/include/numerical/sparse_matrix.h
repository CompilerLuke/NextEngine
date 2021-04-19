#pragma once

template<typename T>
class SparseMatrix {
	uint n;
	uint m;

	SparseMatrix();
	SparseMatrix operator*(const SparseMatrix&);
};

void solve_system();