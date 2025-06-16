# Search Blueprints Tool - Technical Specification

## Overview

This document outlines the implementation plan for the `search_blueprints` MCP tool that will allow external clients to search for Blueprints based on various criteria including strings, asset references, and variables.

## Tool Description

**Tool Name:** `search_blueprints`
**Purpose:** Search Blueprints for strings, asset references, variables, and other content with detailed results including asset paths and specific reference locations.

## Research Summary

Based on research into Unreal Engine's capabilities, we have several powerful APIs available:

### 1. Asset Registry API
- `FAssetRegistryModule` for finding Blueprint assets
- `GetAssets()` with filters for finding Blueprints by class
- `GetBlueprintAssets()` (UE5+) for finding Blueprint subclasses
- Asset tags for searching Blueprint metadata

### 2. Blueprint Content Search
- Unreal has built-in "Find References" functionality
- Blueprint graphs can be searched for node references
- Variable references can be found across multiple Blueprints
- String-based searching within Blueprint content

### 3. Key Unreal Classes
- `UBlueprint` - The Blueprint asset itself
- `UBlueprintGeneratedClass` - The runtime class generated from Blueprint
- `FAssetData` - Metadata about assets without loading them
- `AssetRegistryHelpers` - Helper functions for asset operations

## Implementation Plan

### Phase 1: Basic Blueprint Asset Discovery
**Goal:** Find Blueprint assets by name patterns and class inheritance

**Implementation Details:**
1. Use `FAssetRegistryModule` to get all Blueprint assets
2. Support filtering by:
   - Blueprint name (string matching)
   - Parent class (inheritance search)
   - Package path (folder location)
   - Asset tags

**API Design:**
```json
{
  "name": "search_blueprints",
  "description": "Search for Blueprint assets based on various criteria",
  "inputSchema": {
    "type": "object",
    "properties": {
      "searchType": {
        "type": "string",
        "enum": ["name", "parent_class", "all"],
        "description": "Type of search to perform"
      },
      "searchTerm": {
        "type": "string",
        "description": "Search term (Blueprint name, class name, etc.)"
      },
      "packagePath": {
        "type": "string",
        "description": "Optional package path to limit search scope (e.g., '/Game/Blueprints')"
      },
      "recursive": {
        "type": "boolean",
        "default": true,
        "description": "Whether to search recursively in subfolders"
      }
    },
    "required": ["searchType", "searchTerm"]
  }
}
```

### Phase 2: Blueprint Content Analysis
**Goal:** Search within Blueprint contents for variables, nodes, and references

**Implementation Details:**
1. Load Blueprint assets and analyze their graphs
2. Search for:
   - Variable references (get/set nodes)
   - Function calls
   - Asset references (texture, material, etc.)
   - String literals in Blueprint nodes
   - Component references

**Extended API:**
```json
{
  "contentSearch": {
    "type": "object",
    "properties": {
      "searchInContent": {
        "type": "boolean",
        "default": false,
        "description": "Whether to search within Blueprint content"
      },
      "contentType": {
        "type": "string",
        "enum": ["variable", "function", "asset_reference", "string_literal", "all"],
        "description": "Type of content to search for"
      },
      "contentTerm": {
        "type": "string",
        "description": "Content search term"
      }
    }
  }
}
```

### Phase 3: Advanced Reference Finding
**Goal:** Implement comprehensive reference finding similar to UE's "Find References"

**Implementation Details:**
1. Cross-Blueprint reference analysis
2. Dependency tracking
3. Usage statistics and metrics

## Output Format

The tool will return structured results with the following format:

