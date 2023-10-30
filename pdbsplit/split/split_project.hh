struct s_split_project_configuration
{
	c_static_string<MAX_PATH> input_exe_path;
	c_static_string<MAX_PATH> input_xbe_path;
	c_static_string<MAX_PATH> input_pdb_path;
	c_static_string<MAX_PATH> output_folder_path;
};

class c_split_project
{
private:
	s_split_project_configuration& config;

public:
	bool split();

	c_split_project(
		s_split_project_configuration& config);
};
