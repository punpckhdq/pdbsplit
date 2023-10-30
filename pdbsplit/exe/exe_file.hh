using relocation_t = uint32_t;

enum e_exe_section_flags
{
	exe_section_flag_type_no_pad			= 0x00000008,
	exe_section_flag_cnt_code				= 0x00000020,
	exe_section_flag_cnt_initialized_data	= 0x00000040,
	exe_section_flag_cnt_uninitialized_data	= 0x00000080,
	
	exe_section_flag_lnk_other				= 0x00000100,
	exe_section_flag_lnk_info				= 0x00000200,
	exe_section_flag_lnk_remove				= 0x00000800,
	exe_section_flag_lnk_comdat				= 0x00001000,

	exe_section_flag_no_defer_spec_exc		= 0x00004000,
	exe_section_flag_gprel					= 0x00008000,
	
	exe_section_flag_mem_purgeable			= 0x00020000,
	exe_section_flag_mem_locked				= 0x00040000,
	exe_section_flag_mem_preload			= 0x00080000,

	//exe_section_flag_align_1bytes			= 0x00100000,
	//exe_section_flag_align_2bytes			= 0x00200000,
	//exe_section_flag_align_4bytes			= 0x00300000,
	//exe_section_flag_align_8bytes			= 0x00400000,
	//exe_section_flag_align_16bytes			= 0x00500000,
	//exe_section_flag_align_32bytes			= 0x00600000,
	//exe_section_flag_align_64bytes			= 0x00700000,
	//exe_section_flag_align_128bytes			= 0x00800000,
	//exe_section_flag_align_256bytes			= 0x00900000,
	//exe_section_flag_align_512bytes			= 0x00a00000,
	//exe_section_flag_align_1024bytes		= 0x00b00000,
	//exe_section_flag_align_2048bytes		= 0x00c00000,
	//exe_section_flag_align_4096bytes		= 0x00d00000,
	//exe_section_flag_align_8192bytes		= 0x00e00000,
	
	exe_section_flag_lnk_nreloc_ovfl		= 0x01000000,
	
	exe_section_flag_mem_discardable		= 0x02000000,
	exe_section_flag_mem_not_cached			= 0x04000000,
	exe_section_flag_mem_not_paged			= 0x08000000,
	exe_section_flag_mem_shared				= 0x10000000,
	exe_section_flag_mem_execute			= 0x20000000,
	exe_section_flag_mem_read				= 0x40000000,
	exe_section_flag_mem_write				= 0x80000000
};

enum e_relocation_type
{
	_relocation_type_unk,
	_relocation_type_abs32,
	_relocation_type_rel32,

	k_number_of_relocation_types
};

struct s_exe_section
{
	uint16_t section_index;
	c_static_string<9> section_name;
	uint32_t start_offset;
	uint32_t start_rva;
	uint32_t virtual_size;
	uint32_t flags;
};

class c_exe_file
{
private:
	std::vector<s_exe_section> sections;
	void* exe_data;

	bool parse();

public:
	uint32_t get_image_base() const;
	const s_exe_section* get_section_for_rva(uint32_t rva) const;
	const void* get_data(uint32_t rva = 0) const;
	const void* get_relocation_data() const;

	c_exe_file(void* exe_data);
	~c_exe_file();
};

struct s_image_base_relocation
{
	uint32_t virtual_address;
	uint32_t size_of_block;
};
