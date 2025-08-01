//============ ------------------------------------------------- ===============//
//
// Purpose: Converts .png, .jpg, etc.. to .psd for vtex compile
//
// $NoKeywords: $
//=============================================================================//
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


//-----------------------------------------------------------------------------
// Purpose: Global vars
//-----------------------------------------------------------------------------
bool			g_bQuiet = false;
bool            g_bSignature = false;
bool            g_bIsSingleFile = true;
bool            g_bUseDir = false;
bool            g_bTempalteGeneration = false;
bool            g_bDeleteSource = false;
float           g_fGlobalTimer;
uint8_t         g_uiCompressionType = 0;
uint8_t         g_uiForceImageBit;
Color			header_color(0, 255, 255, 255);     //TODO: maybe move 'Color' in utils/common/colorscheme.cpp/.h
Color			successful_color(0, 255, 0, 255);
Color			failed(255, 0, 0, 255);
char			g_szGameMaterialSrcDir[MAX_PATH] = ""; 
char			g_szSingleInputFile[MAX_PATH] = "";
char            g_szSignature[1][128];
// Note: These extensions should be in sync with the suported in stb_image.h!!
const char*     g_rgpAssetConvertList[G_RGPASSETCONVERTLISTSIZE] = 
{   
                                                ".jpg",         //0
                                                ".jpeg",        //1
                                                ".png",         //2
                                                ".tga",         //3
                                                ".bmp",         //4
                                                ".gif",         //5
                                                ".hdr",         //6
                                                ".pic",         //7
                                                ".ppm",         //9
                                                ".pgm"          //10
};

// Maybe it is better to check the endname of the file (_whatever) and search the template file in materialsrc/template
const char*     g_rgpEndFileName[G_RGPENDFILENAMESIZE] = 
{  
                                                "_color",         //https://developer.valvesoftware.com/wiki/$basetexture
                                                "_albedo",         //https://developer.valvesoftware.com/wiki/$basetexture
                                                "_normal",        //https://developer.valvesoftware.com/wiki/$bumpmap
                                                "_ao",            //https://developer.valvesoftware.com/wiki/$ambientoccltexture
                                                "_detail",        //https://developer.valvesoftware.com/wiki/$detail
                                                "_envmap",        //https://developer.valvesoftware.com/wiki/$envmap
                                                "_envmapmask",    //https://developer.valvesoftware.com/wiki/$envmapmask
                                                "_trans",         //
                                                "_specular"       //https://developer.valvesoftware.com/wiki/$phongexponenttexture                             
                                                "_flowmap",       //https://developer.valvesoftware.com/wiki/Water_(shader)#Flowing_water
                                                "_selfilum",      //https://developer.valvesoftware.com/wiki/Glowing_textures_(Source)#$selfillum
                                                "_blendmodulate", //https://developer.valvesoftware.com/wiki/$blendmodulatetexture
                                                "_lightwarp",     //https://developer.valvesoftware.com/wiki/$lightwarptexture
                                                "_dudvmap"        //https://developer.valvesoftware.com/wiki/Water_(shader)
                                                "_ramp"           //https://developer.valvesoftware.com/wiki/$ramptexture
                                                "_tintmask",      //this is only in post found in csgo branch?
                                                "_flowmapnoise",  //https://developer.valvesoftware.com/wiki/Water_(shader)#Flowing_water    
};


//-----------------------------------------------------------------------------
// Purpose:   Given a dir checks all files and converts the images only
//-----------------------------------------------------------------------------
static void ProcessDirAndConvertContents(const char* directory, int &files)
{
    char searchPath[MAX_PATH];
    bool bFoundFile = false;
    WIN32_FIND_DATAA findData;
    HANDLE hFind;

    V_snprintf(searchPath, MAX_PATH, "%s\\*.*", directory);

    hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        Error("Failed to open directory: %s\n", directory);
        return;
    }

    do
    {
        const char* name = findData.cFileName;

        if (V_strcmp(name, ".") == 0 || V_strcmp(name, "..") == 0)
            continue;

        // Build full path
        char fullPath[MAX_PATH];
        V_snprintf(fullPath, MAX_PATH, "%s\\%s", directory, name);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ProcessDirAndConvertContents(fullPath, files);
        }
        else
        {
            for (const char* pTemp : g_rgpAssetConvertList)
            {
                if (V_strstr(fullPath, pTemp))
                {
                    CheckAndExportToPsd(fullPath);
                    files++;
                }
            }
        }

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}


