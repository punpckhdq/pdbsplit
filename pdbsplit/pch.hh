#define _CRT_SECURE_NO_WARNINGS
#define MAX_PATH 260

#include <cstdlib>
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cassert>

#include <vector>
#include <unordered_map>

#include <PDB.h>
#include <PDB_RawFile.h>
#include <PDB_DBIStream.h>

#include <elfio/elfio.hpp>

#include <Zydis/Zydis.h>

#define XXH_INLINE_ALL
#include "xxhash.h"

#include "memory/memory_mapped_file.hh"
#include "memory/static_string.hh"
#include "exe/exe_file.hh"
#include "xbe/xbe_file.hh"
#include "pdb/pdb_file.hh"
#include "split/objects.hh"
#include "chunks/chunk.hh"
#include "split/split_project.hh"
#include "split/split_writer.hh"

template<typename T>
using binary_search_function_t = uint32_t(T*, uint32_t);

template<typename T>
static inline void binary_search(
	T* data,
	uint32_t count,
	binary_search_function_t<T> get_key_function,
	uint32_t search_key,
	T* result_out)
{
	uint32_t search_index = 0;
	uint32_t search_end = count - 1;

	while (search_index <= search_end)
	{
		uint32_t search_middle = (search_index + search_end) / 2;
		uint32_t key = get_key_function(data, search_middle);
		if (key < search_key)
		{
			assert(search_middle != UINT32_MAX);
			search_index = search_middle + 1;
		}
		else if (key > search_key)
		{
			assert(search_middle != 0);
			search_end = search_middle - 1;
		}
		else /*(key == search_key)*/
		{
			result_out = data + search_middle;
			break;
		}
	}
}
