class c_split_writer
{
private:
	const c_object_list& m_object_list;
	const c_pdb_file& m_debug_database;
	const c_exe_file& m_exe_file;

public:
	void split_executable(const c_static_string<MAX_PATH>& output_folder_path);

	c_split_writer(c_object_list& object_list, c_pdb_file& pdb_file, c_exe_file& exe_file);
	~c_split_writer();
};
