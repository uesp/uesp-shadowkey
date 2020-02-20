// ParseModels.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdarg.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include "d:\\src\uesp\\EsoApps\\common\\devil\\include\\IL\\il.h"
#include "d:\\src\uesp\\EsoApps\\common\\devil\\include\\IL\\ilu.h"
#include "d:\\src\uesp\\EsoApps\\common\\devil\\include\\IL\\ilut.h"


typedef uint32_t dword;
typedef uint16_t word;


#define MODELSINDEX "c:\\downloads\\Shadowkey\\Shadowkey Release\\system\\apps\\6r51\\models.idx"
#define MODELSHUGE  "c:\\downloads\\Shadowkey\\Shadowkey Release\\system\\apps\\6r51\\models.huge"
#define MODELSTXT   "c:\\downloads\\Shadowkey\\Shadowkey Release\\system\\apps\\6r51\\models.txt"
#define MODELOUTPUTPATH "c:\\downloads\\Shadowkey\\models\\"

struct modelindex_t 
{
	dword size;
	dword offset;
};


struct modeldata_t
{
	dword index;
	dword fileOffset;

	dword size1;
	dword size2;
	dword size3;
	std::string name;

	dword dataSize;
	byte* pData;
};


std::vector<modelindex_t> g_ModelIndex;
std::vector<modeldata_t> g_Models;


bool ReportError(const char* pMsg, ...)
{
	va_list Args;

	va_start(Args, pMsg);
	vprintf(pMsg, Args);
	printf("\n");
	va_end(Args);

	return false;
}


// trim from start
static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}


// trim from end
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}


// trim from both ends
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}


word WordSwap(const word s)
{
	unsigned char b1, b2;

	b1 = s & 255;
	b2 = (s >> 8) & 255;

	return (b1 << 8) + b2;
}


dword DwordSwap(const dword i)
{
	unsigned char b1, b2, b3, b4;

	b1 = i & 255;
	b2 = (i >> 8) & 255;
	b3 = (i >> 16) & 255;
	b4 = (i >> 24) & 255;

	return ((dword)b1 << 24) + ((dword)b2 << 16) + ((dword)b3 << 8) + b4;
}


bool ReadDword(FILE* pFile, dword& Output, const bool LittleEndian = true)
{
	if (pFile == nullptr) return ReportError("Error: No file defined to read from!");
	size_t ReadBytes = fread(&Output, 1, sizeof(dword), pFile);
	if (ReadBytes != sizeof(dword)) return ReportError("Error: Only read %u of %u bytes from position 0x%X!", ReadBytes, sizeof(dword), ftell(pFile));
	if (!LittleEndian) Output = DwordSwap(Output);
	return true;
}



bool ReadWord(FILE* pFile, word&  Output, const bool LittleEndian = true)
{
	if (pFile == nullptr) return ReportError("Error: No file defined to read from!");
	size_t ReadBytes = fread(&Output, 1, sizeof(word), pFile);
	if (ReadBytes != sizeof(word)) return ReportError("Error: Only read %u of %u bytes from position 0x%X!", ReadBytes, sizeof(word), ftell(pFile));
	if (!LittleEndian) Output = WordSwap(Output);
	return true;
}


bool LoadModelIndex()
{
	FILE* pFile = fopen(MODELSINDEX, "rb");
	dword numModels = 0;

	if (pFile == nullptr) return ReportError("Error: Failed to open models index file '%s'!", MODELSINDEX);
	
	if (!ReadDword(pFile, numModels)) return ReportError("Error: Failed to read number of models from index file!");

	g_ModelIndex.clear();
	g_ModelIndex.reserve(numModels);

	for (dword i = 0; i < numModels; ++i) 
	{
		modelindex_t value;

		if (!ReadDword(pFile, value.offset)) return ReportError("Error: Failed to read number model offset from index file at offset 0x%08X!", ftell(pFile));
		if (!ReadDword(pFile, value.size)) return ReportError("Error: Failed to read number model size from index file at offset 0x%08X!", ftell(pFile));

		g_ModelIndex.push_back(value);
	}

	printf("Found %d models in index file.\n", g_ModelIndex.size());
	
	fclose(pFile);
	return true;
}


bool LoadModelData(FILE* pFile, modelindex_t Index, const dword modelIndex)
{
	modeldata_t Model;

	Model.dataSize = Index.size;
	Model.fileOffset = Index.offset;
	Model.pData = new byte[Model.dataSize + 256];
	Model.index = modelIndex;

	if (Model.dataSize == 0) 
	{
		g_Models.push_back(Model);
		return true;
	}

	int seekErr = fseek(pFile, Index.offset, SEEK_SET);
	if (seekErr != 0) return ReportError("%d) Error: Failed to seek to model data at offset 0x%08X!", modelIndex, Index.offset);

	size_t bytesRead = fread(Model.pData, 1, Model.dataSize, pFile);
	if (bytesRead != Model.dataSize) return ReportError("%d) Error: Only read %d of %d bytes from model at offset 0x%08X!", modelIndex, bytesRead, Model.dataSize, Index.offset);

	g_Models.push_back(Model);

	return true;
}


