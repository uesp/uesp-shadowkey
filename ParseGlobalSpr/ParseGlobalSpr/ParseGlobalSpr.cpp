// ParseGlobalSpr.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdarg.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include "d:\\src\uesp\\EsoApps\\common\\devil\\include\\IL\\il.h"
#include "d:\\src\uesp\\EsoApps\\common\\devil\\include\\IL\\ilu.h"
#include "d:\\src\uesp\\EsoApps\\common\\devil\\include\\IL\\ilut.h"

#define GLOBALSPR "c:\\Downloads\\Shadowkey\\global.spr"
#define OUTPUTPATH "c:\\Downloads\\Shadowkey\\images\\"

#define REFIMAGE1 "c:\\Downloads\\Shadowkey\\SK-narrative-COH1-2a.png"
#define REFIMAGE2 "c:\\Downloads\\Shadowkey\\images\\222a.png"
#define REFINDEX 222
#define OUTPUTPALETTE "c:\\Downloads\\Shadowkey\\global1.pal"
#define GLOBALPALETTE "c:\\Downloads\\Shadowkey\\global.pal"
#define TRANSPARENTINDEX 125

const int bytesPerPixel = 3; /// red, green, blue
const int fileHeaderSize = 14;
const int infoHeaderSize = 40;


typedef uint32_t dword;
typedef uint16_t word;

byte* pGlobalPal = nullptr;


