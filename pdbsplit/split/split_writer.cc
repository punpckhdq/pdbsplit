struct s_elf_exe_section_mapping
{
	ELFIO::section* data_section;
	ELFIO::section* relocation_section;
};

struct s_local_chunk_info
{
	const s_chunk* chunk;
	uint32_t section_offset;
	s_elf_exe_section_mapping* section_mapping;
};

struct s_external_chunk_info
{
	const s_chunk* chunk;
	uint32_t chunk_offset;
};

ELFIO::Elf_Word get_elf_section_type_for_exe_section(const s_exe_section* exe_section)
{
	ELFIO::Elf_Word section_type = ELFIO::SHT_PROGBITS;
	uint32_t exe_flags = exe_section->flags;

	if ((exe_flags & exe_section_flag_cnt_uninitialized_data) != 0)
	{
		section_type = ELFIO::SHT_NOBITS;
	}

	return section_type;
}

struct s_section_flag_conversion
{
	uint32_t exe_flag;
	uint64_t elf_section_flag;
};

const s_section_flag_conversion elf_exe_section_flag_map[] =
{
	{ exe_section_flag_mem_execute, ELFIO::SHF_EXECINSTR },
	{ exe_section_flag_mem_write, ELFIO::SHF_WRITE },
};

ELFIO::Elf_Xword get_elf_section_flags_for_exe_section(const s_exe_section* exe_section)
{
	ELFIO::Elf_Xword section_flags = ELFIO::SHF_ALLOC;
	uint32_t exe_flags = exe_section->flags;

	for (const auto& conversion : elf_exe_section_flag_map)
	{
		if ((exe_flags & conversion.exe_flag) != 0)
		{
			section_flags |= conversion.elf_section_flag;
		}
	}

	return section_flags;
}

ELFIO::Elf_Xword get_elf_section_alignment_for_exe_section(const s_exe_section* exe_section)
{
	return 0x10;
}

