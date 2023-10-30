struct s_chunk_debug_database_info
{
	uint16_t object_file_path_index;
	uint32_t debug_characteristics;
};

using chunk_offset_t = uint32_t;

struct s_chunk_symbol
{
	public_name_string_t name;
	chunk_offset_t offset;
};

struct s_chunk;

struct s_chunk_relocation
{
	e_relocation_type type;
	chunk_offset_t offset;
	relocation_t target;
	s_chunk* target_chunk;
	chunk_offset_t target_chunk_offset;
};

struct s_chunk
{
	const s_section_contribution* contribution;

	uint32_t rva;
	uint32_t length;

	std::vector<s_chunk_relocation> relocations;
	std::vector<s_chunk_symbol> symbols;
};

class c_chunk_manager
{
private:
	std::vector<s_chunk> m_chunks;

	const c_pdb_file& m_debug_database;
	const c_exe_file& m_exe_file;
	const c_xbe_file& m_xbe_file;
	c_object_list& m_object_list;

	bool populate_chunks();
	bool populate_chunk_accesses();
public:
	std::vector<s_chunk>& get_chunks();

	c_chunk_manager(const c_exe_file& exe_file, const c_xbe_file& xbe_file, const c_pdb_file& debug_database, c_object_list& object_list);
	~c_chunk_manager();
};
