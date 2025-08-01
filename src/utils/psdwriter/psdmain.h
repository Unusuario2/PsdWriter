//============ ------------------------------------------------- ===============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#ifndef PSDMAIN_H
#define PSDMAIN_H

#ifdef _WIN32
#pragma once
#endif // _WIN32


#define G_RGPENDFILENAMESIZE		14u
#define G_RGPASSETCONVERTLISTSIZE	10u


extern bool			g_bIsSingleFile;
extern bool			g_bSignature;
extern bool			g_bTempalteGeneration;
extern bool			g_bDeleteSource;
extern float		g_fGlobalTimer;
extern uint8_t		g_uiCompressionType;
extern uint8_t      g_uiForceImageBit;
extern Color		header_color;
extern Color		successful_color;
extern Color		failed;
extern char			g_szSignature[1][128];
extern char			g_szGameMaterialSrcDir[MAX_PATH];
extern const char*	g_rgpAssetConvertList[G_RGPASSETCONVERTLISTSIZE];
extern const char*	g_rgpEndFileName[G_RGPENDFILENAMESIZE];


#endif // PSDMAIN_H