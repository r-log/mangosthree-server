/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "spells/classes/priest/holy/PrayerOfMending.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstdint>

namespace
{
    /// Prayer of Mending's aura (41635): aura type 225, which no other spell uses.
    uint32 const MENDING_AURA[] = { 41635 };

    /// Prayer of Mending's heal: a cast of the holder's, for the priest, that the aura's holder takes when hit.
    uint32 const MENDING_HEAL = 33110;

    /// Glyph of Prayer of Mending: its effect 0, a dummy aura, holds the percent the first heal gains (60).
    uint32 const GLYPH_OF_PRAYER_OF_MENDING = 55685;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// The yards a jump reaches from its holder: the aura's effect radius, else its spell's range, with the priest's
    /// radius modifiers.
    float MendingRadius(SpellEntry const& aura, SpellEffectIndex effect, Unit& priest)
    {
        SpellEffectEntry const* spellEffect = aura.GetSpellEffect(effect);
        float radius;
        if (spellEffect && spellEffect->GetRadiusIndex())
        {
            radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(spellEffect->GetRadiusIndex()));
        }
        else
        {
            radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(aura.RangeIndex));
        }
        if (SpellModMgr* mods = priest.GetSpellMods())
        {
            mods->ApplySpellMod(aura.ID, SPELLMOD_RADIUS, radius);
        }
        return radius;
    }

    /// The charges Prayer of Mending's aura holds on its first holder: the data's, with the priest's charge modifiers
    /// (the two pieces of Regalia of Faith, 60154, add one), as the holder's constructor counts them.
    uint32 MendingFullCharges(SpellEntry const& aura, Unit* priest)
    {
        uint32 charges = aura.GetProcCharges();
        if (SpellModMgr* mods = priest ? priest->GetSpellMods() : NULL)
        {
            mods->ApplySpellMod(aura.ID, SPELLMOD_CHARGES, charges);
        }
        return charges;
    }

    /// The part of Prayer of Mending's heal that the priest's spell power adds after its amount, as
    /// Unit::SpellHealingBonusDone reckons it (his healing bonus through the heal's coefficient), so that the glyph
    /// raises the whole heal; 0 when the heal's spell is missing.
    int32 MendingSpellPowerPart(Unit& priest)
    {
        SpellEntry const* heal = sSpellStore.LookupEntry(MENDING_HEAL);
        if (!heal)
        {
            return 0;
        }
        return priest.SpellBonusWithCoeffs(heal, 0, priest.SpellBaseHealingBonusDone(GetSpellSchoolMask(heal)), 0, HEAL,
                                           true, SPELL_BONUS_KIND_HEAL);
    }

    /// 41635: Prayer of Mending's aura, when its holder is hit: the proc handler of aura type 225. While the holder
    /// keeps more than one charge and its caster is a player in the world, the aura jumps to a unit of the holder's
    /// group within its radius (OnProcContext::RandomGroupUnitNear, with the radius the priest's modifiers give), when
    /// there is one (OnProcContext::MoveAuraTo, one charge fewer). Then the holder is healed: it casts Prayer of Mending's heal at itself, triggered, for the aura's amount,
    /// raised by the priest's Glyph of Prayer of Mending when this is the cast's first heal, by its percent of the whole
    /// heal, his spell power's part included (Priest::MendingHealAmount),
    /// with the priest as the original caster and the aura's spell as the trigger, and no triggering aura, so that the
    /// heal starts the priest's procs. OK, so the engine spends the holder's charge (the old holder is gone when the
    /// aura jumped).
    ProcOutcome MendingProc(OnProcContext& ctx)
    {
        // read before the jump: the old holder and its aura may be gone when it returns
        ObjectGuid const casterGuid = ctx.aura.CasterGuid();
        int32 const amount = ctx.triggerAmount;
        uint32 const charges = ctx.aura.Charges();
        Unit* priest = ctx.aura.Caster();
        int32 const glyph = priest ? Priest::MendingGlyph(*priest) : 0;

        uint32 const jumpCharges = Priest::MendingJumpCharges(charges);
        if (jumpCharges && priest && casterGuid.IsPlayer() && priest->GetTypeId() == TYPEID_PLAYER)
        {
            float const radius = MendingRadius(ctx.spell, ctx.effIndex, *priest);
            if (Unit* next = ctx.RandomGroupUnitNear(radius))
            {
                ctx.MoveAuraTo(*next, *priest, jumpCharges);
            }
        }

        uint32 const fullCharges = MendingFullCharges(ctx.spell, priest);
        int32 const spellPower = priest && glyph > 0 && charges == fullCharges ? MendingSpellPowerPart(*priest) : 0;
        int32 const heal = Priest::MendingHealAmount(amount, spellPower, glyph, charges, fullCharges);
        ctx.caster.CastCustomSpell(&ctx.caster, MENDING_HEAL, &heal, NULL, NULL, true, NULL, NULL, casterGuid,
                                   &ctx.spell);
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }
}

uint32 Priest::RegisterPrayerOfMending(SpellHandlerRegistry& registry)
{
    return RegisterRanks<OnProcHook>(registry, MENDING_AURA, &MendingProc);
}

int32 Priest::MendingHealAmount(int32 amount, int32 spellPower, int32 glyph, uint32 charges, uint32 fullCharges)
{
    if (glyph <= 0 || !fullCharges || charges != fullCharges)
    {
        return amount;
    }
    int64 const raised = int64(amount) + (int64(amount) + int64(spellPower)) * glyph / 100;
    return raised > INT32_MAX ? INT32_MAX : int32(raised);
}

uint32 Priest::MendingJumpCharges(uint32 charges)
{
    return charges > 1 ? charges - 1 : 0;
}

int32 Priest::MendingGlyph(Unit const& priest)
{
    SpellAuraHolder* glyph = priest.GetSpellAuraHolder(GLYPH_OF_PRAYER_OF_MENDING);
    return glyph ? AuraHandle(*glyph).Amount(EFFECT_INDEX_0) : 0;
}
