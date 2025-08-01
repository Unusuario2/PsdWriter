//============ ------------------------------------------------- ===============//
//
// Purpose: Converts .png, .jpg, etc.. to .psd for vtex compile
//
// $NoKeywords: $
//=============================================================================//
#include <io.h>
#include <cstddef>
#include <vector>
#include <windows.h>
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tools_minidump.h"
#include "loadcmdline.h"
#include "cmdlib.h" 
#include "filesystem_init.h" 
#include "filesystem_tools.h" 
#include "color.h" 
#include "psdwriter.h"
#include "psdmain.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// disable annoying warning caused by xlocale(337): warning C4530: 
// C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#pragma warning(disable:4530)
#include <string>
#include <sstream>

// psd_sdk lib
#include "psd/Psd.h"
#include "psd/PsdPlatform.h"
#include "psd/PsdMallocAllocator.h"
#include "psd/PsdNativeFile.h"
#include "psd/PsdDocument.h"
#include "psd/PsdExport.h"
#include "psd/PsdExportDocument.h"


/*
Supported native bit-depths for g_rgpAssetConvertList[] formats:

Format     | 8-bit | 16-bit | 32-bit Float | Notes
---------------------------------------------------------------
.jpg       |  Yes  |   No   |      No      | Always 8-bit lossy RGB
.jpeg      |  Yes  |   No   |      No      | Same as .jpg
.png       |  Yes  |  Yes   |      No      | 8/16-bit per channel, no float
.tga       |  Yes  |   No   |      No      | 8-bit only, optional alpha
.bmp       |  Yes  |   No   |      No      | Integer format only (1, 4, 8, 24, 32-bit int)
.gif       |  Yes  |   No   |      No      | 8-bit paletted image (only firt frame)
.hdr       |   No  |   No   |     Yes      | Radiance HDR, native float (RGBE)
.pic       |  Yes  |   No   |     Yes      | Softimage PIC, supports float RGB
.ppm       |  Yes  |  Yes   |      No      | ASCII/binary PPM (RGB), 8/16-bit
.pgm       |  Yes  |  Yes   |      No      | Grayscale only, 8/16-bit

Note:
- stbi_loadf() returns float data for all formats, but only .hdr and .pic store native float.
*/


//-----------------------------------------------------------------------------
// Purpose: internal global vars, helpers for writing .psd files
//-----------------------------------------------------------------------------
int			g_iImageWidth;
int			g_iImageHeight;
int			g_iChannels;


//-----------------------------------------------------------------------------
// Purpose: Check if the number is a power of two =)
//-----------------------------------------------------------------------------
FORCEINLINE static bool IsPowerOfTwo(const int n)
{
	return (n > 0) && ((n & (n - 1)) == 0);
}


//-----------------------------------------------------------------------------
// Purpose:		Checks if the image format is suported.
//-----------------------------------------------------------------------------
static void CheckExtensionImageFileConvert(const char* pFileName)
{
	bool bTemp = false;

	for (const char* pExt : g_rgpAssetConvertList)
	{
		if (V_strstr(pFileName, pExt))
		{
			bTemp = true;
		}
	}

	if (!bTemp)
	{
		Warning("No match found for: \'%s\'\n", pFileName);
		Warning("Supported extension are:\n");

		for (const char* pExt2 : g_rgpAssetConvertList)
		{
			Warning("\t%s\n", pExt2);
		}

		exit(-1);
	}
}


