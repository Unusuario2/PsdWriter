//============ ------------------------------------------------- ===============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#ifndef PSDWRITER_H
#define PSDWRITER_H

#ifdef _WIN32
#pragma once
#endif // _WIN32

#define MATERIALSRC_DIR		"materialsrc"

//-----------------------------------------------------------------------------
// Purpose: Basic image definitions
//-----------------------------------------------------------------------------
#define IMAGE8BITS_ID				8u
#define IMAGE16BITS_ID				16u
#define IMAGE32BITS_ID				32u
#define MAX_WIGHT_RES_X				4096u
#define MAX_WIGHT_RES_Y				MAX_WIGHT_RES_X
#define MAX_IMAGE_CHANNELS			4u
#define TEMAPLATE_COFIG_NAME_BASE	"template"		// e.g: tempalte_color, template_normal, etc...


void CheckAndExportToPsd(const char* pFileName);


#endif // PSDWRITER_H