#include "UMCP_UriTemplate.h"

#include "FindInBlueprintManager.h"


FString::ElementType FUMCP_UriTemplateComponent::GetPrefixChar() const
{
	switch (ExpressionOperator)
	{
	case '=': // Reserved
	case ',': // Reserved
	case '!': // Reserved
	case '@': // Reserved
	case '|': // Reserved
		return 0;
	case '.': // Level 3 - Name lables or extensions prefixed by "."
	case '/': // Level 3 - Path segments prefixed by "/"
	case ';': // Level 3 - Path parameter name or name=value pairs prefixed by ";"
	case '?': // Level 3 - Query component beginning with "?" and consisting of name=value pairs separated by "&"
	case '&': // Level 3 - continuation of query-style &name=value pairs within a literal query component
	case '#': // Level 2 - fragment characters prefixed by "#"
		return ExpressionOperator;
	case '+': // Level 2 - Reserved character strings
	default:
		return 0;
	}
}

FString::ElementType FUMCP_UriTemplateComponent::GetSeparatorChar() const
{
	switch (ExpressionOperator)
	{
	case '=': // Reserved
	case ',': // Reserved
	case '!': // Reserved
	case '@': // Reserved
	case '|': // Reserved
	case '.': // Level 3 - Name lables or extensions prefixed by "."
	case '/': // Level 3 - Path segments prefixed by "/"
	case ';': // Level 3 - Path parameter name or name=value pairs prefixed by ";"
		return ExpressionOperator;
	case '?': // Level 3 - Query component beginning with "?" and consisting of name=value pairs separated by "&"
	case '&': // Level 3 - continuation of query-style &name=value pairs within a literal query component
		return '&';
	case '#': // Level 2 - fragment characters prefixed by "#"
	case '+': // Level 2 - Reserved character strings
	default:
		return ',';
	}
}

void FUMCP_UriTemplateComponent::FromLiteral(FString Literal, FUMCP_UriTemplateComponent& OutComp, FString& OutError)
{
	OutComp.Literal = MoveTemp(Literal);
	OutComp.Type = EUMCP_UriTemplateComponentType::Literal;
}

void FUMCP_UriTemplateComponent::FromVarList(FString VarList, FUMCP_UriTemplateComponent& OutComp, FString& OutError)
{
	if (VarList.IsEmpty())
	{
		OutError = TEXT("VarList must not be empty");
		return;
	}
	switch (VarList[0])
	{
	case '=': // Reserved
	case ',': // Reserved
	case '!': // Reserved
	case '@': // Reserved
	case '|': // Reserved
	case '.': // Level 3 - Name lables or extensions prefixed by "."
	case '/': // Level 3 - Path segments prefixed by "/"
	case ';': // Level 3 - Path parameter name or name=value pairs prefixed by ";"
	case '?': // Level 3 - Query component beginning with "?" and consisting of name=value pairs separated by "&"
	case '&': // Level 3 - continuation of query-style &name=value pairs within a literal query component
	case '+': // Level 2 - Reserved character strings
	case '#': // Level 2 - fragment characters prefixed by "#"
		OutComp.ExpressionOperator = VarList[0];
		VarList = VarList.RightChop(1);
		break;
	default:
		OutComp.ExpressionOperator = 0;
		break;
	}
	TArray<FString> VarSpecs;
	VarList.ParseIntoArray(VarSpecs, TEXT(","));
	if (VarSpecs.IsEmpty())
	{
		OutError = TEXT("VarList found no specs");
		return;
	}

	for (const FString& VarSpecStr : VarSpecs)
	{
		auto& VarSpec = OutComp.VarSpecs.Emplace_GetRef();
		if (VarSpecStr.Len() >= 2 && VarSpecStr[VarSpecStr.Len() - 1] == TEXT('*'))
		{
			VarSpec.Type = EUMCP_UriTemplateComponentVarSpecType::Exploded;
			VarSpec.Val = VarSpecStr.LeftChop(1);
			continue;
		}

		FString VarSpecPrefix, VarSpecMaxLength;
		if (VarSpecStr.Split(TEXT(":"), &VarSpecPrefix, &VarSpecMaxLength, ESearchCase::CaseSensitive))
		{
			VarSpec.Type = EUMCP_UriTemplateComponentVarSpecType::Prefixed;
			LexFromString(VarSpec.MaxLength, *VarSpecMaxLength);
			VarSpec.Val = VarSpecPrefix;
			continue;
		}

		VarSpec.Val = VarSpecStr;
	}
	
	OutComp.Type = EUMCP_UriTemplateComponentType::VarList;
}