//-----------------------------------------------------------------------------
// Purpose:   Setup the search path
//-----------------------------------------------------------------------------
static void Init()
{
    g_fGlobalTimer = Plat_FloatTime();

    if (!g_bIsSingleFile && !g_bUseDir)
    {
        V_snprintf(g_szGameMaterialSrcDir, sizeof(g_szGameMaterialSrcDir), "%s\\%s", gamedir, MATERIALSRC_DIR);
    }

    qprintf("Gamedir path:          %s\n", gamedir);
    qprintf("Material Source path:  %s\n", g_szGameMaterialSrcDir);
}


//-----------------------------------------------------------------------------
// Purpose:   Prints the header
//-----------------------------------------------------------------------------
static void PrintHeader()
{
	ColorSpewMessage(SPEW_MESSAGE, &header_color, "Unusuario2 - psdwriter.exe (Build: %s %s)\n", __DATE__, __TIME__);
}


//-----------------------------------------------------------------------------
// Purpose:   Print psdwriter usage
//-----------------------------------------------------------------------------
static void PrintUsage(int argc, char* argv[])
{
    Msg("Usage: psdwriter.exe [options] -i or -game <path>\n\n");
    ColorSpewMessage(SPEW_MESSAGE, &header_color, " General Options:\n");
    Msg("   -i <file>:                      Path to the file to convert (e.g: \"C:\\dota 2\\materialsrc\\default\\default_color.tga\").\n"
        "   -dir <path>:                    Converts all the files in the dir.\n"
        "   -psdtemplategeneration or -psdtg: For every file that has a end name, like _normal or _color for example it adds the config vtex file.\n"
        "                                   For this feature to work, you will need to generate a config file in materialsrc/templates and also run the tool with -game.\n"
        "   -deletesource or -dltsrc:       Deletes source files if the .psd conversion is generated.\n"
		"   -game <path>:                   Specify the folder of the gameinfo.txt file. Makes the tool look in the folder materialsrc for files to convert.\n"
        "   -vproject <path>:               Same as \'-game\'.\n"
        "\n");
    ColorSpewMessage(SPEW_MESSAGE, &header_color, " Spew Options:\n");
    Msg("   -v or -verbose:                 Enables verbose.\n"
        "   -quiet:                         Prints minimal text. (Note: Disables \'-verbose\').\n"
        "\n");
    ColorSpewMessage(SPEW_MESSAGE, &header_color, " Other Options:\n");
    Msg("   -FullMinidumps:                 Write large minidumps on crash.\n"
        "   -psdcompresion <n>:             Manages the compresion of the .psd file when importing a image (default 0).\n"
        "                                   0 = Raw data (big filesize)\n"
        "                                   1 = RLE-compressed data (using the PackBits algorithm).\n"
        "                                   2 = ZIP-compressed data.\n"
        "                                   3 = ZIP-compressed data with prediction (delta-encoding).\n"
        "   -signature <studio> <mod>:      (e.g: -signature myfirststudio myfirstsourcemod)\n"
        "\n");

    DeleteCmdLine(argc, argv);
    CmdLib_Cleanup();
    CmdLib_Exit(1);
}