//-----------------------------------------------------------------------------
// Purpose:		Generate name_(endname).txt for the .psd file  
//				e.g: foo_albedo.psd, foo_albedo.txt
//-----------------------------------------------------------------------------
static void GenerateVtexConfigFile(const char* pFileName)
{
	if (!g_bTempalteGeneration)
	{
		return;
	}

	float start = Plat_FloatTime();
	char* pPsdFile = V_strdup(pFileName);
	V_StripExtension(pPsdFile, pPsdFile, V_strlen(pPsdFile));
	Msg("Generating config file for: %s.psd... ", pPsdFile);
	delete[] pPsdFile;

	// Sanity check!
	if (gamedir[0] == '\0')
	{
		Warning("\n"
				"Template generation cannot be executed!\n"
				"Set \'-game or -vproject\' path to enable this feature!\n"
		);
		return;
	}

	bool bEndFileName = false;
	char szNameEnding[64]; // e.g: _normal, _detail, etc...
	const char* pImageFileName = V_strrchr(pFileName, '\\') + 1; // We skip the first char '\'

	if (pImageFileName)
	{
		for (const char* pEnding : g_rgpEndFileName)
		{
			if (V_strstr(pImageFileName, pEnding))
			{
				V_strcpy(szNameEnding, pEnding);
				bEndFileName = true;
				break;
			}
		}
	}
	else
	{
		// Sanity check!
		Error("\n"
			"**INTERAL ERROR**: GenerateVtexConfigFile(); pImageFileName == nullptr!\n"
			"Speak to a dev, this shouldnt happen!\n"
		);
	}

	if (!bEndFileName)
	{
		Warning("\n"
			"File: %s does not have a valid end-name.\n"
			"Template config compile settings will not be applied!\n",
			pImageFileName);
		return;
	}

	qprintf("\n");

	// The path where we expect the template config files to be game/materialsrc/template/template_endname.txt
	char szSrcTemplateConfigFile[MAX_PATH];
	V_snprintf(szSrcTemplateConfigFile, sizeof(szSrcTemplateConfigFile), "%s\\%s\\%s%s.txt", g_szGameMaterialSrcDir, TEMAPLATE_COFIG_NAME_BASE, TEMAPLATE_COFIG_NAME_BASE, szNameEnding);
	qprintf("Source template config file:    %s\n", szSrcTemplateConfigFile);

	// Setup the template config to copy.
	char* pImageFileConfig = V_strdup(pFileName);
	V_StripExtension(pImageFileConfig, pImageFileConfig, V_strlen(pImageFileConfig));
	V_snprintf(pImageFileConfig, MAX_PATH, "%s.txt", pImageFileConfig);
	qprintf("Template config .psd file :     %s\n", pImageFileConfig);

	// Sanity check!
	if (_access(szSrcTemplateConfigFile, 0))
	{
		Warning("\n"
				"Could not find a template config file in: %s\n"
				"Config file: %s\n"
				, g_szGameMaterialSrcDir, szSrcTemplateConfigFile
		);
	}
	else
	{
		// Copy the config file to the .psd dir!
		if (CopyFileA(szSrcTemplateConfigFile, pImageFileConfig, FALSE))
		{
			Msg("done(%.2fs)\n", Plat_FloatTime() - start);
		}
		else
		{
			DWORD errorCode = GetLastError();
			Warning("\n"
					"Could not generate the cofing file for the .psd file!\n"
					"Config file: %s\n"
					"CopyFileA(); Error code : % lu\n",
					errorCode, szSrcTemplateConfigFile
			);
		}
	}

	delete[] pImageFileConfig;
}


