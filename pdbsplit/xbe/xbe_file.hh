struct s_xbe_section
{
	uint16_t section_index;
	uint32_t start_offset;
	uint32_t start_rva;
	uint32_t virtual_size;
};

constexpr uint32_t exe_header_size = 0x600;
constexpr uint32_t xbe_header_size = 0x2000;

class c_xbe_file
{
private:
	std::vector<s_xbe_section> m_sections;
	void* m_xbe_data;

	bool parse();
public:
	const uint32_t get_base_address();
	const void* get_data_for_index_offset(uint16_t index, uint32_t offset) const;

	c_xbe_file(void* xbe_data);
	~c_xbe_file();
};
