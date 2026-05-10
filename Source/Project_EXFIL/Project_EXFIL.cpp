// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_EXFIL.h"
#include "Modules/ModuleManager.h"
#include "Internationalization/StringTableRegistry.h"
#include "Core/EXFILLog.h"

class FProjectEXFILModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		LOCTABLE_FROMFILE_GAME(
			"/Game/Localization/ST_UI",
			"EXFILUI",
			"Localization/ST_UI.csv");
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FProjectEXFILModule, Project_EXFIL, "Project_EXFIL");

DEFINE_LOG_CATEGORY(LogProject_EXFIL)
DEFINE_LOG_CATEGORY(LogEXFIL)
