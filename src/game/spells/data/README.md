# The spell tables (`spells/data/`)

The spell tables. `SpellMgr` and its `SpellMgr*.cpp` parts load the spell data (chains, areas, pet auras, proc events, the lookups derived from the DBC files) and answer what a spell is. Nothing here sends a packet. The one cast is `SpellArea::ApplyOrRemoveSpellIfCan`, which casts an area's spell on a player who meets its requirements and removes it from one who does not. A spell-ID label that only looks something up ends here as a table. The directory is on the `game` target's include path.
