# The spell tables (`spells/data/`)

The spell tables. `SpellMgr` and its `SpellMgr*.cpp` parts load the spell data (chains, areas, pet auras, proc events, the lookups derived from the DBC files) and answer what a spell is. Nothing here casts, applies an aura or sends a packet. A spell-ID label that only looks something up ends here as a table. The directory is on the `game` target's include path.