//-----------------------------------------------------------------------------
// Purpose:	Check if the psd was writen
//-----------------------------------------------------------------------------
static bool IsPsdOK(const char* pFileName)
{
	char szFile[MAX_PATH];
	V_snprintf(szFile, sizeof(szFile), "%s.psd", pFileName);

	if (_access(szFile, 0))
	{
		ColorSpewMessage(SPEW_MESSAGE, &failed, "FAILED - %s\n", szFile);
		return true;
	}
	else
	{
		ColorSpewMessage(SPEW_MESSAGE, &successful_color, "OK"); Msg(" - %s (%.2fs)\n", szFile, Plat_FloatTime() - g_fGlobalTimer);
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add metadata to the .psd file
//-----------------------------------------------------------------------------
static void SetMetaData(psd::ExportDocument* pDocument, psd::MallocAllocator allocator /*TODO: maybe fix this?*/, const char* pFileName)
{
	float start = Plat_FloatTime();
	char szPsdWriterDateBuild[64];

	Msg("Adding meta data to the .psd file... ");

	V_snprintf(szPsdWriterDateBuild, sizeof(szPsdWriterDateBuild), "Build date: %s %s", __DATE__, __TIME__);

	AddMetaData(pDocument, &allocator, "Auto - generated with psdwriter.exe", szPsdWriterDateBuild);
	AddMetaData(pDocument, &allocator, "Resolution [x, y]:", ""); // TODO, put here the res!

	if (g_bSignature)
	{
		AddMetaData(pDocument, &allocator, g_szSignature[0], g_szSignature[1]);
	}

	Msg("done(%.2fs)\n", Plat_FloatTime() - start);
}


//-----------------------------------------------------------------------------
// Purpose: Load the image
//-----------------------------------------------------------------------------
template<typename T>
static T* AllocVectorImageData(const char* pFileName)
{
	float start = Plat_FloatTime();
	char* pTemp = V_strdup(pFileName);
	T* stb_array = nullptr;

	qprintf("Allocating image vector data... %s", verbose ? "\n" : "");

	if constexpr (std::is_same<T, uint8_t>::value) 
	{
		stb_array = stbi_load(pFileName, &g_iImageWidth, &g_iImageHeight, &g_iChannels, 0);
	}
	else if constexpr (std::is_same<T, uint16_t>::value)
	{
		stb_array = stbi_load_16(pFileName, &g_iImageWidth, &g_iImageHeight, &g_iChannels, 0);

	}
	else if constexpr (std::is_same<T, float32_t>::value)
	{
		stb_array = stbi_loadf(pFileName, &g_iImageWidth, &g_iImageHeight, &g_iChannels, 0);
	}
	else 
	{
		static_assert(std::is_same<T, void>::value, "Unsupported type. Use uint8_t, uint16_t or float32_t.");
	}

	if (stb_array)
	{
		if ((g_iImageWidth < MAX_WIGHT_RES_X) || (g_iImageHeight < MAX_WIGHT_RES_Y))
		{
			Msg("Loaded image: %dx%d with %d channels.\n", g_iImageWidth, g_iImageHeight, g_iChannels);
		}
		else
		{
			Warning("Image: %s, exceeds the maximum texture resolution in source engine!!\n"
					"Maximun: x = %i, y = %i\n"
					"File: x = %i, y = %i\n", 
					pFileName, MAX_WIGHT_RES_X, MAX_WIGHT_RES_Y, g_iImageWidth, g_iImageHeight
				 );
		}
	}
	else
	{
		Error("Failed to load image: %s\n", stbi_failure_reason());
	}

	if(!IsPowerOfTwo(g_iImageWidth) || !IsPowerOfTwo(g_iImageHeight))
	{
		Warning("The file does not have a power-of-two resolution!\n"
				"This will cause an error in vtex.exe when compiling the texture!\n"
			   );
	}
	
	delete[] pTemp;

	qprintf("done(%.2fs)\n%s", Plat_FloatTime() - start, verbose ? "\n" : "");

	return stb_array;
}


//-----------------------------------------------------------------------------
// Purpose:	Free the alloc memory of the loaded image
//-----------------------------------------------------------------------------
template<typename T> 
static void DestroyAllocVectorImageData(T* stb_array)
{
	stbi_image_free(stb_array);
}


//-----------------------------------------------------------------------------
// Purpose: Given a complete RGB array returns a vector with only one image channel
//-----------------------------------------------------------------------------
template<typename T>
static std::vector<T> SeparateRGBALayersFromSTB(const T* CompleteArray, const psd::exportChannel::Enum cChannelType)
{
	const int uiChannels = g_iChannels;
	const psd::exportChannel::Enum Channels[4] = {	psd::exportChannel::RED, 
													psd::exportChannel::GREEN, 
													psd::exportChannel::BLUE,	
													psd::exportChannel::ALPHA 
												  };
	std::vector<T> pTemp;

	int ratio = 0;
	if (uiChannels == 3) 
	{
		ratio = 3;
	}
	else if (uiChannels == 4) 
	{
		ratio = 4;
	}

	int iIndex = 0; // default Red.
	for (const psd::exportChannel::Enum Temp : Channels)
	{
		if(Temp == cChannelType) { break; }
		iIndex++;
	}

	int iArraySize = g_iImageWidth * g_iImageHeight * uiChannels;
	for (int i = iIndex; i < iArraySize; i += ratio) 
	{
		pTemp.push_back(CompleteArray[i]);
	}

	return pTemp;
}


//-----------------------------------------------------------------------------
// Purpose:  Writes a 8, 16 or 32 bit .psd file
//-----------------------------------------------------------------------------
template<typename T>
static void WriteNBitsPsd(const char* pFileName, const char* pLayer1)
{
	// We should alloc here the array and load all the stuff

	float start = Plat_FloatTime();

	char pPsdFileName[MAX_PATH];
	V_StripExtension(pFileName, pPsdFileName, MAX_PATH);

	wchar_t szWideName[MAX_PATH];
	mbstowcs(szWideName, pPsdFileName, MAX_PATH);

	wchar_t szBuffer[MAX_PATH];
	swprintf(szBuffer, MAX_PATH, L"%s.psd", szWideName);

	// Load the image, later we will separe it into RGBA channels
	T* stb_array = AllocVectorImageData<T>(pFileName);

	psd::MallocAllocator allocator;
	psd::NativeFile file(&allocator);

	// try opening the file. if it fails, bail out.
	if (!file.OpenWrite(szBuffer))
	{
		if(g_bIsSingleFile)
		{
			Error("Cannot create the .psd file.\n");
		}
		else
		{
			Warning("Cannot create .psd file, skipping the file!\n");
			return;
		}
	}

	Msg("Exporting %s as .psd\n", V_strrchr(pFileName, '\\') + 1);

	psd::ExportDocument* document = nullptr;
	if constexpr (std::is_same<T, uint8_t>::value)
	{
		document = CreateExportDocument(&allocator, g_iImageWidth, g_iImageHeight, IMAGE8BITS_ID, psd::exportColorMode::RGB);
	}
	else if constexpr (std::is_same<T, uint16_t>::value)
	{
		document = CreateExportDocument(&allocator, g_iImageWidth, g_iImageHeight, IMAGE16BITS_ID, psd::exportColorMode::RGB);
	}
	else if constexpr (std::is_same<T, float32_t>::value)
	{
		document = CreateExportDocument(&allocator, g_iImageWidth, g_iImageHeight, IMAGE32BITS_ID, psd::exportColorMode::RGB);
	}
	else
	{
		static_assert(std::is_same<T, void>::value, "Unsupported type. Use uint8_t, uint16_t or float32_t.");
	}

	SetMetaData(document, allocator, pFileName);

	// when adding a layer to the document, you first need to get a new index into the layer table.
	// with a valid index, layers can be updated in parallel, in any order.
	// this also allows you to only update the layer data that has changed, which is crucial when working with large data sets.
	start = Plat_FloatTime();
	Msg("Creating \'%s\' layer... ", pLayer1);
	uint layer1 = AddLayer(document, &allocator, pLayer1);
	Msg("done(%.2fs)\n", Plat_FloatTime() - start);

	// note that each layer has its own compression type. it is perfectly legal to compress different channels of different layers with different settings.
	// RAW is pretty much just a raw data dump. fastest to write, but large.
	// RLE stores run-length encoded data which can be good for 8-bit channels, but not so much for 16-bit or 32-bit data.
	// ZIP is a good compromise between speed and size.
	// ZIP_WITH_PREDICTION first delta encodes the data, and then zips it. slowest to write, but also smallest in size for most images.
	// Note: This values need to be in sync with psd::compressionType in PsdCompressionType.h
	psd::compressionType::Enum compression = psd::compressionType::RAW;
	if		(g_uiCompressionType == 0) { compression = psd::compressionType::RAW; }
	else if (g_uiCompressionType == 1) { compression = psd::compressionType::RLE; }
	else if (g_uiCompressionType == 2) { compression = psd::compressionType::ZIP; }
	else if (g_uiCompressionType == 3) { compression = psd::compressionType::ZIP_WITH_PREDICTION; }

	// Note: We cannot pass all the 3 or 4 channels at once, we need then in separate R, G, B and A layers.
	start = Plat_FloatTime();
	Msg("Creating RGBA layers: ");	
	Msg("red, ");	UpdateLayer(document, &allocator, layer1, psd::exportChannel::RED, 0, 0, g_iImageWidth, g_iImageHeight, SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::RED).data(), compression);
	Msg("green, ");	UpdateLayer(document, &allocator, layer1, psd::exportChannel::GREEN, 0, 0, g_iImageWidth, g_iImageHeight, SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::GREEN).data(), compression);
	Msg("blue");	UpdateLayer(document, &allocator, layer1, psd::exportChannel::BLUE, 0, 0, g_iImageWidth, g_iImageHeight, SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::BLUE).data(), compression);
	Msg("%s ", g_iChannels == MAX_IMAGE_CHANNELS ? "," : "");
	if (g_iChannels == MAX_IMAGE_CHANNELS)
	{
		Msg("alpha");UpdateLayer(document, &allocator, layer1, psd::exportChannel::ALPHA, 0, 0, g_iImageWidth, g_iImageHeight, SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::ALPHA).data(), compression);
	}
	Msg("... done(%.2fs)\n", Plat_FloatTime() - start);

	start = Plat_FloatTime();
	Msg("Merging RGBA layers... ");
	// merged image data is optional. if none is provided, black channels will be exported instead.
	UpdateMergedImage(document, &allocator, SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::RED).data(), SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::GREEN).data(), SeparateRGBALayersFromSTB<T>(stb_array, psd::exportChannel::BLUE).data());
	Msg("done(%.2fs)\n", Plat_FloatTime() - start);

	start = Plat_FloatTime();
	Msg("Writting .psd file... ");
	WriteDocument(document, &allocator, &file);
	Msg("done(%.2fs)\n", Plat_FloatTime() - start);

	DestroyExportDocument(document, &allocator);
	file.Close();

	DestroyAllocVectorImageData<T>(stb_array);

	//Check if the psd exists!
	IsPsdOK(pPsdFileName);
}


