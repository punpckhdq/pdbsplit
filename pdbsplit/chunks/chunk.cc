static inline uint32_t binary_search_get_public_symbol_rva(
	const s_public_symbol* data,
	uint32_t search_middle)
{
	return data[search_middle].rva;
}

static inline void fill_public_symbols_for_range(
	const std::vector<s_public_symbol>& symbols,
	std::vector<const s_public_symbol*>& names,
	uint32_t start,
	uint32_t end)
{
	const s_public_symbol* data = symbols.data();
	const s_public_symbol* head = nullptr;

	binary_search<const s_public_symbol>(
		data,
		symbols.size(),
		binary_search_get_public_symbol_rva,
		start,
		head);

	// 99% of cases: public name at the start of a section contribution
	if (head)
	{
		while (head->rva < end)
		{
			names.push_back(head);
			head++;
		}
	}
	else
	{
		for (const s_public_symbol& symbol : symbols)
		{
			if (symbol.rva >= start)
			{
				if (symbol.rva < end)
				{
					names.push_back(&symbol);
				}
				else
				{
					break;
				}
			}
		}
	}
}

static inline void fill_relocations_for_range_data(
	s_chunk& chunk,
	const c_exe_file& exe_file)
{
	const void* relocation_data = exe_file.get_relocation_data();
	uint32_t image_base = exe_file.get_image_base();
	const s_image_base_relocation* header = reinterpret_cast<const s_image_base_relocation*>(relocation_data);

	uint32_t start_rva = chunk.rva;
	uint32_t end_rva = chunk.rva + chunk.length;

	for (;;)
	{
		if (header->size_of_block == 0 ||
			header->virtual_address >= end_rva)
		{
			break;
		}

		if (header->virtual_address >= start_rva ||
			header->virtual_address + header->size_of_block < end_rva)
		{
			for (size_t i = 0; ; i++)
			{
				const uint16_t* offset_ptr = reinterpret_cast<const uint16_t*>(
					reinterpret_cast<const uint8_t*>(header) + sizeof(s_image_base_relocation) + (i * 2));
				uint16_t offset = *offset_ptr & 0x0fff;

				uint16_t flag = *offset_ptr & 0xf000;
				//assert(!flag);

				if (offset == 0 && i != 0)
				{
					break;
				}

				uint32_t address = header->virtual_address + offset;
				if (address >= start_rva && address < end_rva)
				{
					s_chunk_relocation relocation{};
					relocation.type = _relocation_type_abs32;
					relocation.offset = address - start_rva;
					relocation.target = *reinterpret_cast<const uint32_t*>(exe_file.get_data(address)) - image_base;
					chunk.relocations.push_back(relocation);
				}
			}
		}

		header = reinterpret_cast<const s_image_base_relocation*>(
			reinterpret_cast<const uint8_t*>(header) + header->size_of_block);
	}
}

static inline bool push_back_relocation_if_sane(
	std::vector<s_chunk_relocation>& relocations,
	s_chunk_relocation& relocation)
{
	if (relocation.target < 0x600)
	{
		return false;
	}

	// Hack: rva is within sane bounds?
	//assert(target_rva >= text_section->start_rva && target_rva < text_section->start_rva + text_section->virtual_size);
	// Kept running into jump tables at the end of functions
	if (relocation.target > (0x00A25FBF - 0x400000))
	{
		return false;
	}

	relocations.push_back(relocation);

	return true;
}

// find relative jmp/calls
static inline void fill_relocations_for_range_disassemble(
	s_chunk& chunk,
	const c_exe_file& exe_file,
	ZydisDecoder* decoder)
{
	const uint8_t* data = (const uint8_t*)exe_file.get_data(chunk.rva);
	const s_exe_section* text_section = exe_file.get_section_for_rva(chunk.rva);

	//ZyanUSize switch_table_offset = chunk.length;
	ZyanUSize offset = 0;
	ZydisDecodedInstruction instruction;
	ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
	while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(
		decoder,
		data + offset,
		chunk.length - offset,
		&instruction,
		operands)))
	{
		switch (instruction.opcode)
		{
		// JMP/CALL relative
		case 0xe9:
		case 0xe8:
		{
			ZyanU64 zyan_target_rva = 0;
			ZyanStatus zyan_status = ZydisCalcAbsoluteAddress(&instruction, &operands[0], chunk.rva + offset, &zyan_target_rva);
			if (ZYAN_SUCCESS(zyan_status))
			{
				s_chunk_relocation relocation{};
				relocation.type = _relocation_type_rel32;
				relocation.offset = offset + 1;
				relocation.target = (uint32_t)zyan_target_rva;
				push_back_relocation_if_sane(chunk.relocations, relocation);
			}
			break;
		}
		default:
		{
			break;
		}
		}

		offset += instruction.length;
	}
}

