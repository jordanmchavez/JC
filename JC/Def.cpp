#include "JC/Def.h"

#include "JC/File.h"
#include "JC/Json.h"

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

/*
---BEGIN CLAUDE---

This is a first-draft of my data definition loading interface.
The idea is that all non-binary game data is stored in .json5 files with a common format.
"Non-binary game data" means: sprite atlases, font definitions, unit definitions, spells, abilities, races, battle mechanics, etc.
The idea is to have as much of the game data-driven as possible.
Instead of having dedicated files for each data type (one for units, one for spells, etc), I want it so that any file can have any definitions.
The game loads all json5 def files on startup. Top-level arrays are aggregated across file. For example:
// HumanUnits.json5
{
	units: [ some unit defs, ... ]
}

// OrcUnits.json5
{
	units: [ some unit defs, ... ]
}

The two files both have unit defs. After loading all json5 def files, there would be a single `units` array available for the game `Unit` subsystem to ingest.

Each subsystem defines the json format that it cares about. For example, `Unit.cpp` defines the `UnitDef` struct and the corresponding `Json` converters (see `JC/Json.cpp`).
`Unit.cpp` then calls Def::RegisterArray<UnitDef>("units", 1024).
The Def system has its dedicated arena. Def allocates a block of 1024 max UnitDefs in its arena. All unit loads across all files to into that array after being parsed.

After the loading step is done, each subsytem has an `InitDefs` call that can "pull" the loaded defs via `Span<Unit const> Def::GetArray<Unit>()`, which returns the data.
Def system can then free its arena, its job is done.
This also allows a uniform path to dynamically reload all game data from hot-changed defs (though we can't *selectively* reload files).

`Def` supports both single objects (strings, complex objects) and arrays.

Since def are spread across multiple json files, we can either allow "latest wins" for duplicate defs, or issue an error.
Perhaps a mix, where we load json defs in bnatches (main game, dlc1, dlc2, user stuff), allowing each "batch" to overwrite stuff already defined, but disallow batches from conflicting internally.

What are your thoughts on this design from an architecture perspective? Any questions? What am I not thinking of? What could be improved upon? What are the pitfalls/landmines?
---END CLAUDE---
*/
template<class T> void RegisterObject(Str name) { RegisterJsonTraits(name, GetJsonTraits(T(), 1); }
template<class T> void RegisterArray(Str name, U32 maxLen) { RegisterJsonTraits(name, GetJsonTraits(T(), m1xLen); }

void RegisterJsonTraits(Str name, Json::Traits const* jsonTraits, U32 maxLen);

template <class T> T const* GetObject(Str name);
template <class T> Span<T const> GetArray(Str name);

Res<> Load(Str path);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def