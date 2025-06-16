# Notebook for UnrealMCPServer

This notebook is for recording potentially useful information, findings, and tidbits related to the UnrealMCPServer plugin project.

## Initial Thoughts & Research

- **MCP Specification:** Review [https://modelcontextprotocol.io/specification/2025-03-26](https://modelcontextprotocol.io/specification/2025-03-26) thoroughly for endpoint definitions and message structures.
- **Unreal FHttpServerModule:** Investigate usage examples and best practices for `FHttpServerModule` in Unreal Engine.
  - Key classes: `IHttpRouter`, `FHttpRouteHandle`, `FHttpServerRequest`, `FHttpServerResponse`.
- **JSON Parsing in Unreal:** Unreal uses `FJsonObject` and related classes for JSON manipulation. This will be crucial for MCP message processing.
- **Port:** 30069 (as specified by user).
- **Plugin Type:** Editor Plugin.

## Blueprint Search Research Findings

### Unreal Engine Search Capabilities Discovery

**Key Finding:** Unreal Engine has robust built-in search capabilities that we can leverage rather than building from scratch.

#### Asset Registry API
- `FAssetRegistryModule` provides fast metadata-based asset searches
- `GetAssets()` with `FARFilter` allows complex filtering
- `GetDerivedClassNames()` can find Blueprint subclasses
- `GetBlueprintAssets()` (UE5+) specifically designed for Blueprint searches
- Asset tags can store searchable metadata without loading assets

#### Blueprint Content Analysis
- `UBlueprint::GetBlueprintGraph()` provides access to Blueprint graphs
- `UEdGraph::GetAllNodesOfClass()` can find specific node types
- Blueprint nodes contain searchable data for variables, functions, asset references
- Unreal's "Find References" functionality can be emulated programmatically

#### Important Unreal Classes for Blueprint Search
- `UBlueprint` - The Blueprint asset container
- `UBlueprintGeneratedClass` - Runtime class generated from Blueprint
- `FAssetData` - Lightweight asset metadata (avoids loading)
- `UEdGraph` - Blueprint graph representation
- `UK2Node` - Base class for Blueprint nodes
- `AssetRegistryHelpers` - Utility functions for asset operations

#### Performance Considerations
- Asset Registry searches are fast (metadata only, no asset loading)
- Loading Blueprint content for deep analysis is expensive
- Should implement caching for frequently searched content
- Package path filtering can significantly reduce search scope

#### Search Patterns from Research
1. **Name-based searches:** Use Asset Registry with string pattern matching
2. **Class inheritance searches:** Use `GetDerivedClassNames()` or `GetBlueprintAssets()`
3. **Content searches:** Require loading Blueprints and analyzing graphs
4. **Reference searches:** Can traverse Blueprint node connections

### Technical Implementation Notes

**Module Dependencies:**
- `AssetRegistry` - Core asset discovery functionality
- `BlueprintGraph` - Blueprint graph analysis
- `EditorStyle` - Consistent UI if needed
- `ToolMenus` - Potential editor integration

**Key Headers to Include:**
```cpp
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "BlueprintGraph/Classes/K2Node.h"
#include "Engine/Blueprint.h"
```

**Search Implementation Strategy:**
1. Start with Asset Registry for fast metadata searches
2. Only load Blueprint content when deep analysis is required
3. Cache search results to improve performance
4. Use progressive disclosure (basic results first, detailed on demand)

### Lessons Learned
- Prefer Unreal's built-in capabilities over custom implementations
- Asset Registry is the foundation for all efficient asset searches
- Blueprint content analysis requires careful memory management
- Search scope limitation is crucial for performance in large projects

## Phase 1 Implementation Findings

### Successfully Implemented
- **Asset Registry Integration**: `FAssetRegistryModule` works perfectly for Blueprint discovery
- **Search Types**: Name pattern matching and parent class inheritance searches working
- **JSON Schema**: Comprehensive input/output format implemented
- **Performance**: Asset Registry searches are very fast (metadata only)

### Key Implementation Details
- Module dependencies: Added `AssetRegistry` and `BlueprintGraph` to build.cs
- Tool registration: Added to `UMCP_CommonTools::Register()` with proper JSON schema
- Search filtering: Uses `FARFilter` with `ClassPaths` and `PackagePaths` filters
- Result structure: Rich JSON format with match details and context information

### Search API Examples
```json
// Name search
{
  "searchType": "name",
  "searchTerm": "Player", 
  "packagePath": "/Game/Blueprints",
  "recursive": true
}

// Parent class search  
{
  "searchType": "parent_class",
  "searchTerm": "Actor"
}

// Comprehensive search
{
  "searchType": "all",
  "searchTerm": "Health"
}
```

### Technical Notes
- `AssetData.GetTagValue()` used for parent class information
- Blueprint assets found via `UBlueprint::StaticClass()->GetClassPathName()`
- JSON serialization handled through Unreal's built-in JSON utilities
- Comprehensive error handling and logging implemented

### Next Phase Considerations
- Will need to load actual Blueprint assets for content analysis
- Graph traversal will be more expensive than Asset Registry searches
- Should implement caching and progress reporting for content searches
- May need async processing for large Blueprint content analysis
