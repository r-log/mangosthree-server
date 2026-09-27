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

/**
 * @file PlayerRune.cpp
 * @brief Decoupling D4k: the character's side of its runes (spells/RuneMgr).
 *
 * RuneMgr holds the six runes and the rules over them and never sees the character. The
 * wrappers here are the part of the old RuneMgr bodies that read or wrote the character: the
 * regeneration auras and the haste rating, the facts about a slot's convert aura, the class, the
 * regeneration fields, the T11 Death Eater cast and the removal of a convert aura. Each input is
 * computed with the old expression, in the old order, just before the one call into the manager;
 * each write is a callback the manager calls where the old body wrote. The forwarders with no
 * such input stay inline in Player.h, and the packets go through Player::SessionSink().
 */

#include "Player.h"
#include "SpellAuras.h"
#include "SpellMgr.h"

namespace
{
    /// The rune regeneration fields, written where RuneMgr wrote them before decoupling D4k.
    RuneMgr::RegenSink RegenFields(Player* player)
    {
        return [player](uint32 runeType, float value)
        {
            player->SetFloatValue(PLAYER_RUNE_REGEN_1 + runeType, value);
        };
    }

    /// RuneMgr::RestoreBaseRune's last statement before decoupling D4k: a convert-rune aura that
    /// no slot holds any more leaves its target.
    void DropConvertAura(Aura const* aura)
    {
        if (Unit* target = aura->GetTarget())
        {
            target->RemoveSpellAuraHolder(const_cast<Aura*>(aura)->GetHolder());
        }
    }
}

void Player::UpdateRuneRegen(RuneType rune)
{
    float auraMod = 1.0f;
    Unit::AuraList const& regenAuras = GetAurasByType(SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    for (Unit::AuraList::const_iterator i = regenAuras.begin(); i != regenAuras.end(); ++i)
        if ((*i)->GetMiscValue() == POWER_RUNE && (*i)->GetSpellEffect()->EffectMiscValue_1 == rune)
        {
            auraMod *= (100.0f + (*i)->GetModifier()->m_amount) / 100.0f;
        }

    // Unholy Presence
    if (Aura* aura = GetAura(48265, EFFECT_INDEX_0))
    {
        auraMod *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }

    // Runic Corruption
    if (Aura* aura = GetAura(51460, EFFECT_INDEX_0))
    {
        auraMod *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }

    float hasteRating = GetRatingBonusValue(CR_HASTE_MELEE);

    m_runeMgr.UpdateRuneRegen(rune, auraMod, hasteRating, RegenFields(this));
}

void Player::UpdateRuneRegen()
{
    for (uint8 i = 0; i < NUM_RUNE_TYPES; ++i)
    {
        UpdateRuneRegen(RuneType(i));
    }
}

void Player::AddRuneByAuraEffect(uint8 index, RuneType newType, Aura const* aura)
{
    // Item - Death Knight T11 DPS 4P Bonus
    if (newType == RUNE_DEATH && HasAura(90459))
    {
        CastSpell(this, 90507, true);   // Death Eater
    }

    m_runeMgr.SetRuneConvertAura(index, aura); m_runeMgr.ConvertRune(index, newType, SessionSink());
}

void Player::RestoreBaseRune(uint8 index)
{
    Aura const* aura = m_runeMgr.GetRuneConvertAura(index);

    RuneMgr::ConvertAuraFacts facts;
    // If rune was converted by a non-pasive aura that still active we should keep it converted
    facts.nonPassive = aura && !IsPassiveSpell(aura->GetSpellProto());
    // Blood of the North
    facts.bloodOfTheNorthHeld = aura && aura->GetId() == 54637 && HasAura(54637);
    // Don't drop passive talents providing rune convertion
    facts.convertsRunes = aura && aura->GetModifier()->m_auraname == SPELL_AURA_CONVERT_RUNE;

    m_runeMgr.RestoreBaseRune(index, facts, SessionSink(), DropConvertAura);
}

void Player::InitRunes()
{
    m_runeMgr.Init(getClass(), RegenFields(this));
}
