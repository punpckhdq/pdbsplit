#include <algorithm>

bool c_pdb_file::parse()
{
	PDB::DBIStream dbi_stream = PDB::CreateDBIStream(m_raw_pdb);
	PDB::ImageSectionStream image_section_stream = dbi_stream.CreateImageSectionStream(m_raw_pdb);
	PDB::SectionContributionStream section_contribution_stream = dbi_stream.CreateSectionContributionStream(m_raw_pdb);
	PDB::CoalescedMSFStream symbol_record_stream = dbi_stream.CreateSymbolRecordStream(m_raw_pdb);
	PDB::PublicSymbolStream public_symbol_stream = dbi_stream.CreatePublicSymbolStream(m_raw_pdb);
	PDB::ModuleInfoStream module_info_stream = dbi_stream.CreateModuleInfoStream(m_raw_pdb);

	const PDB::ArrayView<PDB::DBI::SectionContribution> section_contributions = section_contribution_stream.GetContributions();
	m_section_contributions.reserve(section_contributions.GetLength());
	for (const auto& section_contribution : section_contributions)
	{
		const uint32_t rva = image_section_stream.ConvertSectionOffsetToRVA(section_contribution.section, section_contribution.offset);
		assert(rva != 0);

		m_section_contributions.push_back(
			s_section_contribution
			{
				section_contribution.moduleIndex,
				rva,
				section_contribution.size,
				section_contribution.characteristics,
				section_contribution.dataCrc
			});
	}

	static const s_section_contribution unk_contributions[] =
	{
		s_section_contribution
		{
			0x01a5,
			0x002b7f84,
			4,
			0,
			0
		},
		s_section_contribution
		{
			0x012c,
			0x0043ecac,
			4,
			0,
			0
		},
		s_section_contribution
		{
			0x00fa,
			0x005921c4,
			4,
			0,
			0
		},
		s_section_contribution
		{
			0x0089,
			0x00590a9e,
			2,
			0,
			0
		}
	};

	for (const s_section_contribution& contribution : unk_contributions)
	{
		m_section_contributions.push_back(contribution);
	}

	std::sort(m_section_contributions.begin(), m_section_contributions.end(),
		[](const auto& lhs, const auto& rhs)
		{
			return lhs.rva < rhs.rva;
		}
	);

	const PDB::ArrayView<PDB::HashRecord> hash_records = public_symbol_stream.GetRecords();
	for (const auto& hash_record : hash_records)
	{
		const PDB::CodeView::DBI::Record* record = public_symbol_stream.GetRecord(symbol_record_stream, hash_record);
		if (!record)
		{
			// end of data?
			break;
		}

		const uint32_t rva = image_section_stream.ConvertSectionOffsetToRVA(
			record->data.S_PUB32.section, record->data.S_PUB32.offset);
		if (rva == 0)
		{
			// certain symbols (e.g. control-flow guard symbols) don't have a valid RVA, ignore those
			continue;
		}

		s_public_symbol symbol{};
		symbol.name.print("%.*s", record->data.S_PUB32.name.vc60.length, record->data.S_PUB32.name.vc60.string);
		symbol.rva = rva;
		m_public_symbols.push_back(symbol);
	}

	// must sort symbols to binary search later
	std::sort(m_public_symbols.begin(), m_public_symbols.end(),
		[](const auto& lhs, const auto& rhs)
		{
			return lhs.rva < rhs.rva;
		}
	);

	const PDB::ArrayView<PDB::ModuleInfoStream::Module> modules = module_info_stream.GetModules();
	m_object_file_paths.reserve(modules.GetLength());
	for (const auto& module : modules)
	{
		s_object_file_path path;
		path.object_file_path.set(module.GetName().Decay());
		path.library_file_path.set(module.GetObjectName().Decay());
		m_object_file_paths.push_back(path);
	}

	return true;
}

const std::vector<s_object_file_path>& c_pdb_file::get_object_file_paths() const
{
	return m_object_file_paths;
}

const std::vector<s_section_contribution>& c_pdb_file::get_section_contributions() const
{
	return m_section_contributions;
}

std::vector<s_section_contribution>& c_pdb_file::get_section_contributions()
{
	return m_section_contributions;
}

const std::vector<s_public_symbol>& c_pdb_file::get_public_symbols() const
{
	return m_public_symbols;
}

c_pdb_file::c_pdb_file(
	PDB::RawFile& raw_pdb) :
	m_raw_pdb(raw_pdb)
{
	bool parse_result = parse();
	assert(parse_result);
}

c_pdb_file::~c_pdb_file()
{

}
