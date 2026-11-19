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

#include "spells/classes/mage/Mage.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellScriptQueries.h"
#include "ai/CreatureAI.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "Map.h"
#include "entities/motion/MotionMaster.h"
#include "DBCStores.h"

namespace
{
    /// The images of the mage's Mirror Image, one row each: the images without the glyph, the Arcane images and the
    /// Fire images.
    Mage::ImageRow const IMAGES[] =
    {
        { 31216, 58832, 59638, 59637 },
        { 47243, 88091, 88084, 0 },
        { 47244, 88092, 88082, 0 },
    };

    /// Mirror Image.
    uint32 const MIRROR_IMAGE = 55342;

    /// Clone Me!: cast by the mage at an image, it gives the image his look and his name.
    uint32 const CLONE_ME = 45204;

    /// Glyph of Mirror Image (GlyphProperties.dbc).
    uint32 const GLYPH_OF_MIRROR_IMAGE = 699;

    /// The mage's Arcane talent tree (TalentTab.dbc).
    uint32 const TALENT_TREE_ARCANE = 799;

    /// The mage's Fire talent tree (TalentTab.dbc).
    uint32 const TALENT_TREE_FIRE = 851;

    /// The name the images' creature AI is registered under.
    char const* const IMAGE_AI = "MirrorImageAI";

    /// The percent of the mage's spell power an image's spells take.
    int32 const IMAGE_SPELL_POWER_PERCENT = 75;

    /// How far an image stands from its mage before it follows him, in yards.
    float const IMAGE_FOLLOW_RANGE = 40.0f;

    /// The fields of the mage's visible weapons an image takes, in the order of its virtual item slots: main hand, off
    /// hand and ranged.
    uint16 const MAGE_WEAPON_FIELDS[] =
    {
        PLAYER_VISIBLE_ITEM_16_ENTRYID, PLAYER_VISIBLE_ITEM_17_ENTRYID, PLAYER_VISIBLE_ITEM_18_ENTRYID
    };