static inline bool fill_relocations_for_range_xbe_compare(
	s_chunk& chunk,
	const c_exe_file& exe_file,
	const c_xbe_file& xbe_file)
{
	const s_exe_section* exe_section = exe_file.get_section_for_rva(chunk.rva);
	const uint8_t* exe_chunk_data = (const uint8_t*)exe_file.get_data(chunk.rva);
	const uint8_t* xbe_chunk_data = (const uint8_t*)xbe_file.get_data_for_index_offset(exe_section->section_index, chunk.rva - exe_section->start_rva);

	for (size_t i = 0; i < chunk.length; i)
	{
		if (exe_chunk_data[i] != xbe_chunk_data[i])
		{
			const uint32_t exe_org = 0x400600;
			const uint32_t xbe_org = 0x012000;
			const uint32_t exe_xbe_org_diff = exe_org - xbe_org;
			const int32_t disp = -1;
			
			int32_t exe_dword = *(int32_t*)&exe_chunk_data[i+disp];
			int32_t xbe_dword = *(int32_t*)&xbe_chunk_data[i+disp];
			assert(exe_dword - exe_xbe_org_diff == xbe_dword);
			
			s_chunk_relocation relocation{};
			relocation.type = _relocation_type_unk;
			relocation.offset = i + disp;
			relocation.target = exe_dword;

			chunk.relocations.push_back(relocation);
			
			i += 4 + disp;
		}
		else
		{
			i++;
		}
	}

	return true;
}

bool c_chunk_manager::populate_chunks()
{
	const auto& object_file_paths = m_debug_database.get_object_file_paths();
	const auto& section_contributions = m_debug_database.get_section_contributions();
	const auto& public_symbols = m_debug_database.get_public_symbols();

	puts("Populating chunks");

	ZydisDecoder decoder;
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);

	size_t number_of_objects = object_file_paths.size();
	for (size_t object_index = 0; object_index < number_of_objects; object_index++)
	{
		std::vector<const s_section_contribution*> object_section_contributions;
		for (auto& section_contribution : section_contributions)
		{
			if (section_contribution.object_file_path_index == object_index)
			{
				object_section_contributions.push_back(&section_contribution);
			}
		}

		for (auto section_contribution : object_section_contributions)
		{
			s_chunk chunk{};
			chunk.contribution = section_contribution;
			chunk.rva = section_contribution->rva;
			chunk.length = section_contribution->size;

			uint32_t start_rva = chunk.rva;
			uint32_t end_rva = chunk.rva + chunk.length;

			std::vector<const s_public_symbol*> chunk_public_symbols;
			fill_public_symbols_for_range(public_symbols, chunk_public_symbols, start_rva, end_rva);
			for (auto& public_symbol : chunk_public_symbols)
			{
				s_chunk_symbol chunk_public_symbol{};
				chunk_public_symbol.name.set(public_symbol->name.get_string());
				chunk_public_symbol.offset = public_symbol->rva - chunk.rva;
				chunk.symbols.push_back(chunk_public_symbol);
			}

			if ((chunk.contribution->characteristics & (exe_section_flag_cnt_code)) != 0 && chunk.length > 4)
			{
				fill_relocations_for_range_disassemble(chunk, m_exe_file, &decoder);
				fill_relocations_for_range_xbe_compare(chunk, m_exe_file, m_xbe_file);
			}
			else
			{
				fill_relocations_for_range_data(chunk, m_exe_file);
			}
			m_chunks.push_back(chunk);
		}
	}

	return true;
}

static inline s_chunk* get_chunk_for_address(
	std::vector<s_chunk>& chunks,
	uint32_t address,
	uint32_t& referenced_chunk_offset)
{
	size_t number_of_chunks = chunks.size();

	for (size_t i = 0; i < number_of_chunks; i++)
	{
		s_chunk& chunk = chunks[i];

		if (address >= chunk.rva && address < chunk.rva + chunk.length)
		{
			referenced_chunk_offset = address - chunk.rva;
			return &chunk;
		}
	}

	return nullptr;
}