//-----------------------------------------------------------------------------
// Purpose:   Parse command line
//-----------------------------------------------------------------------------
static void ParseCommandline(int argc, char* argv[])
{
    int x = 0;
    if (argc == 1)
    {
        PrintUsage(argc, argv);
    }

    for (int i = 1; i < argc; ++i)
    {
        if (!V_stricmp(argv[i], "-?") || !V_stricmp(argv[i], "-help") || argc == 1)
        {
            PrintUsage(argc, argv);
        }
        else if (!V_stricmp(argv[i], "-v") || !V_stricmp(argv[i], "-verbose"))
        {
            verbose = true;
			g_bQuiet = false;
        }
        else if (!V_stricmp(argv[i], "-quiet"))
        {
			verbose	 = false;
            g_bQuiet = true;
        }
        else if (!V_stricmp(argv[i], "-FullMinidumps"))
        {
			EnableFullMinidumps(true);
		}
        else if (!V_stricmp(argv[i], "-psdtemplategeneration") || !V_stricmp(argv[i], "-psdtg"))
        {
            g_bTempalteGeneration = true;
		}
        else if (!V_stricmp(argv[i], "-deletesource") || !V_stricmp(argv[i], "-dltsrc"))
        {
            g_bDeleteSource = true;
		}
        else if (!V_stricmp(argv[i], "-psdcompresion"))
        {
            const int iValue = atoi(argv[i]);

            // Note: This values need to be in sync with psd::compressionType in PsdCompressionType.h
            if ((iValue >= 0) && (iValue <= 3))
            {
                g_uiCompressionType = static_cast<uint8_t>(iValue);
            }
            else
            {
                Error("Error: \'-psdcompresion\' requires a valid integer value, expected range: [0,3], value: %u\n", iValue);
            }
		}
        else if (!V_stricmp(argv[i], "-signature"))
        {
            const char* pStrTemp1 = argv[i++];
            const char* pStrTemp2 = argv[i++];

            if (pStrTemp1 && pStrTemp2)
            {
                V_strcpy(g_szSignature[0], pStrTemp1);
                V_strcpy(g_szSignature[1], pStrTemp2);
            }
            else
            {
                Error("Error: \'-signature\' requires a TWO strings, (e.g: -signature blah blah2)\n");
            }
		}
        else if (!V_stricmp(argv[i], "-i"))
        {
            if (++i < argc && argv[i][0] != '-')
            {
                char* pInputPath = argv[i];

                if (!pInputPath)
                {
                    Error("Error: \'-i\' requires a valid path argument. NULL path\n");
                }

                g_bIsSingleFile = true;
                V_strcpy(g_szSingleInputFile, pInputPath);
            }
            else
            {
                Error("Error: \'-i\' requires a valid path argument.\n");
            }
        }
        else if (!V_stricmp(argv[i], "-dir"))
        {
            if (++i < argc && argv[i][0] != '-')
            {
                char* pInputPath = argv[i];

                if (!pInputPath)
                {
                    Error("Error: \'-dir\' requires a valid path argument. NULL path\n");
                }

                V_strcpy(g_szGameMaterialSrcDir, pInputPath);
            }
            else
            {
                Error("Error: \'-dir\' requires a valid path argument.\n");
            }
            g_bUseDir = true;
            g_bIsSingleFile = false;
        }
        else if (!V_stricmp(argv[i], "-game") || !V_stricmp(argv[i], "-vproject"))
        {
            if (++i < argc && argv[i][0] != '-')
            {
                char* pGamePath = argv[i];

                if (!pGamePath)
                {
                    Error("Error: \'-game\' requires a valid path argument. NULL path\n");
                }

                V_strcpy(gamedir, pGamePath);
            }
            else
            {
                Error("Error: \'-game\' requires a valid path argument.\n");
            }
            
            g_bIsSingleFile = false;
        }
        else
        {
            Warning("\nUnknown option \'%s\'\n", argv[i]);
            PrintUsage(argc, argv);
        }
    }
}


//-----------------------------------------------------------------------------
// Purpose:   Main funtion
//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	SetupDefaultToolsMinidumpHandler();
	CommandLine()->CreateCmdLine(argc, argv);
	InstallSpewFunction();
	PrintHeader();
	ParseCommandline(argc, argv);
	Init();
	
    if (g_bIsSingleFile)
    {
        CheckAndExportToPsd(g_szSingleInputFile);
    }
    else 
    {
        int files = 0;
        ProcessDirAndConvertContents(g_szGameMaterialSrcDir, files);
        Msg("Converted image files to .psd: %i\n\n", files);
    }

	DeleteCmdLine(argc, argv);
	CmdLib_Cleanup();
	CmdLib_Exit(1);
	return 0;
}