    /// The school mask of `spellId`; none when the spell store lacks it.
    SpellSchoolMask SchoolOf(uint32 spellId)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        return spell ? GetSpellSchoolMask(spell) : SPELL_SCHOOL_MASK_NONE;
    }

    /// 55342 effect 1: Mirror Image: the mage casts the summon of the images his glyph and his primary talent tree
    /// choose (Mage::ImageSummonSpell), triggered. The script ends the effect.
    SpellHandlerOutcome<void> MirrorImageEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex == EFFECT_INDEX_1)
        {
            Spells::SpecializationView const spec = Spells::ActiveSpecialization(ctx.caster);
            bool const glyphed = Mage::HoldsGlyph(spec.glyphs, MAX_GLYPH_SLOT_INDEX, GLYPH_OF_MIRROR_IMAGE);
            ctx.CastTriggered(ctx.caster, Mage::ImageSummonSpell(glyphed, spec.primaryTree));
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 45204 effect 2: Clone Me! cast by a mage at one of his images: the image takes the items of his visible weapons
    /// (main hand, off hand and ranged) in its three virtual item slots, and the script ends the effect, so the image
    /// casts no Copy Off-hand Weapon (45206) back at him. Any other target goes on to the effect's handler.
    SpellHandlerOutcome<void> CloneMeEffect(DummyEffectContext& ctx)
    {
        Unit* image = ctx.target;
        if (ctx.effIndex != EFFECT_INDEX_2 || !image || !Mage::ImageRowFor(image->GetEntry()) ||
            image->GetOwnerGuid() != ctx.caster.GetObjectGuid())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (ctx.caster.GetTypeId() == TYPEID_PLAYER)
        {
            for (uint32 slot = 0; slot < sizeof(MAGE_WEAPON_FIELDS) / sizeof(MAGE_WEAPON_FIELDS[0]); ++slot)
            {
                image->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + slot,
                                      ctx.caster.GetUInt32Value(MAGE_WEAPON_FIELDS[slot]));
            }
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// An image of a mage's Mirror Image, his guardian for the summon's 30 s. On its first update with its mage on its
    /// map it starts: the mage casts Clone Me! (45204) at it, which gives it his look, his name and his weapons, and it
    /// keeps, for its spell and its Fire Blast, 75% of his spell power of that spell's school (Mage::ImageBonusDamage).
    /// Then, while it casts nothing, it casts at the unit Mage::ImageTarget picks among his victim, his selection and
    /// his attacker, the Fire Blast when its cooldown is over (Mage::ImageSpellToCast), else its spell, with the spell
    /// power kept for that spell; a crowd control put on its target stops no cast under way. It follows him while he
    /// stands farther than IMAGE_FOLLOW_RANGE from it and it casts nothing, and stops when it starts a cast. It never
    /// swings, attacks nothing by itself and evades nothing; its stance plays no part.
    class ImageAI : public CreatureAI
    {
        public:
            explicit ImageAI(Creature* image) : CreatureAI(image), m_row(NULL), m_started(false), m_ended(false),
                m_fireBlastMs(0), m_spellBonus(0), m_fireBlastBonus(0) {}

            void UpdateAI(uint32 const diff) override
            {
                if (m_ended)
                {
                    return;
                }
                Unit* owner = Owner();
                if (!owner)
                {
                    return;
                }
                if (!m_started)
                {
                    Start(*owner);
                    if (m_ended)
                    {
                        return;
                    }
                }
                m_fireBlastMs = m_fireBlastMs > int32(diff) ? m_fireBlastMs - int32(diff) : 0;
                if (m_creature->IsNonMeleeSpellCasted(false))
                {
                    return;
                }
                Map* map = m_creature->FindMap();
                Unit* selection = map ? map->GetUnit(owner->GetSelectionGuid()) : NULL;
                Creature* image = m_creature;
                Unit* target = Mage::ImageTarget(owner->getVictim(), selection, owner->getAttackerForHelper(),
                    [image](Unit& unit)
                    {
                        return Mage::ImageMayStrike(unit.IsTargetableForAttack(), image->IsFriendlyTo(&unit),
                                                    unit.hasNegativeAuraWithInterruptFlag(AURA_INTERRUPT_FLAG_DAMAGE));
                    });
                if (target && CastAt(*target))
                {
                    if (m_creature->GetMotionMaster()->ActiveKind() == Motion::Kind::Follow)
                    {
                        m_creature->GetMotionMaster()->MovementExpired(false);
                    }
                    return;
                }
                if (!InReach(*m_creature, *owner, IMAGE_FOLLOW_RANGE) &&
                    m_creature->GetMotionMaster()->ActiveKind() != Motion::Kind::Follow)
                {
                    m_creature->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
                }
            }

            void AttackStart(Unit* /*who*/) override {}
            void AttackedBy(Unit* /*attacker*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterEvadeMode() override {}

            /// PERMIT_BASE_SPECIAL for a creature of the image table, PERMIT_BASE_NO for any other.
            static int Permissible(Creature const* creature)
            {
                return Mage::ImageRowFor(creature->GetEntry()) ? PERMIT_BASE_SPECIAL : PERMIT_BASE_NO;
            }

        private:
            /// The image's owner, looked up on the image's map by his guid; NULL when he is not there.
            Unit* Owner() const
            {
                Map* map = m_creature->FindMap();
                Unit* owner = map ? map->GetUnit(m_creature->GetOwnerGuid()) : NULL;
                return owner && owner->IsInWorld() ? owner : NULL;
            }

            /// The image as the pet the summon made it; NULL when its map holds none by its guid.
            Pet* Image() const
            {
                Map* map = m_creature->FindMap();
                return map ? map->GetPet(m_creature->GetObjectGuid()) : NULL;
            }

            /// The image's start: its row, by its creature, Clone Me! cast at it by `owner`, and the spell power its
            /// spells take from him. An image of no row ends here and does nothing.
            void Start(Unit& owner)
            {
                m_started = true;
                m_row = Mage::ImageRowFor(m_creature->GetEntry());
                m_ended = m_row == NULL;
                if (m_ended)
                {
                    return;
                }
                owner.CastSpell(m_creature, CLONE_ME, true);
                m_spellBonus = Mage::ImageBonusDamage(owner.SpellBaseDamageBonusDone(SchoolOf(m_row->spell)));
                m_fireBlastBonus = m_row->fireBlast ?
                    Mage::ImageBonusDamage(owner.SpellBaseDamageBonusDone(SchoolOf(m_row->fireBlast))) : 0;
            }

            /// Casts the image's next spell at `target` (Mage::ImageSpellToCast), or its spell when the Fire Blast
            /// cannot go; the Fire Blast's cooldown starts when it goes. Answers whether a cast started.
            bool CastAt(Unit& target)
            {
                uint32 const spell = Mage::ImageSpellToCast(*m_row, m_fireBlastMs);
                if (spell == m_row->fireBlast && Cast(target, spell, m_fireBlastBonus))
                {
                    SpellEntry const* fireBlast = sSpellStore.LookupEntry(spell);
                    m_fireBlastMs = fireBlast ? int32(GetSpellRecoveryTime(fireBlast)) : 0;
                    return true;
                }
                return Cast(target, m_row->spell, m_spellBonus);
            }

            /// Casts `spell` at `target` with `bonus` as the spell power its spells take; answers whether it started.
            bool Cast(Unit& target, uint32 spell, int32 bonus)
            {
                if (Pet* image = Image())
                {
                    image->SetBonusDamage(bonus);
                }
                return DoCastSpellIfCan(&target, spell) == CAST_OK;
            }

            Mage::ImageRow const* m_row;        ///< the image's row; NULL until its start
            bool m_started;                     ///< the image has started
            bool m_ended;                       ///< the image had no row
            int32 m_fireBlastMs;                ///< the milliseconds left of its Fire Blast's cooldown
            int32 m_spellBonus;                 ///< the spell power its spell takes
            int32 m_fireBlastBonus;             ///< the spell power its Fire Blast takes
    };
}

Mage::ImageRow const* Mage::ImageRowFor(uint32 creatureEntry)
{
    for (ImageRow const& row : IMAGES)
    {
        if (creatureEntry && row.creature == creatureEntry)
        {
            return &row;
        }
    }
    return NULL;
}

uint32 Mage::ImageSummonSpell(bool glyphed, uint32 primaryTree)
{
    ImageRow const& plain = IMAGES[0];
    ImageRow const& arcane = IMAGES[1];
    ImageRow const& fire = IMAGES[2];
    if (glyphed && primaryTree == TALENT_TREE_ARCANE)
    {
        return arcane.summonSpell;
    }
    if (glyphed && primaryTree == TALENT_TREE_FIRE)
    {
        return fire.summonSpell;
    }
    return plain.summonSpell;
}

bool Mage::HoldsGlyph(uint32 const* glyphs, size_t count, uint32 glyphId)
{
    for (size_t slot = 0; glyphs && glyphId && slot < count; ++slot)
    {
        if (glyphs[slot] == glyphId)
        {
            return true;
        }
    }
    return false;
}

int32 Mage::ImageBonusDamage(int32 spellPower)
{
    return spellPower > 0 ? int32(int64(spellPower) * IMAGE_SPELL_POWER_PERCENT / 100) : 0;
}

uint32 Mage::ImageSpellToCast(ImageRow const& row, int32 fireBlastLeftMs)
{
    return row.fireBlast && fireBlastLeftMs <= 0 ? row.fireBlast : row.spell;
}

bool Mage::ImageMayStrike(bool attackable, bool friendly, bool damageBreaks)
{
    return attackable && !friendly && !damageBreaks;
}

Unit* Mage::ImageTarget(Unit* victim, Unit* selection, Unit* attacker, std::function<bool(Unit&)> const& mayStrike)
{
    for (Unit* unit : { victim, selection, attacker })
    {
        if (unit && mayStrike(*unit))
        {
            return unit;
        }
    }
    return NULL;
}

uint32 Mage::RegisterImages(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(IMAGE_AI))
    {
        (new CreatureAIFactory<ImageAI>(IMAGE_AI))->RegisterSelf();
    }

    RegisterScript<DummyEffectHook>(registry, MIRROR_IMAGE, &MirrorImageEffect);
    RegisterScript<DummyEffectHook>(registry, CLONE_ME, &CloneMeEffect);
    return 2;
}
