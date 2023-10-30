// Copyright 2011-2022, Molecular Matters GmbH <office@molecular-matters.com>
// See LICENSE.txt for licensing details (2-clause BSD License: https://opensource.org/licenses/BSD-2-Clause)

#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define INVALID_HANDLE_VALUE ((long)-1)
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

s_memory_mapped_file_handle open_memmapped_file(const char* path)
{
#ifdef _WIN32
	void* file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return s_memory_mapped_file_handle{ INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, nullptr };
	}

	void* fileMapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);

	if (fileMapping == nullptr)
	{
		CloseHandle(file);

		return s_memory_mapped_file_handle{ INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, nullptr };
	}

	void* baseAddress = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);

	if (baseAddress == nullptr)
	{
		CloseHandle(fileMapping);
		CloseHandle(file);

		return s_memory_mapped_file_handle{ INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, nullptr };
	}

	return s_memory_mapped_file_handle{ file, fileMapping, baseAddress };
#else
	struct stat fileSb;

	int file = open(path, O_RDONLY);

	if (file == INVALID_HANDLE_VALUE)
	{
		return s_memory_mapped_file_handle{ INVALID_HANDLE_VALUE, nullptr, 0 };
	}

	if (fstat(file, &fileSb) == -1)
	{
		close(file);

		return s_memory_mapped_file_handle{ INVALID_HANDLE_VALUE, nullptr, 0 };
	}

	void* baseAddress = mmap(nullptr, fileSb.st_size, PROT_READ, MAP_PRIVATE, file, 0);

	if (baseAddress == MAP_FAILED)
	{
		close(file);

		return s_memory_mapped_file_handle{ INVALID_HANDLE_VALUE, nullptr, 0 };
	}

	return s_memory_mapped_file_handle{ file, baseAddress, fileSb.st_size };
#endif
}

void close_memmapped_file(s_memory_mapped_file_handle& handle)
{
#ifdef _WIN32
	UnmapViewOfFile(handle.base_address);
	CloseHandle(handle.file_mapping);
	CloseHandle(handle.file);

	handle.file = nullptr;
	handle.file_mapping = nullptr;
#else
	munmap(handle.base_address, handle.len);
	close(handle.file);

	handle.file = 0;
#endif

	handle.base_address = nullptr;
}

bool s_memory_mapped_file::valid()
{
	return handle.base_address != nullptr;
}

void* s_memory_mapped_file::get_data()
{
	assert(valid());
	return handle.base_address;
}

s_memory_mapped_file::s_memory_mapped_file(const char* path)
{
	handle = open_memmapped_file(path);
}

s_memory_mapped_file::~s_memory_mapped_file()
{
	close_memmapped_file(handle);
}