static inline const char* get_string_for_chunk_debug_flags(s_chunk& chunk)
{
	if ((chunk.contribution->characteristics & (exe_section_flag_cnt_code)) != 0)
	{
		return "_code";
	}
	else if ((chunk.contribution->characteristics & (exe_section_flag_cnt_initialized_data)) != 0)
	{
		return "_data";
	}
	else if ((chunk.contribution->characteristics & (exe_section_flag_cnt_uninitialized_data)) != 0)
	{
		return "_bss";
	}
	else
	{
		return "_unk";
	}
}

static inline void sanitise_symbol_name(char* string_data)
{
	while (*string_data != '\0')
	{
		switch (*string_data)
		{
		case '?':
		case '@':
			*string_data = '_';
		default:
			break;
		}

		string_data++;
	}
}

bool c_chunk_manager::populate_chunk_accesses()
{
	puts("Analysing chunk relationships");

	uint32_t image_base = m_exe_file.get_image_base();

	for (auto& chunk : m_chunks)
	{
		if (chunk.symbols.size() == 0 || chunk.symbols[0].offset != 0)
		{
			s_chunk_symbol dummy_symbol{};
			dummy_symbol.name.print("%s_%08x", get_string_for_chunk_debug_flags(chunk), chunk.rva + image_base);

			// we do a little trolling
			std::reverse(chunk.symbols.begin(), chunk.symbols.end());
			chunk.symbols.push_back(dummy_symbol);
			std::reverse(chunk.symbols.begin(), chunk.symbols.end());
		}
		else
		{
			for (auto& symbol : chunk.symbols)
			{
				sanitise_symbol_name((char*)symbol.name.get_string());
			}
		}

		for (auto& chunk_relocation : chunk.relocations)
		{
			if (chunk_relocation.type == _relocation_type_unk)
			{
				uint32_t rva_referenced_chunk_offset = 0;
				s_chunk* rva_referenced_chunk = get_chunk_for_address(m_chunks, chunk_relocation.target, rva_referenced_chunk_offset);
				chunk_relocation.type = _relocation_type_abs32;

				uint32_t imagebase_relative_referenced_offset = 0;
				s_chunk* imagebase_relative_referenced_chunk = get_chunk_for_address(m_chunks, chunk_relocation.target - image_base, imagebase_relative_referenced_offset);
				//chunk_relocation.type = _relocation_type_abs32_plus_imagebase;

				s_chunk* preferred_chunk = rva_referenced_chunk;
				uint32_t preferred_chunk_offset = rva_referenced_chunk_offset;
				if (imagebase_relative_referenced_chunk != nullptr)
				{
					preferred_chunk = imagebase_relative_referenced_chunk;
					preferred_chunk_offset = imagebase_relative_referenced_offset;
				}
				assert(preferred_chunk != nullptr);

				chunk_relocation.target_chunk = preferred_chunk;
				chunk_relocation.target_chunk_offset = preferred_chunk_offset;
			}
			else
			{
				uint32_t referenced_chunk_offset = 0;
				s_chunk* referenced_chunk = get_chunk_for_address(m_chunks, chunk_relocation.target, referenced_chunk_offset);
				assert(referenced_chunk != nullptr);

				chunk_relocation.target_chunk = referenced_chunk;
				chunk_relocation.target_chunk_offset = referenced_chunk_offset;
			}
		}
	}

	// awful
	for (auto& object : m_object_list.get_objects())
	{
		for (auto& contribution : object.contributions)
		{
			for (auto& chunk : m_chunks)
			{
				if (chunk.rva == contribution->rva)
				{
					object.chunks.push_back(&chunk);
				}
			}
		}
	}

	return true;
}

std::vector<s_chunk>& c_chunk_manager::get_chunks()
{
	return m_chunks;
}

c_chunk_manager::c_chunk_manager(
	const c_exe_file& exe_file,
	const c_xbe_file& xbe_file,
	const c_pdb_file& debug_database,
	c_object_list& object_list) :
	m_exe_file(exe_file),
	m_xbe_file(xbe_file),
	m_debug_database(debug_database),
	m_object_list(object_list)
{
	bool result = populate_chunks();
	assert(result);
	result = populate_chunk_accesses();
	assert(result);
}

c_chunk_manager::~c_chunk_manager()
{

}
