enum
{
	k_max_symbol_length = 256u
};

using public_name_string_t = c_static_string<k_max_symbol_length>;
using object_file_path_string_t = c_static_string<MAX_PATH>;

struct s_object_file_path
{
	object_file_path_string_t object_file_path;
	object_file_path_string_t library_file_path;
};

struct s_section_contribution
{
	uint32_t object_file_path_index;
	uint32_t rva;
	uint32_t size;
	uint32_t characteristics;
	uint32_t data_crc;

	// not really stored in the pdb. we need to figure out stuff like .data$1
	uint32_t subsection_index;
	uint32_t object_name_hash;
};

struct s_public_symbol
{
	public_name_string_t name;
	uint32_t rva;
};

class c_pdb_file
{
private:
	std::vector<s_object_file_path> m_object_file_paths;
	std::vector<s_section_contribution> m_section_contributions;
	std::vector<s_public_symbol> m_public_symbols;

	PDB::RawFile& m_raw_pdb;

	bool parse();
public:
	const std::vector<s_object_file_path>& get_object_file_paths() const;
	const std::vector<s_section_contribution>& get_section_contributions() const;
	std::vector<s_section_contribution>& get_section_contributions();
	const std::vector<s_public_symbol>& get_public_symbols() const;

	c_pdb_file(PDB::RawFile& raw_pdb);
	~c_pdb_file();
};
