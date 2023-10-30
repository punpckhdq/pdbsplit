void print_linker_script(c_pdb_file& pdb_file, const c_exe_file& exe_file, const c_object_list& object_list)
{
	std::unordered_map<uint32_t, uint32_t> times;
	uint32_t last_object_file = -1;
	uint32_t last_section_hash = -1;
	static bool first = true;
	for (auto& contribution : pdb_file.get_section_contributions())
	{
		const s_object* object = object_list.get_object_for_rva(contribution.rva);

		if (object != nullptr && last_object_file != contribution.object_file_path_index)
		{
			c_static_string<32> section_name;
			get_section_contribution_section_name(exe_file, contribution, section_name);
			uint32_t section_hash = XXH32(section_name.get_string(), section_name.length(), 0);

			if (section_hash != last_section_hash)
			{
				if (!first)
				{
					printf("    }\n\n");
				}
				else
				{
					first = false;
				}

				c_static_string<8> meme;
				uint32_t idx = times[section_hash];
				if (idx > 0)
				{
					meme.print("$%u", idx);
				}
				printf("    %s%s ALIGN(1) :\n    {\n", section_name.get_string(), meme.get_string());

				times[section_hash]++;
				last_section_hash = section_hash;
			}

			last_object_file = contribution.object_file_path_index;

			printf("        PDBSPLIT_OUT_DIR/%s(%s);\n", object->object_file_name.get_string(), section_name.get_string());
		}
	}

	if (!first)
	{
		printf("    }\n}\n");
	}
}

void print_object_list_by_library(const c_object_list& object_list)
{
	std::unordered_map<uint32_t, std::vector<const s_object*>> library_objects_grouping;

	for (auto& object : object_list.get_objects())
	{
		if (object.contributions.size() == 0)
		{
			continue;
		}

		// bad
		uint32_t length = 0;
		if (strstr(object.object_original_library_path.get_string(), "\\halo\\objects\\halobetacache\\") == nullptr)
		{
			length = XXH32(object.object_original_library_path.get_string(), object.object_original_library_path.length(), 0);
		}
		library_objects_grouping[length].push_back(&object);
	}

	for (auto& pair : library_objects_grouping)
	{
		printf("%s:\n", pair.second[0]->object_original_library_path.get_string());
		for (auto* pain : pair.second)
		{
			printf("%s ", pain->object_file_name.get_string());
		}
		printf("\n");
	}
}

bool c_split_project::split()
{
	s_memory_mapped_file executable_file(config.input_exe_path.get_string());
	s_memory_mapped_file xbox_executable_file(config.input_xbe_path.get_string());
	s_memory_mapped_file debug_database_file(config.input_pdb_path.get_string());

	if (executable_file.valid() && xbox_executable_file.valid() && debug_database_file.valid()
		&& PDB::ValidateFile(debug_database_file.get_data()) == PDB::ErrorCode::Success)
	{
		PDB::RawFile raw_pdb = PDB::CreateRawFile(debug_database_file.get_data());
		c_pdb_file debug_database(raw_pdb);
		c_exe_file executable(executable_file.get_data());
		c_object_list objects(debug_database, executable);
		print_linker_script(debug_database, executable, objects);
		print_object_list_by_library(objects);
		c_xbe_file xbox_executable(xbox_executable_file.get_data());
		c_chunk_manager chunk_manager(executable, xbox_executable, debug_database, objects);
		c_split_writer split_writer(objects, debug_database, executable);
		split_writer.split_executable(config.output_folder_path);
	}

	puts("Split successfully");

	return true;
}

c_split_project::c_split_project(
	s_split_project_configuration& _config) :
	config(_config)
{

}
