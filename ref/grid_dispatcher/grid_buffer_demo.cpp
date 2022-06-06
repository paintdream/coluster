#include "grid_buffer.h"
#include <vector>
using namespace grid;

int main(void) {
	bytes_t bytes;
	buffer_t<uint8_t> buffer;
	cache_t<uint8_t> cache;
	cache_allocator_t<double, uint8_t> allocator(&cache);

	char var[] = "12345";
	bytes = bytes_t::make_view((const uint8_t*)var, 5);
	bytes.test(15);
	bytes.set(16); // breaks const rule ...
	buffer = bytes_t::make_view((const uint8_t*)"1234568901234567890", 20);
	cache.link(bytes, buffer);
	bytes_t combined;
	combined.resize(bytes.get_view_size());
	combined.copy(0, bytes);

	// todo: more tests
	std::vector<double, cache_allocator_t<double, uint8_t>> vec(allocator);
	vec.push_back(1234.0f);
	vec.resize(777);

	std::vector<double> dbl_vec;
	grid::binary_insert(dbl_vec, 1234.0f);
	auto it = grid::binary_find(dbl_vec.begin(), dbl_vec.end(), 1234.0f);
	assert(it != dbl_vec.end());
	grid::binary_erase(dbl_vec, 1234.0f);

	std::vector<grid::key_value_t<int, const char*>> str_vec;
	grid::binary_insert(str_vec, grid::make_key_value(1234, "asdf"));
	grid::binary_insert(str_vec, grid::make_key_value(2345, "defa"));
	auto it2 = grid::binary_find(str_vec.begin(), str_vec.end(), 1234);
	assert(it2 != str_vec.end());
	assert(grid::binary_find(str_vec.begin(), str_vec.end(), 1236) == str_vec.end());
	grid::binary_erase(str_vec, 1234);
	grid::binary_erase(str_vec, grid::make_key_value(1234, ""));

	return 0;
}