FString FUMCP_UriTemplateComponent::Expand(const TMap<FString, TArray<FString>>& Values) const
{
	if (Type == EUMCP_UriTemplateComponentType::Literal)
	{
		return Literal;
	}

	// TODO
	return FString();
}

FUMCP_UriTemplate::FUMCP_UriTemplate(FString InUriTemplateStr)
{
	UriTemplateStr = MoveTemp(InUriTemplateStr);
	TryParseTemplate();
}

void FUMCP_UriTemplate::TryParseTemplate()
{
	FString Temp;
	bool bParsingVarlist = false;
	for (FString::ElementType c : UriTemplateStr)
	{
		if (bParsingVarlist)
		{
			if (c == TEXT('}'))
			{
				FUMCP_UriTemplateComponent::FromVarList(MoveTemp(Temp), Components.Emplace_GetRef(), Error);
				if (!Error.IsEmpty())
				{
					return;
				}
				Temp = FString();
				bParsingVarlist = false;
				continue;
			}
		}
		else
		{
			if (c == TEXT('{'))
			{
				if (!Temp.IsEmpty())
				{
					FUMCP_UriTemplateComponent::FromLiteral(MoveTemp(Temp), Components.Emplace_GetRef(), Error);
					if (!Error.IsEmpty())
					{
						return;
					}
				}
				Temp = FString();
				bParsingVarlist = true;
			}
		}
		Temp.AppendChar(c);
	}

	if (!Temp.IsEmpty())
	{
		FUMCP_UriTemplateComponent::FromLiteral(MoveTemp(Temp), Components.Emplace_GetRef(), Error);
	}
}

