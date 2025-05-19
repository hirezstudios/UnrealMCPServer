#include "UMCP_UriTemplate.h" // For FUMCP_UriTemplate
#include "Tests/TestHarnessAdapter.h" // For TEST_CASE_NAMED and CHECK_MESSAGE
#include "Containers/UnrealString.h" // For FString comparisons if needed

#if WITH_TESTS

// --- URI Template Parsing Tests ---

TEST_CASE_NAMED(FUMCP_UriTemplateParseTests_Empty, "Plugin.MCP.UriTemplate.Parse::Empty", "[UriTemplate][Parse][SmokeFilter]")
{
    FString TemplateStr = TEXT("");
    FUMCP_UriTemplate UriTemplate(TemplateStr);

    CHECK_MESSAGE(TEXT("Template should be valid for an empty string."), UriTemplate.IsValid());
    CHECK_MESSAGE(TEXT("ParseError should be empty for an empty string."), UriTemplate.ParseError().IsEmpty());
    CHECK_MESSAGE(TEXT("GetUriTemplateStr should return the original empty template string."), UriTemplate.GetUriTemplateStr() == TemplateStr);
    // Expect 0 components for an empty template string.
    // Add check for Components.Num() == 0 if that's an accessible and desired check.
}

TEST_CASE_NAMED(FUMCP_UriTemplateParseTests_SimpleLiteral, "Plugin.MCP.UriTemplate.Parse::SimpleLiteral", "[UriTemplate][Parse][SmokeFilter]")
{
    FString TemplateStr = TEXT("/simple/literal/path");
    FUMCP_UriTemplate UriTemplate(TemplateStr);

    CHECK_MESSAGE(TEXT("Template should be valid after parsing a simple literal string."), UriTemplate.IsValid());
    CHECK_MESSAGE(TEXT("ParseError should be empty for a valid simple literal."), UriTemplate.ParseError().IsEmpty());
    CHECK_MESSAGE(TEXT("GetUriTemplateStr should return the original template string."), UriTemplate.GetUriTemplateStr() == TemplateStr);
}

TEST_CASE_NAMED(FUMCP_UriTemplateParseTests_SimpleExpression, "Plugin.MCP.UriTemplate.Parse::SimpleExpression", "[UriTemplate][Parse][SmokeFilter]")
{
    FString TemplateStr = TEXT("/users/{id}");
    FUMCP_UriTemplate UriTemplate(TemplateStr);

    CHECK_MESSAGE(TEXT("Template should be valid after parsing a simple expression."), UriTemplate.IsValid());
    CHECK_MESSAGE(TEXT("ParseError should be empty for a valid simple expression."), UriTemplate.ParseError().IsEmpty());
    CHECK_MESSAGE(TEXT("GetUriTemplateStr should return the original template string."), UriTemplate.GetUriTemplateStr() == TemplateStr);
}

TEST_CASE_NAMED(FUMCP_UriTemplateParseTests_PrefixedExpression, "Plugin.MCP.UriTemplate.Parse::PrefixedExpression", "[UriTemplate][Parse][SmokeFilter]")
{
    FString TemplateStr = TEXT("/files/{filename:5}");
    FUMCP_UriTemplate UriTemplate(TemplateStr);

    CHECK_MESSAGE(TEXT("Template should be valid for prefixed expression."), UriTemplate.IsValid());
    CHECK_MESSAGE(TEXT("ParseError should be empty for prefixed expression."), UriTemplate.ParseError().IsEmpty());
    CHECK_MESSAGE(TEXT("GetUriTemplateStr should return the original template string."), UriTemplate.GetUriTemplateStr() == TemplateStr);
}

TEST_CASE_NAMED(FUMCP_UriTemplateParseTests_ExplodedExpression, "Plugin.MCP.UriTemplate.Parse::ExplodedExpression", "[UriTemplate][Parse][SmokeFilter]")
{
    FString TemplateStr = TEXT("/users/{ids*}");
    FUMCP_UriTemplate UriTemplate(TemplateStr);

    CHECK_MESSAGE(TEXT("Template should be valid for exploded expression."), UriTemplate.IsValid());
    CHECK_MESSAGE(TEXT("ParseError should be empty for exploded expression."), UriTemplate.ParseError().IsEmpty());
    CHECK_MESSAGE(TEXT("GetUriTemplateStr should return the original template string."), UriTemplate.GetUriTemplateStr() == TemplateStr);
}


void DoUriTemplateMatchCheck(FString UriTemplateStr, FString UriToCheck, TMap<FString, TArray<FString>> ExpectedVariables)
{
	FUMCP_UriTemplate UriTemplate(UriTemplateStr);
	CHECK_MESSAGE(FString::Printf(TEXT("UriTemplate '%s': not valid: '%s'"), *UriTemplateStr, *UriTemplate.ParseError()), UriTemplate.IsValid());

	FUMCP_UriTemplateMatch MatchResult;
	bool bMatched = UriTemplate.FindMatch(UriToCheck, MatchResult);
	CHECK_MESSAGE(FString::Printf(TEXT("UriTemplate '%s': URI '%s' did not match"), *UriTemplateStr, *UriToCheck), bMatched);
	CHECK_MESSAGE(FString::Printf(TEXT("UriTemplate '%s': URI '%s' variables did not match expected variables"), *UriTemplateStr, *UriToCheck), MatchResult.Variables.OrderIndependentCompareEqual(ExpectedVariables));
}