```json
{
  "results": [
    {
      "assetPath": "/Game/Blueprints/MyBlueprint.MyBlueprint",
      "assetName": "MyBlueprint",
      "packagePath": "/Game/Blueprints/",
      "parentClass": "Actor",
      "matches": [
        {
          "type": "asset_name",
          "location": "Blueprint Asset",
          "context": "Blueprint name matches search term"
        },
        {
          "type": "variable",
          "location": "EventGraph/Node_123",
          "context": "Variable 'PlayerHealth' referenced in Set Variable node",
          "variableName": "PlayerHealth",
          "nodeType": "Set"
        },
        {
          "type": "asset_reference",
          "location": "ConstructionScript/Node_456",
          "context": "Material reference to '/Game/Materials/PlayerMaterial'",
          "referencedAsset": "/Game/Materials/PlayerMaterial"
        }
      ]
    }
  ],
  "totalResults": 1,
  "searchCriteria": {
    "searchType": "name",
    "searchTerm": "Player",
    "contentSearch": true
  }
}
```

## Technical Implementation Details

### Core Functions
1. `SearchBlueprintsByName()` - Find Blueprints by name pattern
2. `SearchBlueprintsByParentClass()` - Find Blueprint subclasses
3. `SearchBlueprintContent()` - Search within Blueprint graphs
4. `AnalyzeAssetReferences()` - Find asset references in Blueprints

### Key Unreal Engine APIs to Use
- `FAssetRegistryModule::Get().GetAssets()`
- `FAssetRegistryModule::Get().GetDerivedClassNames()`
- `UBlueprint::GetBlueprintGraph()`
- `UEdGraph::GetAllNodesOfClass()`
- Asset tagging system for metadata search

### Performance Considerations
1. Use Asset Registry for fast metadata searches (avoid loading assets when possible)
2. Implement caching for frequently searched terms
3. Provide progress callbacks for long-running searches
4. Limit search scope with package path filters

### Error Handling
1. Handle cases where Blueprints fail to load
2. Gracefully handle corrupted or invalid Blueprint assets
3. Provide meaningful error messages for invalid search parameters
4. Handle permission/access issues for protected assets

## Future Enhancements

### Phase 4: Advanced Features
1. **Regex Support:** Allow regular expression patterns in search terms
2. **Fuzzy Matching:** Implement approximate string matching
3. **Search History:** Cache and provide recent search results
4. **Export Results:** Support exporting search results to various formats
5. **Real-time Updates:** Watch for Blueprint changes and update search indices

### Phase 5: Integration Features
1. **Visual Search Results:** Generate thumbnail previews of found Blueprints
2. **Dependency Graphs:** Create visual dependency maps
3. **Bulk Operations:** Support bulk modification of found Blueprints
4. **Custom Filters:** Allow users to define custom search criteria

## Testing Strategy

### Unit Tests
1. Test each search type independently
2. Verify correct handling of edge cases (empty results, invalid paths)
3. Test performance with large numbers of Blueprints

### Integration Tests
1. Test with real project content
2. Verify cross-Blueprint reference detection
3. Test with various Blueprint types (Actor, Component, etc.)

### Performance Tests
1. Benchmark search times with different project sizes
2. Memory usage analysis during content searches
3. Stress testing with concurrent search requests

## Security Considerations

1. **Path Validation:** Ensure search paths are within allowed project directories
2. **Resource Limits:** Prevent excessive memory usage during large searches
3. **Access Control:** Respect Blueprint access permissions if implemented
4. **Sanitization:** Sanitize search terms to prevent injection attacks

## Dependencies

### Required Unreal Modules
- `AssetRegistry` - For asset discovery and metadata
- `BlueprintGraph` - For Blueprint graph analysis
- `ToolMenus` - For potential editor integration
- `EditorStyle` - For consistent editor theming

### Required Headers
```cpp
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "BlueprintGraph/Classes/K2Node.h"
#include "Engine/Blueprint.h"
```

## Implementation Priority

1. **High Priority:** Basic asset discovery by name and class
2. **Medium Priority:** Content search within Blueprint graphs
3. **Low Priority:** Advanced reference analysis and visual features

This specification provides a comprehensive roadmap for implementing a powerful Blueprint search tool that leverages Unreal Engine's existing capabilities while providing a clean, extensible API for MCP clients. 