#pragma once

struct s_chunk;

struct s_object
{
	std::vector<const s_section_contribution*> contributions;
	std::vector<const s_chunk*> chunks;
	object_file_path_string_t object_file_name;
	object_file_path_string_t object_original_library_path;
};

class c_object_list
{
private:
	c_pdb_file& m_pdb_file;
	c_exe_file& m_exe_file;

	std::vector<s_object> m_objects;

	void populate();
public:
	const s_object* get_object_for_rva(uint32_t rva) const;
	const std::vector<s_object>& get_objects() const;
	std::vector<s_object>& get_objects();

	c_object_list(c_pdb_file& pdb_file, c_exe_file& exe_file);
};

void get_section_contribution_section_name(const c_exe_file& exe_file, const s_section_contribution& contribution, c_static_string<32>& section_name);