void DoUriTemplateMatchFail(FString UriTemplateStr, FString UriToCheck)
{
    FUMCP_UriTemplate UriTemplate(UriTemplateStr);
    CHECK_MESSAGE(FString::Printf(TEXT("UriTemplate '%s': not valid: '%s'"), *UriTemplateStr, *UriTemplate.ParseError()), UriTemplate.IsValid());

    FUMCP_UriTemplateMatch MatchResult;
    bool bMatched = UriTemplate.FindMatch(UriToCheck, MatchResult);
    CHECK_MESSAGE(FString::Printf(TEXT("UriTemplate '%s': URI '%s' matched"), *UriTemplateStr, *UriToCheck), !bMatched);
}

// --- URI Template Matching Tests ---
TEST_CASE_NAMED(FUMCP_UriTemplateMatchTests_Level1, "Plugin.MCP.UriTemplate.Match::Level1", "[UriTemplate][Match][Level1]")
{
	DoUriTemplateMatchCheck(TEXT("{var}"), TEXT("value"), TMap<FString, TArray<FString>>{
		{TEXT("var"), TArray<FString>{ TEXT("value") }},
	});
	DoUriTemplateMatchCheck(TEXT("{hello}"), TEXT("Hello%20World%21"), TMap<FString, TArray<FString>>{
		{TEXT("hello"), TArray<FString>{ TEXT("Hello World!") }},
	});
	DoUriTemplateMatchCheck(TEXT("{base}index"), TEXT("http%3A%2F%2Fexample.com%2Fhome%2Findex"), TMap<FString, TArray<FString>>{
		{TEXT("base"), TArray<FString>{ TEXT("http://example.com/home/") }},
	});
    DoUriTemplateMatchCheck(TEXT("/users/{id}/profile"), TEXT("/users/12345/profile"), TMap<FString, TArray<FString>>{
        {TEXT("id"), TArray<FString>{ TEXT("12345") }},
    });
    DoUriTemplateMatchFail(TEXT("/users/{id}/profile"), TEXT("/users/profile"));
    DoUriTemplateMatchFail(TEXT("/users/{id}/profile"), TEXT("/users/12345/profile/extra"));
}

TEST_CASE_NAMED(FUMCP_UriTemplateMatchTests_Level2, "Plugin.MCP.UriTemplate.Match::Level2", "[UriTemplate][Match][Level2]")
{
    SECTION("Reserved Matching")
    {
		DoUriTemplateMatchCheck(TEXT("{+var}"), TEXT("value"), TMap<FString, TArray<FString>>{
			{TEXT("var"), TArray<FString>{ TEXT("value") }},
		});
		DoUriTemplateMatchCheck(TEXT("{+hello}"), TEXT("Hello%20World!"), TMap<FString, TArray<FString>>{
			{TEXT("hello"), TArray<FString>{ TEXT("Hello World!") }},
		});
		DoUriTemplateMatchCheck(TEXT("{+path}/here"), TEXT("/foo/bar/here"), TMap<FString, TArray<FString>>{
			{TEXT("path"), TArray<FString>{ TEXT("/foo/bar") }},
		});
		DoUriTemplateMatchCheck(TEXT("here?ref={+path}"), TEXT("here?ref=/foo/bar"), TMap<FString, TArray<FString>>{
			{TEXT("path"), TArray<FString>{ TEXT("/foo/bar") }},
		});
		DoUriTemplateMatchCheck(TEXT("{+base}index"), TEXT("http://example.com/home/index"), TMap<FString, TArray<FString>>{
			{TEXT("base"), TArray<FString>{ TEXT("http://example.com/home/") }},
		});
		DoUriTemplateMatchCheck(TEXT("O{+empty}X"), TEXT("OX"), TMap<FString, TArray<FString>>{
			{TEXT("empty"), TArray<FString>{ TEXT("") }},
		});
    }
	
    SECTION("Frament Matching")
    {
	    DoUriTemplateMatchCheck(TEXT("X{#var}"), TEXT("X#value"), TMap<FString, TArray<FString>>{
			{TEXT("var"), TArray<FString>{ TEXT("value") }},
		});
    	DoUriTemplateMatchCheck(TEXT("X{#hello}"), TEXT("X#Hello%20World!"), TMap<FString, TArray<FString>>{
			{TEXT("hello"), TArray<FString>{ TEXT("Hello World!") }},
		});
    }
}

#endif //WITH_TESTS
