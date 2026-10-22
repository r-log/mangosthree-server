# The aura (`spells/auras/`)

The aura. `Aura` and `SpellAuraHolder` (`SpellAuras.h`, `SpellAuras.cpp`), the aura type handlers by family (`SpellAura*.cpp`), the aura definitions (`SpellAuraDefines.h`), `Unit`'s aura members (`UnitAura.cpp`) and its proc handling (`UnitAuraProcHandler.cpp`). `CheckLayout` gates this directory, with `spells/AuraContainer.h`, as the `spells/aura` seam: an include into the seam from a file outside it, the spell peer's own files included, is listed per includer file and header and cannot grow. The directory is on the `game` target's include path.
