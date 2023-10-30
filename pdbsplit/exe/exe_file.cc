#define WIN32_LEAN_AND_MEAN
#include <windows.h>

uint32_t c_exe_file::get_image_base() const
{
	PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(exe_data);
	PIMAGE_NT_HEADERS32 nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS32>(reinterpret_cast<uint8_t*>(exe_data) + dos_header->e_lfanew);

	return nt_headers->OptionalHeader.ImageBase;
}

const s_exe_section* c_exe_file::get_section_for_rva(uint32_t rva) const
{
	for (const auto& section : sections)
	{
		if (section.start_rva <= rva && section.start_rva + section.virtual_size > rva)
		{
			return &section;
		}
	}

	return nullptr;
}

const void* c_exe_file::get_data(uint32_t rva) const
{
	const s_exe_section* section = get_section_for_rva(rva);
	if (section != nullptr)
	{
		return reinterpret_cast<uint8_t*>(exe_data) + section->start_offset + (rva - section->start_rva);
	}

	return nullptr;
}

const void* c_exe_file::get_relocation_data() const
{
	for (const auto& section : sections)
	{
		if (!strcmp(".reloc", section.section_name.get_string()))
		{
			return reinterpret_cast<uint8_t*>(exe_data) + section.start_offset;
		}
	}

	return nullptr;
}

bool c_exe_file::parse()
{
	PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(exe_data);
	PIMAGE_NT_HEADERS32 nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS32>(reinterpret_cast<uint8_t*>(exe_data) + dos_header->e_lfanew);

	size_t section_index;
	PIMAGE_SECTION_HEADER section;
	for (section = IMAGE_FIRST_SECTION(nt_headers), section_index = 0; section_index < nt_headers->FileHeader.NumberOfSections; section = &section[1], section_index++)
	{
		s_exe_section section_struct;
		section_struct.section_index = section_index;
		section_struct.section_name.print("%.8s", (const char*)section->Name);
		section_struct.start_offset = section->PointerToRawData;
		section_struct.start_rva = section->VirtualAddress;
		section_struct.virtual_size = section->SizeOfRawData;
		section_struct.flags = section->Characteristics;

		sections.push_back(section_struct);
	}

	return true;
}

c_exe_file::c_exe_file(void* _exe_data) :
	exe_data(_exe_data)
{
	bool parse_result = parse();
	assert(parse_result);
}

c_exe_file::~c_exe_file()
{

}