void write_chunks(
	const c_static_string<MAX_PATH>& output_path,
	const std::vector<const s_chunk*>& chunks,
	const c_exe_file& exe_file)
{
	ELFIO::elfio writer;
	writer.create(ELFIO::ELFCLASS32, ELFIO::ELFDATA2LSB);
	writer.set_os_abi(ELFIO::ELFOSABI_STANDALONE);
	writer.set_type(ELFIO::ET_REL);
	writer.set_machine(ELFIO::EM_386);

	ELFIO::section* string_table_section = writer.sections.add(".strtab");
	string_table_section->set_type(ELFIO::SHT_STRTAB);

	ELFIO::section* symbol_table_section = writer.sections.add(".symtab");
	symbol_table_section->set_type(ELFIO::SHT_SYMTAB);
	symbol_table_section->set_addr_align(0x4);
	symbol_table_section->set_entry_size(writer.get_default_entry_size(ELFIO::SHT_SYMTAB));
	symbol_table_section->set_link(string_table_section->get_index());

	ELFIO::string_section_accessor stra(string_table_section);
	ELFIO::symbol_section_accessor syma(writer, symbol_table_section);

	std::unordered_map<uint32_t, s_external_chunk_info> external_chunk_map;
	std::unordered_map<uint32_t, s_local_chunk_info> internal_chunk_map;
	std::unordered_map<uint16_t, s_elf_exe_section_mapping> section_map;
	std::unordered_map<uint32_t, ELFIO::Elf_Word> symbol_map;

	for (const s_chunk* chunk : chunks)
	{
		const s_exe_section* section = exe_file.get_section_for_rva(chunk->rva);
		// bad
		uint32_t index = section->section_index + (100 * chunk->contribution->subsection_index);

		if (!section_map.contains(index))
		{
			c_static_string<32> section_name_buffer;
			get_section_contribution_section_name(exe_file, *chunk->contribution, section_name_buffer);
			const char* section_name = section_name_buffer.get_string();

			ELFIO::section* new_data_section = writer.sections.add(section_name);
			new_data_section->set_type(get_elf_section_type_for_exe_section(section));
			new_data_section->set_flags(get_elf_section_flags_for_exe_section(section));
			new_data_section->set_addr_align(get_elf_section_alignment_for_exe_section(section));

			c_static_string<16> relocation_section_name;
			relocation_section_name.print(".rela.%s", section_name[0] == '.' ? section_name + 1 : section_name);
			ELFIO::section* new_relocation_section = writer.sections.add(relocation_section_name.get_string());
			new_relocation_section->set_type(ELFIO::SHT_RELA);
			new_relocation_section->set_info(new_data_section->get_index());
			new_relocation_section->set_addr_align(0x4);
			new_relocation_section->set_entry_size(writer.get_default_entry_size(ELFIO::SHT_RELA));
			new_relocation_section->set_link(symbol_table_section->get_index());

			s_elf_exe_section_mapping new_section_mapping
			{
				.data_section = new_data_section,
				.relocation_section = new_relocation_section
			};
			section_map[index] = new_section_mapping;
		}

		s_local_chunk_info chunk_info;
		chunk_info.chunk = chunk;
		chunk_info.section_mapping = &section_map[index];
		internal_chunk_map[chunk->rva] = chunk_info;
	}

	for (auto& internal_chunk_pair : internal_chunk_map)
	{
		s_local_chunk_info* internal_chunk_info = &internal_chunk_pair.second;

		const void* chunk_data = exe_file.get_data(internal_chunk_info->chunk->rva);
		uint64_t chunk_elf_offset = internal_chunk_info->section_mapping->data_section->get_size();
		internal_chunk_info->section_offset = chunk_elf_offset;
		internal_chunk_info->section_mapping->data_section->append_data(reinterpret_cast<const char*>(chunk_data), internal_chunk_info->chunk->length);

		for (const auto& chunk_symbol : internal_chunk_info->chunk->symbols)
		{
			uint8_t bind = ELFIO::STB_GLOBAL;
			uint8_t type = ((internal_chunk_info->chunk->contribution->characteristics & exe_section_flag_cnt_code) != 0) ? ELFIO::STT_FUNC : ELFIO::STT_NOTYPE;
			uint64_t symbol_offset = chunk_elf_offset + chunk_symbol.offset;

			ELFIO::Elf_Word new_symbol = syma.add_symbol(
				stra,
				chunk_symbol.name.get_string(),
				symbol_offset,
				0,
				bind,
				type,
				0,
				internal_chunk_info->section_mapping->data_section->get_index());

			symbol_map[internal_chunk_info->chunk->rva + chunk_symbol.offset] = new_symbol;
		}
	}

	//symbol_table_section->set_info(syma.get_symbols_num());
	symbol_table_section->set_info(1);

	for (const s_chunk* chunk : chunks)
	{
		for (const auto& relocation : chunk->relocations)
		{
			if (!internal_chunk_map.contains(relocation.target_chunk->rva))
			{
				s_external_chunk_info chunk_info;
				chunk_info.chunk = &(*relocation.target_chunk);
				chunk_info.chunk_offset = relocation.target_chunk_offset;
				external_chunk_map[relocation.target_chunk->rva + relocation.target_chunk_offset] = chunk_info;
			}
		}
	}

	for (auto& chunk_info : external_chunk_map)
	{
		const s_chunk_symbol* closest_symbol = &chunk_info.second.chunk->symbols[0];
		int64_t offset_from_closest_symbol = 0;
		for (const s_chunk_symbol& symbol : chunk_info.second.chunk->symbols)
		{
			if (chunk_info.second.chunk_offset >= symbol.offset)
			{
				closest_symbol = &symbol;
				offset_from_closest_symbol = chunk_info.second.chunk_offset - symbol.offset;
			}
		}

		uint8_t bind = ELFIO::STB_GLOBAL;
		uint8_t type = (chunk_info.second.chunk->contribution->characteristics & exe_section_flag_cnt_code) ? ELFIO::STT_FUNC : ELFIO::STT_NOTYPE;
		
		ELFIO::Elf_Word new_symbol = syma.add_symbol(
			stra,
			closest_symbol->name.get_string(),
			0,
			0,
			bind,
			type,
			0,
			0);

		symbol_map[chunk_info.second.chunk->rva + closest_symbol->offset] = new_symbol;
	}

	for (auto& internal_chunk_pair : internal_chunk_map)
	{
		s_local_chunk_info* internal_chunk_info = &internal_chunk_pair.second;
		const s_chunk* chunk = internal_chunk_info->chunk;

		for (const auto& relocation : chunk->relocations)
		{
			const s_chunk_symbol* closest_symbol = &relocation.target_chunk->symbols[0];
			int64_t offset_from_closest_symbol = 0;
			for (const s_chunk_symbol& symbol : relocation.target_chunk->symbols)
			{
				if (relocation.target_chunk_offset >= symbol.offset)
				{
					closest_symbol = &symbol;
					offset_from_closest_symbol = relocation.target_chunk_offset - symbol.offset;
				}
			}

			assert(symbol_map.contains(relocation.target_chunk->rva + closest_symbol->offset));
			ELFIO::Elf_Word target_symbol = symbol_map[relocation.target_chunk->rva + closest_symbol->offset];
			ELFIO::relocation_section_accessor rela(writer, internal_chunk_info->section_mapping->relocation_section);

			uint32_t section_offset_into_reloc = internal_chunk_info->section_offset + relocation.offset;
			int32_t* elf_reloc = reinterpret_cast<int32_t*>(const_cast<char*>(internal_chunk_info->section_mapping->data_section->get_data()) + section_offset_into_reloc);
			uint32_t elf_relocation_type = ELFIO::R_386_NONE;
			
			if (relocation.type == _relocation_type_rel32)
			{
				elf_relocation_type = ELFIO::R_386_PC32;
				offset_from_closest_symbol -= 4;
				*elf_reloc = 0;
			}
			else
			{
				elf_relocation_type = ELFIO::R_386_32;
				*elf_reloc = 0;
			}
			
			rela.add_entry(
				section_offset_into_reloc,
				target_symbol,
				elf_relocation_type,
				offset_from_closest_symbol);
		}
	}

	ELFIO::section* note_section = writer.sections.add(".comment");
	note_section->set_type(ELFIO::SHT_NOTE);
	ELFIO::note_section_accessor note_writer(writer, note_section);
	note_writer.add_note(0x01, "PDBsplit " __FUNCTION__ ", built " __TIMESTAMP__, 0, 0);

	bool write_result = writer.save(output_path.get_string());
	assert(write_result = true);
}

void c_split_writer::split_executable(
	const c_static_string<MAX_PATH>& output_folder_path)
{
	auto& objects = m_object_list.get_objects();

	printf("Exporting %zu objects\n", objects.size());

	for (auto& object : objects)
	{
		assert(object.chunks.size() == object.contributions.size());

		if (object.chunks.size() > 0)
		{
			c_static_string<MAX_PATH> output_path;
			output_path.print("%s/%s", output_folder_path.get_string(), object.object_file_name.get_string());
			printf("%s...\n", output_path.get_string());
			write_chunks(output_path, object.chunks, m_exe_file);
		}
	}
}

c_split_writer::c_split_writer(
	c_object_list& object_list,
	c_pdb_file& pdb_file,
	c_exe_file& exe_file) :
	m_object_list(object_list),
	m_debug_database(pdb_file),
	m_exe_file(exe_file)
{

}

c_split_writer::~c_split_writer()
{

}