bool ReportError(const char* pMsg, ...)
{
	va_list Args;

	va_start(Args, pMsg);
	vprintf(pMsg, Args);
	printf("\n");
	va_end(Args);

	return false;
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


bool ReadDword(FILE* pFile, dword& Output, const bool LittleEndian)
{
	if (pFile == nullptr) return ReportError("Error: No file defined to read from!");
	size_t ReadBytes = fread(&Output, 1, sizeof(dword), pFile);
	if (ReadBytes != sizeof(dword)) return ReportError("Error: Only read %u of %u bytes from position 0x%X!", ReadBytes, sizeof(dword), ftell(pFile));
	if (!LittleEndian) Output = DwordSwap(Output);
	return true;
}



bool ReadWord(FILE* pFile, word&  Output, const bool LittleEndian)
{
	if (pFile == nullptr) return ReportError("Error: No file defined to read from!");
	size_t ReadBytes = fread(&Output, 1, sizeof(word), pFile);
	if (ReadBytes != sizeof(word)) return ReportError("Error: Only read %u of %u bytes from position 0x%X!", ReadBytes, sizeof(word), ftell(pFile));
	if (!LittleEndian) Output = WordSwap(Output);
	return true;
}


dword TranslateColorIndex(byte& r, byte& g, byte& b, byte index, byte* pPalettte = nullptr)
{
	if (pPalettte == nullptr) pPalettte = pGlobalPal;
	if (pPalettte == nullptr) return -1;

	r = pPalettte[index * 3 + 0];
	g = pPalettte[index * 3 + 1];
	b = pPalettte[index * 3 + 2];

	return (dword)r + ((dword)g << 8) + ((dword)b << 16);
}


unsigned char* createBitmapFileHeader(int height, int width, int paddingSize) {
	int fileSize = fileHeaderSize + infoHeaderSize + (bytesPerPixel*width + paddingSize) * height;

	static unsigned char fileHeader[] = {
		0,0, /// signature
		0,0,0,0, /// image file size in bytes
		0,0,0,0, /// reserved
		0,0,0,0, /// start of pixel array
	};

	fileHeader[0] = (unsigned char)('B');
	fileHeader[1] = (unsigned char)('M');
	fileHeader[2] = (unsigned char)(fileSize);
	fileHeader[3] = (unsigned char)(fileSize >> 8);
	fileHeader[4] = (unsigned char)(fileSize >> 16);
	fileHeader[5] = (unsigned char)(fileSize >> 24);
	fileHeader[10] = (unsigned char)(fileHeaderSize + infoHeaderSize);

	return fileHeader;
}


unsigned char* createBitmapInfoHeader(int height, int width) {
	static unsigned char infoHeader[] = {
		0,0,0,0, /// header size
		0,0,0,0, /// image width
		0,0,0,0, /// image height
		0,0, /// number of color planes
		0,0, /// bits per pixel
		0,0,0,0, /// compression
		0,0,0,0, /// image size
		0,0,0,0, /// horizontal resolution
		0,0,0,0, /// vertical resolution
		0,0,0,0, /// colors in color table
		0,0,0,0, /// important color count
	};

	infoHeader[0] = (unsigned char)(infoHeaderSize);
	infoHeader[4] = (unsigned char)(width);
	infoHeader[5] = (unsigned char)(width >> 8);
	infoHeader[6] = (unsigned char)(width >> 16);
	infoHeader[7] = (unsigned char)(width >> 24);
	infoHeader[8] = (unsigned char)(height);
	infoHeader[9] = (unsigned char)(height >> 8);
	infoHeader[10] = (unsigned char)(height >> 16);
	infoHeader[11] = (unsigned char)(height >> 24);
	infoHeader[12] = (unsigned char)(1);
	infoHeader[14] = (unsigned char)(bytesPerPixel * 8);

	return infoHeader;
}


void generateBitmapImage(unsigned char *image, int width, int height, char* imageFileName) {

	unsigned char padding[3] = { 0, 0, 0 };
	int paddingSize = (4 - (width*bytesPerPixel) % 4) % 4;

	unsigned char* fileHeader = createBitmapFileHeader(height, width, paddingSize);
	unsigned char* infoHeader = createBitmapInfoHeader(height, width);

	FILE* imageFile = fopen(imageFileName, "wb");

	fwrite(fileHeader, 1, fileHeaderSize, imageFile);
	fwrite(infoHeader, 1, infoHeaderSize, imageFile);

	int i;
	for (i = 0; i<height; i++) {
		fwrite(image + (i*width*bytesPerPixel), bytesPerPixel, width, imageFile);
		fwrite(padding, 1, paddingSize, imageFile);
	}

	fclose(imageFile);
}


bool ConvertRawPalette(byte* pRawPal, byte* pOutputPal) 
{

	for (size_t i = 0; i < 256; ++i) 
	{
		byte b = ((pRawPal[i * 2] & 0x0f) << 4) + (pRawPal[i * 2] & 0x0f);
		byte g = ((pRawPal[i * 2] & 0xf0) >> 4) + (pRawPal[i * 2] & 0xf0);
		byte r = ((pRawPal[i*2 + 1] & 0x0f) << 4) + (pRawPal[i * 2 + 1] & 0x0f);

		pOutputPal[i * 3 + 0] = r;
		pOutputPal[i * 3 + 1] = g;
		pOutputPal[i * 3 + 2] = b;
	}

	return true;
}


bool ReadImage(FILE* pFile)
{
	static int imageIndex = 0;
	FILE* pOutputFile;
	char OutputFilename[256];
	unsigned char ReadBytes[4096];
	size_t OutputImageOffset = 0;
	size_t OutputAlphaImageOffset = 0;
	int imageReadSize = 0;
	int imageY = 0;
	unsigned char* pOutputImage = nullptr;
	unsigned char* pOutputAlphaImage = nullptr;
	word imageWidth = 0;
	word imageHeight = 0;
	byte RawPalette[0x200 + 100];
	byte Palette[768 + 100];

	++imageIndex;

	long curPos = ftell(pFile);
	printf("%d) Starting image at 0x%08X\n", imageIndex, curPos);

	snprintf(OutputFilename, 250, "%s%d.raw", OUTPUTPATH, imageIndex);
	pOutputFile = fopen(OutputFilename, "wb");
	if (pOutputFile == nullptr) return ReportError("Error: Failed to open image file for output!");
	
	if (!ReadWord(pFile, imageWidth, true)) return false;
	if (!ReadWord(pFile, imageHeight, true)) return false;
	imageReadSize += 4;

	printf("\tFound image %d x %d\n", (int) imageWidth, (int) imageHeight);

	int palBytesRead = fread(RawPalette, 1, 0x200, pFile);
	if (palBytesRead != 0x200)  return ReportError("\tError: Failed to read image palette data at 0x%08X!", ftell(pFile));

	//int seekResult = fseek(pFile, 0x200, SEEK_CUR);
	//if (seekResult != 0) return ReportError("\tError: Failed to seek past image header at 0x%08X!", ftell(pFile));

	ConvertRawPalette(RawPalette, Palette);
	imageReadSize += 0x200;
	
	pOutputImage = new unsigned char[imageHeight * imageWidth * bytesPerPixel + 1024];
	pOutputAlphaImage = new unsigned char[imageHeight * imageWidth * 4 + 1024];

	while (!feof(pFile) && imageY < imageHeight)
	{
		++imageY;

		//printf("\t%d) Reading line at 0x%08X\n", imageY, ftell(pFile));

		word zeroSize = 0;
		word lineSize = 0;

		if (!ReadWord(pFile, zeroSize, true)) return false;
		if (!ReadWord(pFile, lineSize, true)) return false;

		imageReadSize += 4;

		int readSize = lineSize - zeroSize;

		//printf("\t\tZeros = 0x%X, LineSize = 0x%X, ReadSize = 0x%X\n", zeroSize, lineSize, readSize);
				
		int bytesRead = fread(ReadBytes, 1, readSize, pFile);
		imageReadSize += bytesRead;
		if (bytesRead != readSize) return ReportError("\tError: Only read %d of %d bytes at at 0x%08X!", bytesRead, lineSize, ftell(pFile));

			/* Flip image vertically */
		OutputImageOffset = (imageHeight - imageY) * imageWidth * bytesPerPixel;
		OutputAlphaImageOffset = (imageHeight - imageY) * imageWidth * 4;

		for (int i = 0; i < imageWidth; ++i) {
			pOutputImage[OutputImageOffset + i*bytesPerPixel] = 0xff;
			pOutputImage[OutputImageOffset + i*bytesPerPixel + 1] = 0;
			pOutputImage[OutputImageOffset + i*bytesPerPixel + 2] = 0xff;

			pOutputAlphaImage[OutputAlphaImageOffset + i*4] = 0;
			pOutputAlphaImage[OutputAlphaImageOffset + i*4 + 1] = 0;
			pOutputAlphaImage[OutputAlphaImageOffset + i*4 + 2] = 0;
			pOutputAlphaImage[OutputAlphaImageOffset + i*4 + 3] = 0;
		}

		size_t startOffset = 0;
		size_t startAlphaOffset = 0;

		if (zeroSize > 0) 
		{
			startOffset = zeroSize * bytesPerPixel;
			startAlphaOffset = zeroSize * 4;
		}
		
		for (int i = 0; i < readSize; ++i) {
			byte r, g, b;
			byte alpha = 0xff;
			

			if (TranslateColorIndex(r, g, b, ReadBytes[i], Palette) >= 0)
			{
				if (r == 0xff && g == 0 && b == 0xff) alpha = 0;

				pOutputImage[OutputImageOffset + startOffset + i*bytesPerPixel + 0] = b;
				pOutputImage[OutputImageOffset + startOffset + i*bytesPerPixel + 1] = g;
				pOutputImage[OutputImageOffset + startOffset + i*bytesPerPixel + 2] = r;

				
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4] = r;
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4 + 1] = g;
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4 + 2] = b;
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4 + 3] = alpha;
			}
			else
			{
				if (ReadBytes[i] == TRANSPARENTINDEX) alpha = 0;

				pOutputImage[OutputImageOffset + startOffset + i*bytesPerPixel] = ReadBytes[i];
				pOutputImage[OutputImageOffset + startOffset + i*bytesPerPixel + 1] = ReadBytes[i];
				pOutputImage[OutputImageOffset + startOffset + i*bytesPerPixel + 2] = ReadBytes[i];

				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4] = ReadBytes[i];
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4 + 1] = ReadBytes[i];
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4 + 2] = ReadBytes[i];
				pOutputAlphaImage[OutputAlphaImageOffset + startAlphaOffset + i*4 + 3] = alpha;
			}
		}

		//OutputImageOffset += imageWidth * bytesPerPixel;

		fwrite(ReadBytes, 1, readSize, pOutputFile);

		//seekResult = fseek(pFile, readSize, SEEK_CUR);
		//if (seekResult != 0) return ReportError("\tError: Failed to seek past image line, 0x%X bytes at 0x%08X!", lineSize, ftell(pFile));
	}	
	
	fclose(pOutputFile);
	
	ilTexImage(imageWidth, imageHeight, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, pOutputAlphaImage);
	snprintf(OutputFilename, 250, "%s%d.png", OUTPUTPATH, imageIndex);
	ilSave(IL_PNG, OutputFilename);
	
	snprintf(OutputFilename, 250, "%s%d.bmp", OUTPUTPATH, imageIndex);
	generateBitmapImage(pOutputImage, imageWidth, imageHeight, OutputFilename);
	delete[] pOutputImage;
	delete[] pOutputAlphaImage;

	curPos = ftell(pFile);
	printf("\tFinished image at 0x%08X (read %d bytes)\n", curPos, imageReadSize);

	return true;
}


