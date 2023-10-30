int main(int argc, char* argv[])
{
	int return_value = EXIT_FAILURE;

	if (argc == 5)
	{
		s_split_project_configuration config;
		config.input_exe_path.set(argv[1]);
		config.input_xbe_path.set(argv[2]);
		config.input_pdb_path.set(argv[3]);
		config.output_folder_path.set(argv[4]);

		c_split_project project(config);
		if (project.split() != false)
		{
			return_value = EXIT_SUCCESS;
		}
	}

	return return_value;
}