bool FUMCP_UriTemplate::FindMatch(const FString& Uri, FUMCP_UriTemplateMatch& OutMatch) const
{
    OutMatch.Uri = Uri; // Store the original URI in the match result
    FStringView UriRemaining = Uri;

    for (auto Itr = Components.CreateConstIterator(); Itr; ++Itr)
    {
        if (Itr->Type == EUMCP_UriTemplateComponentType::Literal)
        {
            if (UriRemaining.IsEmpty() && !Itr->Literal.IsEmpty()) // More URI needed for literal
            {
                return false;
            }
            if (!UriRemaining.StartsWith(Itr->Literal, ESearchCase::CaseSensitive))
            {
                return false;
            }
            UriRemaining.RightChopInline(Itr->Literal.Len());
            continue;
        }

        if (UriRemaining.IsEmpty() && Itr->Type == EUMCP_UriTemplateComponentType::VarList)
        {
            if (Itr->VarSpecs.Num() > 0) { 
                 bool bCanBeEmpty = (Itr->ExpressionOperator == TEXT('?') || Itr->ExpressionOperator == TEXT('&'));
                 if (!bCanBeEmpty && (Itr+1)) 
                 {
                    return false;
                 }
            } else { 
                return false;
            }
        }


        if (Itr->Type != EUMCP_UriTemplateComponentType::VarList)
        {
            return false; 
        }

        const auto& CurrentComponent = *Itr;
        FStringView ExpressionToMatchThisComponent = UriRemaining;
        const auto RequiredPrefix = CurrentComponent.GetPrefixChar();

        if (RequiredPrefix != 0)
        {
            if (ExpressionToMatchThisComponent.IsEmpty() || ExpressionToMatchThisComponent[0] != RequiredPrefix)
            {
                if (CurrentComponent.ExpressionOperator == TEXT('?') || CurrentComponent.ExpressionOperator == TEXT('&'))
                {
                    continue;
                }
                return false; 
            }
            ExpressionToMatchThisComponent.RightChopInline(1); 
        }

        FStringView NextComponentBoundaryMarker;
        bool bNextComponentIsLiteral = false;
        bool bFoundNextComponentBoundary = false;
        int32 CurrentMatchEndPosition = ExpressionToMatchThisComponent.Len();

        auto NextTemplateComponentItr = Itr + 1;
        if (NextTemplateComponentItr) 
        {
            if (NextTemplateComponentItr->Type == EUMCP_UriTemplateComponentType::Literal)
            {
                int32 BoundaryIdx = ExpressionToMatchThisComponent.Find(NextTemplateComponentItr->Literal);
                if (BoundaryIdx != INDEX_NONE)
                {
                    CurrentMatchEndPosition = BoundaryIdx;
                    NextComponentBoundaryMarker = NextTemplateComponentItr->Literal;
                    bNextComponentIsLiteral = true;
                    bFoundNextComponentBoundary = true;
                }
                else if (!NextTemplateComponentItr->Literal.IsEmpty()) 
                {
                    return false; 
                }
            }
            else if (NextTemplateComponentItr->Type == EUMCP_UriTemplateComponentType::VarList)
            {
                auto NextExpressionPrefixChar = NextTemplateComponentItr->GetPrefixChar();
                if (NextExpressionPrefixChar != 0)
                {
                    int32 NextExpressionStartPosInCurrentMatch;
                    if (ExpressionToMatchThisComponent.FindChar(NextExpressionPrefixChar, NextExpressionStartPosInCurrentMatch))
                    {
                        CurrentMatchEndPosition = NextExpressionStartPosInCurrentMatch;
                        NextComponentBoundaryMarker = ExpressionToMatchThisComponent.Mid(NextExpressionStartPosInCurrentMatch, 1);
                        bFoundNextComponentBoundary = true;
                    }
                }
            }
        }
        
        ExpressionToMatchThisComponent = ExpressionToMatchThisComponent.Left(CurrentMatchEndPosition);

        if (CurrentComponent.VarSpecs.Num() == 1)
        {
            const auto& VarSpec = CurrentComponent.VarSpecs[0];
            if (VarSpec.Type == EUMCP_UriTemplateComponentVarSpecType::Normal && VarSpec.MaxLength == 0)
            {
                if (!ExpressionToMatchThisComponent.IsEmpty())
                {
                    OutMatch.Variables.FindOrAdd(VarSpec.Val).Add(FString(ExpressionToMatchThisComponent));
                }
                else 
                {
                    OutMatch.Variables.FindOrAdd(VarSpec.Val).Add(FString());
                }
                
                UriRemaining.RightChopInline(CurrentMatchEndPosition); 
                
                if (bFoundNextComponentBoundary)
                {
                    if (bNextComponentIsLiteral) {
                         if (UriRemaining.StartsWith(NextComponentBoundaryMarker)) {
                            UriRemaining.RightChopInline(NextComponentBoundaryMarker.Len());
                        } else {
                            return false;
                        }
                    }
                    Itr++; 
                }
                continue; 
            }
        }

        if (CurrentComponent.VarSpecs.Num() > 0) {
            if (CurrentComponent.ExpressionOperator == 0 || CurrentComponent.ExpressionOperator == TEXT('/') || CurrentComponent.ExpressionOperator == TEXT('.')) {
                if (CurrentComponent.VarSpecs.Num() == 1 && !ExpressionToMatchThisComponent.IsEmpty()) {
                     OutMatch.Variables.FindOrAdd(CurrentComponent.VarSpecs[0].Val).Add(FString(ExpressionToMatchThisComponent));
                } else if (CurrentComponent.VarSpecs.Num() > 0) {
                     return false;
                }
            } else {
                return false; 
            }
        }

        UriRemaining.RightChopInline(CurrentMatchEndPosition); 

        if (bFoundNextComponentBoundary)
        {
            if (bNextComponentIsLiteral) {
                if (UriRemaining.StartsWith(NextComponentBoundaryMarker)) {
                    UriRemaining.RightChopInline(NextComponentBoundaryMarker.Len());
                } else {
                    return false; 
                }
            }
            Itr++; 
        }
    } 

    if (!UriRemaining.IsEmpty())
    {
        return false;
    }

    return true;
}

FString FUMCP_UriTemplate::Expand(const TMap<FString, TArray<FString>>& Values) const
{
	FString Result;
	for (const auto& Component : Components)
	{
		Result += Component.Expand(Values);
	}
	return Result;
}
