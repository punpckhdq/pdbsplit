#include "objects.hh"
static bool get_object_file_name(
	c_static_string<MAX_PATH>& file_name,
	const s_object_file_path& file_path)
{
	bool success = false;

	if (strstr(file_path.object_file_path.get_string(), ".obj") != nullptr)
	{
		std::string FUCK = file_path.library_file_path.get_string();
		std::string FUUCK = FUCK.substr(FUCK.find_last_of("/\\") + 1, FUCK.find_last_of(".") - (FUCK.find_last_of("/\\") + 1));

		std::string FUUUCK = file_path.object_file_path.get_string();
		std::string FUUUUCK = FUUUCK.substr(FUUUCK.find_last_of("/\\") + 1, FUUUCK.find_last_of(".") - (FUUUCK.find_last_of("/\\") + 1));

		if (FUUCK != FUUUUCK)
		{
			file_name.print("%s_%s", FUUCK.c_str(), FUUUUCK.c_str());
		}
		else
		{
			file_name.print("%s", FUUUUCK.c_str());
		}

		success = true;
	}
	else
	{
		if (file_path.library_file_path.length() == 0)
		{
			file_name.set("_linker_common");
			success = true;
		}
	}

	return success;
}

void c_object_list::populate()
{
	auto& object_file_paths = m_pdb_file.get_object_file_paths();
	auto& contributions = m_pdb_file.get_section_contributions();

	for (size_t i = 0; i < object_file_paths.size(); i++)
	{
		auto& object_file_path = object_file_paths[i];

		c_static_string<MAX_PATH> file_name;
		bool success = get_object_file_name(file_name, object_file_path);

		if (!success)
		{
			printf("Skipped export definition %s\n", object_file_path.object_file_path.get_string());
			continue;
		}

		struct s_contribution_span
		{
			const s_exe_section* section;
			size_t first;
			size_t length;
		};

		std::vector<s_contribution_span> spans;

		size_t first = 0;
		size_t length = 0;
		bool finding = false;
		for (size_t j = 0; j < contributions.size(); j++)
		{
			auto& contribution = contributions[j];

			if (contribution.object_file_path_index == i)
			{
				if (!finding)
				{
					first = j;
					finding = true;
				}

				length++;
			}
			else if (finding)
			{
				finding = false;

				const s_exe_section* section = m_exe_file.get_section_for_rva(contributions[first].rva);

				s_contribution_span span;
				span.section = section;
				span.first = first;
				span.length = length;
				spans.push_back(span);

				length = 0;
			}
		}

		std::unordered_map<uint32_t, uint32_t> times;
		s_object object;
		object.object_file_name.print("%s.o", file_name.get_string());
		object.object_original_library_path.set(object_file_path.library_file_path.get_string());

		for (auto& span : spans)
		{
			uint32_t subsection_index = times[span.section->section_index]++;

			for (size_t k = span.first; k < span.first + span.length; k++)
			{
				s_section_contribution& contribution = contributions[k];
				contribution.subsection_index = subsection_index;
				contribution.object_name_hash = XXH32(object.object_file_name.get_string(), object.object_file_name.length(), 0);

				object.contributions.push_back(&contribution);
			}
		}

		m_objects.push_back(object);
	}
}

const s_object* c_object_list::get_object_for_rva(uint32_t rva) const
{
	for (auto& object : m_objects)
	{
		for (auto& contribution : object.contributions)
		{
			if (contribution->rva == rva)
			{
				return &object;
			}
		}
	}

	return nullptr;
}

const std::vector<s_object>& c_object_list::get_objects() const
{
	return m_objects;
}

std::vector<s_object>& c_object_list::get_objects()
{
	return m_objects;
}

c_object_list::c_object_list(c_pdb_file& pdb_file, c_exe_file& exe_file)
	: m_pdb_file(pdb_file),
	m_exe_file(exe_file)
{
	populate();
}

void get_section_contribution_section_name(const c_exe_file& exe_file, const s_section_contribution& contribution, c_static_string<32>& section_name)
{
	const s_exe_section* section = exe_file.get_section_for_rva(contribution.rva);
	section_name.set(section->section_name.get_string());

	if (contribution.subsection_index != 0)
	{
		section_name.append_print(".obj_%08x_%u", contribution.object_name_hash, contribution.subsection_index);
	}
}
