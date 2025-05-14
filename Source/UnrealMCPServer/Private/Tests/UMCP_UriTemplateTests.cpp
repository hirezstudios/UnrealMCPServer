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


// --- URI Template Matching Tests ---

TEST_CASE_NAMED(FUMCP_UriTemplateMatchTests_SimpleExpressionValue, "Plugin.MCP.UriTemplate.Match::SimpleExpressionValue", "[UriTemplate][Match][SmokeFilter]")
{
    SECTION("Basic ID match")
    {
        FString TemplateStr = TEXT("/users/{id}/profile");
        FUMCP_UriTemplate UriTemplate(TemplateStr);
        CHECK_MESSAGE(TEXT("Template '/users/{id}/profile' should be valid."), UriTemplate.IsValid());

        FString UriToMatch = TEXT("/users/12345/profile");
        FUMCP_UriTemplateMatch MatchResult;
        bool bMatched = UriTemplate.FindMatch(UriToMatch, MatchResult);

        CHECK_MESSAGE(TEXT("URI '/users/12345/profile' should match the template."), bMatched);
        
        const TArray<FString>* Values = MatchResult.Variables.Find(TEXT("id"));
        CHECK_MESSAGE(TEXT("Variable 'id' should be found in MatchResult."), Values != nullptr);
        if (Values)
        {
            CHECK_MESSAGE(TEXT("Variable 'id' should have 1 value."), Values->Num() == 1);
            if (Values->Num() == 1)
            {
                CHECK_MESSAGE(TEXT("Variable 'id' should be '12345'."), (*Values)[0] == TEXT("12345"));
            }
        }
        CHECK_MESSAGE(TEXT("Original URI in match result should be correct"), MatchResult.Uri == UriToMatch);
    }

    SECTION("Alternative ID match")
    {
        FString TemplateStr = TEXT("/users/{id}/profile");
        FUMCP_UriTemplate UriTemplate(TemplateStr); // Re-init for clarity, though state should be const
        CHECK_MESSAGE(TEXT("Template '/users/{id}/profile' should be valid for alternative ID test."), UriTemplate.IsValid());

        FString UriToMatch = TEXT("/users/another-id/profile");
        FUMCP_UriTemplateMatch MatchResult; // Reset
        bool bMatched = UriTemplate.FindMatch(UriToMatch, MatchResult);
        CHECK_MESSAGE(TEXT("URI '/users/another-id/profile' with different ID should match."), bMatched);
        const TArray<FString>* Values = MatchResult.Variables.Find(TEXT("id"));
        CHECK_MESSAGE(TEXT("Variable 'id' (another-id) should be found."), Values != nullptr);
        if (Values && Values->Num() == 1) // Check Values pointer again for safety in this section
        {
            CHECK_MESSAGE(TEXT("Variable 'id' should be 'another-id'."), (*Values)[0] == TEXT("another-id"));
        }
    }
    
    SECTION("Non-match: Missing ID segment")
    {
        FString TemplateStr = TEXT("/users/{id}/profile");
        FUMCP_UriTemplate UriTemplate(TemplateStr);
        CHECK_MESSAGE(TEXT("Template '/users/{id}/profile' should be valid for non-match test."), UriTemplate.IsValid());
        
        FString UriToMatch = TEXT("/users/profile"); 
        FUMCP_UriTemplateMatch MatchResult; // Reset
        bool bMatched = UriTemplate.FindMatch(UriToMatch, MatchResult);
        CHECK_MESSAGE(TEXT("URI '/users/profile' missing ID segment should not match."), !bMatched);
    }

    SECTION("Non-match: Extra segment at end")
    {
        FString TemplateStr = TEXT("/users/{id}/profile");
        FUMCP_UriTemplate UriTemplate(TemplateStr);
        CHECK_MESSAGE(TEXT("Template '/users/{id}/profile' should be valid for extra segment test."), UriTemplate.IsValid());

        FString UriToMatch = TEXT("/users/someid/profile/extra");
        FUMCP_UriTemplateMatch MatchResult;
        bool bMatched = UriTemplate.FindMatch(UriToMatch, MatchResult);
        CHECK_MESSAGE(TEXT("URI '/users/someid/profile/extra' with extra segment should not match."), !bMatched);
    }

    SECTION("Dot operator simple case")
    {
        FString TemplateStrWithDot = TEXT("/file{.ext}");
        FUMCP_UriTemplate UriTemplateDot(TemplateStrWithDot);
        CHECK_MESSAGE(TEXT("Template '/file{.ext}' with dot operator should be valid."), UriTemplateDot.IsValid());
        
        FString UriToMatch = TEXT("/file.txt");
        FUMCP_UriTemplateMatch MatchResult;
        bool bMatched = UriTemplateDot.FindMatch(UriToMatch, MatchResult);
        CHECK_MESSAGE(TEXT("URI '/file.txt' with dot operator should match."), bMatched);
        const TArray<FString>* Values = MatchResult.Variables.Find(TEXT("ext"));
        CHECK_MESSAGE(TEXT("Variable 'ext' should be found for dot operator."), Values != nullptr);
        if (Values && Values->Num() == 1)
        {
            CHECK_MESSAGE(TEXT("Variable 'ext' should be 'txt'."), (*Values)[0] == TEXT("txt"));
        }
    }
}

#endif //WITH_TESTS
