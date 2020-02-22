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


const size_t MODEL_REC1_SIZE = 6;
const size_t MODEL_REC2_SIZE = 4;
const size_t MODEL_REC3_SIZE = 12;
const size_t MODEL_REC4_SIZE = 4;
const size_t MODEL_REC5_SIZE = 8;


struct modelindex_t 
{
	dword size;
	dword offset;
};


struct color_t {
	byte r;
	byte g;
	byte b;
};


struct coor_t {
	word x;
	word y;
	word z;
};


struct uvcoor_t {
	word u;
	word v;
};


struct face_t {
	word v[6];
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
	
	word textureCount;
	word textureWidth;
	word textureHeight;

	std::vector<coor_t> vertexes;
	std::vector<uvcoor_t> uvcoors;
	std::vector<face_t> faces;
	std::vector<byte* > textures;

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


bool ParseDword(const byte* pData, dword& Output, const bool LittleEndian = true)
{
	memcpy((void *)&Output, pData, sizeof(dword));
	
	if (!LittleEndian) Output = DwordSwap(Output);
	return true;
}


bool ParseWord(const byte* pData, word&  Output, const bool LittleEndian = true)
{
	memcpy((void *)&Output, pData, sizeof(word));
	
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


bool ParseModelData(modeldata_t& model)
{
	word header[7];
	dword sectionSizes[7];
	byte* pData = model.pData;
	size_t Offset = 0;

	if (model.dataSize == 0) return true;
	if (model.dataSize < 7 * 2) return ReportError("Error: Model %d data too small to parse 14 bytes of header data!", model.index);

	ParseWord(pData +  0, header[0]);
	ParseWord(pData +  2, header[1]);
	ParseWord(pData +  4, header[2]);
	ParseWord(pData +  6, header[3]);
	ParseWord(pData +  8, header[4]);
	ParseWord(pData + 10, header[5]);
	ParseWord(pData + 12, header[6]);
	Offset += 14;

	sectionSizes[0] = 0;
	sectionSizes[1] = header[1];
	sectionSizes[2] = header[2] * 6;
	sectionSizes[3] = header[3] * 4;
	sectionSizes[4] = header[4] * 12;
	sectionSizes[5] = 2;
	sectionSizes[6] = header[6];

	dword section2Size = (dword) sectionSizes[1] * (dword) sectionSizes[2];
	if (Offset + section2Size > model.dataSize) return ReportError("Error: Model %d data overrun in vertex data (Offset 0x%08X, Size 0x%08X!", model.index, Offset, section2Size);

	word numVertexes = section2Size / 6;
	model.vertexes.reserve(numVertexes);

	for (size_t i = 0; i < numVertexes; ++i)
	{
		coor_t coor;

		ParseWord(pData + Offset + i * 6 + 0, coor.x);
		ParseWord(pData + Offset + i * 6 + 2, coor.y);
		ParseWord(pData + Offset + i * 6 + 4, coor.z);

		model.vertexes.push_back(coor);
	}

	Offset += section2Size;

	if (Offset + sectionSizes[3] > model.dataSize) return ReportError("Error: Model %d data overrun in UV data (Offset 0x%08X, Size 0x%08X!", model.index, Offset, sectionSizes[3]);

	word numUVCoors = sectionSizes[3] / 4;
	model.uvcoors.reserve(numVertexes);

	for (size_t i = 0; i < numUVCoors; ++i)
	{
		uvcoor_t coor;

		ParseWord(pData + Offset + i * 4 + 0, coor.u);
		ParseWord(pData + Offset + i * 4 + 2, coor.v);

		model.uvcoors.push_back(coor);
	}

	Offset += sectionSizes[3];

	if (Offset + sectionSizes[4] > model.dataSize) return ReportError("Error: Model %d data overrun in face data (Offset 0x%08X, Size 0x%08X!", model.index, Offset, sectionSizes[4]);

	word numFaces = sectionSizes[4] / 12;
	model.faces.reserve(numFaces);

	for (size_t i = 0; i < numFaces; ++i)
	{
		face_t face;

		ParseWord(pData + Offset + i * 12 + 0, face.v[0]);
		ParseWord(pData + Offset + i * 12 + 0, face.v[1]);
		ParseWord(pData + Offset + i * 12 + 0, face.v[2]);
		ParseWord(pData + Offset + i * 12 + 0, face.v[3]);
		ParseWord(pData + Offset + i * 12 + 0, face.v[4]);
		ParseWord(pData + Offset + i * 12 + 0, face.v[5]);

		model.faces.push_back(face);
	}

	Offset += sectionSizes[4];
	
	if (Offset + 6 > model.dataSize) return ReportError("Error: Model %d data overrun in texture header (Offset 0x%08X, Size 0x%08X!", model.index, Offset, 6);
	word section5Header[3];
	ParseWord(pData + Offset + 0, section5Header[0]);
	ParseWord(pData + Offset + 2, section5Header[1]);
	ParseWord(pData + Offset + 4, section5Header[2]);
	Offset += 6;

	model.textureCount  = section5Header[0];
	model.textureWidth  = section5Header[1];
	model.textureHeight = section5Header[2];

	sectionSizes[5] *= (dword) section5Header[0] * (dword) section5Header[1] * (dword) section5Header[2];
	if (Offset + sectionSizes[5] > model.dataSize) return ReportError("Error: Model %d data overrun in texture data (Offset 0x%08X, Size 0x%08X!", model.index, Offset, sectionSizes[5]);

	for (size_t i = 0; i < model.textureCount; ++i)
	{
		byte* pTexture = pData + Offset + 2 * model.textureWidth * model.textureHeight * i;
		model.textures.push_back(pTexture);
	}

	Offset += sectionSizes[5];

	if (sectionSizes[6] != 1) ReportError("Warning: Model %d data section 6 size is %d.", model.index, sectionSizes[6]);

	if (Offset + 2 > model.dataSize) return ReportError("Error: Model %d data overrun in footer header (Offset 0x%08X, Size 0x%08X!", model.index, Offset, 2);
	word section6count;
	ParseWord(pData + Offset, section6count);
	Offset += 2;

	dword section6Size = (dword) section6count * 6;
	if (Offset + section6Size > model.dataSize) return ReportError("Error: Model %d data overrun in footer data (Offset 0x%08X, Size 0x%08X!", model.index, Offset, section6Size);
	Offset += section6Size;

	if (Offset != model.dataSize) 
	{
		int leftOverBytes = model.dataSize - Offset;
		ReportError("Warning: Model %d has 0x%08X bytes left over in data buffer!", model.index, leftOverBytes);
	}
	
	return true;
}


bool ParseModelData()
{

	for (auto& model : g_Models)
	{
		ParseModelData(model);
	}

	return true;
}


bool CheckUVCoors() 
{
	int minUValue = 65536;
	int maxUValue = 0;
	int minVValue = 65536;
	int maxVValue = 0;
	int count = 0;

	std::unordered_map<word, uvcoor_t> uValues;
	std::unordered_map<word, uvcoor_t> vValues;

	for (auto& model : g_Models)
	{
		if (uValues.find(model.textureWidth)  == uValues.end()) uValues[model.textureWidth]  = { 65535, 0 };
		if (vValues.find(model.textureHeight) == vValues.end()) vValues[model.textureHeight] = { 65535, 0 };

		uvcoor_t& uVal = uValues[model.textureWidth];
		uvcoor_t& vVal = vValues[model.textureHeight];
			
		for (auto& uv : model.uvcoors)
		{
			++count;

			if (minUValue > uv.u) minUValue = uv.u;
			if (minVValue > uv.v) minVValue = uv.v;
			if (maxUValue < uv.u) maxUValue = uv.u;
			if (maxVValue < uv.v) maxVValue = uv.v;

			if (uVal.u > uv.u) uVal.u = uv.u;
			if (uVal.v < uv.u) uVal.v = uv.u;
			if (vVal.u > uv.v) vVal.u = uv.v;
			if (vVal.v < uv.v) vVal.v = uv.v;
		}
	}

	printf("Found %d UV coordinates.\n", count);
	printf("U Coordinates = %d to %d\n", minUValue, maxUValue);
	printf("V Coordinates = %d to %d\n", minVValue, maxVValue);

	printf("U Coordinates (%d):\n", uValues.size());

	for (auto& value : uValues)
	{
		word width = value.first;
		uvcoor_t uv = value.second;
		printf("\t%d = %d to %d\n", width, uv.u, uv.v);
	}
	
	printf("V Coordinates (%d):\n", vValues.size());

	for (auto& value : vValues)
	{
		word width = value.first;
		uvcoor_t uv = value.second;
		printf("\t%d = %d to %d\n", width, uv.u, uv.v);
	}

	return true;
}


void TranslateColorWord(byte& r, byte& g, byte& b, const byte* pTexture)
{
	r = ((pTexture[1] & 0x0f) << 4) + (pTexture[1] & 0x0f);
	g = ((pTexture[0] & 0xf0) >> 4) + (pTexture[0] & 0xf0);
	b = ((pTexture[0] & 0x0f) << 4) + (pTexture[0] & 0x0f);
}


bool ExportTexture(const byte* pTexture, const dword width, const dword height, const dword modelIndex, const dword textureIndex, const std::string name)
{
	if (height == 0 || width == 0) return true;

	char OutputFilename[256];
	//snprintf(OutputFilename, 250, "%s%d-%d.png", MODELOUTPUTPATH, modelIndex, textureIndex);
	snprintf(OutputFilename, 250, "%s%s-%d.png", MODELOUTPUTPATH, name.c_str(), textureIndex);

	byte* pOutputImage = new byte[height * width * 4 + 1024];
	byte r, g, b;
	size_t imageOffset = 0;

	for (size_t y = 0; y < height; ++y)
	{
		for (size_t x = 0; x < width; ++x)
		{
			byte alpha = 0xff;

			TranslateColorWord(r, g, b, pTexture);

			if (r == 0xff && g == 0 && b == 0xff) alpha = 0;

			imageOffset = (height - y - 1) * width * 4 + x * 4;
			//imageOffset = y * width * 4 + x * 4;

			pOutputImage[imageOffset + 0] = r;
			pOutputImage[imageOffset + 1] = g;
			pOutputImage[imageOffset + 2] = b;
			pOutputImage[imageOffset + 3] = alpha;

			pTexture += 2;
		}
	}

	ilTexImage(width, height, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, pOutputImage);
	ilSave(IL_PNG, OutputFilename);
	
	delete[] pOutputImage;
	return true;
}


bool ExportTextures()
{
	
	for (auto& model : g_Models)
	{
		dword textureIndex = 0;

		for (auto& texture : model.textures)
		{
			ExportTexture(texture, model.textureWidth, model.textureHeight, model.index, textureIndex, model.name);
			++textureIndex;
		}
	}

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

	//SaveRawModels();

	ParseModelData();
	CheckUVCoors();
	ExportTextures();

    return 0;
}