bool SaveRawModel(modeldata_t& Model)
{
	char buffer[256];
	snprintf(buffer, 255, "%s%d.dat", MODELOUTPUTPATH, Model.index);

	FILE* pFile = fopen(buffer, "wb");
	if (pFile == nullptr) return ReportError("Error: Failed to open file '%s' for output!", buffer);

	size_t bytesWritten = fwrite(Model.pData, 1, Model.dataSize, pFile);
	fclose(pFile);

	if (bytesWritten != Model.dataSize) return ReportError("Error: Only wrote %d of %d bytes for model %d!", bytesWritten, Model.dataSize, Model.index);

	snprintf(buffer, 255, "%s%s", MODELOUTPUTPATH, Model.name.c_str());

	pFile = fopen(buffer, "wb");
	if (pFile == nullptr) return ReportError("Error: Failed to open file '%s' for output!", buffer);

	bytesWritten = fwrite(Model.pData, 1, Model.dataSize, pFile);
	fclose(pFile);

	if (bytesWritten != Model.dataSize) return ReportError("Error: Only wrote %d of %d bytes for model %d!", bytesWritten, Model.dataSize, Model.index);
	return true;
}


bool SaveRawModels()
{
	int successCount = 0;

	for (auto &model : g_Models) 
	{
		if (SaveRawModel(model)) ++successCount;
	}

	printf("Successfully saved %d of %d model data.\n", successCount, g_Models.size());

	return true;
}


bool LoadModelData()
{
	int successCount = 0;

	FILE* pFile = fopen(MODELSHUGE, "rb");
	if (pFile == nullptr) return ReportError("Error: Failed to open models data file '%s'!", MODELSHUGE);

	g_Models.clear();
	g_Models.reserve(g_ModelIndex.size());

	for (size_t i = 0; i < g_ModelIndex.size(); ++i)
	{
		if (LoadModelData(pFile, g_ModelIndex[i], i)) ++successCount;
	}

	fclose(pFile);

	printf("Successfully loaded %d of %d models!\n", successCount, g_ModelIndex.size());
	return true;
}


bool LoadModelText()
{
	char inputLine[256];
	int errorCount = 0;
	int lineCount = 0;

	FILE* pFile = fopen(MODELSTXT, "rb");
	if (pFile == nullptr) return ReportError("Error: Failed to open models text file '%s'!", MODELSTXT);

	while (!feof(pFile))
	{
		char* pError = fgets(inputLine, 255, pFile);
		++lineCount;

		if (pError == nullptr)
		{
			if (feof(pFile)) break;
			ReportError("Error: Failed reading line %d in models txt file!", lineCount);
			++errorCount;
			break;
		}

		char* pToken = strtok(inputLine, " ");
		std::vector<char*> Tokens;

		while (pToken) 
		{
			Tokens.push_back(pToken);
			pToken = strtok(nullptr, " ");
		}

		if (Tokens.size() < 5) 
		{
			ReportError("Error: Only found %d tokens on line %d in models.txt file!", Tokens.size(), lineCount);
			++errorCount;
			continue;
		}
		else if (Tokens.size() > 5)
		{
			ReportError("Warning: Found %d tokens on line %d in models.txt file!", Tokens.size(), lineCount);
			++errorCount;
		}

		dword index = strtoul(Tokens[0], nullptr, 10);
		dword count = strtoul(Tokens[1], nullptr, 10);
		dword size1 = strtoul(Tokens[2], nullptr, 10);
		dword size2 = strtoul(Tokens[3], nullptr, 10);
		char* pFilename = Tokens[4];

		if (index < g_Models.size())
		{
			modeldata_t& model = g_Models[index];
			model.size1 = count;
			model.size2 = size1;
			model.size3 = size2;
			model.name = trim(std::string(pFilename));
		}
		else
		{
			ReportError("Error: Found invalid model index %u on line %d!", index, lineCount);
			++errorCount;
		}
	}

	fclose(pFile);

	printf("Successfully loaded %d of %d lines from model txt file!\n", lineCount - errorCount, lineCount);

	return true;
}


int main()
{
	ilInit();
	iluInit();
	ilEnable(IL_FILE_OVERWRITE);
	
	LoadModelIndex();
	LoadModelData();
	LoadModelText();

	SaveRawModels();

    return 0;
}