bool TestParse()
{
	FILE* pFile;

	pFile = fopen(GLOBALSPR, "rb");
	if (pFile == nullptr) return ReportError("Failed to open file!");

	int seekResult = fseek(pFile, 0, SEEK_END);
	if (seekResult != 0) return ReportError("Failed to seek to EOF!");
	long fileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	seekResult = fseek(pFile, 0x600, SEEK_SET);
	if (seekResult != 0) return ReportError("Failed to seek past file header!");

	while (ReadImage(pFile))
	{
	}

	long curPos = ftell(pFile);
	long leftOverBytes = fileSize - curPos;

	printf("Stopped reading images at file position 0x%08X (0x%X bytes left over).\n", curPos, leftOverBytes);

	fclose(pFile);

	return true;
}


bool CreatePalette()
{
	ILuint ImageName1;
	ILuint ImageName2;

	ilGenImages(1, &ImageName1);
	ilGenImages(1, &ImageName2);

	ilBindImage(ImageName1);
	auto loadResult1 = ilLoadImage(REFIMAGE1);

	ILuint width1, height1;
	width1 = ilGetInteger(IL_IMAGE_WIDTH);
	height1 = ilGetInteger(IL_IMAGE_HEIGHT);
	ILubyte * pImageData1 = ilGetData();

	ilBindImage(ImageName2);
	auto loadResult2 = ilLoadImage(REFIMAGE2);

	ILuint width2, height2;
	width2 = ilGetInteger(IL_IMAGE_WIDTH);
	height2 = ilGetInteger(IL_IMAGE_HEIGHT);
	ILubyte * pImageData2 = ilGetData();

	if (!loadResult1) return ReportError("Error: Failed to load image '%s'!", REFIMAGE1);
	if (!loadResult2) return ReportError("Error: Failed to load image '%s'!", REFIMAGE2);

	ilBindImage(ImageName1);
	//iluScale(width2, height2, 1);
	
	width1 = ilGetInteger(IL_IMAGE_WIDTH);
	height1 = ilGetInteger(IL_IMAGE_HEIGHT);
	//pImageData1 = ilGetData();

	ReportError("\tImage1 = %d x %d", width1, height1);
	ReportError("\tImage2 = %d x %d", width2, height2);

	size_t imageSize = width1 * height2;
	ILubyte* pData1 = pImageData1;
	ILubyte* pData2 = pImageData2;

	std::unordered_map<byte, dword> ColorMap;

	ReportError("Start Palette Creation..\n");
	//imageSize = 1000;

	dword x = 0;
	dword y = 0;
	dword errorCount = 0;

	for (size_t i = 0; i < imageSize; ++i) {
		byte grayScale = *pData2;
		byte r = pData1[0];
		byte g = pData1[1];
		byte b = pData1[2];
		dword color = (dword)r + ((dword)g << 8) + ((dword)b << 16);

		//ReportError("\t(%d, %d) = #%d, (%d, %d, %d)", x, y, grayScale, r, g, b);

		if (ColorMap.find(grayScale) != ColorMap.end()) {
			dword origColor = ColorMap[grayScale];

			if (origColor != color) {
				ReportError("\tColor #%d Mismatch: 0x%06X != 0x%06X", grayScale, origColor, color);
				++errorCount;
			}
			
		}
		else {
			ColorMap[grayScale] = color;
			ReportError("\tColor #%d = 0x%06X", grayScale, color);
		}

		pData1 += 3;
		pData2 += 3;

		x++;

		if (x >= width1) {
			x = 0;
			y++;
		}
	}

	size_t count = ColorMap.size();
	ReportError("Found %d color indexes with %d mismatches!", count, errorCount);

	FILE* pFile = fopen(OUTPUTPALETTE, "wb");

	for (size_t i = 0; i < 256; ++i) 
	{
		dword color = 0;

		if (ColorMap.find((byte) i) != ColorMap.end()) color = ColorMap[(byte)i];

		fwrite((void *)&color, 1, 3, pFile);
	}

	fclose(pFile);

	return true;
}


bool LoadGlobalPalette()
{
	FILE* pFile = fopen(GLOBALPALETTE, "rb");
	if (pFile == nullptr) return ReportError("Error: Failed to load global palette!");

	pGlobalPal = new byte[768 + 16];

	size_t bytesRead = fread(pGlobalPal, 1, 768, pFile);

	fclose(pFile);

	if (bytesRead != 768) return ReportError("Error: Only read %u of 768 bytes from global palette!", bytesRead);

	return true;
}


int main()
{
	ilInit();
	iluInit();
	ilEnable(IL_FILE_OVERWRITE);

	LoadGlobalPalette();

	TestParse();
	//CreatePalette();

    return 0;
}

