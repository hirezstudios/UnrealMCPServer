#pragma once

#include "UMCP_Types.h"

class FUMCP_CommonTools
{
public:
	void Register(class FUMCP_Server* Server);

private:
	bool ExportBlueprintToT3D(TSharedPtr<FJsonObject> arguments, TArray<FUMCP_CallToolResultContent>& OutContent);
};