//-----------------------------------------------------------------------------
// Purpose:  Given an image it converts it to .psd
//-----------------------------------------------------------------------------
void CheckAndExportToPsd(const char* pFileName)
{
	char szLayerName[64];

	// Sanity check!
	if(_access(pFileName, 0))
	{
		Error("The file: %s, doesnt exist!\n", pFileName);
	}

	// Get the file name, we skip the first char '\'
	const char* pImageFileName = V_strrchr(pFileName, '\\') + 1;

	// Sanity check!
	if (pImageFileName == nullptr)
	{
		Error(	"**INTERAL ERROR**: pImageFileName == nullptr!\n"
				"Speak to a dev, this shouldnt happen!\n"
			 );
	}

	// Default layer name
	V_strcpy(szLayerName, pImageFileName);

	// one more Sanity check, i swear..
	CheckExtensionImageFileConvert(pFileName);
	
	if		(V_strstr(pFileName, g_rgpAssetConvertList[0])) { WriteNBitsPsd<uint8_t>(pFileName, szLayerName);   /*.jpg	- 8bits*/ }
	else if (V_strstr(pFileName, g_rgpAssetConvertList[1])) { WriteNBitsPsd<uint8_t>(pFileName, szLayerName);   /*.jpeg - 8bits*/ }
	else if (V_strstr(pFileName, g_rgpAssetConvertList[2])) { WriteNBitsPsd<uint16_t>(pFileName, szLayerName);  /*.png	- 16bits*/}
	else if (V_strstr(pFileName, g_rgpAssetConvertList[3])) { WriteNBitsPsd<uint8_t>(pFileName, szLayerName);   /*.tga	- 8bits*/ }
	else if (V_strstr(pFileName, g_rgpAssetConvertList[4])) { WriteNBitsPsd<uint8_t>(pFileName, szLayerName);   /*.bmp	- 8bits*/ }
	else if (V_strstr(pFileName, g_rgpAssetConvertList[5])) { WriteNBitsPsd<uint8_t>(pFileName, szLayerName);   /*.gif	- 8bits*/ }
	else if (V_strstr(pFileName, g_rgpAssetConvertList[6])) { WriteNBitsPsd<float32_t>(pFileName, szLayerName); /*.hdr	- 32bits*/}
	else if (V_strstr(pFileName, g_rgpAssetConvertList[7])) { WriteNBitsPsd<float32_t>(pFileName, szLayerName); /*.pic	- 32bits*/}
	else if (V_strstr(pFileName, g_rgpAssetConvertList[8])) { WriteNBitsPsd<uint16_t>(pFileName, szLayerName);  /*.ppm	- 16bits*/}
	else if (V_strstr(pFileName, g_rgpAssetConvertList[9])) { WriteNBitsPsd<uint16_t>(pFileName, szLayerName);  /*.pgm	- 16bits*/}

	// Delete the older source file.
	if (g_bDeleteSource)
	{
		if (remove(pFileName) != 0)
		{
			Warning("Could not delete source file: %s!\n", pFileName);
		}
		else
		{
			Msg("Deleting source file at: %s\n", pFileName);
		}
	}

	GenerateVtexConfigFile(pFileName);
}