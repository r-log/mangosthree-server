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

/// Tests of the spell handler registry.
///
/// The registry is tested without a map: its sites here are test sites with their own keys and contexts, and the game's
/// sites (HandleAuraDummy's, HandleAuraTransform's, HandleModThreat's, EffectTransmitted's, EffectEnergize's,
/// EffectActivateObject's, EffectResurrect's, the two of SetTargetMap, CheckCast's, CheckTarget's, EffectWeaponDmg's,
/// EffectSchoolDMG's, EffectTriggerSpell's, the two of EffectTeleportUnits, the generic and five family switches of
/// EffectDummy, the five of SpellAuraPeriodic.cpp, the eighteen of SpellAuras.cpp and the generic and five family
/// switches of EffectScriptEffect, GetSpellAllowedInLocationError's, and the dummy-aura switches of
/// SpellDamageBonusTaken, SpellHealingBonusDone and MeleeDamageBonusTaken, the generic and seven family
/// switches of HandleDummyAuraProc, HandleHasteAuraProc's, HandleSpellCritChanceAuraProc's and the two of
/// HandleProcTriggerSpellAuraProc) are checked for their keys, their defaults and their contexts,
/// and run where a body needs no live Unit (the quest-tame labels, the removal labels on a mode that keeps them off the
/// Unit, the transmitted-object, energize, resurrect, targeting, cast-check and target-check defaults, the
/// periodic-trigger labels on such a mode, the health labels on an apply that is not real, the health default, the
/// trigger-spell labels that only set the trigger spell id, the Sniper Training ranks on a standing player shape, the
/// self-cast trigger labels, the health-level removal labels on a creature below full health, the periodic dummy
/// tick labels that return before they act (Forsaken Skills, Force of Nature, Mirror Image off its first tick,
/// Frenzied Regeneration on a creature with no rage), the boost labels that only set the boost spell ids, the
/// Deathbloom and Pushing the Limit labels on a holder whose remove mode the test sets, the
/// energize injector labels on a bare Creature, the 54732 defibrillate roll, which has no failure spell, the
/// area-target labels on bare Creatures, the cast-check labels on a creature caster, the target-check labels on
/// creature targets, the weapon-damage and divided school-damage labels on a target list built by hand, the other
/// school-damage labels on creatures with their update fields, the Vanish, Cloak of Shadows, Shadowfiend and Mirror
/// Image labels on creatures with no aura and no pet, the recall label on creature targets, and the dummy-effect labels
/// that end the effect with no unit target, with no cast item, for a creature caster or for a caster with no pet, and
/// the script-effect labels that end the effect with no unit target, Bending Shinbone for a creature caster, Break
/// Kaliri Egg for a spell with no affective caster object, the Mirror Image Aura for an original caster that is
/// none or no player, and for a player on a session with no socket, whose cast of a spell on cooldown fails to its
/// session, Death Gate and Emblazon Runeblade for a damage that names no spell in the test's spell store, and the
/// location-check labels for no player, a player in no battleground and one in a battleground the manager does not
/// hold, the bonus labels on an aura's storage with its modifier set, for a creature and a player shape, and the
/// dummy and proc-trigger labels that set what the proc casts, or fail it, without reading the unit);
/// a body that casts, sets a display, reads a level or an aura or acts on a game object needs a live Unit or game
/// object, which the harness record covers where a scenario reaches it (931: 41101 and 53790, applied and removed; the
/// coverage scenario two-feigns-one-lift: the feign-death body, through 29266 and 31261; 933: the generic dummy proc
/// label 66063, Earth Shield, whose 66064 heal the record holds, and the triggered-spell proc site's miss on
/// Lightning Shield's trigger 26364, whose damage the record holds; no scenario reaches a druid,
/// quest-tame, transform, threat, transmitted-object, energize, activate-object, resurrect, targeting, cast-check,
/// target-check, weapon-damage, school-damage, trigger-spell, teleport, dummy-effect, periodic-aura, boost,
/// script-effect, location-check, bonus, other dummy proc, haste, crit-chance or proc-trigger label, nor another
/// removal one).
/// Each dispatch mutant the note names has a test here that kills it:
///   lost key                 SpellHandlerRegistry_FindReturnsTheRegisteredFunction,
///                            AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances,
///                            AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyFourLabelsAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheTableRegistersEveryRowOnce,
///                            AuraDummyHandlers_TheHunterShamanAndPaladinSitesHoldTheirLabelsAndNoDefault,
///                            AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault,
///                            AuraControlHandlers_TheThreatSiteHoldsItsTwoLabelsAndNoDefault,
///                            SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault,
///                            AuraPeriodicHandlers_TheProcTriggerSiteHoldsItsTwoLabelsAndTheDefault,
///                            AuraPeriodicHandlers_ThePeriodicTriggerSiteHoldsItsFourLabelsAndTheDefault,
///                            AuraPeriodicHandlers_TheEnergizeSiteHoldsItsFiveLabelsAndTheDefault,
///                            AuraPeriodicHandlers_TheRogueSiteHoldsItsOneLabelAndNoDefault,
///                            AuraPeriodicHandlers_TheIncreaseHealthSiteHoldsItsFourteenLabelsAndTheDefault,
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault,
///                            AurasHandlers_TheTableRegistersEveryRowOnce,
///                            AurasHandlers_TheTriggerSpellGenericSiteHoldsItsLabelsAndTheDefault,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheTickSitesHoldTheirLabelsAndTheDamageDefault,
///                            AurasHandlers_ThePeriodicDummyTickSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheTableRegistersEveryRowOnce,
///                            SpellEffectScriptHandlers_TheFamilySitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellMgrHandlers_TheTableRegistersEveryRowOnce,
///                            SpellMgrHandlers_TheLocationSiteHoldsItsTwentyLabelsAndNoDefault,
///                            UnitSpellBonusHandlers_TheTableRegistersEveryRowOnce,
///                            UnitSpellBonusHandlers_TheThreeSitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheTableRegistersEveryRowOnce,
///                            UnitAuraProcHandlerHandlers_TheThreeDummySitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheFiveFamilySitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheHasteCritAndTriggerSitesHoldTheirLabelsAndNoDefault
///   wrong site               SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyFourLabelsAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraDummyHandlers_TheHunterShamanAndPaladinSitesHoldTheirLabelsAndNoDefault,
///                            AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault,
///                            AuraControlHandlers_TheThreatSiteHoldsItsTwoLabelsAndNoDefault,
///                            SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault,
///                            the five AuraPeriodicHandlers_The...SiteHolds... tests (no id at another site),
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault,
///                            AurasHandlers_TheTriggerSpellGenericSiteHoldsItsLabelsAndTheDefault,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheTickSitesHoldTheirLabelsAndTheDamageDefault,
///                            AurasHandlers_ThePeriodicDummyTickSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheFamilySitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellMgrHandlers_TheLocationSiteHoldsItsTwentyLabelsAndNoDefault,
///                            UnitSpellBonusHandlers_TheThreeSitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheThreeDummySitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheFiveFamilySitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheHasteCritAndTriggerSitesHoldTheirLabelsAndNoDefault
///   default first            SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss,
///                            AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs
///   a lost default           AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault,
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            AurasHandlers_TheTriggerSpellGenericSiteHoldsItsLabelsAndTheDefault,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheTickSitesHoldTheirLabelsAndTheDamageDefault,
///                            AurasHandlers_ThePeriodicDummyTickSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheFamilySitesHoldTheirLabelsAndDefaults
///   a default added          AuraDummyHandlers_TheHunterShamanAndPaladinSitesHoldTheirLabelsAndNoDefault,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheTickSitesHoldTheirLabelsAndTheDamageDefault,
///                            AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheFamilySitesHoldTheirLabelsAndDefaults,
///                            SpellEffectScriptHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellMgrHandlers_TheLocationSiteHoldsItsTwentyLabelsAndNoDefault,
///                            UnitSpellBonusHandlers_TheThreeSitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheThreeDummySitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheFiveFamilySitesHoldTheirLabelsAndNoDefault,
///                            UnitAuraProcHandlerHandlers_TheHasteCritAndTriggerSitesHoldTheirLabelsAndNoDefault
///   Continue taken as Return SpellHandlerRegistry_ContinueAndReturnAreDistinct,
///                            AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts,
///                            AuraPeriodicHandlers_TheProcTriggerSiteHoldsItsTwoLabelsAndTheDefault,
///                            AuraPeriodicHandlers_ThePeriodicTriggerSiteHoldsItsFourLabelsAndTheDefault,
///                            AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs,
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            AurasHandlers_FeedCapturedAnimalSetsTheTriggerSpell,
///                            AurasHandlers_TheTriggerSpellGenericSiteHoldsItsLabelsAndTheDefault,
///                            AurasHandlers_SniperTrainingCountsDownTheModifierThroughTheAura,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheTickSitesHoldTheirLabelsAndTheDamageDefault,
///                            AurasHandlers_ThePeriodicDummyTickSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheBoostBodiesReadTheRemoveModeThroughTheHolder,
///                            SpellEffectScriptHandlers_TheFamilySitesHoldTheirLabelsAndDefaults,
///                            UnitSpellBonusHandlers_TheDamageTakenBodiesScaleByTheAurasAmount,
///                            UnitSpellBonusHandlers_TheLifebloomIdolsAddASeventhOfTheAmount,
///                            UnitAuraProcHandlerHandlers_TheTriggerLabelsSetWhatTheProcCasts,
///                            UnitAuraProcHandlerHandlers_IgniteTakesItsRanksShareOfTheDamage,
///                            UnitAuraProcHandlerHandlers_TheFamilyTriggerLabelsSetWhatTheProcCasts,
///                            UnitAuraProcHandlerHandlers_TheProcTriggerLabelsSetTheSpellOrTheTarget,
///                            UnitAuraProcHandlerHandlers_TheProcTriggerGuardsEndTheProcBeforeTheUnitIsRead
///   Return taken as Continue SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault,
///                            AurasHandlers_SniperTrainingCountsDownTheModifierThroughTheAura,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_ThePeriodicDummyTickBodiesReturnWhenThereIsNothingToDo,
///                            AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_TheBoostBodiesReadTheRemoveModeThroughTheHolder,
///                            SpellEffectScriptHandlers_ANullTargetEndsTheEffect,
///                            SpellMgrHandlers_TheLocationBodiesAnswerForAPlayerOutsideABattleground,
///                            SpellMgrHandlers_TheLocationBodiesAnswerForNoPlayer,
///                            UnitAuraProcHandlerHandlers_IgniteTakesItsRanksShareOfTheDamage,
///                            UnitAuraProcHandlerHandlers_TheGuardsEndTheProcBeforeTheUnitIsRead,
///                            UnitAuraProcHandlerHandlers_TheFamilyTriggerLabelsSetWhatTheProcCasts,
///                            UnitAuraProcHandlerHandlers_TheFamilyGuardsEndTheProcBeforeTheUnitIsRead,
///                            UnitAuraProcHandlerHandlers_TheProcTriggerGuardsEndTheProcBeforeTheUnitIsRead
///   a lost fall-through      AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs
///   a lost loop continue     SpellHandlerRegistry_LoopContinueIsAFourthOutcome,
///                            SpellHandlerRegistry_ALoopContinueSkipsTheRestOfTheLoopBody,
///                            UnitSpellBonusHandlers_TheDamageTakenBodiesScaleByTheAurasAmount
///   a lost live-out          SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite,
///                            AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheApplyRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheQuestTameContextAliasesFinalSpellId,
///                            AuraShapeshiftHandlers_TheTransformContextAliasesTheTargetLocal,
///                            AuraControlHandlers_TheThreatContextAliasesTheThreeLocals,
///                            SpellEffectTailHandlers_TheTransmittedContextAliasesTheCasterAndTheEntry,
///                            the five AuraPeriodicHandlers_The...ContextAliases... tests,
///                            SpellEffectHealPowerHandlers_TheEnergizeContextAliasesTheSpellAndTheLevelLocals,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectContextAliasesTheSpell,
///                            SpellEffectObjectCombatHandlers_TheResurrectContextAliasesTheSpell,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaContextAliasesTheSpellAndTheLocals,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummyContextAliasesTheSpell,
///                            SpellChecksHandlers_TheCastAuraDummyContextAliasesTheCaster,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetContextAliasesTheCasterAndTheTarget,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostContextAliasesTheCaster,
///                            SpellEffectDummyHandlers_TheGenericContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheMageContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheWarriorContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheRogueContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheHunterContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_ThePaladinContextAliasesTheSpell,
///                            AurasHandlers_TheTriggerSpellContextAliasesItsLocals,
///                            AurasHandlers_FeedCapturedAnimalSetsTheTriggerSpell,
///                            AurasHandlers_TheTickContextsAliasTheirLocals,
///                            AurasHandlers_SniperTrainingCountsDownTheModifierThroughTheAura,
///                            AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults,
///                            AurasHandlers_ThePeriodicDummyTickContextAliasesItsLocals,
///                            AurasHandlers_TheBoostContextAliasesItsLocals,
///                            AurasHandlers_TheBoostBodiesReadTheRemoveModeThroughTheHolder,
///                            SpellEffectScriptHandlers_TheContextAliasesTheSpellsMembers,
///                            SpellEffectScriptHandlers_BendingShinboneReadsTheItemTargetThroughTheContext,
///                            SpellEffectScriptHandlers_BreakKaliriEggSummonsFromTheSpellsAffectiveCasterObject,
///                            SpellEffectScriptHandlers_TheMirrorImageAuraReadsTheOriginalCasterThroughTheContext,
///                            SpellEffectScriptHandlers_DeathGateAndEmblazonRunebladeCastTheDamagesSpell,
///                            SpellMgrHandlers_TheLocationContextAliasesTheMapAndThePlayer,
///                            UnitSpellBonusHandlers_TheContextsAliasTheirLocals,
///                            UnitSpellBonusHandlers_TheDamageTakenBodiesScaleByTheAurasAmount,
///                            UnitSpellBonusHandlers_TheLifebloomIdolsAddASeventhOfTheAmount,
///                            UnitAuraProcHandlerHandlers_TheContextAliasesTheFunctionsLocals,
///                            UnitAuraProcHandlerHandlers_TheTriggerLabelsSetWhatTheProcCasts,
///                            UnitAuraProcHandlerHandlers_IgniteTakesItsRanksShareOfTheDamage,
///                            UnitAuraProcHandlerHandlers_TheFamilyTriggerLabelsSetWhatTheProcCasts,
///                            UnitAuraProcHandlerHandlers_TheHasteCritAndTriggerContextsAliasTheFunctionsLocals,
///                            UnitAuraProcHandlerHandlers_TheProcTriggerLabelsSetTheSpellOrTheTarget
///   a rank's value changed   AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts (all 18 id -> value pairs)
///   a returned value changed SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellMgrHandlers_TheLocationBodiesAnswerForAPlayerOutsideABattleground,
///                            SpellMgrHandlers_TheLocationBodiesAnswerForNoPlayer,
///                            UnitAuraProcHandlerHandlers_TheGuardsEndTheProcBeforeTheUnitIsRead,
///                            UnitAuraProcHandlerHandlers_TheFamilyGuardsEndTheProcBeforeTheUnitIsRead,
///                            UnitAuraProcHandlerHandlers_TheProcTriggerGuardsEndTheProcBeforeTheUnitIsRead
///   a damage value changed   SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits,
///                            UnitSpellBonusHandlers_TheDamageTakenBodiesScaleByTheAurasAmount,
///                            UnitSpellBonusHandlers_TheLifebloomIdolsAddASeventhOfTheAmount,
///                            UnitAuraProcHandlerHandlers_TheTriggerLabelsSetWhatTheProcCasts,
///                            UnitAuraProcHandlerHandlers_IgniteTakesItsRanksShareOfTheDamage,
///                            UnitAuraProcHandlerHandlers_TheFamilyTriggerLabelsSetWhatTheProcCasts
///   a stale removal mode     AuraDummyHandlers_ARemovalBodyReadsTheModeWhenItRuns,
///                            AurasHandlers_TheBoostBodiesReadTheRemoveModeThroughTheHolder

#include "TestHarness.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AuraDummyHandlers.h"
#include "spells/handlers/AuraShapeshiftHandlers.h"
#include "spells/handlers/AuraControlHandlers.h"
#include "spells/handlers/SpellEffectTailHandlers.h"
#include "spells/handlers/AuraPeriodicHandlers.h"
#include "spells/handlers/SpellEffectHealPowerHandlers.h"
#include "spells/handlers/SpellEffectObjectCombatHandlers.h"
#include "spells/handlers/SpellTargetingHandlers.h"
#include "spells/handlers/SpellChecksHandlers.h"
#include "spells/handlers/SpellCheckTargetHandlers.h"
#include "spells/handlers/SpellEffectSkillEnchantPetHandlers.h"
#include "spells/handlers/SpellEffectDamageTeleportHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectScriptHandlers.h"
#include "spells/handlers/SpellMgrHandlers.h"
#include "spells/handlers/UnitSpellBonusHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "DBCStructure.h"
#include "Unit.h"                                               // SpellAuraProcResult
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"                                       // PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL
#include "spells/pipeline/Spell.h"                                              // SpellCastTargets
#include "Creature.h"
#include "Player.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Auth/BigNumber.h"
#include "Log.h"
#include "MapPhase.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <list>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // A void site, like HandleAuraDummy's: the context holds one live-out local and a trace.
    struct VoidContext
    {
        VoidContext(int& liveOut) : value(liveOut), handled(0), defaults(0) {}

        int& value;
        uint32 handled;
        uint32 defaults;
    };

    struct VoidSiteA
    {
        static constexpr uint32 Key = 1001;
        typedef void Value;
        typedef VoidContext Context;
    };

    struct VoidSiteB
    {
        static constexpr uint32 Key = 1002;
        typedef void Value;
        typedef VoidContext Context;
    };

    // A proc site, like HandleDummyAuraProc's: the function returns a SpellAuraProcResult, which
    // its caller uses to decide the charge (Unit.cpp's proc loop).
    struct ProcContext
    {
        ProcContext(uint32& triggeredSpellId) : triggered_spell_id(triggeredSpellId) {}

        uint32& triggered_spell_id;
    };

    struct ProcSite
    {
        static constexpr uint32 Key = 1003;
        typedef SpellAuraProcResult Value;
        typedef ProcContext Context;
    };

    typedef SpellHandlerOutcome<void> VoidOutcome;
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    VoidOutcome ReturnsA(VoidContext& ctx)
    {
        ctx.handled = 1;
        return VoidOutcome::Return();
    }

    VoidOutcome ReturnsB(VoidContext& ctx)
    {
        ctx.handled = 2;
        return VoidOutcome::Return();
    }

    VoidOutcome Continues(VoidContext& ctx)
    {
        ctx.handled = 3;
        ctx.value = 42;                                         // a live-out write, as a case body's
        return VoidOutcome::Continue();
    }

    VoidOutcome DefaultBody(VoidContext& ctx)
    {
        ++ctx.defaults;
        return VoidOutcome::Continue();
    }

    ProcOutcome ProcFails(ProcContext& ctx)
    {
        ctx.triggered_spell_id = 12345;
        return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
    }

    ProcOutcome ProcSetsTriggerAndContinues(ProcContext& ctx)
    {
        ctx.triggered_spell_id = 54321;
        return ProcOutcome::Continue();
    }

    // The shape a void site takes where its switch stood: Return leaves the function, Continue and
    // Miss carry on after the switch. Returns whether the code after the switch ran.
    bool RunVoidSite(SpellHandlerRegistry const& registry, uint32 spellId, VoidContext& ctx)
    {
        if (registry.Dispatch<VoidSiteA>(spellId, ctx).IsReturn())
        {
            return false;
        }
        return true;
    }

    // The shape a proc site takes: Return(value) is the function's result; after the switch the
    // tail uses the live-outs, and a trigger id set by a Continue handler reaches it.
    SpellAuraProcResult RunProcSite(SpellHandlerRegistry const& registry, uint32 spellId, uint32& triggered)
    {
        ProcContext ctx(triggered);
        ProcOutcome outcome = registry.Dispatch<ProcSite>(spellId, ctx);
        if (outcome.IsReturn())
        {
            return outcome.GetValue();
        }
        return triggered ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_CANT_TRIGGER;
    }

    VoidOutcome LoopContinues(VoidContext& ctx)
    {
        ctx.handled = 4;
        return VoidOutcome::LoopContinue();
    }

    ProcOutcome ProcLoopContinues(ProcContext& ctx)
    {
        ctx.triggered_spell_id = 777;
        return ProcOutcome::LoopContinue();
    }

    // The shape a void site takes inside a loop with a statement after its switch: Return leaves the
    // function, LoopContinue takes the next id, Continue and Miss run the statement after the switch.
    // Returns how many times that statement ran.
    uint32 RunVoidSiteInALoop(SpellHandlerRegistry const& registry, uint32 const* spellIds, std::size_t count,
                              VoidContext& ctx)
    {
        uint32 after = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            VoidOutcome outcome = registry.Dispatch<VoidSiteA>(spellIds[i], ctx);
            if (outcome.IsReturn())
            {
                return after;
            }
            if (outcome.IsLoopContinue())
            {
                continue;
            }
            ++after;
        }
        return after;
    }
}

TEST(SpellHandlerRegistry_FindReturnsTheRegisteredFunction)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(200, &ReturnsB));
    CHECK(registry.Find<VoidSiteA>(100) == &ReturnsA);
    CHECK(registry.Find<VoidSiteA>(200) == &ReturnsB);
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(2));

    int local = 0;
    VoidContext ctx(local);
    CHECK(registry.Dispatch<VoidSiteA>(200, ctx).IsReturn());
    CHECK_EQ(ctx.handled, uint32(2));
}

TEST(SpellHandlerRegistry_AMissReturnsTheMissMarker)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Find<VoidSiteA>(101) == NULL);
    CHECK(registry.FindDefault<VoidSiteA>() == NULL);

    int local = 0;
    VoidContext ctx(local);
    VoidOutcome outcome = registry.Dispatch<VoidSiteA>(101, ctx);
    CHECK(outcome.IsMiss());
    CHECK(!outcome.IsReturn());
    CHECK(!outcome.IsContinue());
    CHECK_EQ(ctx.handled, uint32(0));
    CHECK(RunVoidSite(registry, 101, ctx));                     // no default: after the switch
}

TEST(SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Find<VoidSiteB>(100) == NULL);               // registering at A says nothing of B
    CHECK(registry.Register<VoidSiteB>(100, &ReturnsB));
    CHECK(registry.Find<VoidSiteA>(100) == &ReturnsA);
    CHECK(registry.Find<VoidSiteB>(100) == &ReturnsB);
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(VoidSiteB::Key), std::size_t(1));

    int local = 0;
    VoidContext ctx(local);
    registry.Dispatch<VoidSiteB>(100, ctx);
    CHECK_EQ(ctx.handled, uint32(2));
    registry.Dispatch<VoidSiteA>(100, ctx);
    CHECK_EQ(ctx.handled, uint32(1));

    // Spell ids use all 32 bits (4.3.4 has ids above 65535): (A, 100 + 65536) is not (B, 100),
    // which a key packing the site into bits 16 and up would make it (1001 << 16 | 65636 ==
    // 1002 << 16 | 100).
    CHECK(registry.Register<VoidSiteA>(100 + 65536, &Continues));
    CHECK(registry.Find<VoidSiteA>(100 + 65536) == &Continues);
    CHECK(registry.Find<VoidSiteB>(100) == &ReturnsB);
    CHECK(registry.Find<VoidSiteB>(100 + 65536) == NULL);
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(VoidSiteB::Key), std::size_t(1));
}

TEST(SpellHandlerRegistry_ATakenKeyIsRefused)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(!registry.Register<VoidSiteA>(100, &ReturnsB));
    CHECK(registry.Find<VoidSiteA>(100) == &ReturnsA);
    CHECK(!registry.Register<VoidSiteA>(300, NULL));
    CHECK(registry.Find<VoidSiteA>(300) == NULL);
    CHECK(registry.RegisterDefault<VoidSiteA>(&DefaultBody));
    CHECK(!registry.RegisterDefault<VoidSiteA>(&Continues));
    CHECK(registry.FindDefault<VoidSiteA>() == &DefaultBody);
    CHECK_EQ(registry.Count(), std::size_t(1));
}

TEST(SpellHandlerRegistry_LabelsSharingABodyRegisterOneFunction)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(101, &ReturnsA));
    CHECK(registry.Find<VoidSiteA>(100) == registry.Find<VoidSiteA>(101));
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(2));
}

TEST(SpellHandlerRegistry_ContinueAndReturnAreDistinct)
{
    CHECK(VoidOutcome::Return().IsReturn());
    CHECK(!VoidOutcome::Return().IsContinue());
    CHECK(!VoidOutcome::Return().IsMiss());
    CHECK(VoidOutcome::Continue().IsContinue());
    CHECK(!VoidOutcome::Continue().IsReturn());
    CHECK(!VoidOutcome::Continue().IsMiss());
    CHECK(VoidOutcome::Miss().IsMiss());
    CHECK(!VoidOutcome::Miss().IsReturn());
    CHECK(!VoidOutcome::Miss().IsContinue());

    CHECK(ProcOutcome::Return(SPELL_AURA_PROC_OK).IsReturn());
    CHECK(!ProcOutcome::Continue().IsReturn());
    CHECK(ProcOutcome::Continue().IsContinue());
    CHECK(!ProcOutcome::Miss().IsReturn());

    // Through a dispatch: a Continue handler carries on after the switch, a Return one leaves.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(300, &Continues));
    int local = 0;
    VoidContext ctx(local);
    CHECK(!RunVoidSite(registry, 100, ctx));
    CHECK(RunVoidSite(registry, 300, ctx));
    CHECK_EQ(ctx.handled, uint32(3));
}

TEST(SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.RegisterDefault<VoidSiteA>(&DefaultBody));
    int local = 0;
    VoidContext ctx(local);

    CHECK(registry.Dispatch<VoidSiteA>(100, ctx).IsReturn());   // a hit: the default does not run
    CHECK_EQ(ctx.defaults, uint32(0));
    CHECK_EQ(ctx.handled, uint32(1));

    VoidOutcome outcome = registry.Dispatch<VoidSiteA>(999, ctx); // a miss: the default, once
    CHECK(outcome.IsContinue());
    CHECK_EQ(ctx.defaults, uint32(1));

    CHECK(registry.Dispatch<VoidSiteB>(999, ctx).IsMiss());     // another site's default is not this one's
    CHECK_EQ(ctx.defaults, uint32(1));
}

TEST(SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(300, &Continues));
    int local = 7;
    VoidContext ctx(local);
    CHECK(RunVoidSite(registry, 300, ctx));
    CHECK_EQ(local, 42);

    CHECK(registry.Register<ProcSite>(400, &ProcSetsTriggerAndContinues));
    uint32 triggered = 0;
    CHECK_EQ(int(RunProcSite(registry, 400, triggered)), int(SPELL_AURA_PROC_OK));
    CHECK_EQ(triggered, uint32(54321));
}

TEST(SpellHandlerRegistry_AProcSiteReturnsItsValue)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<ProcSite>(500, &ProcFails));
    CHECK(registry.Register<ProcSite>(400, &ProcSetsTriggerAndContinues));

    // FAILED is the function's result, not replaced by what the tail would have answered: the
    // caller keeps the charge on FAILED.
    uint32 triggered = 0;
    CHECK_EQ(int(RunProcSite(registry, 500, triggered)), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(triggered, uint32(12345));

    ProcContext ctx(triggered);
    ProcOutcome outcome = registry.Dispatch<ProcSite>(500, ctx);
    CHECK(outcome.IsReturn());
    CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int(ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER).GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));

    // A miss with no default reaches the tail with the live-outs as they were.
    triggered = 0;
    CHECK_EQ(int(RunProcSite(registry, 600, triggered)), int(SPELL_AURA_PROC_CANT_TRIGGER));
}

TEST(SpellHandlerRegistry_LoopContinueIsAFourthOutcome)
{
    CHECK(VoidOutcome::LoopContinue().IsLoopContinue());
    CHECK(!VoidOutcome::LoopContinue().IsReturn());
    CHECK(!VoidOutcome::LoopContinue().IsContinue());
    CHECK(!VoidOutcome::LoopContinue().IsMiss());
    CHECK(!VoidOutcome::Return().IsLoopContinue());
    CHECK(!VoidOutcome::Continue().IsLoopContinue());
    CHECK(!VoidOutcome::Miss().IsLoopContinue());

    CHECK(ProcOutcome::LoopContinue().IsLoopContinue());
    CHECK(!ProcOutcome::LoopContinue().IsReturn());
    CHECK(!ProcOutcome::LoopContinue().IsContinue());
    CHECK(!ProcOutcome::LoopContinue().IsMiss());
    CHECK(!ProcOutcome::Return(SPELL_AURA_PROC_OK).IsLoopContinue());
    CHECK(!ProcOutcome::Continue().IsLoopContinue());
    CHECK(!ProcOutcome::Miss().IsLoopContinue());

    // Through a dispatch: the handler's answer reaches the site; a miss is not a loop continue.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(700, &LoopContinues));
    CHECK(registry.Register<ProcSite>(700, &ProcLoopContinues));
    int local = 0;
    VoidContext ctx(local);
    CHECK(registry.Dispatch<VoidSiteA>(700, ctx).IsLoopContinue());
    CHECK_EQ(ctx.handled, uint32(4));
    CHECK(!registry.Dispatch<VoidSiteA>(701, ctx).IsLoopContinue());

    uint32 triggered = 0;
    ProcContext procCtx(triggered);
    ProcOutcome outcome = registry.Dispatch<ProcSite>(700, procCtx);
    CHECK(outcome.IsLoopContinue());
    CHECK(!outcome.IsReturn());
    CHECK_EQ(triggered, uint32(777));
    CHECK(!registry.Dispatch<ProcSite>(701, procCtx).IsLoopContinue());
}

TEST(SpellHandlerRegistry_ALoopContinueSkipsTheRestOfTheLoopBody)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(300, &Continues));
    CHECK(registry.Register<VoidSiteA>(700, &LoopContinues));
    int local = 0;
    VoidContext ctx(local);

    static uint32 const loopContinues[] = { 700, 700 };
    CHECK_EQ(RunVoidSiteInALoop(registry, loopContinues, 2, ctx), uint32(0));
    CHECK_EQ(ctx.handled, uint32(4));

    // 300 continues and 999 misses: both run the statement; 700 skips it; 100 returns before the last 300.
    static uint32 const mixed[] = { 300, 700, 999, 100, 300 };
    CHECK_EQ(RunVoidSiteInALoop(registry, mixed, 5, ctx), uint32(2));
    CHECK_EQ(ctx.handled, uint32(1));
}

TEST(AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances)
{
    static uint32 const stances[] = { 41099, 41100, 41101, 53790, 53791, 53792 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK_EQ(registry.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    std::set<SpellHandler<AuraDummyApplyWarriorSite>::Function> distinct;
    for (uint32 spellId : stances)
    {
        SpellHandler<AuraDummyApplyWarriorSite>::Function function = registry.Find<AuraDummyApplyWarriorSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(6));                  // six bodies, none shared

    // The site has no default: a miss (Overpower 7384) is Miss() and the Overpower block runs after it.
    CHECK(registry.Find<AuraDummyApplyWarriorSite>(7384) == NULL);
    CHECK(registry.FindDefault<AuraDummyApplyWarriorSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyContext ctx(NULL, target);
    CHECK(registry.Dispatch<AuraDummyApplyWarriorSite>(7384, ctx).IsMiss());

    // The game's table is the same one.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    for (uint32 spellId : stances)
    {
        CHECK(game.Find<AuraDummyApplyWarriorSite>(spellId) == registry.Find<AuraDummyApplyWarriorSite>(spellId));
    }
}

TEST(AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = reinterpret_cast<Aura*>(units[1]);
    AuraDummyApplyContext ctx(aura, target);
    CHECK(ctx.aura == aura);
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
}

namespace
{
    alignas(16) unsigned char s_auraStorage[3][16];

    Aura* FakeAura(int i)
    {
        return reinterpret_cast<Aura*>(s_auraStorage[i]);
    }

    uint32 const QUEST_TAME_RETURNED = 0xFFFFFFFF;

    // The quest-tame site's shape: answers what the tail casts (0: nothing), or the marker when the site returned.
    uint32 RunQuestTame(SpellHandlerRegistry const& registry, uint32 tameId)
    {
        uint32 finalSpellId = 0;
        AuraDummyQuestTameContext tameCtx(finalSpellId);
        if (registry.Dispatch<AuraDummyQuestTameSite>(tameId, tameCtx).IsReturn())
        {
            return QUEST_TAME_RETURNED;
        }
        return finalSpellId;
    }
}

TEST(AuraDummyHandlers_TheTableRegistersEveryRowOnce)
{
    // 41 generic apply labels, 6 warrior stances, 1 hunter and 1 shaman apply label, 18 quest-tame labels, 34 removal
    // labels, 40 generic apply and remove labels, 2 druid labels, 1 rogue, 1 hunter and 1 paladin label: 146 rows and
    // 146 keys, none twice, and no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(146));
    CHECK_EQ(registry.Count(), std::size_t(146));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraDummyApplyGenericSite::Key), std::size_t(41));
    CHECK_EQ(registry.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraDummyApplyHunterSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyApplyShamanSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyRemoveSite::Key), std::size_t(34));
    CHECK_EQ(registry.CountAt(AuraDummyQuestTameSite::Key), std::size_t(18));
    CHECK_EQ(registry.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(40));
    CHECK_EQ(registry.CountAt(AuraDummyDruidSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraDummyRogueSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyHunterSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyPaladinSite::Key), std::size_t(1));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(146));
    CHECK_EQ(registry.Count(), std::size_t(146));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));

    // The game's table holds these, the transform site's 9 rows and default, the threat site's 2 rows, the
    // transmitted-object site's row and default, the periodic auras' 26 rows and 4 defaults, the energize site's
    // 9 rows and default, the activate-object site's 33 rows, the resurrect site's 3 rows and default, the two
    // targeting sites' 10 and 1 rows and their two defaults, the cast-check site's 2 rows and default, the
    // target-check site's 3 rows and default, the weapon-damage site's 6 rows, the school-damage, trigger-spell
    // and two teleport sites' 74, 7, 3 and 3 rows, the generic dummy-effect site's 216 rows, the five dummy-effect
    // family sites' 5, 11, 4, 4 and 4 rows, the six trigger-spell aura sites' 45, 1, 3, 5, 2 and 11 rows and five
    // defaults, the damage-tick and mana-leech tick sites' 8 and 3 rows and one default, the five periodic dummy
    // tick sites' 44, 1, 2, 1 and 2 rows and five defaults, the five boost sites' 7, 6, 2, 6 and 4 rows and four
    // defaults, the five script-effect family sites' 11, 1, 3, 3 and 1 rows and two defaults, the generic
    // script-effect site's 283 rows, the location-check site's 20 rows, the three bonus sites' 6, 2 and 3 rows,
    // the eight dummy proc sites' 37, 9, 10, 8, 11, 5, 26 and 10 rows and the haste, crit-chance and two
    // proc-trigger sites' 2, 1, 16 and 20 rows.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(1223));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
    CHECK_EQ(game.CountAt(AuraDummyRemoveSite::Key), std::size_t(34));
}

TEST(AuraDummyHandlers_TheDruidSiteHoldsItsTwoLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    SpellHandler<AuraDummyDruidSite>::Function savageRoar = registry.Find<AuraDummyDruidSite>(52610);
    SpellHandler<AuraDummyDruidSite>::Function survivalInstincts = registry.Find<AuraDummyDruidSite>(61336);
    CHECK(savageRoar != NULL);
    CHECK(survivalInstincts != NULL);
    CHECK(savageRoar != survivalInstincts);
    CHECK(registry.FindDefault<AuraDummyDruidSite>() == NULL);

    // An unmatched druid id is a Miss: HandleAuraDummy goes on to the Lifebloom and Predatory Strikes
    // blocks after the switch (Lifebloom 33763, Predatory Strikes 16972).
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyDruidSite>(33763, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyDruidSite>(16972, ctx).IsMiss());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraDummyDruidSite>(52610) == savageRoar);
    CHECK(game.Find<AuraDummyDruidSite>(61336) == survivalInstincts);
}

TEST(AuraDummyHandlers_TheApplyRemoveContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = FakeAura(2);

    AuraDummyApplyRemoveContext applyRemove(aura, target, false);
    CHECK(applyRemove.aura == aura);
    CHECK(applyRemove.target == target);
    CHECK(!applyRemove.apply);
    applyRemove.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
}

TEST(AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyFourLabelsAndNoDefault)
{
    static uint32 const stances[] = { 41099, 41100, 41101, 53790, 53791, 53792 };
    static uint32 const byMode[] = { 12774, 32045, 32051, 32052, 32286, 42454,
                                     43969, 45934, 50141, 51870, 52098, 56511, 61900 };
    static uint32 const byBody[] = { 10255, 12479, 28169, 35079, 36730, 42517, 44191, 45963, 46308, 46637, 48385,
                                     51405, 53039, 68839 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK_EQ(registry.CountAt(AuraDummyRemoveSite::Key), std::size_t(34));
    for (uint32 spellId : stances)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);

        // The same id at the warrior apply site is another key with another body.
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) != NULL);
        CHECK(reinterpret_cast<void (*)()>(registry.Find<AuraDummyApplyWarriorSite>(spellId))
              != reinterpret_cast<void (*)()>(function));
    }

    // The thirteen labels whose bodies read the removal mode: thirteen bodies, none shared, and none a stance's.
    std::set<SpellHandler<AuraDummyRemoveSite>::Function> distinct;
    for (uint32 spellId : byMode)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(13));
    for (uint32 spellId : stances)
    {
        CHECK(distinct.count(registry.Find<AuraDummyRemoveSite>(spellId)) == 0);
    }

    // The fifteen labels whose bodies read no mode: fourteen bodies, 35079 and 59628 one function; none a stance's or
    // a mode reader's.
    std::set<SpellHandler<AuraDummyRemoveSite>::Function> others;
    for (uint32 spellId : byBody)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);
        CHECK(distinct.count(function) == 0);
        others.insert(function);
    }
    CHECK_EQ(others.size(), std::size_t(14));
    // 35079 and 59628 one function: proven by verbatim, not here: this binary links with COMDAT folding, so an
    // identical second function folds to the same address.
    CHECK(registry.Find<AuraDummyRemoveSite>(59628) == registry.Find<AuraDummyRemoveSite>(35079));
    for (uint32 spellId : stances)
    {
        CHECK(others.count(registry.Find<AuraDummyRemoveSite>(spellId)) == 0);
    }

    // No default: an id the site never held misses, and the Living Bomb block after it runs.
    CHECK(registry.FindDefault<AuraDummyRemoveSite>() == NULL);
    Unit* target = NULL;
    AuraDummyRemoveContext ctx(NULL, target);
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(11920, ctx).IsMiss());

    // Keyed on the removal switch only: the quest-tame and feign-death sites do not hold its labels.
    CHECK(registry.Find<AuraDummyQuestTameSite>(32045) == NULL);
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(61900) == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyRemoveSite::Key), std::size_t(34));
    for (uint32 spellId : stances)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
    for (uint32 spellId : byMode)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
    for (uint32 spellId : byBody)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
    CHECK(game.Find<AuraDummyRemoveSite>(59628) == registry.Find<AuraDummyRemoveSite>(59628));
}

namespace
{
    // An Aura's storage without its constructor (which needs the spell store): SetRemoveMode and GetRemoveMode
    // touch only the mode's bits, and a body that returns on the mode reads nothing else of the aura.
    struct AuraStorage
    {
        alignas(Aura) unsigned char bytes[sizeof(Aura)];
    };

    AuraStorage s_modeAura = {};

    Aura* ModeAura()
    {
        return reinterpret_cast<Aura*>(s_modeAura.bytes);
    }

    AuraRemoveMode s_modeTheBodyRead = AURA_REMOVE_BY_DEFAULT;
}

TEST(AuraDummyHandlers_GetRemoveModeAnswersTheModeSetRemoveModeWrote)
{
    static AuraRemoveMode const modes[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL,
        AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_EXPIRE,
        AURA_REMOVE_BY_TRACKING,
    };
    Aura* aura = ModeAura();
    for (AuraRemoveMode mode : modes)
    {
        aura->SetRemoveMode(mode);                              // Unit::RemoveAura's write
        CHECK_EQ(int(aura->GetRemoveMode()), int(mode));
    }
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
}

namespace
{
    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A holder's storage without its constructor, every byte set to one value.
    SpellAuraHolder* HolderFilledWith(HolderStorage& storage, unsigned char value)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = value;
        }
        return reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    const uint32 kHolderSpell = 92001;
    alignas(SpellEntry) unsigned char s_holderSpellBytes[sizeof(SpellEntry)] = {};

    // A Spell.dbc row put into the spell store, which a holder's constructor checks it against.
    SpellEntry const* HolderSpell()
    {
        static bool seeded = false;
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_holderSpellBytes);
        if (!seeded)
        {
            seeded = true;
            row->ID = kHolderSpell;
            sSpellStore.SetEntry(kHolderSpell, row);
        }
        return row;
    }

    // A creature with its update fields allocated and no world, map, template or AI.
    class HolderTarget : public Creature
    {
        public:
            HolderTarget()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist.
            ~HolderTarget()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

TEST(SpellAuraHolder_GetRemoveModeAnswersTheModeSetRemoveModeWrote)
{
    static AuraRemoveMode const modes[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL,
        AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_EXPIRE,
        AURA_REMOVE_BY_TRACKING,
    };
    static unsigned char const fills[] = { 0x00, 0xA5 };
    for (unsigned char fill : fills)
    {
        HolderStorage storage;
        SpellAuraHolder* holder = HolderFilledWith(storage, fill);
        for (AuraRemoveMode mode : modes)
        {
            holder->SetRemoveMode(mode);                        // Unit::RemoveSpellAuraHolder's write
            CHECK_EQ(int(holder->GetRemoveMode()), int(mode));
        }
    }

    HolderTarget target;
    SpellAuraHolder fresh(HolderSpell(), &target, NULL, NULL, NULL);
    CHECK_EQ(int(fresh.GetRemoveMode()), int(AURA_REMOVE_BY_DEFAULT));
}

namespace
{
    int BitsThatDiffer(AuraStorage const& left, AuraStorage const& right)
    {
        int bits = 0;
        for (std::size_t i = 0; i < sizeof(left.bytes); ++i)
        {
            for (unsigned int diff = left.bytes[i] ^ right.bytes[i]; diff != 0; diff &= diff - 1)
            {
                ++bits;
            }
        }
        return bits;
    }
}

TEST(AuraDummyHandlers_SetPeriodicChangesTheFlagAndNothingElse)
{
    // SetPeriodic changes one bit, and setting the flag back or to the value it holds restores every byte.
    AuraStorage storage = {};
    Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
    aura->GetModifier()->periodictime = 30000;
    CHECK(!aura->IsPeriodic());
    AuraStorage const withPeriod = storage;
    aura->SetPeriodic(true);
    CHECK(aura->IsPeriodic());
    CHECK_EQ(BitsThatDiffer(storage, withPeriod), 1);
    aura->SetPeriodic(false);
    CHECK(!aura->IsPeriodic());
    CHECK_EQ(BitsThatDiffer(storage, withPeriod), 0);

    for (std::size_t i = 0; i < sizeof(storage.bytes); ++i)
    {
        storage.bytes[i] = static_cast<unsigned char>(i * 37 + 11);
    }
    AuraStorage const patterned = storage;
    bool const was = aura->IsPeriodic();
    aura->SetPeriodic(!was);
    CHECK(aura->IsPeriodic() == !was);
    CHECK_EQ(BitsThatDiffer(storage, patterned), 1);
    aura->SetPeriodic(was);
    CHECK(aura->IsPeriodic() == was);
    CHECK_EQ(BitsThatDiffer(storage, patterned), 0);
    aura->SetPeriodic(was);
    CHECK_EQ(BitsThatDiffer(storage, patterned), 0);
}

TEST(AuraDummyHandlers_ARemovalBodyReadsTheModeWhenItRuns)
{
    // A double at the site: the context carries the aura, and the body reads the mode set after the context was
    // built, before the body ran.
    SpellHandlerRegistry doubled;
    CHECK(doubled.Register<AuraDummyRemoveSite>(32045, [](AuraDummyRemoveContext& ctx)
    {
        s_modeTheBodyRead = ctx.aura->GetRemoveMode();
        return SpellHandlerOutcome<void>::Return();
    }));
    Aura* aura = ModeAura();
    Unit* target = NULL;
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
    AuraDummyRemoveContext doubleCtx(aura, target);
    aura->SetRemoveMode(AURA_REMOVE_BY_EXPIRE);
    CHECK(doubled.Dispatch<AuraDummyRemoveSite>(32045, doubleCtx).IsReturn());
    CHECK_EQ(int(s_modeTheBodyRead), int(AURA_REMOVE_BY_EXPIRE));

    // The real bodies: each context is built while the aura holds the mode its body acts on (a cast on the
    // target, a read of the caster); the mode then changes to one the body returns on before touching anything.
    // A body that read the mode when its context was built would act on the zeroed aura or the NULL target and crash.
    struct ModeBody
    {
        uint32 spellId;
        AuraRemoveMode acts;
        AuraRemoveMode returns;
    };
    static ModeBody const bodies[] =
    {
        { 12774, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_DEATH },
        { 32045, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 32051, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 32052, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 32286, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 42454, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE },
        { 43969, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 45934, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEFAULT },
        { 50141, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 52098, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 56511, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE },
        { 61900, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DEFAULT },
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (ModeBody const& body : bodies)
    {
        aura->SetRemoveMode(body.acts);
        AuraDummyRemoveContext ctx(aura, target);
        aura->SetRemoveMode(body.returns);
        CHECK(game.Dispatch<AuraDummyRemoveSite>(body.spellId, ctx).IsReturn());
    }
    CHECK(target == NULL);
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
}

TEST(AuraDummyHandlers_TheRemoveContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = FakeAura(1);
    AuraDummyRemoveContext ctx(aura, target);
    CHECK(ctx.aura == aura);
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
}

TEST(AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault)
{
    static uint32 const tames[] = { 19548, 19674, 19687, 19688, 19689, 19692, 19693, 19694, 19696,
                                    19697, 19699, 19700, 30646, 30653, 30654, 30099, 30102, 30105 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    for (uint32 spellId : tames)
    {
        CHECK(registry.Find<AuraDummyQuestTameSite>(spellId) != NULL);
    }

    // No default: a quest-tame id the switch never held (73461), another site's id or none is a Miss.
    CHECK(registry.FindDefault<AuraDummyQuestTameSite>() == NULL);
    uint32 finalSpellId = 0;
    AuraDummyQuestTameContext ctx(finalSpellId);
    CHECK(registry.Dispatch<AuraDummyQuestTameSite>(73461, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyQuestTameSite>(41101, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyQuestTameSite>(12345, ctx).IsMiss());
    CHECK_EQ(finalSpellId, uint32(0));
    CHECK(registry.Find<AuraDummyRemoveSite>(19548) == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyQuestTameSite::Key), std::size_t(18));
    for (uint32 spellId : tames)
    {
        CHECK(game.Find<AuraDummyQuestTameSite>(spellId) == registry.Find<AuraDummyQuestTameSite>(spellId));
    }
}

TEST(AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts)
{
    // Each body writes `finalSpellId` and answers Continue, so the tail casts it; a miss leaves it at 0.
    static uint32 const tames[][2] =
    {
        { 19548, 19597 }, { 19674, 19677 }, { 19687, 19676 }, { 19688, 19678 }, { 19689, 19679 }, { 19692, 19680 },
        { 19693, 19684 }, { 19694, 19681 }, { 19696, 19682 }, { 19697, 19683 }, { 19699, 19685 }, { 19700, 19686 },
        { 30646, 30647 }, { 30653, 30648 }, { 30654, 30652 }, { 30099, 30100 }, { 30102, 30103 }, { 30105, 30104 },
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (auto const& tame : tames)
    {
        CHECK_EQ(RunQuestTame(game, tame[0]), tame[1]);
    }
    CHECK_EQ(RunQuestTame(game, 73461), uint32(0));

    // Against a Return handler at the same site, which leaves before the tail.
    SpellHandlerRegistry returning;
    CHECK(returning.Register<AuraDummyQuestTameSite>(19548, [](AuraDummyQuestTameContext& ctx)
    {
        ctx.finalSpellId = 19597;
        return SpellHandlerOutcome<void>::Return();
    }));
    CHECK_EQ(RunQuestTame(returning, 19548), QUEST_TAME_RETURNED);
}

TEST(AuraDummyHandlers_TheQuestTameContextAliasesFinalSpellId)
{
    uint32 finalSpellId = 0;
    AuraDummyQuestTameContext ctx(finalSpellId);
    ctx.finalSpellId = 19684;                                   // a body's write to `finalSpellId`...
    CHECK_EQ(finalSpellId, uint32(19684));                      // ...is the block's local
    finalSpellId = 30647;
    CHECK_EQ(ctx.finalSpellId, uint32(30647));
}

TEST(AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels)
{
    static uint32 const feigns[] = { 29266, 31261, 37493, 52593, 55795, 57626, 57685, 58768,
                                     58806, 58951, 64461, 65985, 70592, 70628, 70630, 71598 };

    // The sixteen labels share one body: one function under sixteen keys.
    // Proven by verbatim, not here: this binary links with COMDAT folding, so an identical second function folds
    // to the same address.
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    SpellHandler<AuraDummyApplyRemoveGenericSite>::Function feignDeath =
        registry.Find<AuraDummyApplyRemoveGenericSite>(29266);
    CHECK(feignDeath != NULL);
    for (uint32 spellId : feigns)
    {
        CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) == feignDeath);
    }
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(31261)
          == registry.Find<AuraDummyApplyRemoveGenericSite>(71598));
    CHECK_EQ(registry.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(40));

    // The spawn feign deaths are another body. No default: an id the site never held is a Miss.
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(35356) != feignDeath);
    CHECK(registry.FindDefault<AuraDummyApplyRemoveGenericSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyApplyRemoveGenericSite>(12345, ctx).IsMiss());

    // Keyed on the generic switch only: the druid labels are not its keys, and its labels are no other site's.
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(52610) == NULL);
    CHECK(registry.Find<AuraDummyDruidSite>(29266) == NULL);
    CHECK(registry.Find<AuraDummyRemoveSite>(29266) == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(40));
    for (uint32 spellId : feigns)
    {
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId) == feignDeath);
    }
}

TEST(AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsItsTwentyFourOtherLabels)
{
    static uint32 const labels[] = { 11196, 24658, 24661, 40133, 40132, 40214, 42515, 42583, 68987, 47178, 56422, 70733,
                                     58204, 58205, 27978, 40131, 66936, 66948 };
    static uint32 const motion[] = { 6606, 35356, 35357, 42557, 51329, 43874 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    SpellHandler<AuraDummyApplyRemoveGenericSite>::Function feignDeath =
        registry.Find<AuraDummyApplyRemoveGenericSite>(29266);
    std::set<SpellHandler<AuraDummyApplyRemoveGenericSite>::Function> distinct;
    for (uint32 spellId : labels)
    {
        SpellHandler<AuraDummyApplyRemoveGenericSite>::Function function =
            registry.Find<AuraDummyApplyRemoveGenericSite>(spellId);
        CHECK(function != NULL);
        CHECK(function != feignDeath);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(14));                 // 14 bodies: four of them shared by two labels

    // The four shared bodies, each under two keys.
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(68987)
          == registry.Find<AuraDummyApplyRemoveGenericSite>(42583));
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(70733)
          == registry.Find<AuraDummyApplyRemoveGenericSite>(56422));
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(40131)
          == registry.Find<AuraDummyApplyRemoveGenericSite>(27978));
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(66948)
          == registry.Find<AuraDummyApplyRemoveGenericSite>(66936));

    // The six motion labels: three bodies, the four spawn feign deaths sharing one, none another label's.
    std::set<SpellHandler<AuraDummyApplyRemoveGenericSite>::Function> motionBodies;
    for (uint32 spellId : motion)
    {
        SpellHandler<AuraDummyApplyRemoveGenericSite>::Function function =
            registry.Find<AuraDummyApplyRemoveGenericSite>(spellId);
        CHECK(function != NULL);
        CHECK(function != feignDeath);
        CHECK(distinct.count(function) == 0);
        motionBodies.insert(function);
    }
    CHECK_EQ(motionBodies.size(), std::size_t(3));
    for (uint32 spellId : { 35357u, 42557u, 51329u })
    {
        CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(spellId)
              == registry.Find<AuraDummyApplyRemoveGenericSite>(35356));
    }

    // No default: an id the site never held misses.
    CHECK(registry.FindDefault<AuraDummyApplyRemoveGenericSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyApplyRemoveGenericSite>(12345, ctx).IsMiss());

    // No label is another aura dummy site's key.
    for (uint32 spellId : labels)
    {
        CHECK(registry.Find<AuraDummyApplyGenericSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyQuestTameSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyRemoveSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyDruidSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyRogueSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyHunterSite>(spellId) == NULL);
    }

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : labels)
    {
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId)
              == registry.Find<AuraDummyApplyRemoveGenericSite>(spellId));
    }
}

TEST(AuraDummyHandlers_TheFamilyApplyRemoveSitesHoldTheirLabelsAndNoDefault)
{
    // The rogue and hunter bodies are the same code; this test compares no address between them.
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK(registry.Find<AuraDummyRogueSite>(57934) != NULL);
    CHECK(registry.Find<AuraDummyHunterSite>(34477) != NULL);
    CHECK_EQ(registry.CountAt(AuraDummyRogueSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyHunterSite::Key), std::size_t(1));

    // No default: an id the family switch never held misses, and the code after the family switch runs.
    CHECK(registry.FindDefault<AuraDummyRogueSite>() == NULL);
    CHECK(registry.FindDefault<AuraDummyHunterSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyRogueSite>(12345, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyHunterSite>(12345, ctx).IsMiss());

    // Neither id is a key at the other aura dummy sites.
    static uint32 const ids[] = { 57934, 34477 };
    for (uint32 spellId : ids)
    {
        CHECK(registry.Find<AuraDummyApplyGenericSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyQuestTameSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyRemoveSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyDruidSite>(spellId) == NULL);
    }

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyRogueSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraDummyHunterSite::Key), std::size_t(1));
    CHECK(game.FindDefault<AuraDummyRogueSite>() == NULL);
    CHECK(game.FindDefault<AuraDummyHunterSite>() == NULL);
    CHECK(game.Find<AuraDummyRogueSite>(57934) == registry.Find<AuraDummyRogueSite>(57934));
    CHECK(game.Find<AuraDummyHunterSite>(34477) == registry.Find<AuraDummyHunterSite>(34477));
}

TEST(AuraDummyHandlers_TheHunterShamanAndPaladinSitesHoldTheirLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK(registry.Find<AuraDummyApplyHunterSite>(34026) != NULL);
    CHECK(registry.Find<AuraDummyApplyShamanSite>(55198) != NULL);
    CHECK(registry.Find<AuraDummyPaladinSite>(20911) != NULL);
    CHECK_EQ(registry.CountAt(AuraDummyApplyHunterSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyApplyShamanSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraDummyPaladinSite::Key), std::size_t(1));

    // No default: an id the family switch never held misses, and the code after the family switch runs.
    CHECK(registry.FindDefault<AuraDummyApplyHunterSite>() == NULL);
    CHECK(registry.FindDefault<AuraDummyApplyShamanSite>() == NULL);
    CHECK(registry.FindDefault<AuraDummyPaladinSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyContext apply(NULL, target);
    CHECK(registry.Dispatch<AuraDummyApplyHunterSite>(12345, apply).IsMiss());
    CHECK(registry.Dispatch<AuraDummyApplyShamanSite>(12345, apply).IsMiss());
    AuraDummyApplyRemoveContext applyRemove(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyPaladinSite>(12345, applyRemove).IsMiss());

    // Greater Blessing of Sanctuary (25899) and Sentry Totem (6495) are not 4.3.4 spells and are keys at none of them.
    CHECK(registry.Find<AuraDummyPaladinSite>(25899) == NULL);
    CHECK(registry.Find<AuraDummyApplyShamanSite>(6495) == NULL);

    // No id is a key at the other aura dummy sites.
    static uint32 const ids[] = { 34026, 55198, 20911 };
    for (uint32 spellId : ids)
    {
        CHECK(registry.Find<AuraDummyApplyGenericSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyQuestTameSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyRemoveSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyDruidSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyRogueSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyHunterSite>(spellId) == NULL);
    }

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraDummyApplyHunterSite>(34026) == registry.Find<AuraDummyApplyHunterSite>(34026));
    CHECK(game.Find<AuraDummyApplyShamanSite>(55198) == registry.Find<AuraDummyApplyShamanSite>(55198));
    CHECK(game.Find<AuraDummyPaladinSite>(20911) == registry.Find<AuraDummyPaladinSite>(20911));
}

TEST(AuraDummyHandlers_TheThreatRedirectionBodiesReturnOnTheDefaultRemovalMode)
{
    // At the default removal mode the Tricks of the Trade and Misdirection bodies touch neither aura nor target.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    AuraStorage storage = {};
    Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
    AuraStorage const before = storage;
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(aura, target, false);
    CHECK(game.Dispatch<AuraDummyRogueSite>(57934, ctx).IsReturn());
    CHECK_EQ(BitsThatDiffer(storage, before), 0);
    CHECK(game.Dispatch<AuraDummyHunterSite>(34477, ctx).IsReturn());
    CHECK_EQ(BitsThatDiffer(storage, before), 0);
    CHECK(target == NULL);
}

TEST(AuraDummyHandlers_TheGenericApplySiteHoldsItsFortyOneLabelsAndNoDefault)
{
    static uint32 const labels[] = { 1515, 7057, 10255, 13139, 28832, 28833, 28834, 28835, 31606, 32045, 32051, 32052,
                                     33326, 36587, 38224, 37096, 46354, 39850, 43873, 45963, 46637, 46699, 47190, 47563,
                                     47593, 48143, 51405, 61187, 61190, 62061, 62109, 63122, 63624, 63651, 64132, 65684,
                                     65686, 68912, 70623, 70638, 71563 };
    static uint32 const alsoRemoved[] = { 10255, 32045, 32051, 32052, 45963, 46637, 51405 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK_EQ(registry.CountAt(AuraDummyApplyGenericSite::Key), std::size_t(41));
    std::set<SpellHandler<AuraDummyApplyGenericSite>::Function> distinct;
    for (uint32 spellId : labels)
    {
        SpellHandler<AuraDummyApplyGenericSite>::Function function = registry.Find<AuraDummyApplyGenericSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(33));                 // 33 bodies: four of them shared by their labels

    // The shared bodies: the four Marks, the three Soul Charges, the three gender illusions, the two Twilight Shifts.
    SpellHandler<AuraDummyApplyGenericSite>::Function mark = registry.Find<AuraDummyApplyGenericSite>(28832);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(28833) == mark);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(28834) == mark);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(28835) == mark);
    SpellHandler<AuraDummyApplyGenericSite>::Function soulCharge = registry.Find<AuraDummyApplyGenericSite>(32045);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(32051) == soulCharge);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(32052) == soulCharge);
    SpellHandler<AuraDummyApplyGenericSite>::Function illusion = registry.Find<AuraDummyApplyGenericSite>(38224);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(37096) == illusion);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(46354) == illusion);
    CHECK(registry.Find<AuraDummyApplyGenericSite>(61190) == registry.Find<AuraDummyApplyGenericSite>(61187));

    // No default: an id the site never held misses, and the AT APPLY & REMOVE switch runs.
    CHECK(registry.FindDefault<AuraDummyApplyGenericSite>() == NULL);
    Unit* target = NULL;
    int32 periodicTimer = 0;
    AuraDummyApplyGenericContext ctx(NULL, target, periodicTimer);
    CHECK(registry.Dispatch<AuraDummyApplyGenericSite>(12345, ctx).IsMiss());

    // The seven ids the removal site also holds have another body there; the rest are no other aura dummy site's key.
    for (uint32 spellId : alsoRemoved)
    {
        CHECK(registry.Find<AuraDummyRemoveSite>(spellId) != NULL);
        CHECK(reinterpret_cast<void (*)()>(registry.Find<AuraDummyRemoveSite>(spellId))
              != reinterpret_cast<void (*)()>(registry.Find<AuraDummyApplyGenericSite>(spellId)));
    }
    std::size_t removalKeys = 0;
    for (uint32 spellId : labels)
    {
        if (registry.Find<AuraDummyRemoveSite>(spellId) != NULL)
        {
            ++removalKeys;
        }
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyQuestTameSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(registry.Find<AuraDummyDruidSite>(spellId) == NULL);
    }
    CHECK_EQ(removalKeys, std::size_t(7));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyApplyGenericSite::Key), std::size_t(41));
    CHECK(game.FindDefault<AuraDummyApplyGenericSite>() == NULL);
    for (uint32 spellId : labels)
    {
        CHECK(game.Find<AuraDummyApplyGenericSite>(spellId) == registry.Find<AuraDummyApplyGenericSite>(spellId));
    }
    for (uint32 spellId : alsoRemoved)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
}

TEST(AuraDummyHandlers_TheGenericApplyContextAliasesTheTargetAndTheTimer)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = FakeAura(0);
    int32 periodicTimer = 0;
    AuraDummyApplyGenericContext ctx(aura, target, periodicTimer);
    CHECK(ctx.aura == aura);
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
    ctx.m_periodicTimer = 30000;                                // a body's write to the timer...
    CHECK_EQ(periodicTimer, int32(30000));                      // ...is the member the site bound
    periodicTimer = 5;
    CHECK_EQ(ctx.m_periodicTimer, int32(5));
}

TEST(AuraDummyHandlers_TheHauntingSpiritsBodyMakesTheAuraPeriodic)
{
    // The body sets the periodic flag, a 30 second period and the bound timer from it, reads no target, and returns.
    AuraStorage storage = {};
    Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
    AuraStorage expected = storage;
    reinterpret_cast<Aura*>(expected.bytes)->SetPeriodic(true);
    reinterpret_cast<Aura*>(expected.bytes)->GetModifier()->periodictime = 30000;
    Unit* target = NULL;
    int32 periodicTimer = 0;
    AuraDummyApplyGenericContext ctx(aura, target, periodicTimer);
    CHECK(SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyGenericSite>(7057, ctx).IsReturn());
    CHECK(aura->IsPeriodic());
    CHECK_EQ(aura->GetModifier()->periodictime, uint32(30000));
    CHECK_EQ(periodicTimer, int32(30000));
    CHECK_EQ(BitsThatDiffer(storage, expected), 0);             // nothing else of the aura changed
    CHECK(target == NULL);
}

TEST(AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault)
{
    static uint32 const labels[] = { 16739, 42365, 50517, 51926, 65386, 65495, 65528, 65529, 71450 };

    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraShapeshiftHandlers(registry), uint32(10)); // nine rows and the default
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraTransformSite::Key), std::size_t(9));

    std::set<SpellHandler<AuraTransformSite>::Function> distinct;
    for (uint32 spellId : labels)
    {
        SpellHandler<AuraTransformSite>::Function function = registry.Find<AuraTransformSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    // Seven bodies: 50517 and 51926 share one, 65386 and 65495 another. Proven by verbatim, not here: this binary
    // links with COMDAT folding, so an identical second function folds to the same address.
    CHECK_EQ(distinct.size(), std::size_t(7));
    CHECK(registry.Find<AuraTransformSite>(50517) == registry.Find<AuraTransformSite>(51926));
    CHECK(registry.Find<AuraTransformSite>(65386) == registry.Find<AuraTransformSite>(65495));

    // The default is its own body; an id with no row finds none.
    SpellHandler<AuraTransformSite>::Function onMiss = registry.FindDefault<AuraTransformSite>();
    CHECK(onMiss != NULL);
    CHECK(distinct.count(onMiss) == 0);
    CHECK(registry.Find<AuraTransformSite>(44186) == NULL);     // commented out in the switch: the default's
    CHECK(registry.Find<AuraTransformSite>(12345) == NULL);

    // Registering again on the same table changes nothing: every key and the default are taken.
    CHECK_EQ(RegisterAuraShapeshiftHandlers(registry), uint32(10));
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.FindDefault<AuraTransformSite>() == onMiss);

    // Keyed on the transform site only: its labels are no aura dummy site's, and theirs are not its.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : labels)
    {
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == NULL);
    }
    CHECK(game.Find<AuraTransformSite>(41099) == NULL);
    CHECK(game.Find<AuraTransformSite>(29266) == NULL);

    // The game's table holds the same rows and the same default.
    CHECK_EQ(game.CountAt(AuraTransformSite::Key), std::size_t(9));
    for (uint32 spellId : labels)
    {
        CHECK(game.Find<AuraTransformSite>(spellId) == registry.Find<AuraTransformSite>(spellId));
    }
    CHECK(game.FindDefault<AuraTransformSite>() == onMiss);
}

TEST(AuraShapeshiftHandlers_TheTransformContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    AuraTransformContext ctx(65528, target);
    CHECK_EQ(ctx.spellId, uint32(65528));
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
}

TEST(AuraControlHandlers_TheThreatSiteHoldsItsTwoLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraControlHandlers(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraThreatSite::Key), std::size_t(2));

    // Two labels, two distinct bodies.
    SpellHandler<AuraThreatSite>::Function arcaneShroud = registry.Find<AuraThreatSite>(26400);
    SpellHandler<AuraThreatSite>::Function eyeOfDiminution = registry.Find<AuraThreatSite>(28862);
    CHECK(arcaneShroud != NULL);
    CHECK(eyeOfDiminution != NULL);
    CHECK(arcaneShroud != eyeOfDiminution);

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the member goes on.
    CHECK(registry.FindDefault<AuraThreatSite>() == NULL);
    CHECK(registry.Find<AuraThreatSite>(12345) == NULL);
    Unit* target = NULL;
    int level_diff = 0;
    int multiplier = 0;
    AuraThreatContext ctx(target, level_diff, multiplier);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraThreatSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK_EQ(level_diff, 0);
    CHECK_EQ(multiplier, 0);

    // Registering again on the same table changes nothing: both keys are taken.
    CHECK_EQ(RegisterAuraControlHandlers(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK(registry.Find<AuraThreatSite>(26400) == arcaneShroud);
    CHECK(registry.Find<AuraThreatSite>(28862) == eyeOfDiminution);

    // Keyed on the threat site only: its labels are no other site's, and the other sites' are not its.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 26400u, 28862u })
    {
        CHECK(game.Find<AuraTransformSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == NULL);
    }
    CHECK(game.Find<AuraThreatSite>(16739) == NULL);
    CHECK(game.Find<AuraThreatSite>(29266) == NULL);

    // The game's table holds the same rows and no default.
    CHECK_EQ(game.CountAt(AuraThreatSite::Key), std::size_t(2));
    CHECK(game.Find<AuraThreatSite>(26400) == arcaneShroud);
    CHECK(game.Find<AuraThreatSite>(28862) == eyeOfDiminution);
    CHECK(game.FindDefault<AuraThreatSite>() == NULL);
}

TEST(AuraControlHandlers_TheThreatContextAliasesTheThreeLocals)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    int level_diff = 0;
    int multiplier = 0;
    AuraThreatContext ctx(target, level_diff, multiplier);
    CHECK(ctx.target == target);
    ctx.level_diff = 13;                                        // a body's writes to the two locals...
    ctx.multiplier = 2;
    CHECK_EQ(level_diff, 13);                                   // ...are the function's locals, which it reads after
    CHECK_EQ(multiplier, 2);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    level_diff = -5;
    CHECK_EQ(ctx.level_diff, -5);
}

TEST(SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectTailHandlers(registry), uint32(2)); // one row and the default
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectTransmittedSite::Key), std::size_t(1));

    // One label with its own body, and the default, another body.
    SpellHandler<SpellEffectTransmittedSite>::Function createSoulwell =
        registry.Find<SpellEffectTransmittedSite>(29886);
    SpellHandler<SpellEffectTransmittedSite>::Function onMiss = registry.FindDefault<SpellEffectTransmittedSite>();
    CHECK(createSoulwell != NULL);
    CHECK(onMiss != NULL);
    CHECK(createSoulwell != onMiss);
    CHECK(registry.Find<SpellEffectTransmittedSite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and keeps the
    // effect's entry.
    Unit* caster = NULL;
    uint32 name_id = 177000;
    SpellEffectTransmittedContext ctx(caster, name_id);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellEffectTransmittedSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK_EQ(name_id, uint32(177000));
    CHECK(caster == NULL);

    // Registering again on the same table changes nothing: the key and the default are taken.
    CHECK_EQ(RegisterSpellEffectTailHandlers(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellEffectTransmittedSite>(29886) == createSoulwell);
    CHECK(registry.FindDefault<SpellEffectTransmittedSite>() == onMiss);

    // Keyed on the transmitted-object site only: its label is no other site's, and the other sites' are not its.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraTransformSite>(29886) == NULL);
    CHECK(game.Find<AuraThreatSite>(29886) == NULL);
    CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(29886) == NULL);
    CHECK(game.Find<AuraDummyRemoveSite>(29886) == NULL);
    CHECK(game.Find<SpellEffectTransmittedSite>(16739) == NULL);
    CHECK(game.Find<SpellEffectTransmittedSite>(26400) == NULL);
    CHECK(game.Find<SpellEffectTransmittedSite>(29266) == NULL);

    // The game's table holds the same row and the same default.
    CHECK_EQ(game.CountAt(SpellEffectTransmittedSite::Key), std::size_t(1));
    CHECK(game.Find<SpellEffectTransmittedSite>(29886) == createSoulwell);
    CHECK(game.FindDefault<SpellEffectTransmittedSite>() == onMiss);
}

TEST(SpellEffectTailHandlers_TheTransmittedContextAliasesTheCasterAndTheEntry)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    uint32 name_id = 177000;
    SpellEffectTransmittedContext ctx(caster, name_id);
    CHECK(ctx.m_caster == caster);
    CHECK_EQ(ctx.name_id, uint32(177000));
    ctx.name_id = 183510;                                       // a body's write to `name_id`...
    CHECK_EQ(name_id, uint32(183510));                          // ...is the function's local, which it reads after
    name_id = 183511;
    CHECK_EQ(ctx.name_id, uint32(183511));
    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
}

namespace
{
    // Whether `spellId` is a key at a site other than `site`: the periodic auras' five, the energize, activate-object,
    // resurrect, two targeting, cast-check, target-check, weapon-damage, school-damage, trigger-spell, two teleport and
    // six dummy-effect sites, the rogue, hunter and paladin aura dummy sites, the generic, hunter and shaman aura
    // dummy apply sites, the six trigger-spell aura sites, the two tick sites, the five periodic dummy tick sites, the
    // five boost sites, the six
    // script-effect sites, the location-check site, the three bonus sites, the eight dummy proc sites, the haste,
    // crit-chance and two proc-trigger sites, the generic aura dummy apply-and-remove site and the earlier files'
    // ids.
    // Whether the entry of the shared list naming `spellId` at `site` lists `key` too (defined below the list).
    bool ListedWith(uint32 spellId, uint32 site, uint32 key);

    // `also` is a second site the id may be a key at, the other site of a shared id; 0 is no site's key. Given one,
    // every site the shared list's entry for the id at `site` lists is skipped too; given none, nothing more is.
    bool HeldByAnotherSite(SpellHandlerRegistry const& registry, uint32 site, uint32 spellId, uint32 also = 0)
    {
        auto skips = [site, also, spellId](uint32 key)
        {
            return key != site && key != also && !(also != 0 && ListedWith(spellId, site, key));
        };
        return (skips(AuraProcTriggerSite::Key) && registry.Find<AuraProcTriggerSite>(spellId) != NULL) ||
               (skips(AuraPeriodicTriggerSite::Key) && registry.Find<AuraPeriodicTriggerSite>(spellId) != NULL) ||
               (skips(AuraPeriodicEnergizeSite::Key) && registry.Find<AuraPeriodicEnergizeSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDummyRogueSite::Key) &&
                registry.Find<AuraPeriodicDummyRogueSite>(spellId) != NULL) ||
               (skips(AuraIncreaseHealthSite::Key) && registry.Find<AuraIncreaseHealthSite>(spellId) != NULL) ||
               (skips(SpellEffectEnergizeSite::Key) && registry.Find<SpellEffectEnergizeSite>(spellId) != NULL) ||
               (skips(SpellEffectActivateObjectSite::Key) &&
                registry.Find<SpellEffectActivateObjectSite>(spellId) != NULL) ||
               (skips(SpellEffectResurrectSite::Key) && registry.Find<SpellEffectResurrectSite>(spellId) != NULL) ||
               (skips(SpellTargetAllEnemyInAreaSite::Key) &&
                registry.Find<SpellTargetAllEnemyInAreaSite>(spellId) != NULL) ||
               (skips(SpellTargetEffectDummySite::Key) &&
                registry.Find<SpellTargetEffectDummySite>(spellId) != NULL) ||
               (skips(SpellCheckCastAuraDummySite::Key) &&
                registry.Find<SpellCheckCastAuraDummySite>(spellId) != NULL) ||
               (skips(SpellCheckTargetSite::Key) && registry.Find<SpellCheckTargetSite>(spellId) != NULL) ||
               (skips(SpellEffectWeaponDmgSite::Key) && registry.Find<SpellEffectWeaponDmgSite>(spellId) != NULL) ||
               (skips(SpellEffectSchoolDmgSite::Key) && registry.Find<SpellEffectSchoolDmgSite>(spellId) != NULL) ||
               (skips(SpellEffectTriggerSpellSite::Key) &&
                registry.Find<SpellEffectTriggerSpellSite>(spellId) != NULL) ||
               (skips(SpellEffectTeleportRecallSite::Key) &&
                registry.Find<SpellEffectTeleportRecallSite>(spellId) != NULL) ||
               (skips(SpellEffectTeleportPostSite::Key) &&
                registry.Find<SpellEffectTeleportPostSite>(spellId) != NULL) ||
               (skips(SpellEffectDummyGenericSite::Key) &&
                registry.Find<SpellEffectDummyGenericSite>(spellId) != NULL) ||
               (skips(SpellEffectDummyMageSite::Key) && registry.Find<SpellEffectDummyMageSite>(spellId) != NULL) ||
               (skips(SpellEffectDummyWarriorSite::Key) &&
                registry.Find<SpellEffectDummyWarriorSite>(spellId) != NULL) ||
               (skips(SpellEffectDummyRogueSite::Key) && registry.Find<SpellEffectDummyRogueSite>(spellId) != NULL) ||
               (skips(SpellEffectDummyHunterSite::Key) &&
                registry.Find<SpellEffectDummyHunterSite>(spellId) != NULL) ||
               (skips(SpellEffectDummyPaladinSite::Key) &&
                registry.Find<SpellEffectDummyPaladinSite>(spellId) != NULL) ||
               (skips(AuraDummyRogueSite::Key) && registry.Find<AuraDummyRogueSite>(spellId) != NULL) ||
               (skips(AuraDummyHunterSite::Key) && registry.Find<AuraDummyHunterSite>(spellId) != NULL) ||
               (skips(AuraDummyPaladinSite::Key) && registry.Find<AuraDummyPaladinSite>(spellId) != NULL) ||
               (skips(AuraDummyApplyHunterSite::Key) && registry.Find<AuraDummyApplyHunterSite>(spellId) != NULL) ||
               (skips(AuraDummyApplyShamanSite::Key) && registry.Find<AuraDummyApplyShamanSite>(spellId) != NULL) ||
               (skips(AuraTriggerSpellGenericSite::Key) &&
                registry.Find<AuraTriggerSpellGenericSite>(spellId) != NULL) ||
               (skips(AuraTriggerSpellMageSite::Key) && registry.Find<AuraTriggerSpellMageSite>(spellId) != NULL) ||
               (skips(AuraTriggerSpellHunterSite::Key) &&
                registry.Find<AuraTriggerSpellHunterSite>(spellId) != NULL) ||
               (skips(AuraTriggerSpellDruidSite::Key) && registry.Find<AuraTriggerSpellDruidSite>(spellId) != NULL) ||
               (skips(AuraTriggerSpellShamanSite::Key) &&
                registry.Find<AuraTriggerSpellShamanSite>(spellId) != NULL) ||
               (skips(AuraTriggerSpellCustomSite::Key) &&
                registry.Find<AuraTriggerSpellCustomSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDamageTickSite::Key) &&
                registry.Find<AuraPeriodicDamageTickSite>(spellId) != NULL) ||
               (skips(AuraPeriodicManaLeechTickSite::Key) &&
                registry.Find<AuraPeriodicManaLeechTickSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDummyTickGenericSite::Key) &&
                registry.Find<AuraPeriodicDummyTickGenericSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDummyTickMageSite::Key) &&
                registry.Find<AuraPeriodicDummyTickMageSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDummyTickDruidSite::Key) &&
                registry.Find<AuraPeriodicDummyTickDruidSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDummyTickRogueSite::Key) &&
                registry.Find<AuraPeriodicDummyTickRogueSite>(spellId) != NULL) ||
               (skips(AuraPeriodicDummyTickHunterSite::Key) &&
                registry.Find<AuraPeriodicDummyTickHunterSite>(spellId) != NULL) ||
               (skips(AuraDummyApplyGenericSite::Key) &&
                registry.Find<AuraDummyApplyGenericSite>(spellId) != NULL) ||
               (skips(AuraBoostGenericSite::Key) && registry.Find<AuraBoostGenericSite>(spellId) != NULL) ||
               (skips(AuraBoostMageSite::Key) && registry.Find<AuraBoostMageSite>(spellId) != NULL) ||
               (skips(AuraBoostPriestSite::Key) && registry.Find<AuraBoostPriestSite>(spellId) != NULL) ||
               (skips(AuraBoostHunterSite::Key) && registry.Find<AuraBoostHunterSite>(spellId) != NULL) ||
               (skips(AuraBoostDeathKnightSite::Key) &&
                registry.Find<AuraBoostDeathKnightSite>(spellId) != NULL) ||
               (skips(SpellEffectScriptGenericSite::Key) &&
                registry.Find<SpellEffectScriptGenericSite>(spellId) != NULL) ||
               (skips(SpellEffectScriptWarlockSite::Key) &&
                registry.Find<SpellEffectScriptWarlockSite>(spellId) != NULL) ||
               (skips(SpellEffectScriptPriestSite::Key) &&
                registry.Find<SpellEffectScriptPriestSite>(spellId) != NULL) ||
               (skips(SpellEffectScriptHunterSite::Key) &&
                registry.Find<SpellEffectScriptHunterSite>(spellId) != NULL) ||
               (skips(SpellEffectScriptPotionSite::Key) &&
                registry.Find<SpellEffectScriptPotionSite>(spellId) != NULL) ||
               (skips(SpellEffectScriptDeathKnightSite::Key) &&
                registry.Find<SpellEffectScriptDeathKnightSite>(spellId) != NULL) ||
               (skips(SpellAllowedInLocationSite::Key) && registry.Find<SpellAllowedInLocationSite>(spellId) != NULL) ||
               (skips(SpellDamageTakenSite::Key) && registry.Find<SpellDamageTakenSite>(spellId) != NULL) ||
               (skips(SpellHealingDoneSite::Key) && registry.Find<SpellHealingDoneSite>(spellId) != NULL) ||
               (skips(MeleeDamageTakenSite::Key) && registry.Find<MeleeDamageTakenSite>(spellId) != NULL) ||
               (skips(DummyAuraProcGenericSite::Key) &&
                registry.Find<DummyAuraProcGenericSite>(spellId) != NULL) ||
               (skips(DummyAuraProcMageSite::Key) && registry.Find<DummyAuraProcMageSite>(spellId) != NULL) ||
               (skips(DummyAuraProcWarlockSite::Key) &&
                registry.Find<DummyAuraProcWarlockSite>(spellId) != NULL) ||
               (skips(DummyAuraProcPriestSite::Key) && registry.Find<DummyAuraProcPriestSite>(spellId) != NULL) ||
               (skips(DummyAuraProcDruidSite::Key) && registry.Find<DummyAuraProcDruidSite>(spellId) != NULL) ||
               (skips(DummyAuraProcRogueSite::Key) && registry.Find<DummyAuraProcRogueSite>(spellId) != NULL) ||
               (skips(DummyAuraProcPaladinSite::Key) &&
                registry.Find<DummyAuraProcPaladinSite>(spellId) != NULL) ||
               (skips(DummyAuraProcShamanSite::Key) && registry.Find<DummyAuraProcShamanSite>(spellId) != NULL) ||
               (skips(HasteAuraProcRogueSite::Key) && registry.Find<HasteAuraProcRogueSite>(spellId) != NULL) ||
               (skips(SpellCritChanceAuraProcMageSite::Key) &&
                registry.Find<SpellCritChanceAuraProcMageSite>(spellId) != NULL) ||
               (skips(ProcTriggerSpellAuraProcGenericSite::Key) &&
                registry.Find<ProcTriggerSpellAuraProcGenericSite>(spellId) != NULL) ||
               (skips(ProcTriggerSpellAuraProcTriggeredSite::Key) &&
                registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(spellId) != NULL) ||
               (skips(AuraDummyApplyRemoveGenericSite::Key) &&
                registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) != NULL) ||
               registry.Find<AuraTransformSite>(spellId) != NULL || registry.Find<AuraThreatSite>(spellId) != NULL ||
               registry.Find<SpellEffectTransmittedSite>(spellId) != NULL ||
               registry.Find<AuraDummyRemoveSite>(spellId) != NULL;
    }

    // The ids a pair of listed sites both register, each site's switch holding the label, with the two sites:
    // Cannibalize, at the dummy-effect targeting site and the generic dummy-effect site; Doom, at the all-enemy-in-area
    // targeting site and the generic trigger-spell aura site; Invisibility, at the periodic-trigger site and the mage
    // trigger-spell aura site; Frenzied Regeneration, at the druid trigger-spell aura site and the druid periodic
    // dummy tick site; Haunting Spirits, at the generic aura dummy apply site and the generic periodic dummy tick
    // site; Vigilance, at the proc-trigger site and the generic boost site; Master's Call, at the hunter dummy-effect
    // site and the hunter script-effect site; eight Cleansing Flames ids, at the generic dummy-effect site and the
    // generic script-effect site; Pick Red Riding Hood, at the all-enemy-in-area targeting site and the generic
    // script-effect site; Signal Alliance and the portals to Orgrimmar and Stormwind, at the generic dummy-effect
    // site and the generic script-effect site; Clear Insane, at the generic aura dummy apply site and the generic
    // script-effect site; Cheating Death, at the spell and melee damage-taken sites; Sanctuary, at those two, the
    // paladin dummy proc site and the paladin aura dummy site; 25899, at those two and the paladin dummy proc site;
    // Kill Command, at the cast-check site and the hunter aura dummy apply site; Unstable Power and Restless Strength, at the generic dummy proc site and
    // the generic aura dummy apply-and-remove site; 58914, at the generic dummy proc site and the generic boost site;
    // 74396, at the mage dummy proc site and the mage boost site; Tricks of the Trade, at the rogue dummy proc site
    // and the rogue aura dummy site; Brain Freeze, at the triggered-spell proc site and the mage boost site. An entry
    // lists an id at two to four sites; a listed id held at a site its entry does not list fails HeldByAnotherSite.
    struct SharedSiteId
    {
        uint32 spellId;
        uint32 sites[4];                                        // the sites the id is a key at; 0 past the last
    };

    SharedSiteId const SHARED_SITE_IDS[] =
    {
        { 20577, SpellTargetEffectDummySite::Key, SpellEffectDummyGenericSite::Key },
        { 31347, SpellTargetAllEnemyInAreaSite::Key, AuraTriggerSpellGenericSite::Key },
        { 66, AuraPeriodicTriggerSite::Key, AuraTriggerSpellMageSite::Key },
        { 22842, AuraTriggerSpellDruidSite::Key, AuraPeriodicDummyTickDruidSite::Key },
        { 7057, AuraDummyApplyGenericSite::Key, AuraPeriodicDummyTickGenericSite::Key },
        { 50720, AuraProcTriggerSite::Key, AuraBoostGenericSite::Key },
        { 53271, SpellEffectDummyHunterSite::Key, SpellEffectScriptHunterSite::Key },
        { 29126, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 29135, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 29136, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 29137, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 29138, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 29139, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 46671, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 46672, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 30769, SpellTargetAllEnemyInAreaSite::Key, SpellEffectScriptGenericSite::Key },
        { 45958, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 58418, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 58420, SpellEffectDummyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 63122, AuraDummyApplyGenericSite::Key, SpellEffectScriptGenericSite::Key },
        { 45182, SpellDamageTakenSite::Key, MeleeDamageTakenSite::Key },
        { 20911, SpellDamageTakenSite::Key, MeleeDamageTakenSite::Key, DummyAuraProcPaladinSite::Key,
          AuraDummyPaladinSite::Key },
        { 25899, SpellDamageTakenSite::Key, MeleeDamageTakenSite::Key, DummyAuraProcPaladinSite::Key },
        { 24658, DummyAuraProcGenericSite::Key, AuraDummyApplyRemoveGenericSite::Key },
        { 24661, DummyAuraProcGenericSite::Key, AuraDummyApplyRemoveGenericSite::Key },
        { 58914, DummyAuraProcGenericSite::Key, AuraBoostGenericSite::Key },
        { 74396, DummyAuraProcMageSite::Key, AuraBoostMageSite::Key },
        { 57934, DummyAuraProcRogueSite::Key, AuraDummyRogueSite::Key },
        { 57761, ProcTriggerSpellAuraProcTriggeredSite::Key, AuraBoostMageSite::Key },
        { 34026, SpellCheckCastAuraDummySite::Key, AuraDummyApplyHunterSite::Key },
    };

    // Whether `shared` lists `site` among its sites.
    bool Lists(SharedSiteId const& shared, uint32 site)
    {
        for (uint32 key : shared.sites)
        {
            if (key != 0 && key == site)
            {
                return true;
            }
        }
        return false;
    }

    // The first other site the entry naming `spellId` at `site` lists, or `site` itself when no entry does.
    uint32 SharedWith(uint32 spellId, uint32 site)
    {
        for (SharedSiteId const& shared : SHARED_SITE_IDS)
        {
            if (shared.spellId != spellId || !Lists(shared, site))
            {
                continue;
            }
            for (uint32 key : shared.sites)
            {
                if (key != 0 && key != site)
                {
                    return key;
                }
            }
        }
        return site;
    }

    bool ListedWith(uint32 spellId, uint32 site, uint32 key)
    {
        for (SharedSiteId const& shared : SHARED_SITE_IDS)
        {
            if (shared.spellId == spellId && Lists(shared, site))
            {
                return Lists(shared, key);
            }
        }
        return false;
    }

    // 2 proc-trigger labels, 4 periodic-trigger labels, 5 energize labels, 1 rogue label and 14 health labels: 26
    // rows; four of the five sites have a `default:`.
    uint32 const PERIODIC_ROWS = 26;
    uint32 const PERIODIC_DEFAULTS = 4;

    // Registers the file's handlers on an empty `registry`, checks the counts, and that a second registration
    // changes none of them.
    void RegisterPeriodicTwice(SpellHandlerRegistry& registry)
    {
        CHECK_EQ(RegisterAuraPeriodicHandlers(registry), PERIODIC_ROWS + PERIODIC_DEFAULTS);
        CHECK_EQ(registry.Count(), std::size_t(PERIODIC_ROWS));
        CHECK_EQ(registry.CountDefaults(), std::size_t(PERIODIC_DEFAULTS));
        CHECK_EQ(RegisterAuraPeriodicHandlers(registry), PERIODIC_ROWS + PERIODIC_DEFAULTS);
        CHECK_EQ(registry.Count(), std::size_t(PERIODIC_ROWS));
        CHECK_EQ(registry.CountDefaults(), std::size_t(PERIODIC_DEFAULTS));
    }
}

TEST(AuraPeriodicHandlers_TheProcTriggerSiteHoldsItsTwoLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraProcTriggerSite::Key), std::size_t(2));

    // Two labels, two distinct bodies, and a default that is neither.
    SpellHandler<AuraProcTriggerSite>::Function ascendance = registry.Find<AuraProcTriggerSite>(28200);
    SpellHandler<AuraProcTriggerSite>::Function vigilance = registry.Find<AuraProcTriggerSite>(50720);
    SpellHandler<AuraProcTriggerSite>::Function onMiss = registry.FindDefault<AuraProcTriggerSite>();
    CHECK(ascendance != NULL);
    CHECK(vigilance != NULL);
    CHECK(onMiss != NULL);
    CHECK(ascendance != vigilance);
    CHECK(onMiss != ascendance && onMiss != vigilance);
    CHECK(registry.Find<AuraProcTriggerSite>(12345) == NULL);

    // The switch's `default: break;`: a miss runs the default, which continues after the switch; 28200 at remove
    // reads only `apply` and continues too.
    Unit* target = NULL;
    bool apply = false;
    AuraProcTriggerContext ctx(ModeAura(), target, apply);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraProcTriggerSite>(12345, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());
    CHECK(registry.Dispatch<AuraProcTriggerSite>(28200, ctx).IsContinue());
    CHECK(target == NULL);

    // Vigilance is a key at the generic boost site too, the other site the shared list names for it.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 28200u, 50720u })
    {
        uint32 other = SharedWith(spellId, AuraProcTriggerSite::Key);
        CHECK(!HeldByAnotherSite(game, AuraProcTriggerSite::Key, spellId, other));
    }
    CHECK(SharedWith(28200, AuraProcTriggerSite::Key) == AuraProcTriggerSite::Key);
    CHECK(SharedWith(50720, AuraProcTriggerSite::Key) == AuraBoostGenericSite::Key);
    CHECK_EQ(game.CountAt(AuraProcTriggerSite::Key), std::size_t(2));
    CHECK(game.Find<AuraProcTriggerSite>(28200) == ascendance);
    CHECK(game.Find<AuraProcTriggerSite>(50720) == vigilance);
    CHECK(game.FindDefault<AuraProcTriggerSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_TheProcTriggerContextAliasesTheTargetAndApply)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    bool apply = true;
    AuraProcTriggerContext ctx(ModeAura(), target, apply);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    CHECK(ctx.apply);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    apply = false;
    CHECK(!ctx.apply);
}

TEST(AuraPeriodicHandlers_ThePeriodicTriggerSiteHoldsItsFourLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraPeriodicTriggerSite::Key), std::size_t(4));

    // Four labels, four distinct bodies, and a default that is none of them.
    static uint32 const ids[] = { 66, 42783, 46221, 51912 };
    std::set<SpellHandler<AuraPeriodicTriggerSite>::Function> bodies;
    for (uint32 spellId : ids)
    {
        SpellHandler<AuraPeriodicTriggerSite>::Function body = registry.Find<AuraPeriodicTriggerSite>(spellId);
        CHECK(body != NULL);
        bodies.insert(body);
    }
    CHECK_EQ(bodies.size(), std::size_t(4));
    SpellHandler<AuraPeriodicTriggerSite>::Function onMiss = registry.FindDefault<AuraPeriodicTriggerSite>();
    CHECK(onMiss != NULL);
    CHECK(bodies.count(onMiss) == 0);
    CHECK(registry.Find<AuraPeriodicTriggerSite>(12345) == NULL);

    // A miss runs the `default: break;` and continues after the switch. 66, 42783 and 51912 act only at expiry: at
    // another removal mode they read the mode alone and return from the member at their `return;`.
    Aura* aura = ModeAura();
    Unit* target = NULL;
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
    AuraPeriodicTriggerContext ctx(aura, target);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraPeriodicTriggerSite>(12345, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());
    for (uint32 spellId : { 66u, 42783u, 51912u })
    {
        CHECK(registry.Dispatch<AuraPeriodicTriggerSite>(spellId, ctx).IsReturn());
    }
    CHECK(target == NULL);

    // Keyed on this site: no label is another site's key but Invisibility (66), which the mage trigger-spell aura
    // site holds too and no third site does.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ids)
    {
        uint32 other = SharedWith(spellId, AuraPeriodicTriggerSite::Key);
        CHECK(!HeldByAnotherSite(game, AuraPeriodicTriggerSite::Key, spellId, other));
        CHECK((other != AuraPeriodicTriggerSite::Key) == (spellId == 66));
        CHECK(game.Find<AuraPeriodicTriggerSite>(spellId) == registry.Find<AuraPeriodicTriggerSite>(spellId));
    }
    CHECK_EQ(game.CountAt(AuraPeriodicTriggerSite::Key), std::size_t(4));
    CHECK(game.FindDefault<AuraPeriodicTriggerSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_ThePeriodicTriggerContextAliasesTheTarget)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    AuraPeriodicTriggerContext ctx(ModeAura(), target);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    target = NULL;
    CHECK(ctx.target == NULL);
}

TEST(AuraPeriodicHandlers_TheEnergizeSiteHoldsItsFiveLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraPeriodicEnergizeSite::Key), std::size_t(5));

    // Five labels, four bodies (the two Replenishment labels share one), and a default that is none of them.
    SpellHandler<AuraPeriodicEnergizeSite>::Function glyph = registry.Find<AuraPeriodicEnergizeSite>(54833);
    SpellHandler<AuraPeriodicEnergizeSite>::Function innervate = registry.Find<AuraPeriodicEnergizeSite>(29166);
    SpellHandler<AuraPeriodicEnergizeSite>::Function owlkin = registry.Find<AuraPeriodicEnergizeSite>(48391);
    SpellHandler<AuraPeriodicEnergizeSite>::Function replenishment = registry.Find<AuraPeriodicEnergizeSite>(57669);
    SpellHandler<AuraPeriodicEnergizeSite>::Function onMiss = registry.FindDefault<AuraPeriodicEnergizeSite>();
    std::set<SpellHandler<AuraPeriodicEnergizeSite>::Function> bodies = { glyph, innervate, owlkin, replenishment,
                                                                          onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(5));
    CHECK(registry.Find<AuraPeriodicEnergizeSite>(61782) == replenishment);
    CHECK(registry.Find<AuraPeriodicEnergizeSite>(12345) == NULL);

    // A miss runs the `default: break;` and continues after the switch.
    Unit* target = NULL;
    AuraPeriodicEnergizeContext ctx(ModeAura(), target);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraPeriodicEnergizeSite>(12345, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 54833u, 29166u, 48391u, 57669u, 61782u })
    {
        CHECK(!HeldByAnotherSite(game, AuraPeriodicEnergizeSite::Key, spellId));
        CHECK(game.Find<AuraPeriodicEnergizeSite>(spellId) == registry.Find<AuraPeriodicEnergizeSite>(spellId));
    }
    CHECK_EQ(game.CountAt(AuraPeriodicEnergizeSite::Key), std::size_t(5));
    CHECK(game.FindDefault<AuraPeriodicEnergizeSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_TheEnergizeContextAliasesTheTarget)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    AuraPeriodicEnergizeContext ctx(ModeAura(), target);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    target = NULL;
    CHECK(ctx.target == NULL);
}

TEST(AuraPeriodicHandlers_TheRogueSiteHoldsItsOneLabelAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyRogueSite::Key), std::size_t(1));
    SpellHandler<AuraPeriodicDummyRogueSite>::Function masterOfSubtlety =
        registry.Find<AuraPeriodicDummyRogueSite>(31666);
    CHECK(masterOfSubtlety != NULL);

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the family case goes on.
    CHECK(registry.FindDefault<AuraPeriodicDummyRogueSite>() == NULL);
    CHECK(registry.Find<AuraPeriodicDummyRogueSite>(31665) == NULL);
    Unit* target = NULL;
    bool apply = true;
    AuraPeriodicDummyRogueContext ctx(ModeAura(), target, apply);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraPeriodicDummyRogueSite>(31665, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(!HeldByAnotherSite(game, AuraPeriodicDummyRogueSite::Key, 31666));
    CHECK_EQ(game.CountAt(AuraPeriodicDummyRogueSite::Key), std::size_t(1));
    CHECK(game.Find<AuraPeriodicDummyRogueSite>(31666) == masterOfSubtlety);
    CHECK(game.FindDefault<AuraPeriodicDummyRogueSite>() == NULL);
}

TEST(AuraPeriodicHandlers_TheRogueContextAliasesTheTargetAndApply)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    bool apply = false;
    AuraPeriodicDummyRogueContext ctx(ModeAura(), target, apply);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    CHECK(!ctx.apply);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    apply = true;
    CHECK(ctx.apply);
}

TEST(AuraPeriodicHandlers_TheIncreaseHealthSiteHoldsItsFourteenLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraIncreaseHealthSite::Key), std::size_t(14));

    // Fourteen labels, two bodies and a default: the three percentage labels share one body, the eleven flat
    // labels the other.
    static uint32 const percent[] = { 54443, 55233, 61254 };
    static uint32 const flat[] = { 12976, 28726, 31616, 34511, 44055, 55915, 55917, 67596, 50322, 53479, 59465 };
    SpellHandler<AuraIncreaseHealthSite>::Function percentBody = registry.Find<AuraIncreaseHealthSite>(54443);
    SpellHandler<AuraIncreaseHealthSite>::Function flatBody = registry.Find<AuraIncreaseHealthSite>(12976);
    SpellHandler<AuraIncreaseHealthSite>::Function onMiss = registry.FindDefault<AuraIncreaseHealthSite>();
    CHECK(percentBody != NULL);
    CHECK(flatBody != NULL);
    CHECK(onMiss != NULL);
    CHECK(percentBody != flatBody);
    CHECK(onMiss != percentBody && onMiss != flatBody);
    CHECK(registry.Find<AuraIncreaseHealthSite>(60430) == NULL);   // HandleAuraModIncreaseHealthPercent's, a lookup

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : percent)
    {
        CHECK(registry.Find<AuraIncreaseHealthSite>(spellId) == percentBody);
        CHECK(game.Find<AuraIncreaseHealthSite>(spellId) == percentBody);
        CHECK(!HeldByAnotherSite(game, AuraIncreaseHealthSite::Key, spellId));
    }
    for (uint32 spellId : flat)
    {
        CHECK(registry.Find<AuraIncreaseHealthSite>(spellId) == flatBody);
        CHECK(game.Find<AuraIncreaseHealthSite>(spellId) == flatBody);
        CHECK(!HeldByAnotherSite(game, AuraIncreaseHealthSite::Key, spellId));
    }
    CHECK_EQ(game.CountAt(AuraIncreaseHealthSite::Key), std::size_t(14));
    CHECK(game.FindDefault<AuraIncreaseHealthSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs)
{
    // A bare Creature: HandleStatModifier records the amount in its modifier group and, with its stats not yet
    // modifiable, touches nothing else.
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit* target = &creature;
    Aura* aura = ModeAura();
    aura->GetModifier()->m_amount = 7;
    bool apply = true;
    bool real = false;
    AuraIncreaseHealthContext ctx(aura, target, apply, real);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Not a real apply: the flat labels return from the member without touching the target, and the percentage
    // labels fall into the flat body (no break) and return with it.
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(12976, ctx).IsReturn());
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(54443, ctx).IsReturn());
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(61254, ctx).IsReturn());
    CHECK_EQ(aura->GetModifier()->m_amount, 7);
    CHECK_EQ(creature.GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE), 0.0f);

    // A miss runs the default, which falls off the switch's end: it adds the amount as a flat health bonus and
    // continues after the switch.
    SpellHandlerOutcome<void> miss = game.Dispatch<AuraIncreaseHealthSite>(60430, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());
    CHECK_EQ(creature.GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE), 7.0f);
    apply = false;                                              // the remove takes it back
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(60430, ctx).IsContinue());
    CHECK_EQ(creature.GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE), 0.0f);
    aura->GetModifier()->m_amount = 0;
}

TEST(AuraPeriodicHandlers_TheIncreaseHealthContextAliasesTheTargetApplyAndReal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    bool apply = true;
    bool real = false;
    AuraIncreaseHealthContext ctx(ModeAura(), target, apply, real);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    CHECK(ctx.apply);
    CHECK(!ctx.real);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    apply = false;
    real = true;
    CHECK(!ctx.apply);
    CHECK(ctx.real);
}

TEST(SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectHealPowerHandlers(registry), uint32(10)); // nine rows and the default
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectEnergizeSite::Key), std::size_t(9));

    // Nine labels, five bodies (the four base-mana labels share one, the two injectors another), and a default that
    // is none of them.
    typedef SpellHandler<SpellEffectEnergizeSite>::Function EnergizeBody;
    EnergizeBody restoreEnergy = registry.Find<SpellEffectEnergizeSite>(9512);
    EnergizeBody bloodFury = registry.Find<SpellEffectEnergizeSite>(24571);
    EnergizeBody burstOfEnergy = registry.Find<SpellEffectEnergizeSite>(24532);
    EnergizeBody baseManaPercent = registry.Find<SpellEffectEnergizeSite>(31930);
    EnergizeBody injector = registry.Find<SpellEffectEnergizeSite>(67487);
    EnergizeBody onMiss = registry.FindDefault<SpellEffectEnergizeSite>();
    std::set<EnergizeBody> bodies = { restoreEnergy, bloodFury, burstOfEnergy, baseManaPercent, injector, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(6));
    for (uint32 spellId : { 48542u, 63375u, 68082u })
    {
        CHECK(registry.Find<SpellEffectEnergizeSite>(spellId) == baseManaPercent);
    }
    CHECK(registry.Find<SpellEffectEnergizeSite>(67490) == injector);
    CHECK(registry.Find<SpellEffectEnergizeSite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and changes nothing.
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 100;
    int level_diff = 0;
    int level_multiplier = 0;
    SpellEffectEnergizeContext ctx(caster, target, damage, level_diff, level_multiplier);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellEffectEnergizeSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK_EQ(damage, int32(100));
    CHECK_EQ(level_diff, 0);
    CHECK_EQ(level_multiplier, 0);
    CHECK(caster == NULL);
    CHECK(target == NULL);

    // The injectors on a target that is not a player: the type test fails, the amount stays, and the member goes on.
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    target = &creature;
    for (uint32 spellId : { 67487u, 67490u })
    {
        SpellHandlerOutcome<void> injected = registry.Dispatch<SpellEffectEnergizeSite>(spellId, ctx);
        CHECK(injected.IsContinue());
        CHECK(!injected.IsReturn());
        CHECK_EQ(damage, int32(100));
    }
    target = NULL;

    // Registering again on the same table changes nothing: the keys and the default are taken.
    CHECK_EQ(RegisterSpellEffectHealPowerHandlers(registry), uint32(10));
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellEffectEnergizeSite>(9512) == restoreEnergy);
    CHECK(registry.FindDefault<SpellEffectEnergizeSite>() == onMiss);

    // Keyed on the energize site only: its labels are no other site's, and the other sites' are not its; the game's
    // table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 9512u, 24571u, 24532u, 31930u, 48542u, 63375u, 68082u, 67487u, 67490u })
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectEnergizeSite::Key, spellId));
        CHECK(game.Find<SpellEffectEnergizeSite>(spellId) == registry.Find<SpellEffectEnergizeSite>(spellId));
    }
    for (uint32 spellId : { 29886u, 41099u, 54833u, 29166u, 57669u, 12976u })
    {
        CHECK(game.Find<SpellEffectEnergizeSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectEnergizeSite::Key), std::size_t(9));
    CHECK(game.FindDefault<SpellEffectEnergizeSite>() == onMiss);
}

TEST(SpellEffectHealPowerHandlers_TheEnergizeContextAliasesTheSpellAndTheLevelLocals)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    int32 damage = 100;
    int level_diff = 0;
    int level_multiplier = 0;
    SpellEffectEnergizeContext ctx(caster, target, damage, level_diff, level_multiplier);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.unitTarget == target);
    CHECK_EQ(ctx.damage, int32(100));
    CHECK_EQ(ctx.level_diff, 0);
    CHECK_EQ(ctx.level_multiplier, 0);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    ctx.damage = 125;                                           // a body's write to `damage`...
    CHECK_EQ(damage, int32(125));                               // ...is the spell's amount, which it reads after
    damage = 90;
    CHECK_EQ(ctx.damage, int32(90));
    ctx.level_diff = 7;                                         // the two locals the member shrinks the amount by
    ctx.level_multiplier = 10;
    CHECK_EQ(level_diff, 7);
    CHECK_EQ(level_multiplier, 10);
    level_diff = -5;
    level_multiplier = 4;
    CHECK_EQ(ctx.level_diff, -5);
    CHECK_EQ(ctx.level_multiplier, 4);
}

namespace
{
    // Spell.dbc rows in raw storage, as the loader makes them (SpellEntry has no default constructor: it declares a
    // private copy constructor); zeroed here, with the id set.
    struct ObjectCombatSpellRow
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    ObjectCombatSpellRow s_objectCombatRows[2] = {};

    SpellEntry const* ObjectCombatSpell(std::size_t slot, uint32 id)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_objectCombatRows[slot].bytes);
        row->ID = id;
        return row;
    }

    // The 15 Wind Stone summons, the 11 Simon Game spells, the 5 Skettis summons, Place Fake Fur and Summon Ahune
    // Lieutenant, in the switch's order.
    uint32 const ACTIVATE_OBJECT_IDS[] =
    {
        24734, 24744, 24756, 24758, 24760, 24763, 24765, 24768, 24770, 24772, 24784, 24786, 24788, 24789, 24790,
        40176, 40177, 40178, 40179, 40283, 40284, 40285, 40286, 40494, 40495, 40512,
        40632, 40640, 40642, 40644, 41004,
        46085,
        46592
    };
}

TEST(SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37)); // 33 + 3 rows and the resurrect default
    CHECK_EQ(registry.Count(), std::size_t(36));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectActivateObjectSite::Key), std::size_t(33));

    // 33 labels, five bodies: the Wind Stone summons share one, the Simon Game spells another, the Skettis summons a
    // third; Place Fake Fur and Summon Ahune Lieutenant each have their own.
    typedef SpellHandler<SpellEffectActivateObjectSite>::Function ActivateBody;
    ActivateBody windStone = registry.Find<SpellEffectActivateObjectSite>(24734);
    ActivateBody simonGame = registry.Find<SpellEffectActivateObjectSite>(40176);
    ActivateBody skettis = registry.Find<SpellEffectActivateObjectSite>(40632);
    ActivateBody fakeFur = registry.Find<SpellEffectActivateObjectSite>(46085);
    ActivateBody ahune = registry.Find<SpellEffectActivateObjectSite>(46592);
    std::set<ActivateBody> bodies = { windStone, simonGame, skettis, fakeFur, ahune };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(5));
    for (std::size_t i = 0; i < 33; ++i)
    {
        ActivateBody body = registry.Find<SpellEffectActivateObjectSite>(ACTIVATE_OBJECT_IDS[i]);
        CHECK(body == (i < 15 ? windStone : i < 26 ? simonGame : i < 31 ? skettis : i == 31 ? fakeFur : ahune));
    }

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the misc value's case goes on to
    // its own `break;` with nothing changed.
    CHECK(registry.FindDefault<SpellEffectActivateObjectSite>() == NULL);
    CHECK(registry.Find<SpellEffectActivateObjectSite>(12345) == NULL);
    GameObject* target = NULL;
    Unit* caster = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 12345);
    SpellEffectActivateObjectContext ctx(target, caster, spellInfo);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectActivateObjectSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(target == NULL);
    CHECK(caster == NULL);
    CHECK(spellInfo->ID == 12345);

    // Registering again on the same table changes nothing: every key and the resurrect default are taken.
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37));
    CHECK_EQ(registry.Count(), std::size_t(36));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellEffectActivateObjectSite>(24734) == windStone);
    CHECK(registry.Find<SpellEffectActivateObjectSite>(46592) == ahune);

    // Keyed on the activate-object site only: its labels are no other site's, and the other sites' are not its; the
    // game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ACTIVATE_OBJECT_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectActivateObjectSite::Key, spellId));
        CHECK(game.Find<SpellEffectActivateObjectSite>(spellId) == registry.Find<SpellEffectActivateObjectSite>(spellId));
    }
    for (uint32 spellId : { 8342u, 22999u, 54732u, 29886u, 9512u, 41099u, 54833u })
    {
        CHECK(game.Find<SpellEffectActivateObjectSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectActivateObjectSite::Key), std::size_t(33));
    CHECK(game.FindDefault<SpellEffectActivateObjectSite>() == NULL);
}

TEST(SpellEffectObjectCombatHandlers_TheActivateObjectContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[4][16];
    GameObject* target = reinterpret_cast<GameObject*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 24734);
    SpellEffectActivateObjectContext ctx(target, caster, spellInfo);
    CHECK(ctx.gameObjTarget == target);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.m_spellInfo == spellInfo);

    ctx.gameObjTarget = reinterpret_cast<GameObject*>(objects[2]); // the target is the spell's member, not a copy
    CHECK(target == reinterpret_cast<GameObject*>(objects[2]));
    target = reinterpret_cast<GameObject*>(objects[0]);
    CHECK(ctx.gameObjTarget == reinterpret_cast<GameObject*>(objects[0]));
    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // so is the caster
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_spellInfo = ObjectCombatSpell(1, 40176);              // and the spell's entry
    CHECK(spellInfo->ID == 40176);
    spellInfo = ObjectCombatSpell(0, 24734);
    CHECK(ctx.m_spellInfo->ID == 24734);
}

TEST(SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37));
    CHECK_EQ(registry.CountAt(SpellEffectResurrectSite::Key), std::size_t(3));

    // Three labels, one body, and a default that is not it.
    typedef SpellHandler<SpellEffectResurrectSite>::Function ResurrectBody;
    ResurrectBody defibrillate = registry.Find<SpellEffectResurrectSite>(8342);
    ResurrectBody onMiss = registry.FindDefault<SpellEffectResurrectSite>();
    CHECK(defibrillate != NULL);
    CHECK(onMiss != NULL);
    CHECK(defibrillate != onMiss);
    CHECK(registry.Find<SpellEffectResurrectSite>(22999) == defibrillate);
    CHECK(registry.Find<SpellEffectResurrectSite>(54732) == defibrillate);
    CHECK(registry.Find<SpellEffectResurrectSite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and changes nothing.
    Unit* caster = NULL;
    Item* castItem = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 12345);
    SpellEffectResurrectContext ctx(caster, castItem, spellInfo);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellEffectResurrectSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK(caster == NULL);
    CHECK(castItem == NULL);

    // 54732 has no failure spell, so its body needs no live caster: a failed roll (33 in 100) answers Return, ending
    // the resurrection, and a successful one Continue; never a miss, and neither casts. 400 rolls see both.
    spellInfo = ObjectCombatSpell(0, 54732);
    uint32 returned = 0;
    uint32 continued = 0;
    for (uint32 roll = 0; roll < 400; ++roll)
    {
        SpellHandlerOutcome<void> rolled = registry.Dispatch<SpellEffectResurrectSite>(54732, ctx);
        CHECK(rolled.IsReturn() || rolled.IsContinue());
        if (rolled.IsReturn())
        {
            ++returned;
        }
        else if (rolled.IsContinue())
        {
            ++continued;
        }
    }
    CHECK_EQ(returned + continued, uint32(400));
    CHECK(returned > 0);
    CHECK(continued > 0);
    CHECK(caster == NULL);
    CHECK(castItem == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37));
    CHECK_EQ(registry.CountAt(SpellEffectResurrectSite::Key), std::size_t(3));
    CHECK(registry.Find<SpellEffectResurrectSite>(8342) == defibrillate);
    CHECK(registry.FindDefault<SpellEffectResurrectSite>() == onMiss);

    // Keyed on the resurrect site only; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 8342u, 22999u, 54732u })
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectResurrectSite::Key, spellId));
        CHECK(game.Find<SpellEffectResurrectSite>(spellId) == defibrillate);
    }
    for (uint32 spellId : { 24734u, 40176u, 46592u, 29886u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellEffectResurrectSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectResurrectSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellEffectResurrectSite>() == onMiss);
}

TEST(SpellEffectObjectCombatHandlers_TheResurrectContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[4][16];
    Unit* caster = reinterpret_cast<Unit*>(objects[0]);
    Item* castItem = reinterpret_cast<Item*>(objects[1]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 8342);
    SpellEffectResurrectContext ctx(caster, castItem, spellInfo);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.m_CastItem == castItem);
    CHECK(ctx.m_spellInfo == spellInfo);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[2]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[2]));
    caster = reinterpret_cast<Unit*>(objects[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[0]));
    ctx.m_CastItem = reinterpret_cast<Item*>(objects[3]);       // so is the item it is cast with
    CHECK(castItem == reinterpret_cast<Item*>(objects[3]));
    castItem = NULL;
    CHECK(ctx.m_CastItem == NULL);
    ctx.m_spellInfo = ObjectCombatSpell(1, 22999);              // and the spell's entry
    CHECK(spellInfo->ID == 22999);
    spellInfo = ObjectCombatSpell(0, 8342);
    CHECK(ctx.m_spellInfo->ID == 8342);
}

namespace
{
    // The nine labels that keep the caster's victim out of the targets, then Bloodboil, in the switch's order.
    uint32 const ALL_ENEMY_IN_AREA_IDS[] = { 30769, 30843, 31347, 37676, 38028, 40618, 41376, 62166, 63981, 42005 };

    // Places the creature in map 0's world frame at (x, 0, 0).
    void PlaceTargetAt(Creature& creature, float x)
    {
        creature.Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(x, 0.0f, 0.0f), 0.0f);
    }

    /// A Creature with its update fields allocated (a cast's unit target reads its guid), never in a world.
    class ValuedCreature : public Creature
    {
        public:
            ValuedCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~ValuedCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

TEST(SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13)); // 10 + 1 rows and the two defaults
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));
    CHECK_EQ(registry.CountAt(SpellTargetAllEnemyInAreaSite::Key), std::size_t(10));

    // Ten labels, two bodies: the nine victim labels share one, Bloodboil has its own; the default is neither.
    typedef SpellHandler<SpellTargetAllEnemyInAreaSite>::Function AreaBody;
    AreaBody skipVictim = registry.Find<SpellTargetAllEnemyInAreaSite>(30769);
    AreaBody bloodboil = registry.Find<SpellTargetAllEnemyInAreaSite>(42005);
    AreaBody onMiss = registry.FindDefault<SpellTargetAllEnemyInAreaSite>();
    std::set<AreaBody> bodies = { skipVictim, bloodboil, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));
    for (std::size_t i = 0; i < 10; ++i)
    {
        AreaBody body = registry.Find<SpellTargetAllEnemyInAreaSite>(ALL_ENEMY_IN_AREA_IDS[i]);
        CHECK(body == (i < 9 ? skipVictim : bloodboil));
    }
    CHECK(registry.Find<SpellTargetAllEnemyInAreaSite>(12345) == NULL);

    // A caster at the origin and two targets in the area, 5 and 10 yards from it; the effect may hit one target.
    Creature caster(CREATURE_SUBTYPE_GENERIC);
    Creature near(CREATURE_SUBTYPE_GENERIC);
    Creature far(CREATURE_SUBTYPE_GENERIC);
    PlaceTargetAt(caster, 0.0f);
    PlaceTargetAt(near, 5.0f);
    PlaceTargetAt(far, 10.0f);
    Unit* casterMember = &caster;
    std::list<Unit*> targets = { &near, &far };
    uint32 maxTargets = 1;
    SpellTargetAllEnemyInAreaContext ctx(casterMember, targets, maxTargets);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and keeps the targets.
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK_EQ(targets.size(), std::size_t(2));
    CHECK(targets.front() == &near);
    CHECK(targets.back() == &far);

    // The victim labels on a caster with no victim: nothing to drop, and the member goes on.
    for (std::size_t i = 0; i < 9; ++i)
    {
        uint32 spellId = ALL_ENEMY_IN_AREA_IDS[i];
        SpellHandlerOutcome<void> skipped = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(spellId, ctx);
        CHECK(skipped.IsContinue());
        CHECK(!skipped.IsReturn());
        CHECK_EQ(targets.size(), std::size_t(2));
        CHECK(targets.front() == &near);
    }

    // Bloodboil reads the most targets the effect may hit as the member holds it when the body runs: as many targets
    // as that are kept as they are; more are sorted furthest from the caster first and cut to that many.
    maxTargets = 2;
    SpellHandlerOutcome<void> kept = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(42005, ctx);
    CHECK(kept.IsContinue());
    CHECK(!kept.IsReturn());
    CHECK_EQ(targets.size(), std::size_t(2));
    CHECK(targets.front() == &near);
    CHECK(targets.back() == &far);
    maxTargets = 1;
    SpellHandlerOutcome<void> cut = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(42005, ctx);
    CHECK(cut.IsContinue());
    CHECK(!cut.IsReturn());
    REQUIRE(targets.size() == 1);
    CHECK(targets.front() == &far);
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing: every key and both defaults are taken.
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13));
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));
    CHECK(registry.Find<SpellTargetAllEnemyInAreaSite>(30769) == skipVictim);
    CHECK(registry.FindDefault<SpellTargetAllEnemyInAreaSite>() == onMiss);

    // Keyed on the area site: its labels are no other site's but Doom (31347), which the generic trigger-spell aura
    // site holds too, and Pick Red Riding Hood (30769), which the generic script-effect site holds too, and no third
    // site does; the other sites' are not its; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ALL_ENEMY_IN_AREA_IDS)
    {
        uint32 other = SharedWith(spellId, SpellTargetAllEnemyInAreaSite::Key);
        CHECK(!HeldByAnotherSite(game, SpellTargetAllEnemyInAreaSite::Key, spellId, other));
        CHECK((other != SpellTargetAllEnemyInAreaSite::Key) == (spellId == 31347 || spellId == 30769));
        AreaBody body = registry.Find<SpellTargetAllEnemyInAreaSite>(spellId);
        CHECK(game.Find<SpellTargetAllEnemyInAreaSite>(spellId) == body);
    }
    for (uint32 spellId : { 20577u, 24734u, 8342u, 9512u, 29886u, 41099u, 54833u })
    {
        CHECK(game.Find<SpellTargetAllEnemyInAreaSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellTargetAllEnemyInAreaSite::Key), std::size_t(10));
    CHECK(game.FindDefault<SpellTargetAllEnemyInAreaSite>() == onMiss);
    CHECK(SharedWith(31347, SpellTargetAllEnemyInAreaSite::Key) == AuraTriggerSpellGenericSite::Key);
    CHECK(SharedWith(30769, SpellTargetAllEnemyInAreaSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(game.Find<SpellEffectScriptGenericSite>(30769) != NULL);
}

TEST(SpellTargetingHandlers_TheAllEnemyInAreaContextAliasesTheSpellAndTheLocals)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    std::list<Unit*> targets;
    uint32 maxTargets = 5;
    SpellTargetAllEnemyInAreaContext ctx(caster, targets, maxTargets);
    CHECK(ctx.m_caster == caster);
    CHECK(&ctx.targetUnitMap == &targets);
    CHECK_EQ(ctx.unMaxTargets, uint32(5));

    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.targetUnitMap.push_back(caster);                        // the target list is the member's parameter
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.targetUnitMap.empty());
    maxTargets = 3;                                             // and the most targets is the member's local, read
    CHECK_EQ(ctx.unMaxTargets, uint32(3));
}

TEST(SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13));
    CHECK_EQ(registry.CountAt(SpellTargetEffectDummySite::Key), std::size_t(1));

    // One label, and a default that is not its body.
    typedef SpellHandler<SpellTargetEffectDummySite>::Function DummyBody;
    DummyBody cannibalize = registry.Find<SpellTargetEffectDummySite>(20577);
    DummyBody onMiss = registry.FindDefault<SpellTargetEffectDummySite>();
    CHECK(cannibalize != NULL);
    CHECK(onMiss != NULL);
    CHECK(cannibalize != onMiss);
    CHECK(registry.Find<SpellTargetEffectDummySite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default:` does and targets the unit target the
    // cast holds, if any; it reads nothing else of the spell.
    Creature caster(CREATURE_SUBTYPE_GENERIC);
    ValuedCreature target;
    PlaceTargetAt(caster, 0.0f);
    PlaceTargetAt(target, 5.0f);
    Spell* spell = NULL;
    Unit* casterMember = &caster;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 12345);
    SpellCastTargets castTargets;
    std::list<Unit*> targets;
    SpellTargetEffectDummyContext ctx(spell, casterMember, spellInfo, castTargets, targets);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellTargetEffectDummySite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK(targets.empty());
    castTargets.setUnitTarget(&target);
    SpellHandlerOutcome<void> targeted = registry.Dispatch<SpellTargetEffectDummySite>(12345, ctx);
    CHECK(targeted.IsContinue());
    CHECK(!targeted.IsReturn());
    REQUIRE(targets.size() == 1);
    CHECK(targets.front() == &target);
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13));
    CHECK_EQ(registry.CountAt(SpellTargetEffectDummySite::Key), std::size_t(1));
    CHECK(registry.Find<SpellTargetEffectDummySite>(20577) == cannibalize);
    CHECK(registry.FindDefault<SpellTargetEffectDummySite>() == onMiss);

    // Keyed on the dummy-effect site, and Cannibalize is a shared id: the generic dummy-effect site holds it too and
    // no third site does; the game's table holds the same row and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(SharedWith(20577, SpellTargetEffectDummySite::Key) == SpellEffectDummyGenericSite::Key);
    CHECK(game.Find<SpellEffectDummyGenericSite>(20577) != NULL);
    CHECK(!HeldByAnotherSite(game, SpellTargetEffectDummySite::Key, 20577, SpellEffectDummyGenericSite::Key));
    CHECK(game.Find<SpellTargetEffectDummySite>(20577) == cannibalize);
    for (uint32 spellId : { 30769u, 42005u, 24734u, 8342u, 9512u, 29886u, 41099u })
    {
        CHECK(game.Find<SpellTargetEffectDummySite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellTargetEffectDummySite::Key), std::size_t(1));
    CHECK(game.FindDefault<SpellTargetEffectDummySite>() == onMiss);
}

TEST(SpellTargetingHandlers_TheEffectDummyContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[3][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 20577);
    SpellCastTargets castTargets;
    std::list<Unit*> targets;
    SpellTargetEffectDummyContext ctx(spell, caster, spellInfo, castTargets, targets);
    CHECK(ctx.spell == spell);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.m_spellInfo == spellInfo);
    CHECK(&ctx.m_targets == &castTargets);
    CHECK(&ctx.targetUnitMap == &targets);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[2]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[2]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_spellInfo = ObjectCombatSpell(1, 12345);              // so is the spell's entry
    CHECK(spellInfo->ID == 12345);
    spellInfo = ObjectCombatSpell(0, 20577);
    CHECK(ctx.m_spellInfo->ID == 20577);
    ctx.m_targets.m_targetMask = TARGET_FLAG_DEST_LOCATION;     // and its targets, which the member reads after
    CHECK_EQ(castTargets.m_targetMask, uint32(TARGET_FLAG_DEST_LOCATION));
    castTargets.m_targetMask = 0;
    CHECK_EQ(ctx.m_targets.m_targetMask, uint32(0));
    ctx.targetUnitMap.push_back(caster);                        // the target list is the member's parameter
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.targetUnitMap.empty());
}

TEST(SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellChecksHandlers(registry), uint32(3)); // 2 rows and the default
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellCheckCastAuraDummySite::Key), std::size_t(2));

    // Two labels, two bodies; the default is neither.
    typedef SpellHandler<SpellCheckCastAuraDummySite>::Function CastBody;
    CastBody killCommand = registry.Find<SpellCheckCastAuraDummySite>(34026);
    CastBody survivalInstincts = registry.Find<SpellCheckCastAuraDummySite>(61336);
    CastBody onMiss = registry.FindDefault<SpellCheckCastAuraDummySite>();
    std::set<CastBody> bodies = { killCommand, survivalInstincts, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));
    CHECK(registry.Find<SpellCheckCastAuraDummySite>(12345) == NULL);

    // A creature caster with no pet, in no world.
    ValuedCreature caster;
    Unit* casterMember = &caster;
    SpellCheckCastAuraDummyContext ctx(casterMember);

    // A miss runs the default, which answers Continue as the switch's `default: break;` does.
    SpellHandlerOutcome<SpellCastResult> missed = registry.Dispatch<SpellCheckCastAuraDummySite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());

    // Kill Command with no pet fails the cast with its result.
    SpellHandlerOutcome<SpellCastResult> noPet = registry.Dispatch<SpellCheckCastAuraDummySite>(34026, ctx);
    REQUIRE(noPet.IsReturn());
    CHECK(noPet.GetValue() == SPELL_FAILED_NO_PET);

    // Survival Instincts on a caster that is not a player fails the cast with its result.
    SpellHandlerOutcome<SpellCastResult> notFeral = registry.Dispatch<SpellCheckCastAuraDummySite>(61336, ctx);
    REQUIRE(notFeral.IsReturn());
    CHECK(notFeral.GetValue() == SPELL_FAILED_ONLY_SHAPESHIFT);
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing: both keys and the default are taken.
    CHECK_EQ(RegisterSpellChecksHandlers(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellCheckCastAuraDummySite>(34026) == killCommand);
    CHECK(registry.FindDefault<SpellCheckCastAuraDummySite>() == onMiss);

    // Survival Instincts is keyed on the cast-check site only; Kill Command also on the hunter aura dummy apply site,
    // as the shared list's entry says. The game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(!HeldByAnotherSite(game, SpellCheckCastAuraDummySite::Key, 61336));
    CHECK(HeldByAnotherSite(game, SpellCheckCastAuraDummySite::Key, 34026));
    CHECK(!HeldByAnotherSite(game, SpellCheckCastAuraDummySite::Key, 34026, AuraDummyApplyHunterSite::Key));
    for (uint32 spellId : { 34026u, 61336u })
    {
        CHECK(game.Find<SpellCheckCastAuraDummySite>(spellId) == registry.Find<SpellCheckCastAuraDummySite>(spellId));
    }
    for (uint32 spellId : { 37433u, 68921u, 69049u, 20577u, 30769u, 24734u, 8342u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellCheckCastAuraDummySite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellCheckCastAuraDummySite::Key), std::size_t(2));
    CHECK(game.FindDefault<SpellCheckCastAuraDummySite>() == onMiss);
}

TEST(SpellChecksHandlers_TheCastAuraDummyContextAliasesTheCaster)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    SpellCheckCastAuraDummyContext ctx(caster);
    CHECK(ctx.m_caster == caster);

    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
}

TEST(SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellCheckTargetHandlers(registry), uint32(4)); // 3 rows and the default
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellCheckTargetSite::Key), std::size_t(3));

    // Three labels, two bodies: the two Soulstorm labels share one, Spout has its own; the default is neither.
    typedef SpellHandler<SpellCheckTargetSite>::Function TargetBody;
    TargetBody spout = registry.Find<SpellCheckTargetSite>(37433);
    TargetBody soulstorm = registry.Find<SpellCheckTargetSite>(68921);
    TargetBody onMiss = registry.FindDefault<SpellCheckTargetSite>();
    std::set<TargetBody> bodies = { spout, soulstorm, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));
    CHECK(registry.Find<SpellCheckTargetSite>(69049) == soulstorm);
    CHECK(registry.Find<SpellCheckTargetSite>(12345) == NULL);

    // A caster at the origin and two creature targets, 5 and 15 yards from it.
    Creature caster(CREATURE_SUBTYPE_GENERIC);
    Creature near(CREATURE_SUBTYPE_GENERIC);
    Creature far(CREATURE_SUBTYPE_GENERIC);
    PlaceTargetAt(caster, 0.0f);
    PlaceTargetAt(near, 5.0f);
    PlaceTargetAt(far, 15.0f);
    Unit* casterMember = &caster;
    Unit* target = &near;
    SpellCheckTargetContext ctx(casterMember, target);

    // A miss runs the default, which answers Continue as the switch's `default: break;` does: the target stands.
    SpellHandlerOutcome<bool> missed = registry.Dispatch<SpellCheckTargetSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());

    // Spout refuses a target that is not a player.
    SpellHandlerOutcome<bool> notPlayer = registry.Dispatch<SpellCheckTargetSite>(37433, ctx);
    REQUIRE(notPlayer.IsReturn());
    CHECK(!notPlayer.GetValue());

    // Soulstorm refuses a target within 10 yards of the caster and lets one further away stand; the body reads the
    // target the member holds when it runs.
    for (uint32 spellId : { 68921u, 69049u })
    {
        target = &near;
        SpellHandlerOutcome<bool> tooNear = registry.Dispatch<SpellCheckTargetSite>(spellId, ctx);
        REQUIRE(tooNear.IsReturn());
        CHECK(!tooNear.GetValue());
        target = &far;
        SpellHandlerOutcome<bool> farEnough = registry.Dispatch<SpellCheckTargetSite>(spellId, ctx);
        CHECK(farEnough.IsContinue());
        CHECK(!farEnough.IsReturn());
    }
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing: every key and the default are taken.
    CHECK_EQ(RegisterSpellCheckTargetHandlers(registry), uint32(4));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellCheckTargetSite>(37433) == spout);
    CHECK(registry.FindDefault<SpellCheckTargetSite>() == onMiss);

    // Keyed on the target-check site only; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 37433u, 68921u, 69049u })
    {
        CHECK(!HeldByAnotherSite(game, SpellCheckTargetSite::Key, spellId));
        CHECK(game.Find<SpellCheckTargetSite>(spellId) == registry.Find<SpellCheckTargetSite>(spellId));
    }
    for (uint32 spellId : { 34026u, 61336u, 20577u, 30769u, 24734u, 8342u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellCheckTargetSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellCheckTargetSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellCheckTargetSite>() == onMiss);
}

TEST(SpellCheckTargetHandlers_TheTargetContextAliasesTheCasterAndTheTarget)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    SpellCheckTargetContext ctx(caster, target);
    CHECK(ctx.m_caster == caster);
    CHECK(&ctx.target == &target);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    target = reinterpret_cast<Unit*>(units[3]);                 // and the target is the member's parameter, read
    CHECK(ctx.target == reinterpret_cast<Unit*>(units[3]));
}

namespace
{
    // SpellEffect.dbc rows in raw storage, as the loader makes them; zeroed here, with the effect index set.
    struct WeaponDamageEffectRow
    {
        alignas(SpellEffectEntry) unsigned char bytes[sizeof(SpellEffectEntry)];
    };

    WeaponDamageEffectRow s_weaponDamageEffects[2] = {};

    SpellEffectEntry const* WeaponDamageEffect(std::size_t slot, uint32 effectIndex)
    {
        SpellEffectEntry* row = reinterpret_cast<SpellEffectEntry*>(s_weaponDamageEffects[slot].bytes);
        row->EffectIndex = effectIndex;
        return row;
    }

    uint32 const WEAPON_DAMAGE_IDS[] = { 66765, 66809, 67331, 67333, 69055, 71021 };

    // Three targets whose effect masks are 1, 1 and 2: two carry effect 0, one carries effect 1.
    Spell::TargetList WeaponDamageTargets()
    {
        Spell::TargetList targets;
        for (uint8 mask : { uint8(1), uint8(1), uint8(2) })
        {
            Spell::TargetList::value_type entry = Spell::TargetList::value_type();
            entry.effectMask = mask;
            targets.push_back(entry);
        }
        return targets;
    }
}

TEST(SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectSkillEnchantPetHandlers(registry), uint32(6)); // 6 rows, no default
    CHECK_EQ(registry.Count(), std::size_t(6));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectWeaponDmgSite::Key), std::size_t(6));

    // Six labels, one body.
    typedef SpellHandler<SpellEffectWeaponDmgSite>::Function WeaponDamageBody;
    WeaponDamageBody divide = registry.Find<SpellEffectWeaponDmgSite>(66765);
    CHECK(divide != NULL);
    for (uint32 spellId : WEAPON_DAMAGE_IDS)
    {
        CHECK(registry.Find<SpellEffectWeaponDmgSite>(spellId) == divide);
    }

    Spell::TargetList targets = WeaponDamageTargets();
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    float totalDamagePercentMod = 1.0f;
    SpellEffectWeaponDmgContext ctx(targets, effect, totalDamagePercentMod);

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the family's case goes on to its
    // own `break;` with nothing changed.
    CHECK(registry.FindDefault<SpellEffectWeaponDmgSite>() == NULL);
    CHECK(registry.Find<SpellEffectWeaponDmgSite>(12345) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectWeaponDmgSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(totalDamagePercentMod == 1.0f);
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK_EQ(effect->EffectIndex, uint32(0));

    // Each label divides the multiplier by the number of targets the effect hits and continues: effect 0 hits two
    // of the three targets, effect 1 one. The body reads the effect the member holds when it runs.
    for (uint32 spellId : WEAPON_DAMAGE_IDS)
    {
        effect = WeaponDamageEffect(0, 0);
        totalDamagePercentMod = 1.0f;
        SpellHandlerOutcome<void> first = registry.Dispatch<SpellEffectWeaponDmgSite>(spellId, ctx);
        CHECK(first.IsContinue());
        CHECK(!first.IsReturn());
        CHECK(totalDamagePercentMod == 0.5f);

        effect = WeaponDamageEffect(1, 1);
        totalDamagePercentMod = 1.0f;
        SpellHandlerOutcome<void> second = registry.Dispatch<SpellEffectWeaponDmgSite>(spellId, ctx);
        CHECK(second.IsContinue());
        CHECK(!second.IsReturn());
        CHECK(totalDamagePercentMod == 1.0f);
    }
    CHECK_EQ(targets.size(), std::size_t(3));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectSkillEnchantPetHandlers(registry), uint32(6));
    CHECK_EQ(registry.Count(), std::size_t(6));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<SpellEffectWeaponDmgSite>(71021) == divide);

    // Keyed on the weapon-damage site only: its labels are no other site's, and the other sites' are not its; the
    // game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : WEAPON_DAMAGE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectWeaponDmgSite::Key, spellId));
        CHECK(game.Find<SpellEffectWeaponDmgSite>(spellId) == registry.Find<SpellEffectWeaponDmgSite>(spellId));
    }
    for (uint32 spellId : { 34026u, 61336u, 37433u, 68921u, 69049u, 20577u, 30769u, 24734u, 8342u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellEffectWeaponDmgSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectWeaponDmgSite::Key), std::size_t(6));
    CHECK(game.FindDefault<SpellEffectWeaponDmgSite>() == NULL);
}

TEST(SpellEffectSkillEnchantPetHandlers_TheWeaponDamageContextAliasesTheSpell)
{
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    float totalDamagePercentMod = 1.0f;
    SpellEffectWeaponDmgContext ctx(targets, effect, totalDamagePercentMod);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.effect == &effect);
    CHECK(&ctx.totalDamagePercentMod == &totalDamagePercentMod);

    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // the target list is the spell's member
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    ctx.totalDamagePercentMod = 0.25f;                                  // the multiplier is the member's local
    CHECK(totalDamagePercentMod == 0.25f);
    totalDamagePercentMod = 1.0f;
    CHECK(ctx.totalDamagePercentMod == 1.0f);
    effect = WeaponDamageEffect(1, 1);                                  // and the effect is its parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

namespace
{
    // The sixty ids whose damage is divided among the targets, in the switch's order.
    uint32 const SCHOOL_DAMAGE_DIVIDE_IDS[] =
    {
        24340, 26558, 28884, 36837, 38903, 41276, 57467, 26789, 31436, 35181, 40810, 43267, 43268, 42384, 45150,
        64422, 64688, 70492, 72505, 71904, 72624, 72625, 77679, 92968, 92969, 92970, 82935, 88915, 88916, 88917,
        86014, 92863, 92864, 92865, 86367, 93135, 93136, 93137, 86825, 92879, 92880, 92881, 88942, 95172, 89348,
        95178, 98474, 100212, 100213, 100214, 103414, 108571, 109033, 109034, 105069, 108094, 106375, 109182, 109183,
        109184
    };

    // The fourteen other school-damage labels, in the switch's order.
    uint32 const SCHOOL_DAMAGE_OTHER_IDS[] =
    {
        25599, 20253, 61491, 29142, 35139, 49882, 55269, 37841, 38441, 50341, 62775, 67485, 68793, 69050
    };

    // The trigger-spell, recall and post-teleport labels, in the switches' order.
    uint32 const TRIGGER_SPELL_IDS[] = { 18461, 29284, 29286, 31980, 35729, 41967, 58832 };
    uint32 const TELEPORT_RECALL_IDS[] = { 48129, 60320, 60321 };
    uint32 const TELEPORT_POST_IDS[] = { 23442, 36941, 36890 };

    // The file registers 74 + 7 + 3 + 3 rows and no default.
    uint32 const DAMAGE_TELEPORT_ROWS = 87;
}

TEST(SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DAMAGE_TELEPORT_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectSchoolDmgSite::Key), std::size_t(74));

    // 74 labels, eleven bodies: the sixty divided-damage ids share one, Intercept's two another, the four
    // percent-of-health ids a third; every other label has its own.
    typedef SpellHandler<SpellEffectSchoolDmgSite>::Function SchoolDamageBody;
    SchoolDamageBody divide = registry.Find<SpellEffectSchoolDmgSite>(24340);
    for (uint32 spellId : SCHOOL_DAMAGE_DIVIDE_IDS)
    {
        CHECK(registry.Find<SpellEffectSchoolDmgSite>(spellId) == divide);
    }
    SchoolDamageBody intercept = registry.Find<SpellEffectSchoolDmgSite>(20253);
    CHECK(registry.Find<SpellEffectSchoolDmgSite>(61491) == intercept);
    SchoolDamageBody percent = registry.Find<SpellEffectSchoolDmgSite>(29142);
    for (uint32 spellId : { 35139u, 49882u, 55269u })
    {
        CHECK(registry.Find<SpellEffectSchoolDmgSite>(spellId) == percent);
    }
    std::set<SchoolDamageBody> bodies = { divide, intercept, percent };
    for (uint32 spellId : { 25599u, 37841u, 38441u, 50341u, 62775u, 67485u, 68793u, 69050u })
    {
        bodies.insert(registry.Find<SpellEffectSchoolDmgSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(11));

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the family's case goes on to its
    // own `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    Spell::TargetList targets = WeaponDamageTargets();
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectSchoolDmgContext ctx(caster, target, damage, targets, effect);
    CHECK(registry.FindDefault<SpellEffectSchoolDmgSite>() == NULL);
    CHECK(registry.Find<SpellEffectSchoolDmgSite>(40739) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectSchoolDmgSite>(40739, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK_EQ(damage, int32(90));
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK_EQ(effect->EffectIndex, uint32(0));

    // Each divided-damage label divides the damage by the number of targets the effect hits and continues: effect 0
    // hits two of the three targets, effect 1 one. The body reads the effect the member holds when it runs.
    for (uint32 spellId : SCHOOL_DAMAGE_DIVIDE_IDS)
    {
        effect = WeaponDamageEffect(0, 0);
        damage = 90;
        SpellHandlerOutcome<void> first = registry.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx);
        CHECK(first.IsContinue());
        CHECK(!first.IsReturn());
        CHECK_EQ(damage, int32(45));

        effect = WeaponDamageEffect(1, 1);
        damage = 90;
        SpellHandlerOutcome<void> second = registry.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx);
        CHECK(second.IsContinue());
        CHECK(!second.IsReturn());
        CHECK_EQ(damage, int32(90));
    }
    CHECK_EQ(targets.size(), std::size_t(3));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DAMAGE_TELEPORT_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<SpellEffectSchoolDmgSite>(109184) == divide);

    // Keyed on the school-damage site only: its labels are no other site's, and the other sites' are not its; the
    // game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : SCHOOL_DAMAGE_DIVIDE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectSchoolDmgSite::Key, spellId));
        CHECK(game.Find<SpellEffectSchoolDmgSite>(spellId) == divide);
    }
    for (uint32 spellId : SCHOOL_DAMAGE_OTHER_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectSchoolDmgSite::Key, spellId));
        CHECK(game.Find<SpellEffectSchoolDmgSite>(spellId) == registry.Find<SpellEffectSchoolDmgSite>(spellId));
    }
    for (uint32 spellId : { 18461u, 58832u, 48129u, 23442u, 66765u, 34026u, 37433u, 8342u, 41099u })
    {
        CHECK(game.Find<SpellEffectSchoolDmgSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectSchoolDmgSite::Key), std::size_t(74));
    CHECK(game.FindDefault<SpellEffectSchoolDmgSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits)
{
    // Creatures with their update fields allocated: the caster's attack power 1000, the target's health and maximum
    // health 1000, no mana; neither is a player.
    ValuedCreature casterUnit;
    ValuedCreature targetUnit;
    casterUnit.SetInt32Value(UNIT_FIELD_ATTACK_POWER, 1000);
    targetUnit.SetUInt32Value(UNIT_FIELD_HEALTH, 1000);
    targetUnit.SetUInt32Value(UNIT_FIELD_MAXHEALTH, 1000);
    Unit* caster = &casterUnit;
    Unit* target = &targetUnit;
    int32 damage = 0;
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectSchoolDmgContext ctx(caster, target, damage, targets, effect);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Thundercrash: half the target's health, at least 200.
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(25599, ctx).IsContinue());
    CHECK_EQ(damage, int32(500));
    targetUnit.SetUInt32Value(UNIT_FIELD_HEALTH, 300);
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(25599, ctx).IsContinue());
    CHECK_EQ(damage, int32(200));
    targetUnit.SetUInt32Value(UNIT_FIELD_HEALTH, 1000);

    // Intercept: the damage gains 12% of the caster's attack power.
    for (uint32 spellId : { 20253u, 61491u })
    {
        damage = 100;
        CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx).IsContinue());
        CHECK_EQ(damage, int32(220));
    }

    // The four percent-of-health labels: the damage is that percent of the target's maximum health.
    for (uint32 spellId : { 29142u, 35139u, 49882u, 55269u })
    {
        damage = 25;
        CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx).IsContinue());
        CHECK_EQ(damage, int32(250));
    }

    // Lightning Strike: a target that is not a player is given no credit, and the damage stands.
    damage = 77;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(37841, ctx).IsContinue());
    CHECK_EQ(damage, int32(77));

    // Cataclysmic Bolt and Tympanic Tantrum: a half and a tenth of the target's maximum health.
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(38441, ctx).IsContinue());
    CHECK_EQ(damage, int32(500));
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(62775, ctx).IsContinue());
    CHECK_EQ(damage, int32(100));

    // Touch the Nightmare: the third effect's damage is 30% of the target's maximum health; the others' stand.
    effect = WeaponDamageEffect(0, 2);
    damage = 9;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(50341, ctx).IsContinue());
    CHECK_EQ(damage, int32(300));
    effect = WeaponDamageEffect(0, 0);
    damage = 9;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(50341, ctx).IsContinue());
    CHECK_EQ(damage, int32(9));

    // Hand of Reckoning: the damage gains half the caster's attack power.
    damage = 100;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(67485, ctx).IsContinue());
    CHECK_EQ(damage, int32(600));

    // Magic's Bane: the target has no mana to add, so the damage stands up to the cap, 10000 and 15000 heroic.
    damage = 4000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(68793, ctx).IsContinue());
    CHECK_EQ(damage, int32(4000));
    damage = 20000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(68793, ctx).IsContinue());
    CHECK_EQ(damage, int32(10000));
    damage = 12000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(69050, ctx).IsContinue());
    CHECK_EQ(damage, int32(12000));
    damage = 20000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(69050, ctx).IsContinue());
    CHECK_EQ(damage, int32(15000));

    // No body changes the units the context holds.
    CHECK(caster == &casterUnit);
    CHECK(target == &targetUnit);
}

TEST(SpellEffectDamageTeleportHandlers_TheSchoolDamageContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    int32 damage = 10;
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectSchoolDmgContext ctx(caster, target, damage, targets, effect);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.damage == &damage);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    ctx.damage = 25;                                            // and the damage
    CHECK_EQ(damage, int32(25));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // and the target list
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

TEST(SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectTriggerSpellSite::Key), std::size_t(7));

    // Seven labels, seven bodies.
    typedef SpellHandler<SpellEffectTriggerSpellSite>::Function TriggerBody;
    std::set<TriggerBody> bodies;
    for (uint32 spellId : TRIGGER_SPELL_IDS)
    {
        bodies.insert(registry.Find<SpellEffectTriggerSpellSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(7));

    // No default: a trigger id with no row (Web's 11920) finds nothing, and its dispatch is a miss, so the triggered
    // spell is looked up and cast as usual, with nothing changed.
    ValuedCreature targetUnit;
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    Unit* target = NULL;
    Unit* caster = NULL;
    Item* castItem = NULL;
    ObjectGuid originalCaster;
    SpellEffectTriggerSpellContext ctx(target, caster, castItem, originalCaster);
    CHECK(registry.FindDefault<SpellEffectTriggerSpellSite>() == NULL);
    CHECK(registry.Find<SpellEffectTriggerSpellSite>(11920) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectTriggerSpellSite>(11920, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(target == NULL);
    CHECK(caster == NULL);
    CHECK(castItem == NULL);
    CHECK(originalCaster.IsEmpty());

    // The bodies that need no live unit: Vanish ends the effect for a target that is not a player; Cloak of Shadows
    // with no aura on the target and Shadowfiend with no pet end it too; Mirror Image without the glyph goes on to
    // the triggered spell. The other three cast a spell and need a live unit.
    target = &targetUnit;
    caster = &casterUnit;
    SpellHandlerOutcome<void> vanish = registry.Dispatch<SpellEffectTriggerSpellSite>(18461, ctx);
    CHECK(vanish.IsReturn());
    CHECK(!vanish.IsContinue());
    SpellHandlerOutcome<void> cloak = registry.Dispatch<SpellEffectTriggerSpellSite>(35729, ctx);
    CHECK(cloak.IsReturn());
    SpellHandlerOutcome<void> shadowfiend = registry.Dispatch<SpellEffectTriggerSpellSite>(41967, ctx);
    CHECK(shadowfiend.IsReturn());
    SpellHandlerOutcome<void> mirrorImage = registry.Dispatch<SpellEffectTriggerSpellSite>(58832, ctx);
    CHECK(mirrorImage.IsContinue());
    CHECK(!mirrorImage.IsReturn());
    CHECK(target == &targetUnit);
    CHECK(caster == &casterUnit);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DAMAGE_TELEPORT_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectTriggerSpellSite::Key), std::size_t(7));

    // Keyed on the trigger-spell site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TRIGGER_SPELL_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectTriggerSpellSite::Key, spellId));
        CHECK(game.Find<SpellEffectTriggerSpellSite>(spellId) == registry.Find<SpellEffectTriggerSpellSite>(spellId));
    }
    for (uint32 spellId : { 745u, 24340u, 25599u, 48129u, 23442u, 66765u, 34026u, 8342u, 41099u })
    {
        CHECK(game.Find<SpellEffectTriggerSpellSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectTriggerSpellSite::Key), std::size_t(7));
    CHECK(game.FindDefault<SpellEffectTriggerSpellSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheTriggerSpellContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[4][16];
    Unit* target = reinterpret_cast<Unit*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    Item* castItem = reinterpret_cast<Item*>(objects[2]);
    ObjectGuid originalCaster(HIGHGUID_PLAYER, uint32(7));
    SpellEffectTriggerSpellContext ctx(target, caster, castItem, originalCaster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.m_CastItem == &castItem);
    CHECK(&ctx.m_originalCasterGUID == &originalCaster);

    ctx.unitTarget = reinterpret_cast<Unit*>(objects[3]);       // the unit target is the spell's member, not a copy
    CHECK(target == reinterpret_cast<Unit*>(objects[3]));
    target = reinterpret_cast<Unit*>(objects[0]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[0]));
    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // so is the caster
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_CastItem = NULL;                                      // and the item it is cast with
    CHECK(castItem == NULL);
    castItem = reinterpret_cast<Item*>(objects[2]);
    CHECK(ctx.m_CastItem == reinterpret_cast<Item*>(objects[2]));
    ctx.m_originalCasterGUID = ObjectGuid(HIGHGUID_PLAYER, uint32(9)); // and the original caster
    CHECK(originalCaster == ObjectGuid(HIGHGUID_PLAYER, uint32(9)));
    originalCaster = ObjectGuid();
    CHECK(ctx.m_originalCasterGUID.IsEmpty());
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectTeleportRecallSite::Key), std::size_t(3));

    // Three labels, one body.
    typedef SpellHandler<SpellEffectTeleportRecallSite>::Function RecallBody;
    RecallBody recall = registry.Find<SpellEffectTeleportRecallSite>(48129);
    CHECK(recall != NULL);
    for (uint32 spellId : TELEPORT_RECALL_IDS)
    {
        CHECK(registry.Find<SpellEffectTeleportRecallSite>(spellId) == recall);
    }

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the teleport goes on with nothing
    // changed.
    Unit* target = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 23442);
    SpellEffectTeleportRecallContext recallCtx(target, spellInfo);
    CHECK(registry.FindDefault<SpellEffectTeleportRecallSite>() == NULL);
    CHECK(registry.Find<SpellEffectTeleportRecallSite>(23442) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectTeleportRecallSite>(23442, recallCtx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(target == NULL);
    CHECK(spellInfo->ID == 23442);

    // A target that is not a player is never lost, whatever its level: each scroll goes on to the teleport.
    ValuedCreature targetUnit;
    target = &targetUnit;
    for (uint32 level : { 1u, 81u })
    {
        targetUnit.SetUInt32Value(UNIT_FIELD_LEVEL, level);
        for (uint32 spellId : TELEPORT_RECALL_IDS)
        {
            spellInfo = ObjectCombatSpell(0, spellId);
            SpellHandlerOutcome<void> scroll = registry.Dispatch<SpellEffectTeleportRecallSite>(spellId, recallCtx);
            CHECK(scroll.IsContinue());
            CHECK(!scroll.IsReturn());
        }
    }
    CHECK(target == &targetUnit);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountAt(SpellEffectTeleportRecallSite::Key), std::size_t(3));

    // Keyed on the recall site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TELEPORT_RECALL_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectTeleportRecallSite::Key, spellId));
        CHECK(game.Find<SpellEffectTeleportRecallSite>(spellId) == recall);
    }
    for (uint32 spellId : { 23442u, 36941u, 36890u, 24340u, 18461u, 66765u, 8342u })
    {
        CHECK(game.Find<SpellEffectTeleportRecallSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectTeleportRecallSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellEffectTeleportRecallSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportRecallContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 48129);
    SpellEffectTeleportRecallContext recallCtx(target, spellInfo);
    CHECK(&recallCtx.unitTarget == &target);
    CHECK(&recallCtx.m_spellInfo == &spellInfo);

    recallCtx.unitTarget = reinterpret_cast<Unit*>(units[1]);   // the unit target is the spell's member, not a copy
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    target = reinterpret_cast<Unit*>(units[0]);
    CHECK(recallCtx.unitTarget == reinterpret_cast<Unit*>(units[0]));
    recallCtx.m_spellInfo = ObjectCombatSpell(1, 60321);        // and so is the spell's entry
    CHECK(spellInfo->ID == 60321);
    spellInfo = ObjectCombatSpell(0, 60320);
    CHECK(recallCtx.m_spellInfo->ID == 60320);
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectTeleportPostSite::Key), std::size_t(3));

    // Three labels, three bodies; each rolls and casts on the caster, so none runs without a live unit.
    typedef SpellHandler<SpellEffectTeleportPostSite>::Function PostBody;
    std::set<PostBody> bodies;
    for (uint32 spellId : TELEPORT_POST_IDS)
    {
        bodies.insert(registry.Find<SpellEffectTeleportPostSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the member reaches its end with
    // nothing changed.
    Unit* caster = NULL;
    SpellEffectTeleportPostContext ctx(caster);
    CHECK(registry.FindDefault<SpellEffectTeleportPostSite>() == NULL);
    CHECK(registry.Find<SpellEffectTeleportPostSite>(48129) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectTeleportPostSite>(48129, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountAt(SpellEffectTeleportPostSite::Key), std::size_t(3));

    // Keyed on the post-teleport site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TELEPORT_POST_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectTeleportPostSite::Key, spellId));
        CHECK(game.Find<SpellEffectTeleportPostSite>(spellId) == registry.Find<SpellEffectTeleportPostSite>(spellId));
    }
    for (uint32 spellId : { 48129u, 60320u, 60321u, 24340u, 18461u, 66765u, 8342u })
    {
        CHECK(game.Find<SpellEffectTeleportPostSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectTeleportPostSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellEffectTeleportPostSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportPostContextAliasesTheCaster)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    SpellEffectTeleportPostContext ctx(caster);
    CHECK(&ctx.m_caster == &caster);

    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
}

namespace
{
    // The generic switch's moved labels and the five family switches' labels, in the switches' order.
    uint32 const DUMMY_GENERIC_IDS[] =
    {
        3360, 7671, 8063, 8213, 9976, 10254, 13120, 13489, 13567, 14537, 15998, 29435, 16589, 17009, 17251, 17271,
        17770, 17950, 19395, 19411, 20474, 19869, 20037, 20577, 21147, 23019, 23074, 23075, 23076, 23133, 23138,
        23448, 23453, 23645, 23725, 24930, 25860, 26074, 28006, 29126, 29135, 29136, 29137, 29138, 29139, 29200,
        29858, 29969, 29979, 30004, 30458, 30507, 32027, 32146, 32300, 32312, 37388, 33060, 34803, 36677, 33923,
        38796, 35745, 37674, 39189, 39635, 39849, 40802, 40834, 40869, 40962, 41283, 41333, 42287, 42485, 42489,
        42628, 42631, 42793, 43014, 43036, 43069, 43096, 43144, 43209, 43498, 43572, 43723, 43882, 44454, 44869,
        44875, 44997, 45030, 45449, 45583, 45685, 45958, 45976, 46177, 45980, 45989, 45990, 46167, 50918, 50926,
        51026, 51592, 51961, 55364, 61832, 63125, 74904, 46171, 46292, 46372, 46485, 46606, 46671, 46672, 46797,
        47110, 47170, 47176, 47305, 47381, 47386, 47389, 47408, 50441, 48046, 48790, 49357, 49634, 49859, 50133,
        50243, 50440, 50546, 50547, 50548, 51276, 51330, 51333, 51336, 51420, 51582, 51840, 51964, 51858, 51866, 51872, 52308,
        52369, 52371, 52759, 52845, 53341, 53343, 53475, 53487, 54015, 53808, 54577, 54850, 54092, 55004, 55818,
        56430, 57578, 57908, 58418, 58420, 58601, 59640, 60932, 62105, 62278, 62652, 62797, 62907, 63030, 63499,
        63545, 63744, 63745, 63747, 63984, 64172, 64385, 64402, 64489, 64543, 64555, 64673, 64981, 65206, 65346,
        65869, 66312, 66314, 66390, 67019, 69922, 71307, 71445, 71471, 71718, 72040, 71837, 71861, 72202, 72254,
        72261, 72285, 74452
    };
    uint32 const DUMMY_MAGE_IDS[] = { 11958, 31687, 32826, 38194, 42955 };
    uint32 const DUMMY_WARRIOR_IDS[] = { 21977, 12975, 23881, 30012, 30284, 37144, 37146, 37148, 37151, 37152, 37153 };
    uint32 const DUMMY_ROGUE_IDS[] = { 5938, 14185, 31231, 51662 };
    uint32 const DUMMY_HUNTER_IDS[] = { 23989, 37506, 53478, 53271 };
    uint32 const DUMMY_PALADIN_IDS[] = { 19740, 20217, 31789, 37877 };

    // The six chess moves share one body.
    uint32 const DUMMY_CHESS_MOVE_IDS[] = { 37144, 37146, 37148, 37151, 37152, 37153 };

    // The file registers 216 + 5 + 11 + 4 + 4 + 4 rows and no default.
    uint32 const DUMMY_ROWS = 244;
}

TEST(SpellEffectDummyHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyGenericSite::Key), std::size_t(216));

    // Two hundred and thirteen labels, a hundred and seventy-eight functions: the five Cleansing Flames cities share
    // one, as do Move 1 and Move 2, the two Sonic Booms, the two Throw Glaives, the two Open Portals, the ten cover
    // spells, the two Exodar and Silvermoon Cleansing Flames, the five Reagent Checks, the three Ley Line Focus
    // Controls, Sara's Anger, Blessing and Fervor, the two Twilight Bloodbolts, Conjure Flame with Conjure Empowered
    // Flame, Capture Worg Pup with Capture Female Kaliri Hatchling, the two Lava Bombs, the two Detonate Explosives,
    // Rune of Cinderglacier with Rune of Razorice, the three Oracle and Wolvar faction spells, and the two Portals.
    typedef SpellHandler<SpellEffectDummyGenericSite>::Function GenericBody;
    GenericBody cleansingFlames = registry.Find<SpellEffectDummyGenericSite>(29135);
    for (uint32 spellId : { 29136u, 29137u, 29138u, 29139u })
    {
        CHECK(registry.Find<SpellEffectDummyGenericSite>(spellId) == cleansingFlames);
    }
    CHECK(registry.Find<SpellEffectDummyGenericSite>(37388) == registry.Find<SpellEffectDummyGenericSite>(32312));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(38796) == registry.Find<SpellEffectDummyGenericSite>(33923));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(39849) == registry.Find<SpellEffectDummyGenericSite>(39635));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(29126) != cleansingFlames);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(42628) != registry.Find<SpellEffectDummyGenericSite>(42631));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(46177) == registry.Find<SpellEffectDummyGenericSite>(45976));
    GenericBody cover = registry.Find<SpellEffectDummyGenericSite>(46167);
    for (uint32 spellId : { 50918u, 50926u, 51026u, 51592u, 51961u, 55364u, 61832u, 63125u, 74904u })
    {
        CHECK(registry.Find<SpellEffectDummyGenericSite>(spellId) == cover);
    }
    CHECK(registry.Find<SpellEffectDummyGenericSite>(46672) == registry.Find<SpellEffectDummyGenericSite>(46671));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(46671) != registry.Find<SpellEffectDummyGenericSite>(29126));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(46671) != cleansingFlames);
    GenericBody reagentCheck = registry.Find<SpellEffectDummyGenericSite>(47381);
    for (uint32 spellId : { 47386u, 47389u, 47408u, 50441u })
    {
        CHECK(registry.Find<SpellEffectDummyGenericSite>(spellId) == reagentCheck);
    }
    CHECK(registry.Find<SpellEffectDummyGenericSite>(50547) == registry.Find<SpellEffectDummyGenericSite>(50546));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(50548) == registry.Find<SpellEffectDummyGenericSite>(50546));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(43882) != registry.Find<SpellEffectDummyGenericSite>(50133));
    GenericBody sara = registry.Find<SpellEffectDummyGenericSite>(63744);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(63745) == sara);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(63747) == sara);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(71471) == registry.Find<SpellEffectDummyGenericSite>(71445));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(72040) == registry.Find<SpellEffectDummyGenericSite>(71718));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(65869) != registry.Find<SpellEffectDummyGenericSite>(60932));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(64673) != registry.Find<SpellEffectDummyGenericSite>(64489));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(29435) == registry.Find<SpellEffectDummyGenericSite>(15998));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(20474) == registry.Find<SpellEffectDummyGenericSite>(19411));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(52371) == registry.Find<SpellEffectDummyGenericSite>(52369));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(53343) == registry.Find<SpellEffectDummyGenericSite>(53341));
    GenericBody oracle = registry.Find<SpellEffectDummyGenericSite>(53475);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(53487) == oracle);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(54015) == oracle);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(58420) == registry.Find<SpellEffectDummyGenericSite>(58418));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(8213) != registry.Find<SpellEffectDummyGenericSite>(8063));

    // A hundred and seventy-five bodies up to the three byte-identical sets: Icicle, Vile Gas and Vile Gas Trigger
    // count as one body, as do the Sara function, Destabilization Matrix, Conjure Flame and Lava Strike, and Holiday
    // Cheer with the Portals (one code but for a comment), whether the linker gives a set one address or one each (a
    // set's functions fold together or not at all); every other function is its own body.
    GenericBody icicle = registry.Find<SpellEffectDummyGenericSite>(63545);
    GenericBody vileGas = registry.Find<SpellEffectDummyGenericSite>(71307);
    GenericBody vileGasTrigger = registry.Find<SpellEffectDummyGenericSite>(72285);
    GenericBody destabilizationMatrix = registry.Find<SpellEffectDummyGenericSite>(65206);
    GenericBody conjureFlame = registry.Find<SpellEffectDummyGenericSite>(71718);
    GenericBody lavaStrike = registry.Find<SpellEffectDummyGenericSite>(57578);
    GenericBody holidayCheer = registry.Find<SpellEffectDummyGenericSite>(26074);
    GenericBody portal = registry.Find<SpellEffectDummyGenericSite>(58418);
    CHECK((vileGas == icicle) == (vileGasTrigger == icicle));
    CHECK((destabilizationMatrix == sara) == (conjureFlame == sara));
    CHECK((lavaStrike == sara) == (conjureFlame == sara));
    std::set<GenericBody> bodies;
    for (uint32 spellId : DUMMY_GENERIC_IDS)
    {
        GenericBody body = registry.Find<SpellEffectDummyGenericSite>(spellId);
        if (body == vileGas || body == vileGasTrigger)
        {
            body = icicle;
        }
        else if (body == destabilizationMatrix || body == conjureFlame || body == lavaStrike)
        {
            body = sara;
        }
        else if (body == portal)
        {
            body = holidayCheer;
        }
        bodies.insert(body);
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(175));

    // No default: an id the site never held finds nothing, and its dispatch is a miss, so the code after the family
    // switch runs with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 10254);
    int32 basePoints[MAX_EFFECT_INDEX] = { 7, 8, 9 };
    Item* castItem = NULL;
    Unit* caster = NULL;
    ObjectGuid originalCasterGuid(HIGHGUID_PLAYER, uint32(7));
    Unit* originalCaster = NULL;
    int32 duration = 3000;
    Unit* target = NULL;
    Item* itemTarget = NULL;
    int32 damage = 90;
    Spell::TargetList targets = WeaponDamageTargets();
    SpellEntry const* triggeredBy = ObjectCombatSpell(1, 26467);
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyGenericContext ctx(spell, spellInfo, basePoints, castItem, caster, originalCasterGuid,
                                       originalCaster, duration, target, itemTarget, damage, targets, triggeredBy,
                                       effect);
    CHECK(registry.FindDefault<SpellEffectDummyGenericSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyGenericSite>(12345) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyGenericSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(ctx.spell == spell);
    CHECK(spellInfo->ID == 10254);
    CHECK_EQ(basePoints[0], int32(7));
    CHECK(castItem == NULL);
    CHECK(caster == NULL);
    CHECK(originalCasterGuid == ObjectGuid(HIGHGUID_PLAYER, uint32(7)));
    CHECK(originalCaster == NULL);
    CHECK_EQ(duration, int32(3000));
    CHECK(target == NULL);
    CHECK(itemTarget == NULL);
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK(triggeredBy->ID == 26467);
    CHECK_EQ(effect->EffectIndex, uint32(0));

    // The bodies that need no live unit end the effect with nothing changed: Holiday Cheer and the two Portals, which
    // do nothing; the item bodies with no cast item; the target bodies with no unit target (Cast Ooze Zap When
    // Energized on the first effect, the Open Portals before they answer continue, the Ley Line Focus Controls and
    // Siphon of Acherus with no original caster either, Dummy Trigger with its triggering aura); the player-caster
    // bodies for a creature caster, Test Fetid Skull with no item target either. The others cast, summon, despawn,
    // create an item, remove the caster's auras, turn the caster, read the caster's area or auras, read the spell's
    // effects or finish the spell, and need a live unit and a spell.
    for (uint32 spellId : { 26074u, 58418u, 58420u, 23074u, 23075u, 23076u, 23133u, 30458u, 30507u, 55004u })
    {
        SpellHandlerOutcome<void> noItem = registry.Dispatch<SpellEffectDummyGenericSite>(spellId, ctx);
        CHECK(noItem.IsReturn());
        CHECK(!noItem.IsContinue());
    }
    for (uint32 spellId : { 3360u, 7671u, 9976u, 13120u, 13489u, 13567u, 14537u, 15998u, 29435u, 17009u, 17251u, 17950u,
                            19395u, 19411u, 20474u, 19869u, 20037u, 20577u, 21147u, 23019u, 23138u, 28006u, 29126u,
                            29135u, 29136u, 29137u, 29138u, 29139u, 29858u, 29969u, 29979u, 30004u, 32146u, 32300u,
                            32312u, 37388u, 36677u, 33923u, 38796u, 37674u, 39189u, 39635u, 39849u, 40834u, 40869u,
                            40962u, 41283u, 41333u, 42485u, 42489u, 42628u, 42631u, 42793u, 43036u, 43096u, 43144u,
                            43209u, 43498u, 43572u, 43882u, 44454u, 44869u, 44875u, 44997u, 45583u, 45976u, 46177u,
                            45989u, 45990u, 46167u, 50918u, 50926u, 51026u, 51592u, 51961u, 55364u, 61832u, 63125u,
                            74904u, 46171u, 46292u, 46372u, 46485u, 46606u, 46671u, 46672u, 46797u, 47170u, 47176u,
                            47305u, 48046u, 48790u, 49634u, 49859u, 50133u, 50440u, 50546u, 50547u, 50548u, 51276u,
                            51330u, 51333u, 51336u, 51420u, 51840u, 51964u, 51858u, 52369u, 52371u, 52759u, 54577u,
                            54092u, 56430u, 57578u, 60932u, 62105u, 62278u, 62652u, 62797u, 62907u, 63030u, 63499u,
                            63545u, 63744u, 63745u, 63747u, 63984u, 64172u, 64402u, 64489u, 64543u, 64555u, 64673u,
                            65206u, 65346u, 65869u, 66312u, 66314u, 66390u, 69922u, 71307u, 71445u, 71471u, 71718u,
                            72040u, 71837u, 71861u, 72202u, 72254u, 72261u, 72285u, 74452u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyGenericSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    caster = &casterUnit;
    for (uint32 spellId : { 8063u, 8213u, 16589u, 17271u, 29200u, 33060u, 42287u, 43723u, 45980u, 47381u, 47386u,
                            47389u, 47408u, 50441u, 49357u, 50243u, 51582u, 52845u, 53475u, 53487u, 54015u, 59640u,
                            67019u })
    {
        SpellHandlerOutcome<void> notPlayer = registry.Dispatch<SpellEffectDummyGenericSite>(spellId, ctx);
        CHECK(notPlayer.IsReturn());
        CHECK(!notPlayer.IsContinue());
    }
    CHECK(caster == &casterUnit);
    CHECK(castItem == NULL);
    CHECK(target == NULL);
    CHECK_EQ(duration, int32(3000));
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK(targets.back().effectMask == 2);

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyGenericSite::Key), std::size_t(216));
    CHECK(registry.Find<SpellEffectDummyGenericSite>(29139) == cleansingFlames);

    // Keyed on the generic site: its labels are no other site's but the shared ids, which the targeting site or the
    // generic script-effect site holds too, and the other sites' and the standing labels are not its; the game's
    // table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_GENERIC_IDS)
    {
        uint32 other = SharedWith(spellId, SpellEffectDummyGenericSite::Key);
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyGenericSite::Key, spellId, other));
        if (other == SpellTargetEffectDummySite::Key)
        {
            CHECK(game.Find<SpellTargetEffectDummySite>(spellId) != NULL);
        }
        else if (other != SpellEffectDummyGenericSite::Key)
        {
            CHECK(other == SpellEffectScriptGenericSite::Key);
            CHECK(game.Find<SpellEffectScriptGenericSite>(spellId) != NULL);
        }
        CHECK(game.Find<SpellEffectDummyGenericSite>(spellId) ==
              registry.Find<SpellEffectDummyGenericSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 21977u, 5938u, 23989u, 19740u })
    {
        CHECK(game.Find<SpellEffectDummyGenericSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyGenericSite::Key), std::size_t(216));
    CHECK(game.FindDefault<SpellEffectDummyGenericSite>() == NULL);
    CHECK(SharedWith(20577, SpellEffectDummyGenericSite::Key) == SpellTargetEffectDummySite::Key);
    CHECK(SharedWith(29126, SpellEffectDummyGenericSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(SharedWith(46672, SpellEffectDummyGenericSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(SharedWith(45958, SpellEffectDummyGenericSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(SharedWith(58418, SpellEffectDummyGenericSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(SharedWith(58420, SpellEffectDummyGenericSite::Key) == SpellEffectScriptGenericSite::Key);
}

TEST(SpellEffectDummyHandlers_TheGenericContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[11][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 23074);
    int32 basePoints[MAX_EFFECT_INDEX] = { 1, 2, 3 };
    Item* castItem = reinterpret_cast<Item*>(objects[1]);
    Unit* caster = reinterpret_cast<Unit*>(objects[2]);
    ObjectGuid originalCasterGuid(HIGHGUID_PLAYER, uint32(7));
    Unit* originalCaster = reinterpret_cast<Unit*>(objects[3]);
    int32 duration = 1000;
    Unit* target = reinterpret_cast<Unit*>(objects[4]);
    Item* itemTarget = reinterpret_cast<Item*>(objects[5]);
    int32 damage = 10;
    Spell::TargetList targets;
    SpellEntry const* triggeredBy = ObjectCombatSpell(1, 26467);
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyGenericContext ctx(spell, spellInfo, basePoints, castItem, caster, originalCasterGuid,
                                       originalCaster, duration, target, itemTarget, damage, targets, triggeredBy,
                                       effect);
    CHECK(ctx.spell == spell);                                  // the spell itself, whose methods the bodies call
    CHECK(&ctx.m_spellInfo == &spellInfo);
    CHECK(&ctx.m_currentBasePoints == &basePoints);
    CHECK(&ctx.m_CastItem == &castItem);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.m_originalCasterGUID == &originalCasterGuid);
    CHECK(&ctx.m_originalCaster == &originalCaster);
    CHECK(&ctx.m_duration == &duration);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.itemTarget == &itemTarget);
    CHECK(&ctx.damage == &damage);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.m_triggeredByAuraSpell == &triggeredBy);
    CHECK(&ctx.effect == &effect);

    SpellEntry const* otherEntry = ObjectCombatSpell(1, 26467);
    ctx.m_spellInfo = otherEntry;                               // the spell's entry is the spell's member, not a copy
    CHECK(spellInfo == otherEntry);
    spellInfo = ObjectCombatSpell(0, 23074);
    CHECK(ctx.m_spellInfo == ObjectCombatSpell(0, 23074));
    ctx.m_currentBasePoints[1] = 29135;                         // and its base points, the array itself
    CHECK_EQ(basePoints[1], int32(29135));
    basePoints[2] = 33923;
    CHECK_EQ(ctx.m_currentBasePoints[2], int32(33923));
    ctx.m_CastItem = reinterpret_cast<Item*>(objects[6]);       // and its cast item
    CHECK(castItem == reinterpret_cast<Item*>(objects[6]));
    castItem = reinterpret_cast<Item*>(objects[1]);
    CHECK(ctx.m_CastItem == reinterpret_cast<Item*>(objects[1]));
    ctx.m_caster = reinterpret_cast<Unit*>(objects[7]);         // and the caster
    CHECK(caster == reinterpret_cast<Unit*>(objects[7]));
    caster = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[2]));
    ctx.m_originalCasterGUID = ObjectGuid(HIGHGUID_PLAYER, uint32(9)); // and the original caster's guid
    CHECK(originalCasterGuid == ObjectGuid(HIGHGUID_PLAYER, uint32(9)));
    originalCasterGuid = ObjectGuid();
    CHECK(ctx.m_originalCasterGUID == ObjectGuid());
    ctx.m_originalCaster = reinterpret_cast<Unit*>(objects[8]); // and the original caster
    CHECK(originalCaster == reinterpret_cast<Unit*>(objects[8]));
    originalCaster = reinterpret_cast<Unit*>(objects[3]);
    CHECK(ctx.m_originalCaster == reinterpret_cast<Unit*>(objects[3]));
    ctx.m_duration = 2000;                                      // and the duration
    CHECK_EQ(duration, int32(2000));
    duration = 4000;
    CHECK_EQ(ctx.m_duration, int32(4000));
    ctx.unitTarget = reinterpret_cast<Unit*>(objects[9]);       // and the unit target
    CHECK(target == reinterpret_cast<Unit*>(objects[9]));
    target = reinterpret_cast<Unit*>(objects[4]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[4]));
    ctx.itemTarget = reinterpret_cast<Item*>(objects[10]);      // and the item target
    CHECK(itemTarget == reinterpret_cast<Item*>(objects[10]));
    itemTarget = reinterpret_cast<Item*>(objects[5]);
    CHECK(ctx.itemTarget == reinterpret_cast<Item*>(objects[5]));
    ctx.damage = 20;                                            // and the damage
    CHECK_EQ(damage, int32(20));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // and its targets
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    ctx.m_triggeredByAuraSpell = ObjectCombatSpell(0, 23074);   // and the aura that triggered it
    CHECK(triggeredBy == ObjectCombatSpell(0, 23074));
    triggeredBy = ObjectCombatSpell(1, 26467);
    CHECK(ctx.m_triggeredByAuraSpell == ObjectCombatSpell(1, 26467));
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

TEST(SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));

    // Five labels, five bodies.
    typedef SpellHandler<SpellEffectDummyMageSite>::Function MageBody;
    std::set<MageBody> bodies;
    for (uint32 spellId : DUMMY_MAGE_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyMageSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(5));

    // No default: an id with no row (the warrior's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case goes on to the Conjure Mana Gem check with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 1);
    SpellEffectDummyMageContext ctx(spell, caster, target, damage, effect);
    CHECK(registry.FindDefault<SpellEffectDummyMageSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(12975) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyMageSite>(12975, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(ctx.spell == spell);
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(effect->EffectIndex, uint32(1));

    // The bodies that need no live unit end the effect: Polymorph Cast Visual and Blink with no unit target, Cold
    // Snap for a caster that is not a player. Summon Water Elemental casts, and Conjure Refreshment reads the
    // target's level and creates an item through the spell; both need a live unit and a spell.
    for (uint32 spellId : { 32826u, 38194u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyMageSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    caster = &casterUnit;
    SpellHandlerOutcome<void> coldSnap = registry.Dispatch<SpellEffectDummyMageSite>(11958, ctx);
    CHECK(coldSnap.IsReturn());
    CHECK(!coldSnap.IsContinue());
    CHECK(caster == &casterUnit);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));

    // Keyed on the mage site only: its labels are no other site's, and the other sites' are not its; the game's
    // table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_MAGE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyMageSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyMageSite>(spellId) == registry.Find<SpellEffectDummyMageSite>(spellId));
    }
    for (uint32 spellId : { 21977u, 5938u, 23989u, 19740u, 24340u, 18461u, 66765u, 8342u })
    {
        CHECK(game.Find<SpellEffectDummyMageSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));
    CHECK(game.FindDefault<SpellEffectDummyMageSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheMageContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[5][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    Unit* target = reinterpret_cast<Unit*>(objects[2]);
    int32 damage = 10;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyMageContext ctx(spell, caster, target, damage, effect);
    CHECK(ctx.spell == spell);                                  // the spell itself, whose item creation reads damage
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.damage == &damage);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.unitTarget = reinterpret_cast<Unit*>(objects[4]);       // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(objects[4]));
    target = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[2]));
    ctx.damage = 20;                                            // and the damage, the stack size the spell reads
    CHECK_EQ(damage, int32(20));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

TEST(SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyWarriorSite::Key), std::size_t(11));

    // Eleven labels, six bodies: the six chess moves share one; every other label has its own.
    typedef SpellHandler<SpellEffectDummyWarriorSite>::Function WarriorBody;
    WarriorBody chessMove = registry.Find<SpellEffectDummyWarriorSite>(37144);
    for (uint32 spellId : DUMMY_CHESS_MOVE_IDS)
    {
        CHECK(registry.Find<SpellEffectDummyWarriorSite>(spellId) == chessMove);
    }
    std::set<WarriorBody> bodies;
    for (uint32 spellId : DUMMY_WARRIOR_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyWarriorSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(6));
    CHECK(registry.Find<SpellEffectDummyWarriorSite>(30012) != registry.Find<SpellEffectDummyWarriorSite>(30284));

    // No default: an id with no row (the hunter pet's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case reaches its `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    SpellEffectDummyWarriorContext ctx(caster, target, damage);
    CHECK(registry.FindDefault<SpellEffectDummyWarriorSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyWarriorSite>(53478) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyWarriorSite>(53478, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // With no unit target, Warrior's Wrath, Move (before it reaches Change Facing's body), Change Facing and the
    // chess moves end the effect. Last Stand and Bloodthirst cast and need a live unit.
    for (uint32 spellId : { 21977u, 30012u, 30284u, 37144u, 37146u, 37148u, 37151u, 37152u, 37153u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyWarriorSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyWarriorSite::Key), std::size_t(11));
    CHECK(registry.Find<SpellEffectDummyWarriorSite>(37153) == chessMove);

    // Keyed on the warrior site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_WARRIOR_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyWarriorSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyWarriorSite>(spellId) == registry.Find<SpellEffectDummyWarriorSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 5938u, 53478u, 19740u, 20253u, 24340u, 18461u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyWarriorSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyWarriorSite::Key), std::size_t(11));
    CHECK(game.FindDefault<SpellEffectDummyWarriorSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheWarriorContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    int32 damage = 10;
    SpellEffectDummyWarriorContext ctx(caster, target, damage);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.damage == &damage);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    ctx.damage = 25;                                            // and the damage Bloodthirst passes on
    CHECK_EQ(damage, int32(25));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
}

TEST(SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyRogueSite::Key), std::size_t(4));

    // Four labels, four bodies.
    typedef SpellHandler<SpellEffectDummyRogueSite>::Function RogueBody;
    std::set<RogueBody> bodies;
    for (uint32 spellId : DUMMY_ROGUE_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyRogueSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(4));

    // No default: an id with no row (Readiness) finds nothing, and its dispatch is a miss, so the family's case
    // reaches its `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    SpellEffectDummyRogueContext ctx(caster, target);
    CHECK(registry.FindDefault<SpellEffectDummyRogueSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyRogueSite>(23989) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyRogueSite>(23989, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);

    // For a caster that is not a player, Shiv and Preparation end the effect. Cheat Death and Hunger for Blood cast
    // and need a live unit.
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    caster = &casterUnit;
    for (uint32 spellId : { 5938u, 14185u })
    {
        SpellHandlerOutcome<void> notPlayer = registry.Dispatch<SpellEffectDummyRogueSite>(spellId, ctx);
        CHECK(notPlayer.IsReturn());
        CHECK(!notPlayer.IsContinue());
    }
    CHECK(caster == &casterUnit);
    CHECK(target == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyRogueSite::Key), std::size_t(4));

    // Keyed on the rogue site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_ROGUE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyRogueSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyRogueSite>(spellId) == registry.Find<SpellEffectDummyRogueSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 21977u, 23989u, 19740u, 18461u, 35729u, 24340u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyRogueSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyRogueSite::Key), std::size_t(4));
    CHECK(game.FindDefault<SpellEffectDummyRogueSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheRogueContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    SpellEffectDummyRogueContext ctx(caster, target);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
}

TEST(SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyHunterSite::Key), std::size_t(4));

    // Four labels, four bodies.
    typedef SpellHandler<SpellEffectDummyHunterSite>::Function HunterBody;
    std::set<HunterBody> bodies;
    for (uint32 spellId : DUMMY_HUNTER_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyHunterSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(4));

    // No default: an id with no row (the warrior's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case reaches its `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 1);
    SpellEffectDummyHunterContext ctx(caster, target, effect);
    CHECK(registry.FindDefault<SpellEffectDummyHunterSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyHunterSite>(12975) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyHunterSite>(12975, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(effect->EffectIndex, uint32(1));

    // Last Stand with no unit target ends the effect; so do Readiness and Scatter Shot for a caster that is not a
    // player, and Master's Call for a caster with no pet.
    SpellHandlerOutcome<void> lastStand = registry.Dispatch<SpellEffectDummyHunterSite>(53478, ctx);
    CHECK(lastStand.IsReturn());
    CHECK(!lastStand.IsContinue());
    ValuedCreature casterUnit;
    caster = &casterUnit;
    for (uint32 spellId : { 23989u, 37506u, 53271u })
    {
        SpellHandlerOutcome<void> outcome = registry.Dispatch<SpellEffectDummyHunterSite>(spellId, ctx);
        CHECK(outcome.IsReturn());
        CHECK(!outcome.IsContinue());
    }
    CHECK(caster == &casterUnit);
    CHECK(target == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyHunterSite::Key), std::size_t(4));

    // Keyed on the hunter site only, but Master's Call, which the shared list names with the hunter script-effect
    // site; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_HUNTER_IDS)
    {
        uint32 other = SharedWith(spellId, SpellEffectDummyHunterSite::Key);
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyHunterSite::Key, spellId, other));
        CHECK(game.Find<SpellEffectDummyHunterSite>(spellId) == registry.Find<SpellEffectDummyHunterSite>(spellId));
    }
    CHECK(SharedWith(23989, SpellEffectDummyHunterSite::Key) == SpellEffectDummyHunterSite::Key);
    CHECK(SharedWith(53271, SpellEffectDummyHunterSite::Key) == SpellEffectScriptHunterSite::Key);
    for (uint32 spellId : { 11958u, 12975u, 5938u, 19740u, 57635u, 61507u, 24340u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyHunterSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyHunterSite::Key), std::size_t(4));
    CHECK(game.FindDefault<SpellEffectDummyHunterSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheHunterContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyHunterContext ctx(caster, target, effect);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    effect = WeaponDamageEffect(1, 2);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(2));
}

TEST(SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyPaladinSite::Key), std::size_t(4));

    // Four labels, three bodies: the two blessings share one.
    typedef SpellHandler<SpellEffectDummyPaladinSite>::Function PaladinBody;
    PaladinBody blessing = registry.Find<SpellEffectDummyPaladinSite>(19740);
    CHECK(registry.Find<SpellEffectDummyPaladinSite>(20217) == blessing);
    std::set<PaladinBody> bodies;
    for (uint32 spellId : DUMMY_PALADIN_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyPaladinSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));

    // No default: an id with no row (Holy Shock's 20473, which the icon switch before it handles) finds nothing,
    // and its dispatch is a miss, so the family's case reaches its `break;` with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 19740);
    int32 basePoints[MAX_EFFECT_INDEX] = { 7, 8, 9 };
    Spell::TargetList targets = WeaponDamageTargets();
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyPaladinContext ctx(spell, caster, target, spellInfo, basePoints, targets, effect);
    CHECK(registry.FindDefault<SpellEffectDummyPaladinSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyPaladinSite>(20473) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyPaladinSite>(20473, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK(spellInfo->ID == 19740);
    CHECK_EQ(basePoints[0], int32(7));
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK(targets.back().effectMask == 2);

    // With no unit target, the blessings and Blessing of Faith end the effect with nothing changed. Righteous
    // Defense answers through the spell's cast result and needs a live spell.
    for (uint32 spellId : { 19740u, 20217u, 37877u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyPaladinSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK(targets.back().effectMask == 2);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyPaladinSite::Key), std::size_t(4));
    CHECK(registry.Find<SpellEffectDummyPaladinSite>(20217) == blessing);

    // Keyed on the paladin site only; the game's table holds the same rows and no default. 31789's own trigger,
    // 31980, stays at the trigger-spell site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_PALADIN_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyPaladinSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyPaladinSite>(spellId) == registry.Find<SpellEffectDummyPaladinSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 21977u, 5938u, 23989u, 31980u, 20473u, 24340u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyPaladinSite>(spellId) == NULL);
    }
    CHECK(game.Find<SpellEffectTriggerSpellSite>(31980) != NULL);
    CHECK_EQ(game.CountAt(SpellEffectDummyPaladinSite::Key), std::size_t(4));
    CHECK(game.FindDefault<SpellEffectDummyPaladinSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheKickNassBodiesEndTheEffectWithNoUnitTarget)
{
    // Kick Nass and Hair Sample Collected end the effect before they read anything else when there is no unit target.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 51866);
    int32 basePoints[MAX_EFFECT_INDEX] = { 7, 8, 9 };
    Item* castItem = NULL;
    Unit* caster = NULL;
    ObjectGuid originalCasterGuid(HIGHGUID_PLAYER, uint32(7));
    Unit* originalCaster = NULL;
    int32 duration = 3000;
    Unit* target = NULL;
    Item* itemTarget = NULL;
    int32 damage = 90;
    Spell::TargetList targets = WeaponDamageTargets();
    SpellEntry const* triggeredBy = NULL;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyGenericContext ctx(spell, spellInfo, basePoints, castItem, caster, originalCasterGuid,
                                       originalCaster, duration, target, itemTarget, damage, targets, triggeredBy,
                                       effect);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 51866u, 51872u })
    {
        CHECK(game.Dispatch<SpellEffectDummyGenericSite>(spellId, ctx).IsReturn());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(duration, int32(3000));
    CHECK_EQ(targets.size(), std::size_t(3));
}

TEST(SpellEffectDummyHandlers_ThePaladinContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[5][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    Unit* target = reinterpret_cast<Unit*>(objects[2]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 31789);
    int32 basePoints[MAX_EFFECT_INDEX] = { 1, 2, 3 };
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyPaladinContext ctx(spell, caster, target, spellInfo, basePoints, targets, effect);
    CHECK(ctx.spell == spell);                                  // the spell itself, whose methods the bodies call
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.m_spellInfo == &spellInfo);
    CHECK(&ctx.m_currentBasePoints == &basePoints);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.unitTarget = reinterpret_cast<Unit*>(objects[4]);       // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(objects[4]));
    target = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[2]));
    ctx.m_spellInfo = ObjectCombatSpell(1, 37877);              // and the spell's entry
    CHECK(spellInfo->ID == 37877);
    spellInfo = ObjectCombatSpell(0, 19740);
    CHECK(ctx.m_spellInfo->ID == 19740);
    ctx.m_currentBasePoints[1] = 20217;                         // and its base points, the array itself
    CHECK_EQ(basePoints[1], int32(20217));
    basePoints[2] = 19740;
    CHECK_EQ(ctx.m_currentBasePoints[2], int32(19740));
    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // and its targets
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

namespace
{
    // The 45 labels of the generic trigger-spell site, in the switch's order.
    uint32 const TRIGGER_SPELL_GENERIC_IDS[] = { 812, 9712, 19695, 23170, 23184, 25041, 37125, 23493, 24210, 24780,
                                                 24834, 25371, 26009, 26136, 27808, 27819, 28096, 28111, 28097, 28109,
                                                 29528, 29917, 30427, 30576, 31347, 31373, 31944, 34229, 36573, 37027,
                                                 37429, 37430, 38495, 38554, 39105, 39857, 40398, 40511, 42581, 42582,
                                                 46736, 58886, 62679, 64345, 69012 };

    // The site's locals, NULL and zero, for a dispatch whose body reads none of them.
    struct TriggerSpellLocals
    {
        ObjectGuid casterGuid;
        Unit* triggerTarget;
        uint32 triggerSpellId;
        SpellEntry const* triggeredSpellInfo;
        SpellEntry const* auraSpellInfo;
        uint32 auraId;
        Unit* target;
        Unit* triggerCaster;

        explicit TriggerSpellLocals(uint32 id)
            : triggerTarget(NULL), triggerSpellId(0), triggeredSpellInfo(NULL), auraSpellInfo(NULL), auraId(id),
              target(NULL), triggerCaster(NULL) {}

        AuraTriggerSpellContext Context(Aura* aura)
        {
            return AuraTriggerSpellContext(aura, casterGuid, triggerTarget, triggerSpellId, triggeredSpellInfo,
                                           auraSpellInfo, auraId, target, triggerCaster);
        }
    };
}

TEST(AurasHandlers_TheTableRegistersEveryRowOnce)
{
    // 153 labels at eighteen sites and fifteen defaults: 168 rows, 153 keys and fifteen defaults, none twice. The
    // generic, mage, hunter, druid, shaman and custom trigger-spell switches hold 45, 1, 3, 5, 2 and 11; the damage
    // tick's 8 and the mana leech's 3; the generic, mage, druid, rogue and hunter periodic dummy tick switches 44, 1,
    // 2, 1 and 2; the generic, mage, priest, hunter and death knight boost switches 7, 6, 2, 6 and 4; the custom,
    // mana-leech and death knight boost switches have no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAurasHandlers(registry), uint32(168));
    CHECK_EQ(registry.Count(), std::size_t(153));
    CHECK_EQ(registry.CountDefaults(), std::size_t(15));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellGenericSite::Key), std::size_t(45));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellHunterSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellDruidSite::Key), std::size_t(5));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellShamanSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellCustomSite::Key), std::size_t(11));
    CHECK_EQ(registry.CountAt(AuraPeriodicDamageTickSite::Key), std::size_t(8));
    CHECK_EQ(registry.CountAt(AuraPeriodicManaLeechTickSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickGenericSite::Key), std::size_t(44));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickDruidSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickRogueSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickHunterSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraBoostGenericSite::Key), std::size_t(7));
    CHECK_EQ(registry.CountAt(AuraBoostMageSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraBoostPriestSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraBoostHunterSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraBoostDeathKnightSite::Key), std::size_t(4));

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterAurasHandlers(registry), uint32(168));
    CHECK_EQ(registry.Count(), std::size_t(153));
    CHECK_EQ(registry.CountDefaults(), std::size_t(15));

    // The game's table holds these with the other files' 1070 rows and 14 defaults.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(1223));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
    CHECK_EQ(game.CountAt(AuraTriggerSpellGenericSite::Key), std::size_t(45));
    CHECK_EQ(game.CountAt(AuraTriggerSpellCustomSite::Key), std::size_t(11));
    CHECK_EQ(game.CountAt(AuraPeriodicDamageTickSite::Key), std::size_t(8));
    CHECK_EQ(game.CountAt(AuraPeriodicDummyTickGenericSite::Key), std::size_t(44));
    CHECK_EQ(game.CountAt(AuraBoostGenericSite::Key), std::size_t(7));
}

TEST(AurasHandlers_TheTriggerSpellGenericSiteHoldsItsLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterAurasHandlers(registry);
    typedef SpellHandler<AuraTriggerSpellGenericSite>::Function Body;
    std::set<Body> distinct;
    for (uint32 spellId : TRIGGER_SPELL_GENERIC_IDS)
    {
        Body body = registry.Find<AuraTriggerSpellGenericSite>(spellId);
        CHECK(body != NULL);
        distinct.insert(body);
    }
    CHECK_EQ(distinct.size(), std::size_t(38));                 // 38 bodies: six of them shared by two or three labels

    // The six shared bodies: the three marks, the two rotations, the two chains, the two passives, the two spouts
    // that cast a fixed spell and the two that cast the base points' value.
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(25041) == registry.Find<AuraTriggerSpellGenericSite>(23184));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(37125) == registry.Find<AuraTriggerSpellGenericSite>(23184));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(26136) == registry.Find<AuraTriggerSpellGenericSite>(26009));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(28111) == registry.Find<AuraTriggerSpellGenericSite>(28096));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(28109) == registry.Find<AuraTriggerSpellGenericSite>(28097));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(37430) == registry.Find<AuraTriggerSpellGenericSite>(37429));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(42582) == registry.Find<AuraTriggerSpellGenericSite>(42581));
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(42581) != registry.Find<AuraTriggerSpellGenericSite>(37429));

    // The default is none of the bodies and answers Continue, as `default: break;` did: the re-read of the trigger
    // spell runs for the mage switch's id and for an id the switch never held.
    Body onMiss = registry.FindDefault<AuraTriggerSpellGenericSite>();
    CHECK(onMiss != NULL);
    CHECK(distinct.count(onMiss) == 0);
    for (uint32 spellId : { 66u, 12345u })
    {
        CHECK(registry.Find<AuraTriggerSpellGenericSite>(spellId) == NULL);
        TriggerSpellLocals locals(spellId);
        AuraTriggerSpellContext ctx = locals.Context(NULL);
        SpellHandlerOutcome<void> missed = registry.Dispatch<AuraTriggerSpellGenericSite>(spellId, ctx);
        CHECK(missed.IsContinue());
        CHECK(!missed.IsReturn());
        CHECK(!missed.IsMiss());
        CHECK_EQ(locals.triggerSpellId, uint32(0));
    }

    // Keyed on this site: no label is another site's key but Doom (31347), which the all-enemy-in-area targeting site
    // holds too and no third site does; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TRIGGER_SPELL_GENERIC_IDS)
    {
        uint32 other = SharedWith(spellId, AuraTriggerSpellGenericSite::Key);
        CHECK(!HeldByAnotherSite(game, AuraTriggerSpellGenericSite::Key, spellId, other));
        CHECK((other != AuraTriggerSpellGenericSite::Key) == (spellId == 31347));
        CHECK(game.Find<AuraDummyApplyWarriorSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyQuestTameSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyDruidSite>(spellId) == NULL);
        CHECK(game.Find<AuraTriggerSpellGenericSite>(spellId) ==
              registry.Find<AuraTriggerSpellGenericSite>(spellId));
    }
    CHECK(SharedWith(31347, AuraTriggerSpellGenericSite::Key) == SpellTargetAllEnemyInAreaSite::Key);
    CHECK(game.Find<SpellTargetAllEnemyInAreaSite>(31347) != NULL);
    CHECK_EQ(game.CountAt(AuraTriggerSpellGenericSite::Key), std::size_t(45));
    CHECK(game.FindDefault<AuraTriggerSpellGenericSite>() == onMiss);
}

TEST(AurasHandlers_TheTriggerSpellContextAliasesItsLocals)
{
    alignas(16) static unsigned char objects[4][16];
    Aura* aura = reinterpret_cast<Aura*>(objects[0]);
    ObjectGuid casterGuid(HIGHGUID_PLAYER, uint32(7));
    Unit* triggerTarget = reinterpret_cast<Unit*>(objects[1]);
    uint32 triggerSpellId = 25779;
    SpellEntry const* triggeredSpellInfo = NULL;
    SpellEntry const* auraSpellInfo = ObjectCombatSpell(0, 812);
    uint32 auraId = 812;
    Unit* target = reinterpret_cast<Unit*>(objects[2]);
    Unit* triggerCaster = triggerTarget;
    AuraTriggerSpellContext ctx(aura, casterGuid, triggerTarget, triggerSpellId, triggeredSpellInfo, auraSpellInfo,
                                auraId, target, triggerCaster);
    CHECK(ctx.aura == aura);
    CHECK(&ctx.casterGUID == &casterGuid);
    CHECK(&ctx.triggerTarget == &triggerTarget);
    CHECK(&ctx.trigger_spell_id == &triggerSpellId);
    CHECK(&ctx.triggeredSpellInfo == &triggeredSpellInfo);
    CHECK(&ctx.auraSpellInfo == &auraSpellInfo);
    CHECK(&ctx.auraId == &auraId);
    CHECK(&ctx.target == &target);
    CHECK(&ctx.triggerCaster == &triggerCaster);

    ctx.triggerTarget = reinterpret_cast<Unit*>(objects[3]);    // a body's write to the trigger target...
    CHECK(triggerTarget == reinterpret_cast<Unit*>(objects[3])); // ...is the function's local
    ctx.trigger_spell_id = 29916;                               // so is one to the trigger spell id
    CHECK_EQ(triggerSpellId, uint32(29916));
    ctx.triggerCaster = reinterpret_cast<Unit*>(objects[2]);    // and one to the trigger caster
    CHECK(triggerCaster == reinterpret_cast<Unit*>(objects[2]));
    ctx.target = reinterpret_cast<Unit*>(objects[1]);           // and one to the target
    CHECK(target == reinterpret_cast<Unit*>(objects[1]));
    auraId = 23184;                                             // the site's locals are read where they stand
    CHECK_EQ(ctx.auraId, uint32(23184));
    auraSpellInfo = ObjectCombatSpell(1, 23184);
    CHECK_EQ(ctx.auraSpellInfo->ID, uint32(23184));
    triggeredSpellInfo = auraSpellInfo;
    CHECK(ctx.triggeredSpellInfo == auraSpellInfo);
    CHECK(ctx.casterGUID == casterGuid);
}

TEST(AurasHandlers_FeedCapturedAnimalSetsTheTriggerSpell)
{
    // Feed Captured Animal, Remote Toy, Tear of Azzinoth Summon Channel, Demon Transform 1 and Personalized Weather
    // set the trigger spell id and go on to its cast; they touch neither the aura nor the locals they do not set.
    struct Row
    {
        uint32 auraId;
        uint32 triggerSpellId;
    };
    static Row const rows[] = { { 29917, 29916 }, { 37027, 37029 }, { 39857, 39856 }, { 40511, 40398 },
                                { 46736, 46737 } };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    AuraStorage storage = {};
    Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
    AuraStorage const before = storage;
    for (Row const& row : rows)
    {
        TriggerSpellLocals locals(row.auraId);
        AuraTriggerSpellContext ctx = locals.Context(aura);
        SpellHandlerOutcome<void> outcome = game.Dispatch<AuraTriggerSpellGenericSite>(row.auraId, ctx);
        CHECK(outcome.IsContinue());
        CHECK(!outcome.IsReturn());
        CHECK_EQ(locals.triggerSpellId, row.triggerSpellId);
        CHECK_EQ(BitsThatDiffer(storage, before), 0);
        CHECK(locals.triggerTarget == NULL);
        CHECK(locals.target == NULL);
        CHECK(locals.triggerCaster == NULL);
    }
}

namespace
{
    // The labels of the five trigger-spell branch sites and the two tick sites, each in its switch's order.
    uint32 const TRIGGER_SPELL_MAGE_IDS[] = { 66 };
    uint32 const TRIGGER_SPELL_HUNTER_IDS[] = { 53302, 53303, 53304 };
    uint32 const TRIGGER_SPELL_DRUID_IDS[] = { 768, 22842, 22895, 22896, 26999 };
    uint32 const TRIGGER_SPELL_SHAMAN_IDS[] = { 28820, 38443 };
    uint32 const TRIGGER_SPELL_CUSTOM_IDS[] = { 9347, 1010, 16191, 33525, 38736, 43149, 44883, 56505, 53563, 56654,
                                                58882 };
    uint32 const PERIODIC_DAMAGE_TICK_IDS[] = { 43093, 31956, 38801, 35321, 38363, 39215, 48920, 38772 };
    uint32 const PERIODIC_MANA_LEECH_TICK_IDS[] = { 32960, 21056, 31447 };

    // Every label of `Site` is found on `registry` and on the game's table with the same body, the bodies are
    // `expected` distinct functions, no label is another site's key but the one the shared list names, and an id
    // the switch never held is no key.
    template <class Site, std::size_t N>
    void CheckSiteLabels(SpellHandlerRegistry const& registry, uint32 const (&ids)[N], std::size_t expected)
    {
        typedef typename SpellHandler<Site>::Function Body;
        std::set<Body> distinct;
        SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
        for (uint32 spellId : ids)
        {
            Body body = registry.Find<Site>(spellId);
            CHECK(body != NULL);
            distinct.insert(body);
            uint32 other = SharedWith(spellId, Site::Key);
            CHECK(!HeldByAnotherSite(game, Site::Key, spellId, other));
            CHECK(game.Find<Site>(spellId) == body);
        }
        CHECK_EQ(distinct.size(), expected);
        CHECK_EQ(registry.CountAt(Site::Key), N);
        CHECK_EQ(game.CountAt(Site::Key), N);
        CHECK(game.FindDefault<Site>() == registry.FindDefault<Site>());
        CHECK(registry.Find<Site>(12345) == NULL);
    }

    /// A creature whose type id reads player, so a body that takes a player's trigger target takes it for one; its
    /// movement flags are clear, so it stands still.
    class StandingPlayerShape : public ValuedCreature
    {
        public:
            StandingPlayerShape()
            {
                m_objectTypeId = TYPEID_PLAYER;
            }
    };
}

TEST(AurasHandlers_TheTriggerSpellBranchSitesHoldTheirLabelsAndDefaults)
{
    SpellHandlerRegistry registry;
    RegisterAurasHandlers(registry);

    // The mage, hunter, druid, shaman and custom switches: 1, 3, 5, 2 and 11 labels in 1, 1, 2, 2 and 9 bodies.
    CheckSiteLabels<AuraTriggerSpellMageSite>(registry, TRIGGER_SPELL_MAGE_IDS, 1);
    CheckSiteLabels<AuraTriggerSpellHunterSite>(registry, TRIGGER_SPELL_HUNTER_IDS, 1);
    CheckSiteLabels<AuraTriggerSpellDruidSite>(registry, TRIGGER_SPELL_DRUID_IDS, 2);
    CheckSiteLabels<AuraTriggerSpellShamanSite>(registry, TRIGGER_SPELL_SHAMAN_IDS, 2);
    CheckSiteLabels<AuraTriggerSpellCustomSite>(registry, TRIGGER_SPELL_CUSTOM_IDS, 9);

    // The shared bodies: the three Sniper Training ranks, the four Frenzied Regeneration labels, Encapsulate with
    // Surge of Power, and the two Rapid Recuperation labels; Cat Form has its own.
    CHECK(registry.Find<AuraTriggerSpellHunterSite>(53303) == registry.Find<AuraTriggerSpellHunterSite>(53302));
    CHECK(registry.Find<AuraTriggerSpellHunterSite>(53304) == registry.Find<AuraTriggerSpellHunterSite>(53302));
    for (uint32 spellId : { 22895u, 22896u, 26999u })
    {
        CHECK(registry.Find<AuraTriggerSpellDruidSite>(spellId) == registry.Find<AuraTriggerSpellDruidSite>(22842));
    }
    CHECK(registry.Find<AuraTriggerSpellDruidSite>(768) != registry.Find<AuraTriggerSpellDruidSite>(22842));
    CHECK(registry.Find<AuraTriggerSpellCustomSite>(56505) == registry.Find<AuraTriggerSpellCustomSite>(44883));
    CHECK(registry.Find<AuraTriggerSpellCustomSite>(58882) == registry.Find<AuraTriggerSpellCustomSite>(56654));

    // Invisibility is a key here and at the periodic-trigger site, and at no third site; the generic switch never
    // held it.
    CHECK(SharedWith(66, AuraTriggerSpellMageSite::Key) == AuraPeriodicTriggerSite::Key);
    CHECK(SpellHandlerRegistry::Game().Find<AuraPeriodicTriggerSite>(66) != NULL);
    CHECK(registry.Find<AuraTriggerSpellGenericSite>(66) == NULL);

    // The four family switches' `default: break;` answers Continue for an id they never held and leaves the locals
    // as they were; the custom switch has none, so there a miss is a miss.
    TriggerSpellLocals locals(12345);
    AuraTriggerSpellContext ctx = locals.Context(NULL);
    CHECK(registry.Dispatch<AuraTriggerSpellMageSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraTriggerSpellHunterSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraTriggerSpellDruidSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraTriggerSpellShamanSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraTriggerSpellCustomSite>(12345, ctx).IsMiss());
    CHECK(registry.FindDefault<AuraTriggerSpellCustomSite>() == NULL);
    CHECK_EQ(locals.triggerSpellId, uint32(0));
    CHECK(locals.triggerTarget == NULL);
    CHECK(locals.triggerCaster == NULL);

    // Invisibility's and Cat Form's bodies only return.
    CHECK(registry.Dispatch<AuraTriggerSpellMageSite>(66, ctx).IsReturn());
    CHECK(registry.Dispatch<AuraTriggerSpellDruidSite>(768, ctx).IsReturn());
    CHECK_EQ(locals.triggerSpellId, uint32(0));

    // Encapsulate and Surge of Power make the trigger target the trigger caster and go on to the cast.
    alignas(16) static unsigned char object[16];
    for (uint32 spellId : { 44883u, 56505u })
    {
        TriggerSpellLocals selfCast(spellId);
        selfCast.triggerTarget = reinterpret_cast<Unit*>(object);
        AuraTriggerSpellContext selfCastCtx = selfCast.Context(NULL);
        CHECK(registry.Dispatch<AuraTriggerSpellCustomSite>(spellId, selfCastCtx).IsContinue());
        CHECK(selfCast.triggerCaster == reinterpret_cast<Unit*>(object));
        CHECK_EQ(selfCast.triggerSpellId, uint32(0));
    }
}

TEST(AurasHandlers_TheTickSitesHoldTheirLabelsAndTheDamageDefault)
{
    SpellHandlerRegistry registry;
    RegisterAurasHandlers(registry);

    // The damage tick's 8 labels in 2 bodies, the mana leech's 3 in 2.
    CheckSiteLabels<AuraPeriodicDamageTickSite>(registry, PERIODIC_DAMAGE_TICK_IDS, 2);
    CheckSiteLabels<AuraPeriodicManaLeechTickSite>(registry, PERIODIC_MANA_LEECH_TICK_IDS, 2);

    // The seven health-level removals share one body and Grievous Wound 38772 has its own; Mark of Kazzak 21056 and
    // Mark of Kaz'rogal share one, and the other Mark of Kazzak, 32960, drains mana first and then runs theirs.
    for (uint32 spellId : { 31956u, 38801u, 35321u, 38363u, 39215u, 48920u })
    {
        CHECK(registry.Find<AuraPeriodicDamageTickSite>(spellId) == registry.Find<AuraPeriodicDamageTickSite>(43093));
    }
    CHECK(registry.Find<AuraPeriodicDamageTickSite>(38772) != registry.Find<AuraPeriodicDamageTickSite>(43093));
    CHECK(registry.Find<AuraPeriodicManaLeechTickSite>(31447) == registry.Find<AuraPeriodicManaLeechTickSite>(21056));
    CHECK(registry.Find<AuraPeriodicManaLeechTickSite>(32960) != registry.Find<AuraPeriodicManaLeechTickSite>(21056));

    // The damage tick's `default: break;` is none of its bodies and answers Continue; the mana leech has none, so
    // there a miss is a miss.
    Unit* target = NULL;
    SpellEntry const* spellProto = NULL;
    Unit* caster = NULL;
    AuraPeriodicDamageTickContext damageCtx(NULL, target, spellProto, caster);
    SpellHandlerOutcome<void> missed = registry.Dispatch<AuraPeriodicDamageTickSite>(12345, damageCtx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsMiss());
    CHECK(registry.FindDefault<AuraPeriodicDamageTickSite>() != registry.Find<AuraPeriodicDamageTickSite>(43093));
    CHECK(registry.FindDefault<AuraPeriodicDamageTickSite>() != registry.Find<AuraPeriodicDamageTickSite>(38772));
    Powers power = POWER_MANA;
    uint32 damage = 0;
    int32 drain = 0;
    AuraPeriodicManaLeechTickContext leechCtx(NULL, target, power, damage, drain);
    CHECK(registry.Dispatch<AuraPeriodicManaLeechTickSite>(12345, leechCtx).IsMiss());
    CHECK(registry.FindDefault<AuraPeriodicManaLeechTickSite>() == NULL);

    // Below full health the seven removal labels go on to the damage and leave the target as it is; at full health
    // the body removes the aura by its id, which needs the aura's holder, so that branch is not run here.
    ValuedCreature creature;
    creature.SetUInt32Value(UNIT_FIELD_HEALTH, 500);
    creature.SetUInt32Value(UNIT_FIELD_MAXHEALTH, 1000);
    target = &creature;
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 43093u, 31956u, 38801u, 35321u, 38363u, 39215u, 48920u })
    {
        CHECK(game.Dispatch<AuraPeriodicDamageTickSite>(spellId, damageCtx).IsContinue());
    }
    CHECK_EQ(creature.GetHealth(), uint32(500));
    CHECK(target == &creature);
}

TEST(AurasHandlers_TheTickContextsAliasTheirLocals)
{
    alignas(16) static unsigned char objects[3][16];
    Aura* aura = reinterpret_cast<Aura*>(objects[0]);
    Unit* target = reinterpret_cast<Unit*>(objects[1]);
    SpellEntry const* spellProto = ObjectCombatSpell(0, 43093);
    Unit* caster = reinterpret_cast<Unit*>(objects[2]);
    AuraPeriodicDamageTickContext damage(aura, target, spellProto, caster);
    CHECK(damage.aura == aura);
    CHECK(&damage.target == &target);
    CHECK(&damage.spellProto == &spellProto);
    CHECK(&damage.pCaster == &caster);
    target = caster;                                            // the site's locals are read where they stand
    CHECK(damage.target == caster);
    spellProto = ObjectCombatSpell(1, 38772);
    CHECK_EQ(damage.spellProto->ID, uint32(38772));
    caster = NULL;
    CHECK(damage.pCaster == NULL);

    Powers power = POWER_MANA;
    uint32 pdamage = 100;
    int32 drain = 50;
    AuraPeriodicManaLeechTickContext leech(aura, target, power, pdamage, drain);
    CHECK(leech.aura == aura);
    CHECK(&leech.target == &target);
    CHECK(&leech.power == &power);
    CHECK(&leech.pdamage == &pdamage);
    CHECK(&leech.drain_amount == &drain);
    leech.pdamage = 25;                                         // a body's write to the amount to drain...
    CHECK_EQ(pdamage, uint32(25));                              // ...is the block's local
    leech.drain_amount = 20;                                    // so is one to the amount drained
    CHECK_EQ(drain, int32(20));
    power = POWER_RAGE;                                         // the power is read where it stands
    CHECK(leech.power == POWER_RAGE);
}

TEST(AurasHandlers_SniperTrainingCountsDownTheModifierThroughTheAura)
{
    // A standing player with reapplies left: each rank's body counts the modifier's amount down by one through the
    // aura's GetModifier() and returns; nothing else of the aura changes and the trigger spell id stays 0.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    AuraStorage storage = {};
    Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
    AuraStorage expected = {};
    reinterpret_cast<Aura*>(expected.bytes)->GetModifier()->m_amount = 2;
    StandingPlayerShape player;
    for (uint32 spellId : TRIGGER_SPELL_HUNTER_IDS)
    {
        aura->GetModifier()->m_amount = 3;
        TriggerSpellLocals locals(spellId);
        locals.triggerTarget = &player;
        AuraTriggerSpellContext ctx = locals.Context(aura);
        CHECK(game.Dispatch<AuraTriggerSpellHunterSite>(spellId, ctx).IsReturn());
        CHECK_EQ(aura->GetModifier()->m_amount, int32(2));
        CHECK_EQ(BitsThatDiffer(storage, expected), 0);
        CHECK_EQ(locals.triggerSpellId, uint32(0));
        CHECK(locals.triggerTarget == &player);
    }

    // With no reapply left the body sets the rank's trigger spell, finds no such aura on the player and goes on to
    // the cast; the aura is left as it is.
    static uint32 const ranks[] = { 64418, 64419, 64420 };
    aura->GetModifier()->m_amount = 0;
    expected = storage;
    for (std::size_t i = 0; i < 3; ++i)
    {
        TriggerSpellLocals locals(TRIGGER_SPELL_HUNTER_IDS[i]);
        locals.triggerTarget = &player;
        AuraTriggerSpellContext ctx = locals.Context(aura);
        CHECK(game.Dispatch<AuraTriggerSpellHunterSite>(TRIGGER_SPELL_HUNTER_IDS[i], ctx).IsContinue());
        CHECK_EQ(locals.triggerSpellId, ranks[i]);
        CHECK_EQ(BitsThatDiffer(storage, expected), 0);
    }

    // A trigger target that is no player returns before any of that.
    ValuedCreature creature;
    aura->GetModifier()->m_amount = 3;
    expected = storage;
    TriggerSpellLocals locals(53302);
    locals.triggerTarget = &creature;
    AuraTriggerSpellContext ctx = locals.Context(aura);
    CHECK(game.Dispatch<AuraTriggerSpellHunterSite>(53302, ctx).IsReturn());
    CHECK_EQ(BitsThatDiffer(storage, expected), 0);
    CHECK_EQ(locals.triggerSpellId, uint32(0));
}

namespace
{
    // The labels of the five periodic dummy tick sites, each in its switch's order.
    uint32 const PERIODIC_DUMMY_TICK_GENERIC_IDS[] =
    {
        7054, 7057, 30019, 42621, 42919, 46041, 46371, 48630, 59275, 49555, 59807, 50789, 59860, 50792, 59859, 50801,
        59858, 50824, 52441, 53035, 53036, 53037, 53520, 55592, 61968, 62018, 62019, 62038, 62039, 62566, 62717, 63050,
        63382, 64101, 64217, 64412, 65272, 66118, 67630, 68646, 68647, 66798, 68875, 68876
    };
    uint32 const PERIODIC_DUMMY_TICK_MAGE_IDS[] = { 55342 };
    uint32 const PERIODIC_DUMMY_TICK_DRUID_IDS[] = { 22842, 33831 };
    uint32 const PERIODIC_DUMMY_TICK_ROGUE_IDS[] = { 51690 };
    uint32 const PERIODIC_DUMMY_TICK_HUNTER_IDS[] = { 53511, 53512 };
}

TEST(AurasHandlers_ThePeriodicDummyTickSitesHoldTheirLabelsAndDefaults)
{
    SpellHandlerRegistry registry;
    RegisterAurasHandlers(registry);

    // The generic, mage, druid, rogue and hunter switches: 44, 1, 2, 1 and 2 labels in 33, 1, 2, 1 and 1 bodies.
    CheckSiteLabels<AuraPeriodicDummyTickGenericSite>(registry, PERIODIC_DUMMY_TICK_GENERIC_IDS, 33);
    CheckSiteLabels<AuraPeriodicDummyTickMageSite>(registry, PERIODIC_DUMMY_TICK_MAGE_IDS, 1);
    CheckSiteLabels<AuraPeriodicDummyTickDruidSite>(registry, PERIODIC_DUMMY_TICK_DRUID_IDS, 2);
    CheckSiteLabels<AuraPeriodicDummyTickRogueSite>(registry, PERIODIC_DUMMY_TICK_ROGUE_IDS, 1);
    CheckSiteLabels<AuraPeriodicDummyTickHunterSite>(registry, PERIODIC_DUMMY_TICK_HUNTER_IDS, 1);

    // The shared bodies: each normal and heroic pair, the three Anub'ar summons, the four Leeching Swarms, the two
    // Wailing Souls and the two Feeding Frenzy ranks; the iron dwarf, iron trogg and ooze summons each have their own.
    typedef SpellHandler<AuraPeriodicDummyTickGenericSite>::Function Body;
    static uint32 const pairs[][2] =
    {
        { 48630, 59275 }, { 49555, 59807 }, { 50789, 59860 }, { 50792, 59859 }, { 50801, 59858 }, { 53035, 53036 },
        { 53035, 53037 }, { 66118, 67630 }, { 66118, 68646 }, { 66118, 68647 }, { 68875, 68876 }
    };
    for (auto const& pair : pairs)
    {
        CHECK(registry.Find<AuraPeriodicDummyTickGenericSite>(pair[1])
              == registry.Find<AuraPeriodicDummyTickGenericSite>(pair[0]));
    }
    Body dwarf = registry.Find<AuraPeriodicDummyTickGenericSite>(50789);
    CHECK(registry.Find<AuraPeriodicDummyTickGenericSite>(50792) != dwarf);
    CHECK(registry.Find<AuraPeriodicDummyTickGenericSite>(50801) != dwarf);
    CHECK(registry.Find<AuraPeriodicDummyTickGenericSite>(50801)
          != registry.Find<AuraPeriodicDummyTickGenericSite>(50792));
    CHECK(registry.Find<AuraPeriodicDummyTickHunterSite>(53512)
          == registry.Find<AuraPeriodicDummyTickHunterSite>(53511));

    // Frenzied Regeneration is a key here and at the druid trigger-spell aura site; Haunting Spirits here and at the
    // generic aura dummy apply site; neither at a third site.
    CHECK(SharedWith(22842, AuraPeriodicDummyTickDruidSite::Key) == AuraTriggerSpellDruidSite::Key);
    CHECK(SpellHandlerRegistry::Game().Find<AuraTriggerSpellDruidSite>(22842) != NULL);
    CHECK(SharedWith(7057, AuraPeriodicDummyTickGenericSite::Key) == AuraDummyApplyGenericSite::Key);
    CHECK(SpellHandlerRegistry::Game().Find<AuraDummyApplyGenericSite>(7057) != NULL);

    // Each switch's `default: break;` is none of its bodies and answers Continue for an id it never held, leaving the
    // locals as they were.
    SpellEntry const* spell = NULL;
    Unit* target = NULL;
    AuraPeriodicDummyTickContext ctx(NULL, spell, target);
    CHECK(registry.Dispatch<AuraPeriodicDummyTickGenericSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraPeriodicDummyTickMageSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraPeriodicDummyTickDruidSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraPeriodicDummyTickRogueSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraPeriodicDummyTickHunterSite>(12345, ctx).IsContinue());
    CHECK(spell == NULL);
    CHECK(target == NULL);
    std::set<Body> generic;
    for (uint32 spellId : PERIODIC_DUMMY_TICK_GENERIC_IDS)
    {
        generic.insert(registry.Find<AuraPeriodicDummyTickGenericSite>(spellId));
    }
    CHECK_EQ(generic.count(registry.FindDefault<AuraPeriodicDummyTickGenericSite>()), std::size_t(0));
    CHECK(registry.FindDefault<AuraPeriodicDummyTickMageSite>() != registry.Find<AuraPeriodicDummyTickMageSite>(55342));
    for (uint32 spellId : PERIODIC_DUMMY_TICK_DRUID_IDS)
    {
        CHECK(registry.FindDefault<AuraPeriodicDummyTickDruidSite>()
              != registry.Find<AuraPeriodicDummyTickDruidSite>(spellId));
    }
    CHECK(registry.FindDefault<AuraPeriodicDummyTickRogueSite>()
          != registry.Find<AuraPeriodicDummyTickRogueSite>(51690));
    CHECK(registry.FindDefault<AuraPeriodicDummyTickHunterSite>()
          != registry.Find<AuraPeriodicDummyTickHunterSite>(53511));
}

TEST(AurasHandlers_ThePeriodicDummyTickContextAliasesItsLocals)
{
    alignas(16) static unsigned char objects[3][16];
    Aura* aura = reinterpret_cast<Aura*>(objects[0]);
    SpellEntry const* spell = ObjectCombatSpell(0, 7054);
    Unit* target = reinterpret_cast<Unit*>(objects[1]);
    AuraPeriodicDummyTickContext ctx(aura, spell, target);
    CHECK(ctx.aura == aura);
    CHECK(&ctx.spell == &spell);
    CHECK(&ctx.target == &target);
    spell = ObjectCombatSpell(1, 55342);                        // the function's locals are read where they stand
    CHECK_EQ(ctx.spell->ID, uint32(55342));
    target = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.target == reinterpret_cast<Unit*>(objects[2]));
}

TEST(AurasHandlers_ThePeriodicDummyTickBodiesReturnWhenThereIsNothingToDo)
{
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Forsaken Skills and Force of Nature only return; they read neither the aura nor the target.
    SpellEntry const* spell = NULL;
    Unit* target = NULL;
    AuraPeriodicDummyTickContext bare(NULL, spell, target);
    CHECK(game.Dispatch<AuraPeriodicDummyTickGenericSite>(7054, bare).IsReturn());
    CHECK(game.Dispatch<AuraPeriodicDummyTickDruidSite>(33831, bare).IsReturn());

    // Mirror Image acts on the aura's first tick only: off it the body returns and leaves the aura as it is.
    AuraStorage storage = {};
    Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
    AuraStorage const expected = storage;
    AuraPeriodicDummyTickContext mirror(aura, spell, target);
    CHECK(game.Dispatch<AuraPeriodicDummyTickMageSite>(55342, mirror).IsReturn());
    CHECK_EQ(BitsThatDiffer(storage, expected), 0);

    // Frenzied Regeneration converts rage only: on a creature whose power is mana it returns and leaves it as it is.
    ValuedCreature creature;
    creature.SetUInt32Value(UNIT_FIELD_HEALTH, 500);
    creature.SetUInt32Value(UNIT_FIELD_MAXHEALTH, 1000);
    CHECK(creature.GetPowerType() == POWER_MANA);
    target = &creature;
    AuraPeriodicDummyTickContext rage(aura, spell, target);
    CHECK(game.Dispatch<AuraPeriodicDummyTickDruidSite>(22842, rage).IsReturn());
    CHECK_EQ(creature.GetHealth(), uint32(500));
    CHECK(creature.GetPowerType() == POWER_MANA);
    CHECK_EQ(BitsThatDiffer(storage, expected), 0);
    CHECK(target == &creature);
}

namespace
{
    // The labels of the five boost sites, each in its switch's order.
    uint32 const BOOST_GENERIC_IDS[] = { 29865, 55053, 50720, 57350, 58914, 62692, 71905 };
    uint32 const BOOST_MAGE_IDS[] = { 11129, 28682, 44401, 48108, 57761, 74396 };
    uint32 const BOOST_PRIEST_IDS[] = { 552, 47585 };
    uint32 const BOOST_HUNTER_IDS[] = { 34074, 19574, 34471, 34027, 35029, 35030 };
    uint32 const BOOST_DEATH_KNIGHT_IDS[] = { 49039, 48263, 48265, 48266 };

    // HandleSpellSpecificBoosts' parameter and locals as the function starts them.
    struct BoostLocals
    {
        bool apply;
        bool castAtRemove;
        uint32 spellId1;
        uint32 spellId2;
        uint32 spellId3;
        uint32 spellId4;

        BoostLocals() : apply(false), castAtRemove(false), spellId1(0), spellId2(0), spellId3(0), spellId4(0) {}

        AuraBoostContext Context(SpellAuraHolder* holder)
        {
            return AuraBoostContext(holder, apply, castAtRemove, spellId1, spellId2, spellId3, spellId4);
        }

        bool Untouched() const
        {
            return !castAtRemove && spellId1 == 0 && spellId2 == 0 && spellId3 == 0 && spellId4 == 0;
        }
    };
}

TEST(AurasHandlers_TheBoostSitesHoldTheirLabelsAndDefaults)
{
    SpellHandlerRegistry registry;
    RegisterAurasHandlers(registry);

    // The generic, mage, priest, hunter and death knight switches: 7, 6, 2, 6 and 4 labels in 7, 4, 2, 5 and 2 bodies.
    CheckSiteLabels<AuraBoostGenericSite>(registry, BOOST_GENERIC_IDS, 7);
    CheckSiteLabels<AuraBoostMageSite>(registry, BOOST_MAGE_IDS, 4);
    CheckSiteLabels<AuraBoostPriestSite>(registry, BOOST_PRIEST_IDS, 2);
    CheckSiteLabels<AuraBoostHunterSite>(registry, BOOST_HUNTER_IDS, 5);
    CheckSiteLabels<AuraBoostDeathKnightSite>(registry, BOOST_DEATH_KNIGHT_IDS, 2);

    // The shared bodies: the three Pushing the Limit triggers, Bestial Wrath with The Beast Within, and the three
    // presences; Lichborne has its own.
    CHECK(registry.Find<AuraBoostMageSite>(48108) == registry.Find<AuraBoostMageSite>(44401));
    CHECK(registry.Find<AuraBoostMageSite>(57761) == registry.Find<AuraBoostMageSite>(44401));
    CHECK(registry.Find<AuraBoostHunterSite>(34471) == registry.Find<AuraBoostHunterSite>(19574));
    CHECK(registry.Find<AuraBoostDeathKnightSite>(48265) == registry.Find<AuraBoostDeathKnightSite>(48263));
    CHECK(registry.Find<AuraBoostDeathKnightSite>(48266) == registry.Find<AuraBoostDeathKnightSite>(48263));
    CHECK(registry.Find<AuraBoostDeathKnightSite>(49039) != registry.Find<AuraBoostDeathKnightSite>(48263));

    // Vigilance is a key here and at the proc-trigger site, and at no third site.
    CHECK(SharedWith(50720, AuraBoostGenericSite::Key) == AuraProcTriggerSite::Key);
    CHECK(SpellHandlerRegistry::Game().Find<AuraProcTriggerSite>(50720) != NULL);

    // The generic, mage and priest switches' `default: return;` answers Return for an id they never held and leaves
    // the locals as they were; the death knight switch has none, so there a miss is a miss. The hunter default reads
    // the holder's spell entry, so it is checked as a body none of its site's labels holds.
    BoostLocals locals;
    AuraBoostContext ctx = locals.Context(NULL);
    CHECK(registry.Dispatch<AuraBoostGenericSite>(12345, ctx).IsReturn());
    CHECK(registry.Dispatch<AuraBoostMageSite>(12345, ctx).IsReturn());
    CHECK(registry.Dispatch<AuraBoostPriestSite>(12345, ctx).IsReturn());
    CHECK(registry.Dispatch<AuraBoostDeathKnightSite>(12345, ctx).IsMiss());
    CHECK(registry.FindDefault<AuraBoostDeathKnightSite>() == NULL);
    CHECK(locals.Untouched());
    for (uint32 spellId : BOOST_GENERIC_IDS)
    {
        CHECK(registry.FindDefault<AuraBoostGenericSite>() != registry.Find<AuraBoostGenericSite>(spellId));
    }
    for (uint32 spellId : BOOST_MAGE_IDS)
    {
        CHECK(registry.FindDefault<AuraBoostMageSite>() != registry.Find<AuraBoostMageSite>(spellId));
    }
    for (uint32 spellId : BOOST_PRIEST_IDS)
    {
        CHECK(registry.FindDefault<AuraBoostPriestSite>() != registry.Find<AuraBoostPriestSite>(spellId));
    }
    for (uint32 spellId : BOOST_HUNTER_IDS)
    {
        CHECK(registry.FindDefault<AuraBoostHunterSite>() != NULL);
        CHECK(registry.FindDefault<AuraBoostHunterSite>() != registry.Find<AuraBoostHunterSite>(spellId));
    }

    // The bodies that only set boost spell ids read nothing of the holder and go on to the casts with them set.
    BoostLocals vigilance;
    AuraBoostContext vigilanceCtx = vigilance.Context(NULL);
    CHECK(registry.Dispatch<AuraBoostGenericSite>(50720, vigilanceCtx).IsContinue());
    CHECK_EQ(vigilance.spellId1, uint32(68066));
    CHECK(!vigilance.castAtRemove);

    BoostLocals despair;
    AuraBoostContext despairCtx = despair.Context(NULL);
    CHECK(registry.Dispatch<AuraBoostGenericSite>(62692, despairCtx).IsContinue());
    CHECK_EQ(despair.spellId1, uint32(64848));

    BoostLocals dispersion;
    AuraBoostContext dispersionCtx = dispersion.Context(NULL);
    CHECK(registry.Dispatch<AuraBoostPriestSite>(47585, dispersionCtx).IsContinue());
    CHECK_EQ(dispersion.spellId1, uint32(60069));
    CHECK_EQ(dispersion.spellId2, uint32(63230));

    for (uint32 spellId : { 19574u, 34471u })
    {
        BoostLocals wrath;
        AuraBoostContext wrathCtx = wrath.Context(NULL);
        CHECK(registry.Dispatch<AuraBoostHunterSite>(spellId, wrathCtx).IsContinue());
        CHECK_EQ(wrath.spellId1, uint32(24395));
        CHECK_EQ(wrath.spellId2, uint32(24396));
        CHECK_EQ(wrath.spellId3, uint32(24397));
        CHECK_EQ(wrath.spellId4, uint32(26592));
    }

    BoostLocals lichborne;
    AuraBoostContext lichborneCtx = lichborne.Context(NULL);
    CHECK(registry.Dispatch<AuraBoostDeathKnightSite>(49039, lichborneCtx).IsContinue());
    CHECK_EQ(lichborne.spellId1, uint32(50397));
    CHECK_EQ(lichborne.spellId2, uint32(0));
}

TEST(AurasHandlers_TheBoostContextAliasesItsLocals)
{
    alignas(16) static unsigned char object[16];
    SpellAuraHolder* holder = reinterpret_cast<SpellAuraHolder*>(object);
    bool apply = false;
    bool cast_at_remove = false;
    uint32 spellId1 = 0;
    uint32 spellId2 = 0;
    uint32 spellId3 = 0;
    uint32 spellId4 = 0;
    AuraBoostContext ctx(holder, apply, cast_at_remove, spellId1, spellId2, spellId3, spellId4);
    CHECK(ctx.holder == holder);
    CHECK(&ctx.apply == &apply);
    CHECK(&ctx.cast_at_remove == &cast_at_remove);
    CHECK(&ctx.spellId1 == &spellId1);
    CHECK(&ctx.spellId2 == &spellId2);
    CHECK(&ctx.spellId3 == &spellId3);
    CHECK(&ctx.spellId4 == &spellId4);
    apply = true;                                               // the parameter is read where it stands
    CHECK(ctx.apply);
    ctx.cast_at_remove = true;                                  // a body's writes are the function's locals
    CHECK(cast_at_remove);
    ctx.spellId1 = 55594;
    ctx.spellId4 = 26592;
    CHECK_EQ(spellId1, uint32(55594));
    CHECK_EQ(spellId4, uint32(26592));
}

TEST(AurasHandlers_TheBoostBodiesReadTheRemoveModeThroughTheHolder)
{
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    HolderStorage storage;
    SpellAuraHolder* holder = HolderFilledWith(storage, 0x00);

    // Deathbloom casts at remove only when the holder expired, and reads the mode the holder has when the body
    // runs, not when the context was made.
    for (uint32 spellId : { 29865u, 55053u })
    {
        BoostLocals locals;
        AuraBoostContext ctx = locals.Context(holder);
        holder->SetRemoveMode(AURA_REMOVE_BY_DISPEL);
        CHECK(game.Dispatch<AuraBoostGenericSite>(spellId, ctx).IsReturn());
        CHECK(locals.Untouched());
        holder->SetRemoveMode(AURA_REMOVE_BY_EXPIRE);
        CHECK(game.Dispatch<AuraBoostGenericSite>(spellId, ctx).IsContinue());
        CHECK(locals.castAtRemove);
        CHECK_EQ(locals.spellId1, spellId == 29865 ? uint32(55594) : uint32(55601));
        CHECK_EQ(locals.spellId2, uint32(0));
    }

    // At apply Deathbloom returns whatever the mode reads.
    BoostLocals applying;
    applying.apply = true;
    AuraBoostContext applyingCtx = applying.Context(holder);
    CHECK(game.Dispatch<AuraBoostGenericSite>(29865, applyingCtx).IsReturn());
    CHECK(applying.Untouched());

    // Pushing the Limit acts only on a holder its charges consumed, the default mode; on an expired one it returns
    // before it reads the caster.
    for (uint32 spellId : { 44401u, 48108u, 57761u })
    {
        BoostLocals locals;
        AuraBoostContext ctx = locals.Context(holder);
        CHECK(game.Dispatch<AuraBoostMageSite>(spellId, ctx).IsReturn());
        CHECK(locals.Untouched());
    }
    holder->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
    CHECK_EQ(int(holder->GetRemoveMode()), int(AURA_REMOVE_BY_DEFAULT));
}

namespace
{
    // The labels of the generic script-effect site this file holds, in the switch's order.
    uint32 const SCRIPT_GENERIC_IDS[] = { 8856, 17512, 20589, 22539, 22972, 22975, 22976, 22977, 22978, 22979, 22980,
                                          22981, 22982, 22983, 22984, 22985, 24194, 24195, 24320, 24324, 24590, 24714,
                                          24717, 24718, 24719, 24720, 24737, 24751, 25140, 25143, 25650, 25652, 29128,
                                          29129, 35376, 35727, 26004, 26137, 26218, 26275, 26465, 26656, 27687, 27695,
                                          28352, 28374, 54426, 71123, 28560, 28859, 29126, 29135, 29136, 29137, 29138,
                                          29139, 46671, 46672, 29395, 29830, 30541, 30469, 30769, 30835, 30918, 37142,
                                          37143, 37147, 37149, 37150, 37220, 32227, 32228, 37337, 37339, 37345, 37348,
                                          32301, 33676, 34653, 36920, 35865, 37431, 37775, 37824, 38358, 39338, 39342,
                                          39341, 39344, 41055, 63416, 69891, 41072, 41126, 41213, 43416, 69222, 73076,
                                          42281, 42492, 42578, 43365, 43375, 44323, 44407, 44364, 44455, 44462, 44876, 44811,
                                          45141, 45151, 45185, 45204, 45206, 69892, 45235, 45260, 45313, 45625, 45668,
                                          45691, 45713, 45714, 45717, 45785, 45833, 45892, 45918, 45958, 46203, 46289,
                                          46430, 46642, 47097, 47311, 47393, 47615, 47638, 47703, 47724, 47958, 57083,
                                          48590, 48603, 48724, 48726, 48728, 48730, 48762, 45759, 69672, 69673, 48810,
                                          48811, 48917, 49380, 59803, 49405, 50217, 50218, 50238, 50252, 50255, 59331,
                                          50439, 50630, 50725, 50742, 50810, 61546, 50894, 51519, 51770, 51864, 51889,
                                          51904, 51910, 52479, 52555, 52694, 52751, 52941, 53110, 54182, 54581, 54436,
                                          55693, 56072, 57082, 57337, 57397, 58466, 58475, 58418, 58420, 59317, 59789,
                                          60893, 61177, 61288, 61756, 64323, 62042, 62217, 62922, 62262, 62282, 62440,
                                          63598, 63601, 62381, 62488, 62524, 62525, 62521, 62552, 62575, 68282, 62688,
                                          62707, 63010, 68307, 68504, 63027, 63119, 64192, 63122, 63633, 63667, 63681,
                                          63795, 65301, 63803, 64164, 64168, 63993, 64059, 64069, 64123, 64131, 64456,
                                          64466, 64467, 64475, 64623, 64767, 64841, 65238, 66477, 66545, 66741, 66744,
                                          67009, 67547, 67590, 67751, 68084, 68861, 68871, 69048, 69051, 69057, 69140,
                                          69147, 69195, 69298, 69377, 69378, 69381, 69828, 71806, 72034, 72096, 72195,
                                          72219, 72257, 72409, 72705, 72900, 73142, 74455 };

    // The generic labels that share a body, each run in the switch's order.
    uint32 const SCRIPT_SHADOW_FLAME_IDS[] = { 22539, 22972, 22975, 22976, 22977, 22978, 22979, 22980, 22981, 22982,
                                               22983, 22984, 22985 };
    uint32 const SCRIPT_TRIBUTE_IDS[] = { 24194, 24195 };
    uint32 const SCRIPT_ORB_TELEPORT_IDS[] = { 25140, 25143, 25650, 25652, 29128, 29129, 35376, 35727 };
    uint32 const SCRIPT_DECIMATE_IDS[] = { 28374, 54426, 71123 };
    uint32 const SCRIPT_CLEANSING_FLAMES_IDS[] = { 28859, 29126, 29135, 29136, 29137, 29138, 29139, 46671, 46672 };
    uint32 const SCRIPT_CHESS_MELEE_IDS[] = { 37142, 37143, 37147, 37149, 37150, 37220, 32227, 32228, 37337, 37339,
                                              37345, 37348 };
    uint32 const SCRIPT_FIREBALL_IDS[] = { 34653, 36920 };
    uint32 const SCRIPT_HAND_OF_MEDIVH_IDS[] = { 39338, 39342 };
    uint32 const SCRIPT_FURY_OF_MEDIVH_IDS[] = { 39341, 39344 };
    uint32 const SCRIPT_COPY_WEAPON_IDS[] = { 41055, 63416, 69891 };
    uint32 const SCRIPT_THROW_SHIELD_IDS[] = { 41213, 43416, 69222, 73076 };
    uint32 const SCRIPT_HAWK_HUNTING_IDS[] = { 44323, 44407 };
    uint32 const SCRIPT_COPY_OFF_HAND_WEAPON_IDS[] = { 45206, 69892 };
    uint32 const SCRIPT_CRYSTAL_SPIKES_IDS[] = { 47958, 57083 };
    uint32 const SCRIPT_DENOUNCEMENT_IDS[] = { 48724, 48726, 48728, 48730 };
    uint32 const SCRIPT_GENDER_DISGUISE_IDS[] = { 48762, 45759, 69672, 69673 };
    uint32 const SCRIPT_CONSUME_IDS[] = { 49380, 59803 };
    uint32 const SCRIPT_POISONED_SPEAR_IDS[] = { 50255, 59331 };
    uint32 const SCRIPT_SHATTER_IDS[] = { 50810, 61546 };
    uint32 const SCRIPT_GIGANTIC_OR_SMALL_FEAST_IDS[] = { 58466, 58475 };
    uint32 const SCRIPT_PORTAL_IDS[] = { 58418, 58420 };
    uint32 const SCRIPT_DISCOVERY_RESEARCH_IDS[] = { 60893, 61177, 61288, 61756, 64323 };
    uint32 const SCRIPT_UNSTABLE_ENERGY_IDS[] = { 62217, 62922 };
    uint32 const SCRIPT_IRON_ROOTS_IDS[] = { 62282, 62440, 63598, 63601 };
    uint32 const SCRIPT_ATTUNED_TO_NATURE_IDS[] = { 62524, 62525, 62521 };
    uint32 const SCRIPT_PLAYER_CHARGE_IDS[] = { 62575, 68282 };
    uint32 const SCRIPT_MOUNTED_CHARGE_IDS[] = { 63010, 68307, 68504 };
    uint32 const SCRIPT_BLOCK_IDS[] = { 63119, 64192 };
    uint32 const SCRIPT_PSYCHOSIS_IDS[] = { 63795, 65301 };
    uint32 const SCRIPT_BRAIN_LINK_IDS[] = { 63803, 64164, 64168 };
    uint32 const SCRIPT_WHITEOUT_IDS[] = { 72034, 72096 };

    // The generic labels whose body checks the unit target before anything else.
    uint32 const SCRIPT_GENERIC_TARGET_CHECKED_IDS[] = { 17512, 20589, 22539, 22972, 22975, 22976, 22977, 22978, 22979,
                                                         22980, 22981, 22982, 22983, 22984, 22985, 24324, 24717, 24718,
                                                         24719, 24720, 24737, 24751, 25140, 25143, 25650, 25652, 29128,
                                                         29129, 35376, 35727, 26004, 26137, 26218, 26656, 27687, 27695,
                                                         28352, 28374, 54426, 71123, 28560, 28859, 29126, 29135, 29136,
                                                         29137, 29138, 29139, 46671, 46672, 30541, 30469, 30769, 30835,
                                                         30918, 37142, 37143, 37147, 37149, 37150, 37220, 32227, 32228,
                                                         37337, 37339, 37345, 37348, 32301, 33676, 34653, 36920, 35865,
                                                         37431, 37775, 37824, 38358, 39338, 39342, 39341, 39344, 41072,
                                                         41126, 41213, 43416, 69222, 73076, 42281, 42492, 42578, 43375,
                                                         44323, 44407, 44364, 44876, 44811, 45141, 45151, 45185, 45204,
                                                         45235, 45260, 45313, 45625, 45668, 45714, 45717, 45785, 45833,
                                                         45892, 45918, 46203, 46289, 46430, 46642, 47097, 47393, 47615,
                                                         47638, 47958, 57083, 48590, 48724, 48726, 48728, 48730, 48762,
                                                         45759, 69672, 69673, 48811, 48917, 49380, 59803, 49405, 50255,
                                                         59331, 50742, 50810, 61546, 50894, 51519, 51904, 52555, 52751,
                                                         53110, 54182, 54436, 55693, 56072, 57082, 57337, 57397, 58466,
                                                         58475, 58418, 58420, 59317, 59789, 62042, 62217, 62922, 62262,
                                                         62282, 62440, 63598, 63601, 62381, 62488, 62524, 62525, 62521,
                                                         62552, 62575, 68282, 62707, 63010, 68307, 68504, 63027, 63119,
                                                         64192, 63122, 63633, 63667, 63681, 63795, 65301, 63803, 64164,
                                                         64168, 63993, 64059, 64069, 64123, 64131, 64456, 64466, 64467,
                                                         64475, 64623, 64767, 64841, 65238, 66477, 66545, 66744, 67009,
                                                         67547, 67590, 67751, 68084, 68861, 69048, 69051, 69057, 69140,
                                                         69147, 69195, 69298, 69377, 69378, 69381, 69828, 71806, 72195,
                                                         72219, 72257, 72409, 72705, 72900, 73142, 74455 };

    // The labels of the five script-effect family sites, each in its switch's order.
    uint32 const SCRIPT_WARLOCK_IDS[] = { 6201, 6202, 5699, 11729, 11730, 27230, 47871, 47878, 47193, 47422, 63521 };
    uint32 const SCRIPT_PRIEST_IDS[] = { 47948 };
    uint32 const SCRIPT_HUNTER_IDS[] = { 53209, 53412, 53271 };
    uint32 const SCRIPT_POTION_IDS[] = { 28698, 28702, 28720 };
    uint32 const SCRIPT_DEATH_KNIGHT_IDS[] = { 50842 };

    // The healthstone ranks share the Create Healthstone body.
    uint32 const SCRIPT_HEALTHSTONE_IDS[] = { 6202, 5699, 11729, 11730, 27230, 47871, 47878 };

    // The spell's members and the effect, all empty: no spell, no entry, no caster and no targets.
    struct ScriptMembers
    {
        SpellEntry const* spellInfo;
        SpellCastTargets targets;
        Unit* caster;
        ObjectGuid originalCasterGuid;
        Unit* originalCaster;
        Unit* unitTarget;
        Item* itemTarget;
        int32 damage;
        SpellEffectEntry const* effect;

        ScriptMembers() : spellInfo(NULL), caster(NULL), originalCaster(NULL), unitTarget(NULL), itemTarget(NULL),
                          damage(0), effect(NULL) {}

        SpellEffectScriptContext Context(Spell* spell = NULL)
        {
            return SpellEffectScriptContext(spell, spellInfo, targets, caster, originalCasterGuid, originalCaster,
                                            unitTarget, itemTarget, damage, effect);
        }

        bool Untouched() const
        {
            return spellInfo == NULL && caster == NULL && originalCasterGuid.IsEmpty() && originalCaster == NULL &&
                   unitTarget == NULL && itemTarget == NULL && damage == 0 && effect == NULL;
        }
    };
}

TEST(SpellEffectScriptHandlers_TheTableRegistersEveryRowOnce)
{
    // 302 labels at six sites and two defaults: 304 rows, 302 keys and two defaults, none twice. The generic,
    // warlock, priest, hunter, potion and death knight switches hold 283, 11, 1, 3, 3 and 1; the priest and hunter
    // switches have a default.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectScriptHandlers(registry), uint32(304));
    CHECK_EQ(registry.Count(), std::size_t(302));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));
    CHECK_EQ(registry.CountAt(SpellEffectScriptGenericSite::Key), std::size_t(283));
    CHECK_EQ(registry.CountAt(SpellEffectScriptWarlockSite::Key), std::size_t(11));
    CHECK_EQ(registry.CountAt(SpellEffectScriptPriestSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectScriptHunterSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(SpellEffectScriptPotionSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(SpellEffectScriptDeathKnightSite::Key), std::size_t(1));

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterSpellEffectScriptHandlers(registry), uint32(304));
    CHECK_EQ(registry.Count(), std::size_t(302));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));

    // The game's table holds these with the other files' 921 rows and 27 defaults.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(1223));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
    CHECK_EQ(game.CountAt(SpellEffectScriptGenericSite::Key), std::size_t(283));
    CHECK_EQ(game.CountAt(SpellEffectScriptWarlockSite::Key), std::size_t(11));
    CHECK_EQ(game.CountAt(SpellEffectScriptHunterSite::Key), std::size_t(3));
}

TEST(SpellEffectScriptHandlers_TheFamilySitesHoldTheirLabelsAndDefaults)
{
    SpellHandlerRegistry registry;
    RegisterSpellEffectScriptHandlers(registry);

    // The warlock, priest, hunter, potion and death knight switches: 11, 1, 3, 3 and 1 labels in 4, 1, 3, 3 and 1
    // bodies.
    CheckSiteLabels<SpellEffectScriptWarlockSite>(registry, SCRIPT_WARLOCK_IDS, 4);
    CheckSiteLabels<SpellEffectScriptPriestSite>(registry, SCRIPT_PRIEST_IDS, 1);
    CheckSiteLabels<SpellEffectScriptHunterSite>(registry, SCRIPT_HUNTER_IDS, 3);
    CheckSiteLabels<SpellEffectScriptPotionSite>(registry, SCRIPT_POTION_IDS, 3);
    CheckSiteLabels<SpellEffectScriptDeathKnightSite>(registry, SCRIPT_DEATH_KNIGHT_IDS, 1);

    // The seven healthstone ranks share the Create Healthstone body; the other warlock labels have their own.
    typedef SpellHandler<SpellEffectScriptWarlockSite>::Function WarlockBody;
    WarlockBody healthstone = registry.Find<SpellEffectScriptWarlockSite>(6201);
    for (uint32 spellId : SCRIPT_HEALTHSTONE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptWarlockSite>(spellId) == healthstone);
    }
    for (uint32 spellId : { 47193u, 47422u, 63521u })
    {
        CHECK(registry.Find<SpellEffectScriptWarlockSite>(spellId) != healthstone);
    }

    // Master's Call is a key here and at the hunter dummy-effect site, and at no third site.
    CHECK(SharedWith(53271, SpellEffectScriptHunterSite::Key) == SpellEffectDummyHunterSite::Key);
    CHECK(SpellHandlerRegistry::Game().Find<SpellEffectDummyHunterSite>(53271) != NULL);
    for (uint32 spellId : { 53209u, 53412u })
    {
        CHECK(SharedWith(spellId, SpellEffectScriptHunterSite::Key) == SpellEffectScriptHunterSite::Key);
    }

    // The priest and hunter switches' `default: break;` answers Continue for an id they never held and leaves the
    // members as they were; the warlock, potion and death knight switches have none, so there a miss is a miss.
    ScriptMembers members;
    SpellEffectScriptContext ctx = members.Context();
    CHECK(registry.FindDefault<SpellEffectScriptPriestSite>() != NULL);
    CHECK(registry.FindDefault<SpellEffectScriptHunterSite>() != NULL);
    CHECK(registry.FindDefault<SpellEffectScriptWarlockSite>() == NULL);
    CHECK(registry.FindDefault<SpellEffectScriptPotionSite>() == NULL);
    CHECK(registry.FindDefault<SpellEffectScriptDeathKnightSite>() == NULL);
    CHECK(registry.Dispatch<SpellEffectScriptPriestSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<SpellEffectScriptHunterSite>(12345, ctx).IsContinue());
    CHECK(registry.Dispatch<SpellEffectScriptWarlockSite>(12345, ctx).IsMiss());
    CHECK(registry.Dispatch<SpellEffectScriptPotionSite>(12345, ctx).IsMiss());
    CHECK(registry.Dispatch<SpellEffectScriptDeathKnightSite>(12345, ctx).IsMiss());
    CHECK(members.Untouched());
    for (uint32 spellId : SCRIPT_PRIEST_IDS)
    {
        CHECK(registry.FindDefault<SpellEffectScriptPriestSite>() !=
              registry.Find<SpellEffectScriptPriestSite>(spellId));
    }
    for (uint32 spellId : SCRIPT_HUNTER_IDS)
    {
        CHECK(registry.FindDefault<SpellEffectScriptHunterSite>() !=
              registry.Find<SpellEffectScriptHunterSite>(spellId));
    }
}

TEST(SpellEffectScriptHandlers_TheContextAliasesTheSpellsMembers)
{
    alignas(16) static unsigned char objects[9][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 6201);
    SpellCastTargets targets;
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    ObjectGuid originalCasterGuid(HIGHGUID_PLAYER, uint32(7));
    Unit* originalCaster = reinterpret_cast<Unit*>(objects[2]);
    Unit* target = reinterpret_cast<Unit*>(objects[3]);
    Item* itemTarget = reinterpret_cast<Item*>(objects[4]);
    int32 damage = 10;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectScriptContext ctx(spell, spellInfo, targets, caster, originalCasterGuid, originalCaster, target,
                                 itemTarget, damage, effect);
    CHECK(ctx.spell == spell);                                  // the spell itself, whose methods the bodies call
    CHECK(&ctx.m_spellInfo == &spellInfo);
    CHECK(&ctx.m_targets == &targets);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.m_originalCasterGUID == &originalCasterGuid);
    CHECK(&ctx.m_originalCaster == &originalCaster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.itemTarget == &itemTarget);
    CHECK(&ctx.damage == &damage);
    CHECK(&ctx.effect == &effect);

    SpellEntry const* otherEntry = ObjectCombatSpell(1, 47193);
    ctx.m_spellInfo = otherEntry;                               // the spell's entry is the spell's member, not a copy
    CHECK(spellInfo == otherEntry);
    spellInfo = ObjectCombatSpell(0, 6201);
    CHECK(ctx.m_spellInfo == ObjectCombatSpell(0, 6201));
    ctx.m_caster = reinterpret_cast<Unit*>(objects[5]);         // and the caster
    CHECK(caster == reinterpret_cast<Unit*>(objects[5]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_originalCasterGUID = ObjectGuid(HIGHGUID_PLAYER, uint32(9)); // and the original caster's guid
    CHECK(originalCasterGuid == ObjectGuid(HIGHGUID_PLAYER, uint32(9)));
    originalCasterGuid = ObjectGuid();
    CHECK(ctx.m_originalCasterGUID == ObjectGuid());
    ctx.m_originalCaster = reinterpret_cast<Unit*>(objects[6]); // and the original caster
    CHECK(originalCaster == reinterpret_cast<Unit*>(objects[6]));
    originalCaster = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.m_originalCaster == reinterpret_cast<Unit*>(objects[2]));
    ctx.unitTarget = reinterpret_cast<Unit*>(objects[7]);       // and the unit target
    CHECK(target == reinterpret_cast<Unit*>(objects[7]));
    target = reinterpret_cast<Unit*>(objects[3]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[3]));
    ctx.itemTarget = reinterpret_cast<Item*>(objects[8]);       // and the item target
    CHECK(itemTarget == reinterpret_cast<Item*>(objects[8]));
    itemTarget = reinterpret_cast<Item*>(objects[4]);
    CHECK(ctx.itemTarget == reinterpret_cast<Item*>(objects[4]));
    ctx.damage = 20;                                            // and the damage
    CHECK_EQ(damage, int32(20));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

TEST(SpellEffectScriptHandlers_ANullTargetEndsTheEffect)
{
    // Every body that checks the unit target first ends the effect when there is none, before it reads the spell,
    // its entry or its caster, and leaves the members as they were. Everlasting Affliction, 63521, Poisonous Blood,
    // Brittle Armor, PX-238 Winter Wondervolt TRAP, Mercurial Shield and High Executor's Branding Iron read the
    // target without a check; Bending Shinbone, the two tributes, Trick, Copy Weapon, The Cleansing: Shrine Cast,
    // Cast Quest Complete on Master, Copy Off-hand Weapon, 45691, 45713, Signal Alliance, 47311, 47703, 47724,
    // 48810, 50217, 50238, 50252, 50439, Eject All Passengers, the two Summon Nass spells, Kickin' Nass, Gift of
    // the Harvester, Recall Eye of Acherus, Song of Cleansing, Mammoth Explosion Spell Spawner, the research
    // spells, Chum the Water, Wailing Souls and Whiteout read the caster first; 50218 reads the original caster
    // first; Summon Wave - 10 Mob reads the effect first; Break Kaliri Egg, Use Mirren's Drinking Hat, Vigilance
    // and Emblazon Runeblade reach the spell first. They are not run here.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    ScriptMembers members;
    SpellEffectScriptContext ctx = members.Context();
    CHECK(game.Dispatch<SpellEffectScriptWarlockSite>(6201, ctx).IsReturn());
    CHECK(game.Dispatch<SpellEffectScriptWarlockSite>(47193, ctx).IsReturn());
    CHECK(game.Dispatch<SpellEffectScriptPriestSite>(47948, ctx).IsReturn());
    for (uint32 spellId : SCRIPT_HUNTER_IDS)
    {
        CHECK(game.Dispatch<SpellEffectScriptHunterSite>(spellId, ctx).IsReturn());
    }
    for (uint32 spellId : SCRIPT_POTION_IDS)
    {
        CHECK(game.Dispatch<SpellEffectScriptPotionSite>(spellId, ctx).IsReturn());
    }
    CHECK(game.Dispatch<SpellEffectScriptDeathKnightSite>(50842, ctx).IsReturn());
    for (uint32 spellId : SCRIPT_GENERIC_TARGET_CHECKED_IDS)
    {
        CHECK(game.Dispatch<SpellEffectScriptGenericSite>(spellId, ctx).IsReturn());
    }
    CHECK(members.Untouched());
}

TEST(SpellEffectScriptHandlers_TheGenericSiteHoldsItsMovedLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterSpellEffectScriptHandlers(registry);

    // The 283 labels of the switch this file holds, in 202 bodies. Some bodies are one code: Bloodbolt, Coldflame
    // (69140) and Glittering Sparks with Fury of Medivh; Clone Me!, Invader Taunt Trigger, Empowering Shadows
    // (64466), Halls of Reflection Clone, Rune of Blood and Conflagration with Throw Shield; Arcane Chains,
    // Coldflame (69147) and Consume Soul with Sinister Reflection Clone; Shattered Illusion, Pungent Blight and
    // Cancel Resistant to Blight with Unstable Energy; Remove Marks of the Fallen Champion with Clear Insane. Each
    // counts as one body whether the linker gives it one address or more.
    std::size_t folds = 0;
    for (std::pair<uint32, uint32> same : { std::make_pair(41072u, 39341u), std::make_pair(45204u, 41213u),
                                           std::make_pair(49405u, 41213u), std::make_pair(45625u, 45785u),
                                           std::make_pair(69140u, 39341u), std::make_pair(71806u, 39341u),
                                           std::make_pair(64466u, 41213u), std::make_pair(69828u, 41213u),
                                           std::make_pair(72409u, 41213u), std::make_pair(74455u, 41213u),
                                           std::make_pair(69147u, 45785u), std::make_pair(65238u, 62217u),
                                           std::make_pair(69195u, 62217u), std::make_pair(69298u, 62217u),
                                           std::make_pair(68861u, 45785u), std::make_pair(72257u, 63122u) })
    {
        if (registry.Find<SpellEffectScriptGenericSite>(same.first) ==
            registry.Find<SpellEffectScriptGenericSite>(same.second))
        {
            ++folds;
        }
    }
    CheckSiteLabels<SpellEffectScriptGenericSite>(registry, SCRIPT_GENERIC_IDS, 202 - folds);

    // Each shared run's labels have one body, and no two runs share one.
    typedef SpellHandler<SpellEffectScriptGenericSite>::Function GenericBody;
    std::set<GenericBody> runBodies;
    for (uint32 const* run : { SCRIPT_SHADOW_FLAME_IDS, SCRIPT_TRIBUTE_IDS, SCRIPT_ORB_TELEPORT_IDS,
                               SCRIPT_DECIMATE_IDS, SCRIPT_CLEANSING_FLAMES_IDS, SCRIPT_CHESS_MELEE_IDS,
                               SCRIPT_FIREBALL_IDS, SCRIPT_HAND_OF_MEDIVH_IDS, SCRIPT_FURY_OF_MEDIVH_IDS,
                               SCRIPT_COPY_WEAPON_IDS, SCRIPT_THROW_SHIELD_IDS, SCRIPT_HAWK_HUNTING_IDS,
                               SCRIPT_COPY_OFF_HAND_WEAPON_IDS, SCRIPT_CRYSTAL_SPIKES_IDS, SCRIPT_DENOUNCEMENT_IDS,
                               SCRIPT_GENDER_DISGUISE_IDS, SCRIPT_CONSUME_IDS, SCRIPT_POISONED_SPEAR_IDS,
                               SCRIPT_SHATTER_IDS, SCRIPT_GIGANTIC_OR_SMALL_FEAST_IDS, SCRIPT_PORTAL_IDS,
                               SCRIPT_DISCOVERY_RESEARCH_IDS, SCRIPT_UNSTABLE_ENERGY_IDS, SCRIPT_IRON_ROOTS_IDS,
                               SCRIPT_ATTUNED_TO_NATURE_IDS, SCRIPT_PLAYER_CHARGE_IDS, SCRIPT_MOUNTED_CHARGE_IDS,
                               SCRIPT_BLOCK_IDS, SCRIPT_PSYCHOSIS_IDS, SCRIPT_BRAIN_LINK_IDS, SCRIPT_WHITEOUT_IDS })
    {
        runBodies.insert(registry.Find<SpellEffectScriptGenericSite>(run[0]));
    }
    CHECK_EQ(runBodies.size(), std::size_t(31));
    for (uint32 spellId : SCRIPT_SHADOW_FLAME_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(22539));
    }
    CHECK(registry.Find<SpellEffectScriptGenericSite>(24195) == registry.Find<SpellEffectScriptGenericSite>(24194));
    for (uint32 spellId : SCRIPT_ORB_TELEPORT_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(25140));
    }
    for (uint32 spellId : SCRIPT_DECIMATE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(28374));
    }
    for (uint32 spellId : SCRIPT_CLEANSING_FLAMES_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(28859));
    }
    for (uint32 spellId : SCRIPT_CHESS_MELEE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(37142));
    }
    for (uint32 spellId : SCRIPT_FIREBALL_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(34653));
    }
    for (uint32 spellId : SCRIPT_HAND_OF_MEDIVH_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(39338));
    }
    for (uint32 spellId : SCRIPT_FURY_OF_MEDIVH_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(39341));
    }
    for (uint32 spellId : SCRIPT_COPY_WEAPON_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(41055));
    }
    for (uint32 spellId : SCRIPT_THROW_SHIELD_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(41213));
    }
    for (uint32 spellId : SCRIPT_HAWK_HUNTING_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(44323));
    }
    for (uint32 spellId : SCRIPT_COPY_OFF_HAND_WEAPON_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(45206));
    }
    for (uint32 spellId : SCRIPT_CRYSTAL_SPIKES_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(47958));
    }
    for (uint32 spellId : SCRIPT_DENOUNCEMENT_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(48724));
    }
    for (uint32 spellId : SCRIPT_GENDER_DISGUISE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(48762));
    }
    for (uint32 spellId : SCRIPT_CONSUME_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(49380));
    }
    for (uint32 spellId : SCRIPT_POISONED_SPEAR_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(50255));
    }
    for (uint32 spellId : SCRIPT_SHATTER_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(50810));
    }
    for (uint32 spellId : SCRIPT_GIGANTIC_OR_SMALL_FEAST_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(58466));
    }
    for (uint32 spellId : SCRIPT_PORTAL_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(58418));
    }
    for (uint32 spellId : SCRIPT_DISCOVERY_RESEARCH_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(60893));
    }
    for (uint32 spellId : SCRIPT_UNSTABLE_ENERGY_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(62217));
    }
    for (uint32 spellId : SCRIPT_IRON_ROOTS_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(62282));
    }
    for (uint32 spellId : SCRIPT_ATTUNED_TO_NATURE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(62524));
    }
    for (uint32 spellId : SCRIPT_PLAYER_CHARGE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(62575));
    }
    for (uint32 spellId : SCRIPT_MOUNTED_CHARGE_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(63010));
    }
    for (uint32 spellId : SCRIPT_BLOCK_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(63119));
    }
    for (uint32 spellId : SCRIPT_PSYCHOSIS_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(63795));
    }
    for (uint32 spellId : SCRIPT_BRAIN_LINK_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(63803));
    }
    for (uint32 spellId : SCRIPT_WHITEOUT_IDS)
    {
        CHECK(registry.Find<SpellEffectScriptGenericSite>(spellId) ==
              registry.Find<SpellEffectScriptGenericSite>(72034));
    }
    // The single bodies are none of the runs'; Bloodbolt, Clone Me!, Invader Taunt Trigger, Coldflame (69140),
    // Glittering Sparks, Empowering Shadows (64466), Halls of Reflection Clone, Rune of Blood, Conflagration,
    // Shattered Illusion, Pungent Blight and Cancel Resistant to Blight are left out, as they may share Fury of
    // Medivh's, Throw Shield's or Unstable Energy's address.
    for (uint32 spellId : { 8856u, 17512u, 20589u, 24320u, 24324u, 24590u, 24714u, 24717u, 24718u, 24719u, 24720u,
                            24737u, 24751u, 26004u, 26137u, 26218u, 26275u, 26465u, 26656u, 27687u, 27695u, 28352u,
                            28560u, 29395u, 29830u, 30541u, 30469u, 30769u, 30835u, 30918u, 32301u, 33676u, 35865u,
                            37431u, 37775u, 37824u, 38358u, 41126u, 42281u, 42492u, 42578u, 43365u, 43375u, 44364u,
                            44462u, 44876u, 44811u, 45141u, 45151u, 45185u, 45235u, 45260u, 45313u, 45625u, 45668u,
                            45691u, 45713u, 45714u, 45717u, 45785u, 45833u, 45892u, 45918u, 45958u, 46203u, 46289u,
                            46430u, 46642u, 47097u, 47311u, 47393u, 47615u, 47638u, 47703u, 47724u, 48590u, 48603u,
                            48810u, 48811u, 48917u, 50217u, 50218u, 50238u, 50252u, 50439u, 50630u, 50725u, 50742u,
                            50894u, 51519u, 51770u, 51864u, 51889u, 51904u, 51910u, 52479u, 52555u, 52694u, 52751u,
                            52941u, 53110u, 54182u, 54581u, 54436u, 55693u, 56072u, 57082u, 57337u, 57397u, 59317u,
                            59789u, 62042u, 62262u, 62381u, 62488u, 62552u, 62688u, 62707u, 63027u, 63122u, 63633u,
                            63667u, 63681u, 63993u, 64059u, 64069u, 64123u, 64131u, 64456u, 64467u, 64475u, 64623u,
                            64767u, 64841u, 66477u, 66545u, 66741u, 66744u, 67009u, 67547u, 67590u, 67751u, 68084u,
                            68861u, 68871u, 69048u, 69051u, 69057u, 69147u, 69377u, 69378u, 69381u, 72195u, 72219u,
                            72257u, 72705u, 72900u, 73142u })
    {
        CHECK(runBodies.count(registry.Find<SpellEffectScriptGenericSite>(spellId)) == 0);
    }

    // Eight Cleansing Flames ids are keys here and at the generic dummy-effect site, and at no third site; 28859 is
    // this site's only.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 29126u, 29135u, 29136u, 29137u, 29138u, 29139u, 46671u, 46672u })
    {
        CHECK(SharedWith(spellId, SpellEffectScriptGenericSite::Key) == SpellEffectDummyGenericSite::Key);
        CHECK(game.Find<SpellEffectDummyGenericSite>(spellId) != NULL);
    }
    CHECK(SharedWith(28859, SpellEffectScriptGenericSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(game.Find<SpellEffectDummyGenericSite>(28859) == NULL);

    // Pick Red Riding Hood is a key here and at the all-enemy-in-area targeting site, and at no third site.
    CHECK(SharedWith(30769, SpellEffectScriptGenericSite::Key) == SpellTargetAllEnemyInAreaSite::Key);
    CHECK(game.Find<SpellTargetAllEnemyInAreaSite>(30769) != NULL);

    // Signal Alliance is a key here and at the generic dummy-effect site, and at no third site.
    CHECK(SharedWith(45958, SpellEffectScriptGenericSite::Key) == SpellEffectDummyGenericSite::Key);
    CHECK(game.Find<SpellEffectDummyGenericSite>(45958) != NULL);

    // The portals to Orgrimmar and Stormwind are keys here and at the generic dummy-effect site, and at no third
    // site.
    for (uint32 spellId : SCRIPT_PORTAL_IDS)
    {
        CHECK(SharedWith(spellId, SpellEffectScriptGenericSite::Key) == SpellEffectDummyGenericSite::Key);
        CHECK(game.Find<SpellEffectDummyGenericSite>(spellId) != NULL);
    }

    // Clear Insane is a key here and at the generic aura dummy apply site, and at no third site: each site
    // finds it held by the other unless the other is named as the shared one.
    CHECK(SharedWith(63122, SpellEffectScriptGenericSite::Key) == AuraDummyApplyGenericSite::Key);
    CHECK(SharedWith(63122, AuraDummyApplyGenericSite::Key) == SpellEffectScriptGenericSite::Key);
    CHECK(game.Find<AuraDummyApplyGenericSite>(63122) != NULL);
    CHECK(HeldByAnotherSite(game, SpellEffectScriptGenericSite::Key, 63122));
    CHECK(!HeldByAnotherSite(game, SpellEffectScriptGenericSite::Key, 63122, AuraDummyApplyGenericSite::Key));
    CHECK(HeldByAnotherSite(game, AuraDummyApplyGenericSite::Key, 63122));
    CHECK(!HeldByAnotherSite(game, AuraDummyApplyGenericSite::Key, 63122, SpellEffectScriptGenericSite::Key));

    // The site has no `default:`, so an id it never held is a miss, which goes on to the database script; the members
    // stay as they were.
    ScriptMembers members;
    SpellEffectScriptContext ctx = members.Context();
    CHECK(registry.FindDefault<SpellEffectScriptGenericSite>() == NULL);
    CHECK(game.FindDefault<SpellEffectScriptGenericSite>() == NULL);
    CHECK(game.Find<SpellEffectScriptGenericSite>(12345) == NULL);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(12345, ctx).IsMiss());
    CHECK(members.Untouched());
}

TEST(SpellEffectScriptHandlers_BendingShinboneReadsTheItemTargetThroughTheContext)
{
    // Bending Shinbone ends the effect for a caster that is no player with no item target, before it casts; with an
    // item target it goes on to the cast, which finds no spell in the test's spell store and casts nothing. Both
    // leave the members as they were.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    ValuedCreature casterUnit;
    ScriptMembers members;
    members.caster = &casterUnit;
    SpellEffectScriptContext ctx = members.Context();
    CHECK(ctx.itemTarget == NULL);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(8856, ctx).IsReturn());
    CHECK(members.caster == &casterUnit);
    CHECK(members.itemTarget == NULL);

    alignas(16) static unsigned char item[16];
    members.itemTarget = reinterpret_cast<Item*>(item);         // the member, which the context reads
    CHECK(ctx.itemTarget == reinterpret_cast<Item*>(item));
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(8856, ctx).IsReturn());
    CHECK(members.caster == &casterUnit);
    CHECK(members.itemTarget == reinterpret_cast<Item*>(item));
    CHECK(members.unitTarget == NULL);
}

TEST(SpellEffectScriptHandlers_BreakKaliriEggSummonsFromTheSpellsAffectiveCasterObject)
{
    // Break Kaliri Egg summons from the spell's affective caster object, which the spell answers through a public
    // const getter. A spell whose original caster is a game object, cast by a creature in no world, finds none, so
    // the body summons nothing and ends the effect, leaving the members as they were.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    ValuedCreature casterUnit;
    Spell spell(&casterUnit, HolderSpell(), true, ObjectGuid(HIGHGUID_GAMEOBJECT, uint32(1), uint32(1)));
    Spell const& route = spell;
    CHECK(route.GetAffectiveCasterObject() == NULL);
    ScriptMembers members;
    members.caster = &casterUnit;
    SpellEffectScriptContext ctx = members.Context(&spell);
    CHECK(ctx.spell == &spell);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(29395, ctx).IsReturn());
    CHECK(members.caster == &casterUnit);
    CHECK(members.unitTarget == NULL);
    CHECK(members.itemTarget == NULL);
    CHECK(route.GetAffectiveCasterObject() == NULL);
}

namespace
{
    // A SpellEffect.dbc row in raw storage, zeroed, with the effect index set; its value names the test's seeded
    // spell row.
    alignas(SpellEffectEntry) unsigned char s_mirrorImageEffectBytes[sizeof(SpellEffectEntry)] = {};

    SpellEffectEntry const* MirrorImageEffect(uint32 effectIndex)
    {
        SpellEffectEntry* row = reinterpret_cast<SpellEffectEntry*>(s_mirrorImageEffectBytes);
        row->EffectIndex = effectIndex;
        row->EffectBasePoints = int32(HolderSpell()->ID);
        return row;
    }

    void KeepSentPacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
    }

    /// A player with its update fields allocated, on a session with no socket, in no world, so what it is sent goes
    /// to the session's sink. Its main-hand slot is emptied before it goes.
    class SessionPlayer : public Player
    {
        public:
            explicit SessionPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~SessionPlayer()
            {
                GetInventoryMgr().SlotRef(EQUIPMENT_SLOT_MAINHAND) = NULL;
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

TEST(SpellEffectScriptHandlers_TheMirrorImageAuraReadsTheOriginalCasterThroughTheContext)
{
    // The Cleansing: Your Inner Turmoil's Mirror Image Aura reads the spell's original caster, never its caster, which
    // is a creature here. With no original caster, or one that is no player, it ends the effect. With a player it
    // ends the effect at index 0, and at index 1 with an empty main hand, sending the player nothing. With a main-hand
    // weapon at index 1, the target, the same player, casts the effect's spell on it; the spell is on cooldown, so the
    // player's session is sent one cast failure for that spell. Each step leaves the members as they were.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::vector<WorldPacket> sent;
    WorldSession session(1, "mirror", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&KeepSentPacket, &sent);
    SessionPlayer player(session);
    ValuedCreature casterUnit;
    ScriptMembers members;
    members.caster = &casterUnit;
    members.unitTarget = &player;
    members.effect = MirrorImageEffect(0);
    SpellEffectScriptContext ctx = members.Context();
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(50218, ctx).IsReturn());
    CHECK(members.originalCaster == NULL);

    ValuedCreature creature;
    members.originalCaster = &creature;                         // the member, which the context reads
    CHECK(ctx.m_originalCaster == &creature);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(50218, ctx).IsReturn());

    members.originalCaster = &player;
    CHECK(ctx.m_originalCaster == &player);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(50218, ctx).IsReturn());
    members.effect = MirrorImageEffect(1);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(50218, ctx).IsReturn());
    CHECK(sent.empty());

    Item weapon;
    player.GetInventoryMgr().SlotRef(EQUIPMENT_SLOT_MAINHAND) = &weapon;
    player.GetSpellCooldownMgr().AddSpellCooldown(HolderSpell()->ID, 0, time(NULL) + 3600);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(50218, ctx).IsReturn());
    player.GetInventoryMgr().SlotRef(EQUIPMENT_SLOT_MAINHAND) = NULL;
    CHECK_EQ(sent.size(), std::size_t(1));
    if (sent.size() == 1)
    {
        uint8 castCount = 0;
        uint32 spellId = 0;
        uint8 result = 0;
        sent[0] >> castCount >> spellId >> result;
        CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_CAST_FAILED));
        CHECK_EQ(spellId, HolderSpell()->ID);
        CHECK_EQ(int(result), int(SPELL_FAILED_NOT_READY));
    }
    CHECK(members.caster == &casterUnit);
    CHECK(members.originalCaster == &player);
    CHECK(members.unitTarget == &player);
    CHECK(members.itemTarget == NULL);
    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(SpellEffectScriptHandlers_TheReverseCastBodyEndsTheEffectWithNoUnitTarget)
{
    // Character Script Effect Reverse Cast ends the effect before it reads anything else when there is no unit target.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 44455);
    SpellCastTargets targets;
    Unit* caster = NULL;
    ObjectGuid originalCasterGuid(HIGHGUID_PLAYER, uint32(7));
    Unit* originalCaster = NULL;
    Unit* target = NULL;
    Item* itemTarget = NULL;
    int32 damage = 10;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectScriptContext ctx(spell, spellInfo, targets, caster, originalCasterGuid, originalCaster, target,
                                 itemTarget, damage, effect);
    CHECK(SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptGenericSite>(44455, ctx).IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(10));
}

TEST(SpellEffectScriptHandlers_DeathGateAndEmblazonRunebladeCastTheDamagesSpell)
{
    // Death Gate and Emblazon Runeblade cast the spell the spell's damage member names, which the test's spell store
    // does not hold, so nothing is cast. Death Gate ends the effect for a target that is no death knight and, for a
    // death knight, casts and goes on to the database script. Emblazon Runeblade casts from the spell's affective
    // caster, not from the caster member: with none it ends the effect, with one it casts and goes on. Each leaves
    // the members as they were.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    ValuedCreature target;
    ScriptMembers members;
    members.unitTarget = &target;
    SpellEffectScriptContext ctx = members.Context();
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(52751, ctx).IsReturn());

    target.SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_DEATH_KNIGHT);
    CHECK_EQ(int(target.getClass()), int(CLASS_DEATH_KNIGHT));
    members.damage = 52752;                                     // the member, which the context reads
    CHECK_EQ(ctx.damage, int32(52752));
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(52751, ctx).IsContinue());
    CHECK_EQ(members.damage, int32(52752));
    CHECK(members.unitTarget == &target);

    ValuedCreature casterUnit;
    Spell noCaster(&casterUnit, HolderSpell(), true, ObjectGuid(HIGHGUID_GAMEOBJECT, uint32(1), uint32(1)));
    CHECK(noCaster.GetAffectiveCaster() == NULL);
    SpellEffectScriptContext noCasterCtx = members.Context(&noCaster);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(51770, noCasterCtx).IsReturn());

    Spell spell(&casterUnit, HolderSpell(), true);
    CHECK(spell.GetAffectiveCaster() == &casterUnit);
    SpellEffectScriptContext spellCtx = members.Context(&spell);
    CHECK(game.Dispatch<SpellEffectScriptGenericSite>(51770, spellCtx).IsContinue());
    CHECK(members.caster == NULL);
    CHECK_EQ(members.damage, int32(52752));
    CHECK(members.unitTarget == &target);
    CHECK(members.itemTarget == NULL);
}

namespace
{
    uint32 const LOCATION_IDS[] = { 22564, 22563, 23333, 23335, 34976, 2584, 42792, 43681, 22011, 22012, 24171, 44535,
                                    44521, 32724, 32725, 35774, 35775, 32727, 74410, 74411 };

    // The shape the location site takes where its switch stood: Return(value) is the check's result, and a miss goes
    // on to the check's `return SPELL_CAST_OK;`.
    SpellCastResult RunLocationSite(uint32 spellId, uint32 mapId, Player const* player)
    {
        SpellAllowedInLocationContext ctx(mapId, player);
        SpellHandlerOutcome<SpellCastResult> outcome =
            SpellHandlerRegistry::Game().Dispatch<SpellAllowedInLocationSite>(spellId, ctx);
        if (outcome.IsReturn())
        {
            return outcome.GetValue();
        }
        return SPELL_CAST_OK;
    }

    // One label run on one map: the result its body answers.
    struct LocationCase
    {
        uint32 spellId;
        uint32 mapId;
        SpellCastResult result;
    };

    // Each case answers Return with its result, the site answers the same, and the members stay as they were.
    template <std::size_t N>
    void CheckLocationCases(LocationCase const (&cases)[N], Player const* player)
    {
        SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
        for (LocationCase const& one : cases)
        {
            uint32 mapId = one.mapId;
            Player const* member = player;
            SpellAllowedInLocationContext ctx(mapId, member);
            SpellHandlerOutcome<SpellCastResult> outcome = game.Dispatch<SpellAllowedInLocationSite>(one.spellId, ctx);
            CHECK(outcome.IsReturn());
            CHECK_EQ(int(outcome.IsReturn() ? outcome.GetValue() : SPELL_CAST_OK), int(one.result));
            CHECK_EQ(int(RunLocationSite(one.spellId, one.mapId, player)), int(one.result));
            CHECK_EQ(mapId, one.mapId);
            CHECK(member == player);
        }
    }
}

TEST(SpellMgrHandlers_TheTableRegistersEveryRowOnce)
{
    // 20 labels at one site and no default: 20 rows and 20 keys, none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellMgrHandlers(registry), uint32(20));
    CHECK_EQ(registry.Count(), std::size_t(20));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellAllowedInLocationSite::Key), std::size_t(20));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellMgrHandlers(registry), uint32(20));
    CHECK_EQ(registry.Count(), std::size_t(20));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));

    // The game's table holds these with the other files' 1203 rows and 29 defaults.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(1223));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
    CHECK_EQ(game.CountAt(SpellAllowedInLocationSite::Key), std::size_t(20));
}

TEST(SpellMgrHandlers_TheLocationSiteHoldsItsTwentyLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterSpellMgrHandlers(registry);

    // The 20 labels of the switch, in 10 bodies. Arena - Dampening is one code with the arena team auras' body, so
    // the two count as one body whether the linker gives them one address or two.
    typedef SpellHandler<SpellAllowedInLocationSite>::Function LocationBody;
    std::size_t folds = registry.Find<SpellAllowedInLocationSite>(74410) ==
                        registry.Find<SpellAllowedInLocationSite>(32724) ? 1 : 0;
    CheckSiteLabels<SpellAllowedInLocationSite>(registry, LOCATION_IDS, 10 - folds);
    CHECK(registry.FindDefault<SpellAllowedInLocationSite>() == NULL);

    // Each run's labels have one body, and no two runs share one but the fold above.
    LocationBody recall = registry.Find<SpellAllowedInLocationSite>(22564);
    LocationBody flag = registry.Find<SpellAllowedInLocationSite>(23333);
    LocationBody inBattleground = registry.Find<SpellAllowedInLocationSite>(2584);
    LocationBody spiritHeal = registry.Find<SpellAllowedInLocationSite>(22011);
    LocationBody arenaTeam = registry.Find<SpellAllowedInLocationSite>(32724);
    CHECK(registry.Find<SpellAllowedInLocationSite>(22563) == recall);
    CHECK(registry.Find<SpellAllowedInLocationSite>(23335) == flag);
    CHECK(registry.Find<SpellAllowedInLocationSite>(42792) == inBattleground);
    CHECK(registry.Find<SpellAllowedInLocationSite>(43681) == inBattleground);
    for (uint32 spellId : { 22012u, 24171u, 44535u })
    {
        CHECK(registry.Find<SpellAllowedInLocationSite>(spellId) == spiritHeal);
    }
    for (uint32 spellId : { 32725u, 35774u, 35775u })
    {
        CHECK(registry.Find<SpellAllowedInLocationSite>(spellId) == arenaTeam);
    }
    std::set<LocationBody> runBodies = { recall, flag, registry.Find<SpellAllowedInLocationSite>(34976), inBattleground,
                                         spiritHeal, registry.Find<SpellAllowedInLocationSite>(44521), arenaTeam,
                                         registry.Find<SpellAllowedInLocationSite>(32727),
                                         registry.Find<SpellAllowedInLocationSite>(74411) };
    CHECK_EQ(runBodies.size(), std::size_t(9));

    // An id the switch never held is no key: 12345, and Transporter Malfunction, which the file's IsPositiveEffect
    // switches hold.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 12345u, 36893u })
    {
        CHECK(registry.Find<SpellAllowedInLocationSite>(spellId) == NULL);
        CHECK(game.Find<SpellAllowedInLocationSite>(spellId) == NULL);
        CHECK_EQ(int(RunLocationSite(spellId, 489, NULL)), int(SPELL_CAST_OK));
    }
}

TEST(SpellMgrHandlers_TheLocationContextAliasesTheMapAndThePlayer)
{
    alignas(16) static unsigned char players[2][16];
    uint32 map_id = 489;
    Player const* player = reinterpret_cast<Player const*>(players[0]);
    SpellAllowedInLocationContext ctx(map_id, player);
    CHECK(&ctx.map_id == &map_id);
    CHECK(&ctx.player == &player);
    CHECK_EQ(ctx.map_id, uint32(489));
    CHECK(ctx.player == player);

    map_id = 566;                                               // both are the function's parameters, read
    player = reinterpret_cast<Player const*>(players[1]);
    CHECK_EQ(ctx.map_id, uint32(566));
    CHECK(ctx.player == reinterpret_cast<Player const*>(players[1]));
}

TEST(SpellMgrHandlers_TheLocationBodiesAnswerForAPlayerOutsideABattleground)
{
    // A player in no battleground fails every label's check. Marked in a battleground the manager does not hold, it
    // passes the flags on their maps and the battleground-only auras, and still fails the checks that read the
    // battleground itself or an arena. No map is in the test's map store, so the spirit healer's spells fail as an
    // incorrect area.
    WorldSession session(1, "location", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    SessionPlayer player(session);
    CHECK(!player.InBattleGround());

    LocationCase const outside[] =
    {
        { 22564, 30, SPELL_FAILED_REQUIRES_AREA }, { 22563, 30, SPELL_FAILED_REQUIRES_AREA },
        { 23333, 489, SPELL_FAILED_REQUIRES_AREA }, { 23335, 489, SPELL_FAILED_REQUIRES_AREA },
        { 34976, 566, SPELL_FAILED_REQUIRES_AREA },
        { 2584, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS }, { 42792, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
        { 43681, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
        { 22011, 489, SPELL_FAILED_INCORRECT_AREA }, { 22012, 30, SPELL_FAILED_INCORRECT_AREA },
        { 24171, 566, SPELL_FAILED_INCORRECT_AREA }, { 44535, 0, SPELL_FAILED_INCORRECT_AREA },
        { 44521, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
        { 32724, 559, SPELL_FAILED_ONLY_IN_ARENA }, { 32725, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 35774, 559, SPELL_FAILED_ONLY_IN_ARENA }, { 35775, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 32727, 559, SPELL_FAILED_REQUIRES_AREA },
        { 74410, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 74411, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
    };
    CheckLocationCases(outside, &player);

    player.SetBattleGroundId(1, BATTLEGROUND_WS);
    CHECK(player.InBattleGround());
    CHECK(player.GetBattleGround() == NULL);
    LocationCase const marked[] =
    {
        { 22564, 30, SPELL_FAILED_REQUIRES_AREA }, { 22563, 30, SPELL_FAILED_REQUIRES_AREA },
        { 23333, 489, SPELL_CAST_OK }, { 23335, 489, SPELL_CAST_OK },
        { 23333, 566, SPELL_FAILED_REQUIRES_AREA }, { 23335, 30, SPELL_FAILED_REQUIRES_AREA },
        { 34976, 566, SPELL_CAST_OK }, { 34976, 489, SPELL_FAILED_REQUIRES_AREA },
        { 2584, 489, SPELL_CAST_OK }, { 42792, 566, SPELL_CAST_OK }, { 43681, 30, SPELL_CAST_OK },
        { 22011, 489, SPELL_FAILED_INCORRECT_AREA }, { 44535, 489, SPELL_FAILED_INCORRECT_AREA },
        { 44521, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
        { 32724, 559, SPELL_FAILED_ONLY_IN_ARENA }, { 35775, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 32727, 559, SPELL_FAILED_REQUIRES_AREA },
        { 74410, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 74411, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
    };
    CheckLocationCases(marked, &player);
    player.SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
}

TEST(SpellMgrHandlers_TheLocationBodiesAnswerForNoPlayer)
{
    // With no player, each label's check fails with its own result; the spirit healer's spells read only the map.
    LocationCase const none[] =
    {
        { 22564, 30, SPELL_FAILED_REQUIRES_AREA }, { 22563, 30, SPELL_FAILED_REQUIRES_AREA },
        { 23333, 489, SPELL_FAILED_REQUIRES_AREA }, { 23335, 489, SPELL_FAILED_REQUIRES_AREA },
        { 34976, 566, SPELL_FAILED_REQUIRES_AREA },
        { 2584, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS }, { 42792, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
        { 43681, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
        { 22011, 489, SPELL_FAILED_INCORRECT_AREA }, { 22012, 489, SPELL_FAILED_INCORRECT_AREA },
        { 24171, 489, SPELL_FAILED_INCORRECT_AREA }, { 44535, 489, SPELL_FAILED_INCORRECT_AREA },
        { 44521, 489, SPELL_FAILED_REQUIRES_AREA },
        { 32724, 559, SPELL_FAILED_ONLY_IN_ARENA }, { 32725, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 35774, 559, SPELL_FAILED_ONLY_IN_ARENA }, { 35775, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 32727, 559, SPELL_FAILED_REQUIRES_AREA },
        { 74410, 559, SPELL_FAILED_ONLY_IN_ARENA },
        { 74411, 489, SPELL_FAILED_ONLY_BATTLEGROUNDS },
    };
    CheckLocationCases(none, NULL);
}

namespace
{
    uint32 const SPELL_DAMAGE_TAKEN_IDS[] = { 45182, 20911, 25899, 47580, 47581, 47582 };
    uint32 const SPELL_HEALING_DONE_IDS[] = { 34246, 60779 };
    uint32 const MELEE_DAMAGE_TAKEN_IDS[] = { 45182, 20911, 25899 };

    // A dummy aura's storage with its modifier's misc value and amount set, standing in a list as a unit's auras do.
    struct BonusAura
    {
        AuraStorage storage;
        std::list<Aura*> auras;
        std::list<Aura*>::const_iterator i;

        BonusAura(int32 miscValue, int32 amount) : storage()
        {
            Aura* aura = reinterpret_cast<Aura*>(storage.bytes);
            aura->GetModifier()->m_miscvalue = miscValue;
            aura->GetModifier()->m_amount = amount;
            auras.push_back(aura);
            i = auras.begin();
        }

        BonusAura(BonusAura const&) = delete;
        BonusAura& operator=(BonusAura const&) = delete;
    };

    uint32 const BONUS_OPTIONS_DEATH = 92101;
    uint32 const BONUS_OPTIONS_OTHER = 92102;
    SpellClassOptionsEntry s_bonusOptions[2];
    alignas(SpellEntry) unsigned char s_bonusSpellBytes[3][sizeof(SpellEntry)] = {};

    // Spell.dbc rows for the Pain and Suffering body: 0 has no class options, 1 holds Shadow Word: Death's class flag,
    // 2 the flag next to it.
    SpellEntry const* BonusSpell(std::size_t slot)
    {
        static bool seeded = false;
        if (!seeded)
        {
            seeded = true;
            s_bonusOptions[0].SpellClassMask = ClassFamilyMask(UI64LIT(0x0000000200000000));
            s_bonusOptions[0].SpellClassSet = SPELLFAMILY_PRIEST;
            s_bonusOptions[1].SpellClassMask = ClassFamilyMask(UI64LIT(0x0000000100000000));
            s_bonusOptions[1].SpellClassSet = SPELLFAMILY_PRIEST;
            sSpellClassOptionsStore.SetEntry(BONUS_OPTIONS_DEATH, &s_bonusOptions[0]);
            sSpellClassOptionsStore.SetEntry(BONUS_OPTIONS_OTHER, &s_bonusOptions[1]);
        }
        static uint32 const options[3] = { 0, BONUS_OPTIONS_DEATH, BONUS_OPTIONS_OTHER };
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_bonusSpellBytes[slot]);
        row->ClassOptionsID = options[slot];
        return row;
    }

    // One damage-taken label on one aura: the unit, the spell, the aura's misc value and amount, whether the body
    // takes the loop's next aura, and the multiplier it leaves from 2.
    struct DamageTakenCase
    {
        uint32 spellId;
        Unit* unit;
        SpellEntry const* spell;
        int32 miscValue;
        int32 amount;
        bool loopContinue;
        float result;
    };

    // The spell site answers each case's outcome and multiplier, and the aura's storage is left as it was.
    template <std::size_t N>
    void CheckSpellDamageTaken(DamageTakenCase const (&cases)[N])
    {
        for (DamageTakenCase const& one : cases)
        {
            BonusAura aura(one.miscValue, one.amount);
            AuraStorage const before = aura.storage;
            float TakenTotalMod = 2.0f;
            SpellDamageTakenContext ctx(one.unit, one.spell, TakenTotalMod, aura.i);
            SpellHandlerOutcome<void> outcome =
                SpellHandlerRegistry::Game().Dispatch<SpellDamageTakenSite>(one.spellId, ctx);
            CHECK(one.loopContinue ? outcome.IsLoopContinue() : outcome.IsContinue());
            CHECK_EQ(TakenTotalMod, one.result);
            CHECK_EQ(BitsThatDiffer(aura.storage, before), 0);
        }
    }

    // The melee site answers each case's outcome and percentage, and the aura's storage is left as it was.
    template <std::size_t N>
    void CheckMeleeDamageTaken(DamageTakenCase const (&cases)[N])
    {
        for (DamageTakenCase const& one : cases)
        {
            BonusAura aura(one.miscValue, one.amount);
            AuraStorage const before = aura.storage;
            float TakenPercent = 2.0f;
            MeleeDamageTakenContext ctx(one.unit, TakenPercent, aura.i);
            SpellHandlerOutcome<void> outcome =
                SpellHandlerRegistry::Game().Dispatch<MeleeDamageTakenSite>(one.spellId, ctx);
            CHECK(one.loopContinue ? outcome.IsLoopContinue() : outcome.IsContinue());
            CHECK_EQ(TakenPercent, one.result);
            CHECK_EQ(BitsThatDiffer(aura.storage, before), 0);
        }
    }

    // The shape the spell site takes where its switch stood, over auras whose ids are given beside them: each aura
    // dispatches, and a LoopContinue goes on to the next aura as the end of the loop body does.
    float RunSpellDamageTakenSite(Unit* unit, SpellEntry const* spellProto, uint32 const* ids,
                                  std::list<Aura*> const& auras)
    {
        float TakenTotalMod = 1.0f;
        uint32 const* id = ids;
        for (std::list<Aura*>::const_iterator i = auras.begin(); i != auras.end(); ++i, ++id)
        {
            SpellDamageTakenContext ctx(unit, spellProto, TakenTotalMod, i);
            if (SpellHandlerRegistry::Game().Dispatch<SpellDamageTakenSite>(*id, ctx).IsLoopContinue())
            {
                continue;
            }
        }
        return TakenTotalMod;
    }
}

TEST(UnitSpellBonusHandlers_TheTableRegistersEveryRowOnce)
{
    // 6, 2 and 3 labels at three sites and no default: 11 rows and 11 keys, none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterUnitSpellBonusHandlers(registry), uint32(11));
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellDamageTakenSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(SpellHealingDoneSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(MeleeDamageTakenSite::Key), std::size_t(3));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterUnitSpellBonusHandlers(registry), uint32(11));
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));

    // The game's table holds these with the other files' 1212 rows and 29 defaults.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(1223));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
    CHECK_EQ(game.CountAt(SpellDamageTakenSite::Key), std::size_t(6));
    CHECK_EQ(game.CountAt(SpellHealingDoneSite::Key), std::size_t(2));
    CHECK_EQ(game.CountAt(MeleeDamageTakenSite::Key), std::size_t(3));
}

TEST(UnitSpellBonusHandlers_TheThreeSitesHoldTheirLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterUnitSpellBonusHandlers(registry);

    // The spell site's 6 labels in 3 bodies, the healing site's 2 in 1, the melee site's 3 in 2; no default.
    CheckSiteLabels<SpellDamageTakenSite>(registry, SPELL_DAMAGE_TAKEN_IDS, 3);
    CheckSiteLabels<SpellHealingDoneSite>(registry, SPELL_HEALING_DONE_IDS, 1);
    CheckSiteLabels<MeleeDamageTakenSite>(registry, MELEE_DAMAGE_TAKEN_IDS, 2);
    CHECK(registry.FindDefault<SpellDamageTakenSite>() == NULL);
    CHECK(registry.FindDefault<SpellHealingDoneSite>() == NULL);
    CHECK(registry.FindDefault<MeleeDamageTakenSite>() == NULL);

    // Each run's labels have one body.
    CHECK(registry.Find<SpellDamageTakenSite>(25899) == registry.Find<SpellDamageTakenSite>(20911));
    CHECK(registry.Find<SpellDamageTakenSite>(47581) == registry.Find<SpellDamageTakenSite>(47580));
    CHECK(registry.Find<SpellDamageTakenSite>(47582) == registry.Find<SpellDamageTakenSite>(47580));
    CHECK(registry.Find<SpellHealingDoneSite>(60779) == registry.Find<SpellHealingDoneSite>(34246));
    CHECK(registry.Find<MeleeDamageTakenSite>(25899) == registry.Find<MeleeDamageTakenSite>(20911));

    // The three ids both damage-taken sites hold are keys at those two and no other, Sanctuary and 25899 at the
    // paladin dummy proc site too.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : MELEE_DAMAGE_TAKEN_IDS)
    {
        CHECK(HeldByAnotherSite(game, SpellDamageTakenSite::Key, spellId));
        CHECK(!HeldByAnotherSite(game, SpellDamageTakenSite::Key, spellId, MeleeDamageTakenSite::Key));
        CHECK(HeldByAnotherSite(game, MeleeDamageTakenSite::Key, spellId));
        CHECK(!HeldByAnotherSite(game, MeleeDamageTakenSite::Key, spellId, SpellDamageTakenSite::Key));
    }

    // Given any partner, every site the id's entry lists is skipped; a site the entry does not list still counts.
    CHECK(!HeldByAnotherSite(game, SpellDamageTakenSite::Key, 20911, SpellHealingDoneSite::Key));
    SpellHandlerRegistry unlisted;
    CHECK(unlisted.Register<SpellDamageTakenSite>(20911, [](SpellDamageTakenContext&)
    {
        return SpellHandlerOutcome<void>::Continue();
    }));
    CHECK(unlisted.Register<MeleeDamageTakenSite>(20911, [](MeleeDamageTakenContext&)
    {
        return SpellHandlerOutcome<void>::Continue();
    }));
    CHECK(!HeldByAnotherSite(unlisted, SpellDamageTakenSite::Key, 20911, MeleeDamageTakenSite::Key));
    CHECK(unlisted.Register<SpellHealingDoneSite>(20911, [](SpellHealingDoneContext&)
    {
        return SpellHandlerOutcome<void>::Continue();
    }));
    CHECK(HeldByAnotherSite(unlisted, SpellDamageTakenSite::Key, 20911, MeleeDamageTakenSite::Key));

    // An id the switch never held answers Miss and leaves the multiplier: 12345, Earth Shield's dummy aura, and the
    // labels of the other two sites.
    BonusAura aura(SPELL_SCHOOL_MASK_NORMAL, 100);
    float mod = 1.0f;
    int32 total = 5;
    SpellDamageTakenContext spellCtx(NULL, BonusSpell(1), mod, aura.i);
    SpellHealingDoneContext healCtx(total, aura.i);
    MeleeDamageTakenContext meleeCtx(NULL, mod, aura.i);
    for (uint32 spellId : { 12345u, 66063u, 34246u })
    {
        CHECK(game.Dispatch<SpellDamageTakenSite>(spellId, spellCtx).IsMiss());
        CHECK(game.Dispatch<MeleeDamageTakenSite>(spellId, meleeCtx).IsMiss());
    }
    for (uint32 spellId : { 12345u, 66063u, 45182u, 20911u, 47580u })
    {
        CHECK(game.Dispatch<SpellHealingDoneSite>(spellId, healCtx).IsMiss());
    }
    CHECK(game.Dispatch<MeleeDamageTakenSite>(47580, meleeCtx).IsMiss());
    CHECK_EQ(mod, 1.0f);
    CHECK_EQ(total, int32(5));
}

TEST(UnitSpellBonusHandlers_TheContextsAliasTheirLocals)
{
    ValuedCreature creature;
    Unit* self = &creature;
    std::list<Aura*> auras;
    auras.push_back(FakeAura(0));
    auras.push_back(FakeAura(1));
    std::list<Aura*>::const_iterator i = auras.begin();

    // The spell site: `this`, the spell parameter, the multiplier and the loop's iterator, each by reference.
    SpellEntry const* spellProto = BonusSpell(0);
    float TakenTotalMod = 1.0f;
    SpellDamageTakenContext spellCtx(self, spellProto, TakenTotalMod, i);
    CHECK(spellCtx.unit == self);
    CHECK(&spellCtx.spellProto == &spellProto);
    CHECK(&spellCtx.TakenTotalMod == &TakenTotalMod);
    CHECK(&spellCtx.i == &i);
    spellCtx.TakenTotalMod = 0.5f;                              // a body's write to the multiplier...
    CHECK_EQ(TakenTotalMod, 0.5f);                              // ...is the function's local
    spellProto = BonusSpell(1);                                 // the parameter is read where it stands
    CHECK(spellCtx.spellProto == BonusSpell(1));

    // The healing site: the flat bonus and the iterator.
    int32 DoneTotal = 3;
    SpellHealingDoneContext healCtx(DoneTotal, i);
    CHECK(&healCtx.DoneTotal == &DoneTotal);
    CHECK(&healCtx.i == &i);
    healCtx.DoneTotal += 4;
    CHECK_EQ(DoneTotal, int32(7));

    // The melee site: `this`, the percentage and the iterator.
    float TakenPercent = 1.0f;
    MeleeDamageTakenContext meleeCtx(self, TakenPercent, i);
    CHECK(meleeCtx.unit == self);
    CHECK(&meleeCtx.TakenPercent == &TakenPercent);
    CHECK(&meleeCtx.i == &i);
    meleeCtx.TakenPercent = 0.25f;
    CHECK_EQ(TakenPercent, 0.25f);

    // The loop's step moves the aura every context reads.
    ++i;
    CHECK(*spellCtx.i == FakeAura(1));
    CHECK(*healCtx.i == FakeAura(1));
    CHECK(*meleeCtx.i == FakeAura(1));
}

TEST(UnitSpellBonusHandlers_TheDamageTakenBodiesScaleByTheAurasAmount)
{
    ValuedCreature creature;
    StandingPlayerShape player;
    SpellEntry const* noOptions = BonusSpell(0);
    SpellEntry const* death = BonusSpell(1);
    SpellEntry const* other = BonusSpell(2);
    int32 const physical = SPELL_SCHOOL_MASK_NORMAL;
    int32 const fire = SPELL_SCHOOL_MASK_FIRE;

    // Cheating Death scales a player's multiplier for a physical aura; a creature takes the loop's next aura, and a
    // magic aura leaves it for either. Sanctuary scales for any unit. Pain and Suffering scales only for a spell with
    // Shadow Word: Death's class flag.
    DamageTakenCase const spellCases[] =
    {
        { 45182, &player, noOptions, physical, -50, false, 1.0f },
        { 45182, &player, noOptions, fire, -50, false, 2.0f },
        { 45182, &creature, noOptions, physical, -50, true, 2.0f },
        { 45182, &creature, noOptions, fire, -50, false, 2.0f },
        { 20911, &creature, noOptions, 0, -75, false, 0.5f },
        { 25899, &player, noOptions, 0, 100, false, 4.0f },
        { 47580, &creature, death, 0, 100, false, 4.0f },
        { 47581, &creature, death, 0, -50, false, 1.0f },
        { 47582, &player, death, 0, -75, false, 0.5f },
        { 47580, &creature, noOptions, 0, 100, false, 2.0f },
        { 47581, &creature, other, 0, 100, false, 2.0f },
    };
    CheckSpellDamageTaken(spellCases);

    // The melee site's labels read the same, with no spell.
    DamageTakenCase const meleeCases[] =
    {
        { 45182, &player, NULL, physical, -50, false, 1.0f },
        { 45182, &player, NULL, fire, -50, false, 2.0f },
        { 45182, &creature, NULL, physical, -50, true, 2.0f },
        { 45182, &creature, NULL, fire, -50, false, 2.0f },
        { 20911, &creature, NULL, 0, -75, false, 0.5f },
        { 25899, &player, NULL, 0, 100, false, 4.0f },
    };
    CheckMeleeDamageTaken(meleeCases);

    // Where the switch stood, a creature's Cheating Death goes on to the next aura, which still scales.
    BonusAura cheating(physical, -50);
    BonusAura sanctuary(0, -75);
    std::list<Aura*> auras;
    auras.push_back(*cheating.i);
    auras.push_back(*sanctuary.i);
    uint32 const ids[] = { 45182, 20911 };
    CHECK_EQ(RunSpellDamageTakenSite(&creature, noOptions, ids, auras), 0.25f);
    CHECK_EQ(RunSpellDamageTakenSite(&player, noOptions, ids, auras), 0.125f);
}

TEST(UnitSpellBonusHandlers_TheLifebloomIdolsAddASeventhOfTheAmount)
{
    // Each idol adds its aura's amount divided by 7, in whole numbers, to the flat bonus; the aura is left as it was.
    struct HealingCase
    {
        uint32 spellId;
        int32 amount;
        int32 result;
    };
    HealingCase const cases[] =
    {
        { 34246, 70, 20 }, { 60779, 70, 20 }, { 34246, 13, 11 }, { 60779, 6, 10 }, { 34246, -14, 8 },
    };
    for (HealingCase const& one : cases)
    {
        BonusAura aura(0, one.amount);
        AuraStorage const before = aura.storage;
        int32 DoneTotal = 10;
        SpellHealingDoneContext ctx(DoneTotal, aura.i);
        CHECK(SpellHandlerRegistry::Game().Dispatch<SpellHealingDoneSite>(one.spellId, ctx).IsContinue());
        CHECK_EQ(DoneTotal, one.result);
        CHECK_EQ(BitsThatDiffer(aura.storage, before), 0);
    }
}

namespace
{
    uint32 const DUMMY_PROC_GENERIC_IDS[] = { 9799, 25988, 18765, 35429, 21063, 24658, 24661, 28764, 27539, 28305,
                                              31871, 31872, 33493, 38196, 39446, 45481, 45482, 45483, 45484, 46569,
                                              48504, 52723, 60501, 57989, 58914, 63305, 63320, 65932, 66063, 66725,
                                              68161, 66808, 68160, 69739, 69755, 71903, 72178 };
    uint32 const DUMMY_PROC_MAGE_IDS[] = { 11119, 11120, 12846, 12847, 12848, 56372, 56375, 64411, 74396 };
    uint32 const DUMMY_PROC_WARLOCK_IDS[] = { 18094, 18095, 56218, 30293, 30295, 30296, 37377, 37381, 39437, 63108 };
    uint32 const DUMMY_PROC_PRIEST_IDS[] = { 15286, 40438, 26169, 39372, 28809, 55677, 55680, 70770 };
    uint32 const DUMMY_PROC_DRUID_IDS[] = { 24932, 28719, 28847, 37288, 37295, 40442, 44835, 54754, 54821, 70664,
                                            70723 };
    uint32 const DUMMY_PROC_ROGUE_IDS[] = { 23582, 32748, 57934, 76806, 76838 };
    uint32 const DUMMY_PROC_PALADIN_IDS[] = { 20185, 20186, 20335, 20336, 20337, 20911, 25899, 28789, 31785, 33776,
                                              31801, 31876, 31877, 31878, 40470, 53651, 53736, 54937, 58597, 53601,
                                              64890, 71406, 71545, 71880, 76669, 71892 };
    uint32 const DUMMY_PROC_SHAMAN_IDS[] = { 28823, 28849, 33757, 40463, 55440, 58877, 63280, 64928, 67228, 70808 };

    // Spell rows with only their id and school mask set, for the bodies that read the proc spell or the aura's spell.
    alignas(SpellEntry) unsigned char s_procSpellBytes[2][sizeof(SpellEntry)] = {};

    SpellEntry const* ProcSpellRow(std::size_t slot, uint32 id, uint32 schoolMask)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_procSpellBytes[slot]);
        row->ID = id;
        row->SchoolMask = schoolMask;
        return row;
    }

    alignas(16) unsigned char s_procVictim[16];

    // A victim the bodies under test pass on as the proc's target and never dereference.
    Unit* ProcVictim()
    {
        return reinterpret_cast<Unit*>(s_procVictim);
    }

    // `Unit::HandleDummyAuraProc`'s parameters and locals as its dispatch binds them, with the context over them for
    // `unit`: the damage is 1000, the aura's amount 15, the target the victim and nothing triggered yet.
    struct DummyProcLocals
    {
        Unit* pVictim;
        uint32 damage;
        Aura* triggeredByAura;
        SpellEntry const* procSpell;
        uint32 procFlag;
        uint32 cooldown;
        SpellEntry const* dummySpell;
        SpellEffectIndex effIndex;
        SpellClassOptionsEntry const* procClassOptions;
        int32 triggerAmount;
        Item* castItem;
        uint32 triggered_spell_id;
        Unit* target;
        int32 basepoints[MAX_EFFECT_INDEX];
        SpellCooldownMgr cooldowns;
        DummyAuraProcContext ctx;

        explicit DummyProcLocals(Unit* unit)
            : pVictim(ProcVictim()), damage(1000), triggeredByAura(NULL), procSpell(NULL), procFlag(0), cooldown(0),
              dummySpell(NULL), effIndex(EFFECT_INDEX_0), procClassOptions(NULL), triggerAmount(15), castItem(NULL),
              triggered_spell_id(0), target(pVictim), basepoints(), cooldowns(),
              ctx(unit, cooldowns, pVictim, damage, triggeredByAura, procSpell, procFlag, cooldown, dummySpell,
                  effIndex, procClassOptions, triggerAmount, castItem, triggered_spell_id, target, basepoints)
        {
        }

        DummyProcLocals(DummyProcLocals const&) = delete;
        DummyProcLocals& operator=(DummyProcLocals const&) = delete;

        bool BasePointsAre(int32 first) const
        {
            return basepoints[0] == first && basepoints[1] == 0 && basepoints[2] == 0;
        }
    };

    /// A creature that hands out its own spell cooldowns, the member a proc dispatch binds.
    class CooldownCreature : public ValuedCreature
    {
        public:
            SpellCooldownMgr& Cooldowns()
            {
                return m_spellCooldownMgr;
            }
    };

    // One label that only sets what the proc triggers: the site, the id, the triggered spell, and whether the body
    // also makes the unit the target.
    struct TriggerCase
    {
        uint32 site;
        uint32 spellId;
        uint32 triggered;
        bool selfTarget;
        int32 basepoint;
    };

    SpellHandlerOutcome<SpellAuraProcResult> DispatchDummyProc(uint32 site, uint32 spellId, DummyAuraProcContext& ctx)
    {
        SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
        if (site == DummyAuraProcGenericSite::Key)
        {
            return game.Dispatch<DummyAuraProcGenericSite>(spellId, ctx);
        }
        if (site == DummyAuraProcMageSite::Key)
        {
            return game.Dispatch<DummyAuraProcMageSite>(spellId, ctx);
        }
        if (site == DummyAuraProcWarlockSite::Key)
        {
            return game.Dispatch<DummyAuraProcWarlockSite>(spellId, ctx);
        }
        if (site == DummyAuraProcPriestSite::Key)
        {
            return game.Dispatch<DummyAuraProcPriestSite>(spellId, ctx);
        }
        if (site == DummyAuraProcDruidSite::Key)
        {
            return game.Dispatch<DummyAuraProcDruidSite>(spellId, ctx);
        }
        if (site == DummyAuraProcRogueSite::Key)
        {
            return game.Dispatch<DummyAuraProcRogueSite>(spellId, ctx);
        }
        if (site == DummyAuraProcPaladinSite::Key)
        {
            return game.Dispatch<DummyAuraProcPaladinSite>(spellId, ctx);
        }
        return game.Dispatch<DummyAuraProcShamanSite>(spellId, ctx);
    }
}

TEST(UnitAuraProcHandlerHandlers_TheTableRegistersEveryRowOnce)
{
    // 37, 9, 10, 8, 11, 5, 26 and 10 labels at the eight dummy proc sites, 2, 1, 16 and 20 at the haste, crit-chance
    // and two proc-trigger sites, and no default: 155 rows and 155 keys, none twice.
    std::size_t const perSite[] = { 37, 9, 10, 8, 11, 5, 26, 10, 2, 1, 16, 20 };
    uint32 const siteKeys[] = { DummyAuraProcGenericSite::Key, DummyAuraProcMageSite::Key,
                                DummyAuraProcWarlockSite::Key, DummyAuraProcPriestSite::Key,
                                DummyAuraProcDruidSite::Key, DummyAuraProcRogueSite::Key,
                                DummyAuraProcPaladinSite::Key, DummyAuraProcShamanSite::Key,
                                HasteAuraProcRogueSite::Key, SpellCritChanceAuraProcMageSite::Key,
                                ProcTriggerSpellAuraProcGenericSite::Key, ProcTriggerSpellAuraProcTriggeredSite::Key };
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterUnitAuraProcHandlerHandlers(registry), uint32(155));
    CHECK_EQ(registry.Count(), std::size_t(155));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    for (std::size_t k = 0; k < 12; ++k)
    {
        CHECK_EQ(registry.CountAt(siteKeys[k]), perSite[k]);
    }

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterUnitAuraProcHandlerHandlers(registry), uint32(155));
    CHECK_EQ(registry.Count(), std::size_t(155));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));

    // The game's table holds these with the other files' 1068 rows and 29 defaults.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(1223));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
    for (std::size_t k = 0; k < 12; ++k)
    {
        CHECK_EQ(game.CountAt(siteKeys[k]), perSite[k]);
    }
}

TEST(UnitAuraProcHandlerHandlers_TheThreeDummySitesHoldTheirLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterUnitAuraProcHandlerHandlers(registry);

    // The generic site's 37 labels in 31 bodies, the mage site's 9 in 5, the warlock site's 10 in 6; no default.
    CheckSiteLabels<DummyAuraProcGenericSite>(registry, DUMMY_PROC_GENERIC_IDS, 31);
    CheckSiteLabels<DummyAuraProcMageSite>(registry, DUMMY_PROC_MAGE_IDS, 5);
    CheckSiteLabels<DummyAuraProcWarlockSite>(registry, DUMMY_PROC_WARLOCK_IDS, 6);
    CHECK(registry.FindDefault<DummyAuraProcGenericSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcMageSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcWarlockSite>() == NULL);

    // Each run's labels have one body.
    CHECK(registry.Find<DummyAuraProcGenericSite>(25988) == registry.Find<DummyAuraProcGenericSite>(9799));
    CHECK(registry.Find<DummyAuraProcGenericSite>(35429) == registry.Find<DummyAuraProcGenericSite>(18765));
    CHECK(registry.Find<DummyAuraProcGenericSite>(31872) == registry.Find<DummyAuraProcGenericSite>(31871));
    CHECK(registry.Find<DummyAuraProcGenericSite>(60501) == registry.Find<DummyAuraProcGenericSite>(52723));
    CHECK(registry.Find<DummyAuraProcGenericSite>(68161) == registry.Find<DummyAuraProcGenericSite>(66725));
    CHECK(registry.Find<DummyAuraProcGenericSite>(68160) == registry.Find<DummyAuraProcGenericSite>(66808));
    CHECK(registry.Find<DummyAuraProcGenericSite>(63305) != registry.Find<DummyAuraProcGenericSite>(58914));
    for (uint32 spellId : { 11120u, 12846u, 12847u, 12848u })
    {
        CHECK(registry.Find<DummyAuraProcMageSite>(spellId) == registry.Find<DummyAuraProcMageSite>(11119));
    }
    CHECK(registry.Find<DummyAuraProcWarlockSite>(18095) == registry.Find<DummyAuraProcWarlockSite>(18094));
    CHECK(registry.Find<DummyAuraProcWarlockSite>(56218) == registry.Find<DummyAuraProcWarlockSite>(18094));
    CHECK(registry.Find<DummyAuraProcWarlockSite>(30295) == registry.Find<DummyAuraProcWarlockSite>(30293));
    CHECK(registry.Find<DummyAuraProcWarlockSite>(30296) == registry.Find<DummyAuraProcWarlockSite>(30293));

    // The four ids another registered site holds too are keys at those two and no other: Unstable Power and Restless
    // Strength at the generic aura dummy apply-and-remove site, 58914 at the generic boost site, 74396 at the mage
    // boost site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    struct Shared { uint32 spellId; uint32 site; uint32 other; };
    Shared const shared[] =
    {
        { 24658, DummyAuraProcGenericSite::Key, AuraDummyApplyRemoveGenericSite::Key },
        { 24661, DummyAuraProcGenericSite::Key, AuraDummyApplyRemoveGenericSite::Key },
        { 58914, DummyAuraProcGenericSite::Key, AuraBoostGenericSite::Key },
        { 74396, DummyAuraProcMageSite::Key, AuraBoostMageSite::Key },
    };
    for (Shared const& id : shared)
    {
        CHECK(SharedWith(id.spellId, id.site) == id.other);
        CHECK(SharedWith(id.spellId, id.other) == id.site);
        CHECK(HeldByAnotherSite(game, id.site, id.spellId));
        CHECK(!HeldByAnotherSite(game, id.site, id.spellId, id.other));
        CHECK(HeldByAnotherSite(game, id.other, id.spellId));
        CHECK(!HeldByAnotherSite(game, id.other, id.spellId, id.site));
    }
    CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(24658) != NULL);
    CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(24661) != NULL);
    CHECK(game.Find<AuraBoostGenericSite>(58914) != NULL);
    CHECK(game.Find<AuraBoostMageSite>(74396) != NULL);

    // An id the switch never held answers Miss and leaves the locals: 12345, the other two sites' labels, and a priest
    // label (15286, Vampiric Embrace).
    DummyProcLocals locals(NULL);
    for (uint32 spellId : { 12345u, 11119u, 18094u, 15286u })
    {
        CHECK(game.Dispatch<DummyAuraProcGenericSite>(spellId, locals.ctx).IsMiss());
    }
    for (uint32 spellId : { 12345u, 66063u, 18094u, 15286u })
    {
        CHECK(game.Dispatch<DummyAuraProcMageSite>(spellId, locals.ctx).IsMiss());
    }
    for (uint32 spellId : { 12345u, 66063u, 11119u, 15286u })
    {
        CHECK(game.Dispatch<DummyAuraProcWarlockSite>(spellId, locals.ctx).IsMiss());
    }
    CHECK_EQ(locals.triggered_spell_id, uint32(0));
    CHECK(locals.target == ProcVictim());
    CHECK(locals.BasePointsAre(0));
    CHECK_EQ(locals.triggerAmount, int32(15));
}

TEST(UnitAuraProcHandlerHandlers_TheContextAliasesTheFunctionsLocals)
{
    CooldownCreature creature;
    DummyProcLocals locals(&creature);
    DummyAuraProcContext& ctx = locals.ctx;

    // The dispatch binds the unit's own cooldowns: the context holds the creature's member, and a cooldown set
    // through it is the creature's.
    DummyAuraProcContext bound(&creature, creature.Cooldowns(), locals.pVictim, locals.damage, locals.triggeredByAura,
                               locals.procSpell, locals.procFlag, locals.cooldown, locals.dummySpell, locals.effIndex,
                               locals.procClassOptions, locals.triggerAmount, locals.castItem,
                               locals.triggered_spell_id, locals.target, locals.basepoints);
    CHECK(bound.unit == &creature);
    CHECK(&bound.m_spellCooldownMgr == &creature.Cooldowns());
    time_t now = time(NULL);
    CHECK(!creature.Cooldowns().HasSpellCooldown(66064, now));
    bound.m_spellCooldownMgr.AddSpellCooldown(66064, 0, now + 60);
    CHECK(creature.Cooldowns().HasSpellCooldown(66064, now));

    // `unit` is the pointer passed for `this`; every other member is the local's or parameter's address.
    CHECK(ctx.unit == &creature);
    CHECK(&ctx.m_spellCooldownMgr == &locals.cooldowns);
    CHECK(&ctx.pVictim == &locals.pVictim);
    CHECK(&ctx.damage == &locals.damage);
    CHECK(&ctx.triggeredByAura == &locals.triggeredByAura);
    CHECK(&ctx.procSpell == &locals.procSpell);
    CHECK(&ctx.procFlag == &locals.procFlag);
    CHECK(&ctx.cooldown == &locals.cooldown);
    CHECK(&ctx.dummySpell == &locals.dummySpell);
    CHECK(&ctx.effIndex == &locals.effIndex);
    CHECK(&ctx.procClassOptions == &locals.procClassOptions);
    CHECK(&ctx.triggerAmount == &locals.triggerAmount);
    CHECK(&ctx.castItem == &locals.castItem);
    CHECK(&ctx.triggered_spell_id == &locals.triggered_spell_id);
    CHECK(&ctx.target == &locals.target);
    CHECK(&ctx.basepoints == &locals.basepoints);

    // A body's writes reach the function's locals, and the parameters are read where they stand.
    ctx.triggered_spell_id = 66064;
    ctx.target = &creature;
    ctx.basepoints[EFFECT_INDEX_2] = 7;
    ctx.triggerAmount += 5;
    CHECK_EQ(locals.triggered_spell_id, uint32(66064));
    CHECK(locals.target == &creature);
    CHECK_EQ(locals.basepoints[EFFECT_INDEX_2], int32(7));
    CHECK_EQ(locals.triggerAmount, int32(20));
    locals.damage = 250;
    locals.procFlag = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
    CHECK_EQ(ctx.damage, uint32(250));
    CHECK_EQ(ctx.procFlag, uint32(PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL));
}

TEST(UnitAuraProcHandlerHandlers_TheTriggerLabelsSetWhatTheProcCasts)
{
    // Each body sets the triggered spell and goes on to the cast after the switch; one that makes the unit the target
    // writes the context's unit, here NULL, over the victim, and one with base points writes the first.
    TriggerCase const cases[] =
    {
        { DummyAuraProcGenericSite::Key, 21063, 21064, false, 0 },
        { DummyAuraProcGenericSite::Key, 63320, 63321, false, 0 },
        { DummyAuraProcGenericSite::Key, 65932, 65934, false, 0 },
        { DummyAuraProcGenericSite::Key, 66063, 66064, false, 0 },
        { DummyAuraProcGenericSite::Key, 66725, 66765, false, 0 },
        { DummyAuraProcGenericSite::Key, 68161, 66765, false, 0 },
        { DummyAuraProcGenericSite::Key, 66808, 66809, false, 0 },
        { DummyAuraProcGenericSite::Key, 68160, 66809, false, 0 },
        { DummyAuraProcGenericSite::Key, 72178, 72195, false, 0 },
        { DummyAuraProcGenericSite::Key, 38196, 31285, true, 3000 },     // three times the damage
        { DummyAuraProcGenericSite::Key, 48504, 48503, true, 15 },       // the aura's amount
        { DummyAuraProcGenericSite::Key, 52723, 52724, true, 500 },      // half the damage
        { DummyAuraProcGenericSite::Key, 60501, 52724, true, 500 },
        { DummyAuraProcWarlockSite::Key, 18094, 17941, true, 0 },
        { DummyAuraProcWarlockSite::Key, 18095, 17941, true, 0 },
        { DummyAuraProcWarlockSite::Key, 56218, 17941, true, 0 },
        { DummyAuraProcWarlockSite::Key, 30293, 30294, true, 150 },      // the aura's percent of the damage
        { DummyAuraProcWarlockSite::Key, 30295, 30294, true, 150 },
        { DummyAuraProcWarlockSite::Key, 30296, 30294, true, 150 },
        { DummyAuraProcWarlockSite::Key, 37377, 37379, false, 0 },
        { DummyAuraProcWarlockSite::Key, 39437, 37378, false, 0 },
    };
    for (TriggerCase const& c : cases)
    {
        DummyProcLocals locals(NULL);
        CHECK(DispatchDummyProc(c.site, c.spellId, locals.ctx).IsContinue());
        CHECK_EQ(locals.triggered_spell_id, c.triggered);
        CHECK(locals.target == (c.selfTarget ? NULL : ProcVictim()));
        CHECK(locals.BasePointsAre(c.basepoint));
        CHECK_EQ(locals.triggerAmount, int32(15));
    }

    // The two shards trigger the heal after a positive spell, else the damage.
    uint32 const shards[][3] = { { 69739, 69734, 69730 }, { 69755, 69733, 69729 } };
    for (uint32 const (&shard)[3] : shards)
    {
        DummyProcLocals positive(NULL);
        positive.procFlag = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
        CHECK(DispatchDummyProc(DummyAuraProcGenericSite::Key, shard[0], positive.ctx).IsContinue());
        CHECK_EQ(positive.triggered_spell_id, shard[1]);
        DummyProcLocals other(NULL);
        other.procFlag = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL ^ 0xFFFFFFFF;
        CHECK(DispatchDummyProc(DummyAuraProcGenericSite::Key, shard[0], other.ctx).IsContinue());
        CHECK_EQ(other.triggered_spell_id, shard[2]);
        CHECK(other.target == ProcVictim());
    }
}

TEST(UnitAuraProcHandlerHandlers_IgniteTakesItsRanksShareOfTheDamage)
{
    // The aura's spell picks the share; each rank triggers 12654 on the victim and goes on to the cast.
    uint32 const ranks[][2] = { { 11119, 40 }, { 11120, 80 }, { 12846, 120 }, { 12847, 160 }, { 12848, 200 } };
    for (uint32 const (&rank)[2] : ranks)
    {
        DummyProcLocals locals(NULL);
        locals.dummySpell = ProcSpellRow(0, rank[0], SPELL_SCHOOL_MASK_FIRE);
        CHECK(DispatchDummyProc(DummyAuraProcMageSite::Key, rank[0], locals.ctx).IsContinue());
        CHECK(locals.BasePointsAre(int32(rank[1])));
        CHECK_EQ(locals.triggered_spell_id, uint32(12654));
        CHECK(locals.target == ProcVictim());
    }

    // An aura's spell that is no rank fails the proc from inside the body's own switch, triggering nothing.
    DummyProcLocals locals(NULL);
    locals.dummySpell = ProcSpellRow(0, 99999, SPELL_SCHOOL_MASK_FIRE);
    SpellHandlerOutcome<SpellAuraProcResult> outcome = DispatchDummyProc(DummyAuraProcMageSite::Key, 11119, locals.ctx);
    CHECK(outcome.IsReturn());
    CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
    CHECK_EQ(locals.triggered_spell_id, uint32(0));
    CHECK(locals.BasePointsAre(0));
}

TEST(UnitAuraProcHandlerHandlers_TheGuardsEndTheProcBeforeTheUnitIsRead)
{
    // Each case fails the proc before its body reads the unit, and leaves the trigger, target and base points.
    struct GuardCase { uint32 site; uint32 spellId; uint32 procSpellId; uint32 procSchool; bool victim; };
    GuardCase const cases[] =
    {
        { DummyAuraProcGenericSite::Key, 46569, 0, 0, true },                          // always fails
        { DummyAuraProcGenericSite::Key, 18765, 26654, SPELL_SCHOOL_MASK_NORMAL, true },  // its own trigger procs
        { DummyAuraProcGenericSite::Key, 35429, 26654, SPELL_SCHOOL_MASK_NORMAL, true },
        { DummyAuraProcGenericSite::Key, 24658, 0, 0, true },                          // no proc spell
        { DummyAuraProcGenericSite::Key, 24658, 24659, SPELL_SCHOOL_MASK_NORMAL, true },  // its own stack procs
        { DummyAuraProcGenericSite::Key, 28764, 0, 0, true },                          // no proc spell
        { DummyAuraProcGenericSite::Key, 27539, 0, 0, true },                          // no proc spell
        { DummyAuraProcGenericSite::Key, 27539, 78, SPELL_SCHOOL_MASK_NORMAL, true },     // a physical proc spell
        { DummyAuraProcMageSite::Key, 56375, 0, 0, false },                            // no victim
    };
    for (GuardCase const& c : cases)
    {
        DummyProcLocals locals(NULL);
        if (c.procSpellId)
        {
            locals.procSpell = ProcSpellRow(1, c.procSpellId, c.procSchool);
        }
        if (!c.victim)
        {
            locals.pVictim = NULL;
            locals.target = NULL;
        }
        SpellHandlerOutcome<SpellAuraProcResult> outcome = DispatchDummyProc(c.site, c.spellId, locals.ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
        CHECK_EQ(locals.triggered_spell_id, uint32(0));
        CHECK(locals.target == (c.victim ? ProcVictim() : NULL));
        CHECK(locals.BasePointsAre(0));
    }

    // Obsidian Armor triggers the resistance of the proc spell's school on the unit: holy is 27536.
    DummyProcLocals holy(NULL);
    holy.procSpell = ProcSpellRow(1, 635, SPELL_SCHOOL_MASK_HOLY);
    CHECK(DispatchDummyProc(DummyAuraProcGenericSite::Key, 27539, holy.ctx).IsContinue());
    CHECK_EQ(holy.triggered_spell_id, uint32(27536));
    CHECK(holy.target == NULL);
}

TEST(UnitAuraProcHandlerHandlers_TheFiveFamilySitesHoldTheirLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterUnitAuraProcHandlerHandlers(registry);

    // The priest site's 8 labels in 8 bodies, the druid site's 11 in 10, the rogue site's 5 in 5, the paladin site's
    // 26 in 21 and the shaman site's 10 in 10; no default.
    CheckSiteLabels<DummyAuraProcPriestSite>(registry, DUMMY_PROC_PRIEST_IDS, 8);
    CheckSiteLabels<DummyAuraProcDruidSite>(registry, DUMMY_PROC_DRUID_IDS, 10);
    CheckSiteLabels<DummyAuraProcRogueSite>(registry, DUMMY_PROC_ROGUE_IDS, 5);
    CheckSiteLabels<DummyAuraProcPaladinSite>(registry, DUMMY_PROC_PALADIN_IDS, 21);
    CheckSiteLabels<DummyAuraProcShamanSite>(registry, DUMMY_PROC_SHAMAN_IDS, 10);
    CHECK(registry.FindDefault<DummyAuraProcPriestSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcDruidSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcRogueSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcPaladinSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcShamanSite>() == NULL);

    // Each run's labels have one body.
    CHECK(registry.Find<DummyAuraProcDruidSite>(37295) == registry.Find<DummyAuraProcDruidSite>(37288));
    CHECK(registry.Find<DummyAuraProcPaladinSite>(25899) == registry.Find<DummyAuraProcPaladinSite>(20911));
    CHECK(registry.Find<DummyAuraProcPaladinSite>(33776) == registry.Find<DummyAuraProcPaladinSite>(31785));
    CHECK(registry.Find<DummyAuraProcPaladinSite>(31877) == registry.Find<DummyAuraProcPaladinSite>(31876));
    CHECK(registry.Find<DummyAuraProcPaladinSite>(31878) == registry.Find<DummyAuraProcPaladinSite>(31876));
    CHECK(registry.Find<DummyAuraProcPaladinSite>(71545) == registry.Find<DummyAuraProcPaladinSite>(71406));

    // Seen from another site, a label of each of the five is held elsewhere.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 15286u, 24932u, 23582u, 20185u, 28823u })
    {
        CHECK(HeldByAnotherSite(game, DummyAuraProcGenericSite::Key, spellId));
    }

    // Tricks of the Trade is a key at the rogue aura dummy site too, and at no other.
    CHECK(SharedWith(57934, DummyAuraProcRogueSite::Key) == AuraDummyRogueSite::Key);
    CHECK(SharedWith(57934, AuraDummyRogueSite::Key) == DummyAuraProcRogueSite::Key);
    CHECK(HeldByAnotherSite(game, DummyAuraProcRogueSite::Key, 57934));
    CHECK(!HeldByAnotherSite(game, DummyAuraProcRogueSite::Key, 57934, AuraDummyRogueSite::Key));
    CHECK(HeldByAnotherSite(game, AuraDummyRogueSite::Key, 57934));
    CHECK(!HeldByAnotherSite(game, AuraDummyRogueSite::Key, 57934, DummyAuraProcRogueSite::Key));
    CHECK(game.Find<AuraDummyRogueSite>(57934) != NULL);

    // Sanctuary and 25899 are keys at the paladin site and both damage-taken sites, and at no other: given any of
    // the three as the partner, the other two are skipped too.
    uint32 const sanctuarySites[] = { DummyAuraProcPaladinSite::Key, SpellDamageTakenSite::Key,
                                      MeleeDamageTakenSite::Key };
    for (uint32 spellId : { 20911u, 25899u })
    {
        CHECK(SharedWith(spellId, DummyAuraProcPaladinSite::Key) == SpellDamageTakenSite::Key);
        CHECK(SharedWith(spellId, SpellDamageTakenSite::Key) == MeleeDamageTakenSite::Key);
        CHECK(SharedWith(spellId, MeleeDamageTakenSite::Key) == SpellDamageTakenSite::Key);
        for (uint32 site : sanctuarySites)
        {
            CHECK(HeldByAnotherSite(game, site, spellId));
            for (uint32 partner : sanctuarySites)
            {
                CHECK(ListedWith(spellId, site, partner));
                if (partner != site)
                {
                    CHECK(!HeldByAnotherSite(game, site, spellId, partner));
                }
            }
        }
        CHECK(game.Find<DummyAuraProcPaladinSite>(spellId) != NULL);
        CHECK(game.Find<SpellDamageTakenSite>(spellId) != NULL);
        CHECK(game.Find<MeleeDamageTakenSite>(spellId) != NULL);
    }
    CHECK(!ListedWith(20911, DummyAuraProcPaladinSite::Key, SpellHealingDoneSite::Key));
    CHECK(ListedWith(20911, AuraDummyPaladinSite::Key, DummyAuraProcPaladinSite::Key));
    CHECK(!ListedWith(25899, DummyAuraProcPaladinSite::Key, AuraDummyPaladinSite::Key));
    CHECK(game.Find<AuraDummyPaladinSite>(20911) != NULL);
    CHECK(!ListedWith(45182, SpellDamageTakenSite::Key, DummyAuraProcPaladinSite::Key));

    // An id the switch never held answers Miss and leaves the locals: 12345 and labels of the other proc sites.
    DummyProcLocals locals(NULL);
    uint32 const others[] = { 12345, 66063, 15286, 24932, 23582, 20185, 28823 };
    uint32 const sites[] = { DummyAuraProcPriestSite::Key, DummyAuraProcDruidSite::Key, DummyAuraProcRogueSite::Key,
                             DummyAuraProcPaladinSite::Key, DummyAuraProcShamanSite::Key };
    std::size_t const owner[] = { 2, 3, 4, 5, 6 };
    for (std::size_t s = 0; s < 5; ++s)
    {
        for (std::size_t k = 0; k < 7; ++k)
        {
            if (k != owner[s])
            {
                CHECK(DispatchDummyProc(sites[s], others[k], locals.ctx).IsMiss());
            }
        }
    }
    CHECK_EQ(locals.triggered_spell_id, uint32(0));
    CHECK(locals.target == ProcVictim());
    CHECK(locals.BasePointsAre(0));
    CHECK_EQ(locals.triggerAmount, int32(15));
}

TEST(UnitAuraProcHandlerHandlers_TheFamilyTriggerLabelsSetWhatTheProcCasts)
{
    // Each body but 53601's sets the triggered spell and goes on past the switch; one that makes the unit the target
    // writes the context's unit, here NULL, over the victim, and one with base points writes the first.
    TriggerCase const cases[] =
    {
        { DummyAuraProcPriestSite::Key, 26169, 26170, true, 100 },       // a tenth of the damage
        { DummyAuraProcPriestSite::Key, 28809, 28810, false, 0 },
        { DummyAuraProcPriestSite::Key, 55680, 56161, false, 75 },       // half the aura's percent of the damage
        { DummyAuraProcDruidSite::Key, 28847, 28848, true, 0 },
        { DummyAuraProcDruidSite::Key, 37288, 37238, true, 0 },
        { DummyAuraProcDruidSite::Key, 37295, 37238, true, 0 },
        { DummyAuraProcDruidSite::Key, 44835, 32747, false, 0 },
        { DummyAuraProcDruidSite::Key, 54821, 54820, false, 0 },
        { DummyAuraProcRogueSite::Key, 32748, 32747, false, 0 },         // the victim is not the unit
        { DummyAuraProcPaladinSite::Key, 20335, 21183, false, 0 },
        { DummyAuraProcPaladinSite::Key, 20336, 54498, false, 0 },
        { DummyAuraProcPaladinSite::Key, 20337, 54499, false, 0 },
        { DummyAuraProcPaladinSite::Key, 54937, 54968, false, 150 },     // the aura's percent of the damage
        { DummyAuraProcPaladinSite::Key, 53601, 0, true, 0 },            // the trigger spell is the spell data's
        { DummyAuraProcShamanSite::Key, 28849, 28850, true, 0 },
        { DummyAuraProcShamanSite::Key, 55440, 55533, true, 150 },       // the victim is not the unit
    };
    for (TriggerCase const& c : cases)
    {
        DummyProcLocals locals(NULL);
        CHECK(DispatchDummyProc(c.site, c.spellId, locals.ctx).IsContinue());
        CHECK_EQ(locals.triggered_spell_id, c.triggered);
        CHECK(locals.target == (c.selfTarget ? NULL : ProcVictim()));
        CHECK(locals.BasePointsAre(c.basepoint));
        CHECK_EQ(locals.triggerAmount, int32(15));
    }

    // The priest trinket picks its trigger by the proc spell's class flag: Shadow Word: Pain's, then Renew's.
    uint64 const flags[][2] = { { UI64LIT(0x0000000000008000), 40441 }, { UI64LIT(0x0000000000000010), 40440 } };
    for (uint64 const (&flag)[2] : flags)
    {
        SpellClassOptionsEntry options = SpellClassOptionsEntry();
        options.SpellClassMask = ClassFamilyMask(flag[0]);
        DummyProcLocals locals(NULL);
        locals.procClassOptions = &options;
        CHECK(DispatchDummyProc(DummyAuraProcPriestSite::Key, 40438, locals.ctx).IsContinue());
        CHECK_EQ(locals.triggered_spell_id, uint32(flag[1]));
        CHECK(locals.target == NULL);
        CHECK(locals.BasePointsAre(0));
    }

    // Frozen Shadoweave heals the unit for the aura's percent of a frost or shadow proc spell's damage.
    for (uint32 school : { uint32(SPELL_SCHOOL_MASK_FROST), uint32(SPELL_SCHOOL_MASK_SHADOW) })
    {
        DummyProcLocals locals(NULL);
        locals.procSpell = ProcSpellRow(1, 116, school);
        CHECK(DispatchDummyProc(DummyAuraProcPriestSite::Key, 39372, locals.ctx).IsContinue());
        CHECK_EQ(locals.triggered_spell_id, uint32(39373));
        CHECK(locals.target == NULL);
        CHECK(locals.BasePointsAre(150));
    }

    // Strikes of Opportunity at a certain chance strikes the victim.
    DummyProcLocals strike(NULL);
    strike.triggerAmount = 100;
    CHECK(DispatchDummyProc(DummyAuraProcRogueSite::Key, 76838, strike.ctx).IsContinue());
    CHECK_EQ(strike.triggered_spell_id, uint32(76858));
    CHECK(strike.target == ProcVictim());
    CHECK(strike.BasePointsAre(0));

    // Holy Power and Totemic Power trigger the buff of the victim's class, and fail the proc for a class they have
    // none for from inside the body's own switch.
    struct ClassBuff { uint8 classId; uint32 holyPower; uint32 totemicPower; };
    ClassBuff const buffs[] =
    {
        { CLASS_PALADIN, 28795, 28824 }, { CLASS_PRIEST, 28795, 28824 }, { CLASS_SHAMAN, 28795, 28824 },
        { CLASS_DRUID, 28795, 28824 }, { CLASS_MAGE, 28793, 28825 }, { CLASS_WARLOCK, 28793, 28825 },
        { CLASS_HUNTER, 28791, 28826 }, { CLASS_ROGUE, 28791, 28826 }, { CLASS_WARRIOR, 28790, 28827 },
        { CLASS_DEATH_KNIGHT, 0, 0 },
    };
    for (ClassBuff const& buff : buffs)
    {
        ValuedCreature victim;
        victim.SetByteValue(UNIT_FIELD_BYTES_0, 1, buff.classId);
        uint32 const sites[2][3] = { { DummyAuraProcPaladinSite::Key, 28789, buff.holyPower },
                                     { DummyAuraProcShamanSite::Key, 28823, buff.totemicPower } };
        for (uint32 const (&one)[3] : sites)
        {
            DummyProcLocals locals(NULL);
            locals.pVictim = &victim;
            locals.target = &victim;
            SpellHandlerOutcome<SpellAuraProcResult> outcome = DispatchDummyProc(one[0], one[1], locals.ctx);
            if (one[2])
            {
                CHECK(outcome.IsContinue());
            }
            else
            {
                CHECK(outcome.IsReturn());
                CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
            }
            CHECK_EQ(locals.triggered_spell_id, one[2]);
            CHECK(locals.target == &victim);
            CHECK(locals.BasePointsAre(0));
        }
    }
}

TEST(UnitAuraProcHandlerHandlers_TheFamilyGuardsEndTheProcBeforeTheUnitIsRead)
{
    // Each case fails the proc before its body reads the unit, and leaves the trigger, target and base points.
    struct GuardCase
    {
        uint32 site;
        uint32 spellId;
        uint32 procSpellSchool;                                 // 0: no proc spell
        bool victim;
        SpellEffectIndex effIndex;
        uint32 damage;
        int32 amount;
    };
    uint32 const holy = SPELL_SCHOOL_MASK_HOLY;
    GuardCase const cases[] =
    {
        { DummyAuraProcPriestSite::Key, 15286, 0, false, EFFECT_INDEX_0, 1000, 15 },   // the unit is the victim
        { DummyAuraProcPriestSite::Key, 40438, 0, true, EFFECT_INDEX_0, 1000, 15 },    // no class flag
        { DummyAuraProcPriestSite::Key, 39372, 0, true, EFFECT_INDEX_0, 1000, 15 },    // no proc spell
        { DummyAuraProcPriestSite::Key, 39372, holy, true, EFFECT_INDEX_0, 1000, 15 }, // neither frost nor shadow
        { DummyAuraProcDruidSite::Key, 40442, 0, true, EFFECT_INDEX_0, 1000, 15 },     // no class flag
        { DummyAuraProcDruidSite::Key, 70664, 0, true, EFFECT_INDEX_0, 1000, 15 },     // no proc spell
        { DummyAuraProcRogueSite::Key, 23582, 0, true, EFFECT_INDEX_0, 1000, 15 },     // no proc spell
        { DummyAuraProcRogueSite::Key, 32748, 0, false, EFFECT_INDEX_0, 1000, 15 },    // the unit is the victim
        { DummyAuraProcRogueSite::Key, 76806, 0, true, EFFECT_INDEX_1, 1000, 15 },     // not the first effect
        { DummyAuraProcRogueSite::Key, 76838, 0, true, EFFECT_INDEX_1, 1000, 100 },    // not the first effect
        { DummyAuraProcRogueSite::Key, 76838, 0, true, EFFECT_INDEX_0, 1000, 0 },      // no chance
        { DummyAuraProcPaladinSite::Key, 20185, 0, true, EFFECT_INDEX_0, 0, 15 },      // no damage
        { DummyAuraProcPaladinSite::Key, 20186, 0, true, EFFECT_INDEX_0, 0, 15 },      // no damage
        { DummyAuraProcPaladinSite::Key, 28789, 0, false, EFFECT_INDEX_0, 1000, 15 },  // no victim
        { DummyAuraProcPaladinSite::Key, 31785, 0, false, EFFECT_INDEX_0, 1000, 15 },  // the unit is the victim
        { DummyAuraProcPaladinSite::Key, 33776, 0, false, EFFECT_INDEX_0, 1000, 15 },
        { DummyAuraProcPaladinSite::Key, 31801, 0, true, EFFECT_INDEX_1, 1000, 15 },   // not the first effect
        { DummyAuraProcPaladinSite::Key, 53736, 0, true, EFFECT_INDEX_2, 1000, 15 },   // not the first effect
        { DummyAuraProcPaladinSite::Key, 31876, 0, true, EFFECT_INDEX_0, 1000, 15 },   // no proc spell
        { DummyAuraProcPaladinSite::Key, 31878, 0, true, EFFECT_INDEX_0, 1000, 15 },
        { DummyAuraProcPaladinSite::Key, 40470, 0, true, EFFECT_INDEX_0, 1000, 15 },   // no proc spell
        { DummyAuraProcPaladinSite::Key, 71406, 0, false, EFFECT_INDEX_0, 1000, 15 },  // no victim
        { DummyAuraProcPaladinSite::Key, 76669, 0, true, EFFECT_INDEX_1, 1000, 15 },   // not the first effect
        { DummyAuraProcPaladinSite::Key, 76669, 0, false, EFFECT_INDEX_0, 1000, 15 },  // no victim
        { DummyAuraProcPaladinSite::Key, 76669, 0, true, EFFECT_INDEX_0, 0, 15 },      // no heal
        { DummyAuraProcShamanSite::Key, 28823, 0, false, EFFECT_INDEX_0, 1000, 15 },   // no victim
        { DummyAuraProcShamanSite::Key, 40463, 0, true, EFFECT_INDEX_0, 1000, 15 },    // no proc spell
        { DummyAuraProcShamanSite::Key, 55440, 0, false, EFFECT_INDEX_0, 1000, 15 },   // the unit is the victim
    };
    for (GuardCase const& c : cases)
    {
        DummyProcLocals locals(NULL);
        if (c.procSpellSchool)
        {
            locals.procSpell = ProcSpellRow(1, 635, c.procSpellSchool);
        }
        if (!c.victim)
        {
            locals.pVictim = NULL;
            locals.target = NULL;
        }
        locals.effIndex = c.effIndex;
        locals.damage = c.damage;
        locals.triggerAmount = c.amount;
        SpellHandlerOutcome<SpellAuraProcResult> outcome = DispatchDummyProc(c.site, c.spellId, locals.ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
        CHECK_EQ(locals.triggered_spell_id, uint32(0));
        CHECK(locals.target == (c.victim ? ProcVictim() : NULL));
        CHECK(locals.BasePointsAre(0));
    }
}

namespace
{
    uint32 const HASTE_PROC_ROGUE_IDS[] = { 13877, 33735 };
    uint32 const CRIT_PROC_MAGE_IDS[] = { 54646 };
    uint32 const PROC_TRIGGER_GENERIC_IDS[] = { 23780, 27522, 40336, 46939, 31255, 33896, 45057, 43820, 57345, 64148,
                                                64415, 64440, 64568, 67702, 67771, 69023 };
    uint32 const PROC_TRIGGER_TRIGGERED_IDS[] = { 7099, 39647, 29494, 20233, 15250, 14189, 14157, 70802, 23880, 30824,
                                                  35095, 48090, 50227, 53817, 57761, 52179, 54748, 56453, 59052,
                                                  62606 };

    // `Unit::HandleHasteAuraProc`'s parameters and locals as its dispatch binds them, with the context over them for
    // `unit`: the damage is 1000, the target the victim and nothing triggered yet.
    struct HasteProcLocals
    {
        Unit* pVictim;
        uint32 damage;
        uint32 triggered_spell_id;
        Unit* target;
        int32 basepoints0;
        HasteAuraProcContext ctx;

        explicit HasteProcLocals(Unit* unit)
            : pVictim(ProcVictim()), damage(1000), triggered_spell_id(0), target(pVictim), basepoints0(0),
              ctx(unit, pVictim, damage, triggered_spell_id, target, basepoints0)
        {
        }

        HasteProcLocals(HasteProcLocals const&) = delete;
        HasteProcLocals& operator=(HasteProcLocals const&) = delete;
    };

    // `Unit::HandleSpellCritChanceAuraProc`'s parameter and locals as its dispatch binds them: no aura, the target the
    // victim and nothing triggered yet.
    struct CritProcLocals
    {
        Aura* triggeredByAura;
        uint32 triggered_spell_id;
        Unit* target;
        SpellCritChanceAuraProcContext ctx;

        CritProcLocals()
            : triggeredByAura(NULL), triggered_spell_id(0), target(ProcVictim()),
              ctx(triggeredByAura, triggered_spell_id, target)
        {
        }

        CritProcLocals(CritProcLocals const&) = delete;
        CritProcLocals& operator=(CritProcLocals const&) = delete;
    };

    // `Unit::HandleProcTriggerSpellAuraProc`'s parameters and locals as its two dispatches bind them, with the context
    // over them for `unit`: the damage is 1000, the aura's amount 15, no proc spell, no target yet and no spell to
    // trigger.
    struct ProcTriggerLocals
    {
        Unit* pVictim;
        uint32 damage;
        Aura* triggeredByAura;
        SpellEntry const* procSpell;
        uint32 procFlags;
        uint32 procEx;
        SpellEntry const* auraSpellInfo;
        SpellClassOptionsEntry const* procClassOptions;
        int32 triggerAmount;
        uint32 trigger_spell_id;
        Unit* target;
        int32 basepoints[MAX_EFFECT_INDEX];
        Item* castItem;
        ProcTriggerSpellAuraProcContext ctx;

        explicit ProcTriggerLocals(Unit* unit)
            : pVictim(ProcVictim()), damage(1000), triggeredByAura(NULL), procSpell(NULL), procFlags(0), procEx(0),
              auraSpellInfo(NULL), procClassOptions(NULL), triggerAmount(15), trigger_spell_id(0), target(NULL),
              basepoints(), castItem(NULL),
              ctx(unit, pVictim, damage, triggeredByAura, procSpell, procFlags, procEx, auraSpellInfo,
                  procClassOptions, triggerAmount, trigger_spell_id, target, basepoints, castItem)
        {
        }

        ProcTriggerLocals(ProcTriggerLocals const&) = delete;
        ProcTriggerLocals& operator=(ProcTriggerLocals const&) = delete;

        bool BasePointsAre(int32 first) const
        {
            return basepoints[0] == first && basepoints[1] == 0 && basepoints[2] == 0;
        }
    };

    // The second proc-trigger dispatch is keyed by the spell to trigger, so the test sets that local to the key first.
    SpellHandlerOutcome<SpellAuraProcResult> DispatchTriggered(uint32 spellId, ProcTriggerLocals& locals)
    {
        locals.trigger_spell_id = spellId;
        return SpellHandlerRegistry::Game().Dispatch<ProcTriggerSpellAuraProcTriggeredSite>(spellId, locals.ctx);
    }
}

TEST(UnitAuraProcHandlerHandlers_TheHasteCritAndTriggerSitesHoldTheirLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterUnitAuraProcHandlerHandlers(registry);

    // The haste site's 2 labels in 1 body, the crit-chance site's 1, the generic proc-trigger site's 16 in 14 and the
    // triggered-spell site's 20 in 15; no default. 30824's body and Savage Defense's (62606) are one code, so the two
    // count as one body whether the linker gives them one address or two.
    std::size_t folds = registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(30824) ==
                        registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(62606) ? 1 : 0;
    CheckSiteLabels<HasteAuraProcRogueSite>(registry, HASTE_PROC_ROGUE_IDS, 1);
    CheckSiteLabels<SpellCritChanceAuraProcMageSite>(registry, CRIT_PROC_MAGE_IDS, 1);
    CheckSiteLabels<ProcTriggerSpellAuraProcGenericSite>(registry, PROC_TRIGGER_GENERIC_IDS, 14);
    CheckSiteLabels<ProcTriggerSpellAuraProcTriggeredSite>(registry, PROC_TRIGGER_TRIGGERED_IDS, 15 - folds);
    CHECK(registry.FindDefault<HasteAuraProcRogueSite>() == NULL);
    CHECK(registry.FindDefault<SpellCritChanceAuraProcMageSite>() == NULL);
    CHECK(registry.FindDefault<ProcTriggerSpellAuraProcGenericSite>() == NULL);
    CHECK(registry.FindDefault<ProcTriggerSpellAuraProcTriggeredSite>() == NULL);

    // Each run's labels have one body, and no two runs share one but the fold above.
    CHECK(registry.Find<HasteAuraProcRogueSite>(33735) == registry.Find<HasteAuraProcRogueSite>(13877));
    for (uint32 spellId : { 40336u, 46939u })
    {
        CHECK(registry.Find<ProcTriggerSpellAuraProcGenericSite>(spellId) ==
              registry.Find<ProcTriggerSpellAuraProcGenericSite>(27522));
    }
    for (uint32 spellId : { 39647u, 29494u, 20233u })
    {
        CHECK(registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(spellId) ==
              registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(7099));
    }
    for (uint32 spellId : { 14157u, 70802u })
    {
        CHECK(registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(spellId) ==
              registry.Find<ProcTriggerSpellAuraProcTriggeredSite>(14189));
    }

    // Seen from another site, a label of each of the four is held elsewhere.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 13877u, 54646u, 23780u, 7099u })
    {
        CHECK(HeldByAnotherSite(game, DummyAuraProcGenericSite::Key, spellId));
    }

    // Brain Freeze is a key at the mage boost site too, and at no other.
    CHECK(SharedWith(57761, ProcTriggerSpellAuraProcTriggeredSite::Key) == AuraBoostMageSite::Key);
    CHECK(SharedWith(57761, AuraBoostMageSite::Key) == ProcTriggerSpellAuraProcTriggeredSite::Key);
    CHECK(HeldByAnotherSite(game, ProcTriggerSpellAuraProcTriggeredSite::Key, 57761));
    CHECK(!HeldByAnotherSite(game, ProcTriggerSpellAuraProcTriggeredSite::Key, 57761, AuraBoostMageSite::Key));
    CHECK(HeldByAnotherSite(game, AuraBoostMageSite::Key, 57761));
    CHECK(!HeldByAnotherSite(game, AuraBoostMageSite::Key, 57761, ProcTriggerSpellAuraProcTriggeredSite::Key));
    CHECK(game.Find<AuraBoostMageSite>(57761) != NULL);

    // An id the switch never held answers Miss and leaves the locals: 12345 and labels of the other proc sites.
    HasteProcLocals haste(NULL);
    for (uint32 spellId : { 12345u, 54646u, 23780u, 7099u, 66063u })
    {
        CHECK(game.Dispatch<HasteAuraProcRogueSite>(spellId, haste.ctx).IsMiss());
    }
    CHECK_EQ(haste.triggered_spell_id, uint32(0));
    CHECK(haste.target == ProcVictim());
    CHECK_EQ(haste.basepoints0, int32(0));

    CritProcLocals crit;
    for (uint32 spellId : { 12345u, 13877u, 23780u, 7099u, 66063u })
    {
        CHECK(game.Dispatch<SpellCritChanceAuraProcMageSite>(spellId, crit.ctx).IsMiss());
    }
    CHECK_EQ(crit.triggered_spell_id, uint32(0));
    CHECK(crit.target == ProcVictim());

    ProcTriggerLocals trigger(NULL);
    for (uint32 spellId : { 12345u, 13877u, 54646u, 7099u, 66063u })
    {
        CHECK(game.Dispatch<ProcTriggerSpellAuraProcGenericSite>(spellId, trigger.ctx).IsMiss());
    }
    for (uint32 spellId : { 12345u, 13877u, 54646u, 23780u, 66063u })
    {
        CHECK(game.Dispatch<ProcTriggerSpellAuraProcTriggeredSite>(spellId, trigger.ctx).IsMiss());
    }
    CHECK_EQ(trigger.trigger_spell_id, uint32(0));
    CHECK(trigger.target == NULL);
    CHECK(trigger.BasePointsAre(0));
}

TEST(UnitAuraProcHandlerHandlers_TheHasteCritAndTriggerContextsAliasTheFunctionsLocals)
{
    ValuedCreature creature;

    // `unit` is the pointer passed for `this`; every other member is the local's or parameter's address, a body's
    // writes reach the locals and the parameters are read where they stand.
    HasteProcLocals haste(&creature);
    CHECK(haste.ctx.unit == &creature);
    CHECK(&haste.ctx.pVictim == &haste.pVictim);
    CHECK(&haste.ctx.damage == &haste.damage);
    CHECK(&haste.ctx.triggered_spell_id == &haste.triggered_spell_id);
    CHECK(&haste.ctx.target == &haste.target);
    CHECK(&haste.ctx.basepoints0 == &haste.basepoints0);
    haste.ctx.triggered_spell_id = 22482;
    haste.ctx.target = &creature;
    haste.ctx.basepoints0 = 7;
    CHECK_EQ(haste.triggered_spell_id, uint32(22482));
    CHECK(haste.target == &creature);
    CHECK_EQ(haste.basepoints0, int32(7));
    haste.damage = 250;
    CHECK_EQ(haste.ctx.damage, uint32(250));

    // The crit-chance context holds no unit: its one body reads only the aura.
    CritProcLocals crit;
    CHECK(&crit.ctx.triggeredByAura == &crit.triggeredByAura);
    CHECK(&crit.ctx.triggered_spell_id == &crit.triggered_spell_id);
    CHECK(&crit.ctx.target == &crit.target);
    crit.ctx.triggered_spell_id = 54648;
    crit.ctx.target = &creature;
    CHECK_EQ(crit.triggered_spell_id, uint32(54648));
    CHECK(crit.target == &creature);

    ProcTriggerLocals trigger(&creature);
    ProcTriggerSpellAuraProcContext& ctx = trigger.ctx;
    CHECK(ctx.unit == &creature);
    CHECK(&ctx.pVictim == &trigger.pVictim);
    CHECK(&ctx.damage == &trigger.damage);
    CHECK(&ctx.triggeredByAura == &trigger.triggeredByAura);
    CHECK(&ctx.procSpell == &trigger.procSpell);
    CHECK(&ctx.procFlags == &trigger.procFlags);
    CHECK(&ctx.procEx == &trigger.procEx);
    CHECK(&ctx.auraSpellInfo == &trigger.auraSpellInfo);
    CHECK(&ctx.procClassOptions == &trigger.procClassOptions);
    CHECK(&ctx.triggerAmount == &trigger.triggerAmount);
    CHECK(&ctx.trigger_spell_id == &trigger.trigger_spell_id);
    CHECK(&ctx.target == &trigger.target);
    CHECK(&ctx.basepoints == &trigger.basepoints);
    CHECK(&ctx.castItem == &trigger.castItem);
    ctx.trigger_spell_id = 23781;
    ctx.target = &creature;
    ctx.basepoints[EFFECT_INDEX_1] = 9;
    CHECK_EQ(trigger.trigger_spell_id, uint32(23781));
    CHECK(trigger.target == &creature);
    CHECK_EQ(trigger.basepoints[EFFECT_INDEX_1], int32(9));
    trigger.damage = 250;
    trigger.procFlags = PROC_FLAG_ON_TRAP_ACTIVATION;
    trigger.procEx = PROC_EX_CRITICAL_HIT;
    trigger.triggerAmount = 40;
    CHECK_EQ(ctx.damage, uint32(250));
    CHECK_EQ(ctx.procFlags, uint32(PROC_FLAG_ON_TRAP_ACTIVATION));
    CHECK_EQ(ctx.procEx, uint32(PROC_EX_CRITICAL_HIT));
    CHECK_EQ(ctx.triggerAmount, int32(40));
}

TEST(UnitAuraProcHandlerHandlers_TheProcTriggerLabelsSetTheSpellOrTheTarget)
{
    // Aegis of Preservation and Desperate Defense set the spell to trigger and go on to its checks; the target is
    // left for the cast to pick.
    struct SpellCase
    {
        uint32 spellId;
        uint32 triggered;
    };
    SpellCase const cases[] = { { 23780, 23781 }, { 33896, 33898 } };
    for (SpellCase const& c : cases)
    {
        ProcTriggerLocals locals(NULL);
        CHECK(SpellHandlerRegistry::Game().Dispatch<ProcTriggerSpellAuraProcGenericSite>(c.spellId, locals.ctx)
                  .IsContinue());
        CHECK_EQ(locals.trigger_spell_id, c.triggered);
        CHECK(locals.target == NULL);
        CHECK(locals.BasePointsAre(0));
    }

    // The four positive spells cast on an enemy take the victim as their target and keep the spell to trigger.
    for (uint32 spellId : { 7099u, 39647u, 29494u, 20233u })
    {
        ProcTriggerLocals locals(NULL);
        CHECK(DispatchTriggered(spellId, locals).IsContinue());
        CHECK(locals.target == ProcVictim());
        CHECK_EQ(locals.trigger_spell_id, spellId);
        CHECK(locals.BasePointsAre(0));
    }
}

TEST(UnitAuraProcHandlerHandlers_TheProcTriggerGuardsEndTheProcBeforeTheUnitIsRead)
{
    // Each case ends the proc with FAILED before any body reads the unit, and leaves the locals as they were. A case
    // whose unit is the victim passes one creature, never in a world, for `this` and the victim, which the guard
    // only compares.
    struct GuardCase
    {
        uint32 spellId;
        bool victim;
        bool procSpell;
        uint32 procEx;
        uint32 procFlags;
        int32 amount;
        bool unitIsVictim;
    };
    GuardCase const cases[] =
    {
        { 15250, false, false, 0, 0, 15, false },                                       // no victim
        { 57761, true, false, 0, 0, 15, false },                                        // no proc spell
        { 52179, true, false, PROC_EX_NORMAL_HIT, 0, 15, false },                       // no proc spell
        { 52179, true, true, 0, 0, 15, false },                                         // neither hit nor crit
        { 52179, true, true, PROC_EX_NORMAL_HIT, 0, 15, true },                         // the unit hit itself
        { 52179, true, true, PROC_EX_CRITICAL_HIT, 0, 15, true },
        { 54748, true, false, 0, 0, 15, false },                                        // no proc spell
        { 56453, true, false, 0, 0, 100, false },                                       // no trap
        { 56453, true, false, 0, PROC_FLAG_ON_TRAP_ACTIVATION, 0, false },              // a roll at 0 percent
    };
    ValuedCreature self;
    for (GuardCase const& c : cases)
    {
        ProcTriggerLocals locals(c.unitIsVictim ? &self : NULL);
        locals.pVictim = c.victim ? (c.unitIsVictim ? &self : ProcVictim()) : NULL;
        locals.procSpell = c.procSpell ? ProcSpellRow(0, 1, SPELL_SCHOOL_MASK_NORMAL) : NULL;
        locals.procEx = c.procEx;
        locals.procFlags = c.procFlags;
        locals.triggerAmount = c.amount;
        SpellHandlerOutcome<SpellAuraProcResult> outcome = DispatchTriggered(c.spellId, locals);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
        CHECK_EQ(locals.trigger_spell_id, c.spellId);
        CHECK(locals.target == NULL);
        CHECK(locals.BasePointsAre(0));
    }

    // Brain Freeze after a proc spell with no class options, and Lock and Load from a trap at 100 percent, go on.
    ProcTriggerLocals freeze(NULL);
    freeze.procSpell = ProcSpellRow(0, 1, SPELL_SCHOOL_MASK_FROST);
    CHECK(DispatchTriggered(57761, freeze).IsContinue());
    CHECK(freeze.target == NULL);
    CHECK(freeze.BasePointsAre(0));

    ProcTriggerLocals trap(NULL);
    trap.procFlags = PROC_FLAG_ON_TRAP_ACTIVATION;
    trap.triggerAmount = 100;
    CHECK(DispatchTriggered(56453, trap).IsContinue());
    CHECK(trap.target == NULL);
    CHECK(trap.BasePointsAre(0));
}

namespace
{
    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct SeamMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct WorldObjectCurrentMap
    {
        typedef Map* WorldObject::* type;
        friend type MemberOf(WorldObjectCurrentMap);
    };

    struct LogMainFile
    {
        typedef FILE* Log::* type;
        friend type MemberOf(LogMainFile);
    };

    template struct SeamMemberAccess<WorldObjectCurrentMap, &WorldObject::m_currMap>;
    template struct SeamMemberAccess<LogMainFile, &Log::logfile>;

    // Two maps the check compares by address. Nothing reads through them: the check looks up the unit's vessel,
    // which reads the unit's map, only under a scope that does not own that map, and no case checks a unit there.
    alignas(16) unsigned char s_ownMap[16];
    alignas(16) unsigned char s_otherMap[16];

    Map* OwnMap()
    {
        return reinterpret_cast<Map*>(s_ownMap);
    }

    Map* OtherMap()
    {
        return reinterpret_cast<Map*>(s_otherMap);
    }

    /// A creature in the world on OwnMap(): flagged in world and given the map, nothing else.
    class InWorldCreature : public ValuedCreature
    {
        public:
            InWorldCreature()
            {
                this->*MemberOf(WorldObjectCurrentMap()) = OwnMap();
                Object::AddToWorld();
            }

            ~InWorldCreature()
            {
                Object::RemoveFromWorld();
                ResetMap();
            }
    };

    /// Opens the map phase for the scope, as MapManager::Update does around the map updates.
    class OpenSeamMapPhase
    {
        public:
            OpenSeamMapPhase() { MapPhase::Begin(); }
            ~OpenSeamMapPhase() { MapPhase::End(); }

        private:
            OpenSeamMapPhase(OpenSeamMapPhase const&) = delete;
            OpenSeamMapPhase& operator=(OpenSeamMapPhase const&) = delete;
    };

    /// The violations one call adds.
    template <typename Call>
    uint32 ViolationsOf(Call call)
    {
        uint32 const before = MapPhase::Violations();
        call();
        return MapPhase::Violations() - before;
    }

    /// The violations one call adds when it runs on a thread of its own, which updates no map.
    template <typename Call>
    uint32 ViolationsOnAnotherThread(Call call)
    {
        uint32 const before = MapPhase::Violations();
        std::thread other(call);
        other.join();
        return MapPhase::Violations() - before;
    }

    /// What the log's main file receives while the call runs, from its first "ERROR:" on (the timestamp before it
    /// dropped): the file is swapped for a scratch file and put back.
    template <typename Call>
    std::string LoggedBy(Call call)
    {
        std::filesystem::path const path = std::filesystem::temp_directory_path() /
            ("mangos_spell_seam_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
        FILE* scratch = std::fopen(path.string().c_str(), "w+");
        if (!scratch)
        {
            return std::string();
        }
        FILE* const saved = sLog.*MemberOf(LogMainFile());
        sLog.*MemberOf(LogMainFile()) = scratch;
        call();
        sLog.*MemberOf(LogMainFile()) = saved;

        std::string text;
        std::fseek(scratch, 0, SEEK_SET);
        char buffer[512];
        for (std::size_t n; (n = std::fread(buffer, 1, sizeof(buffer), scratch)) > 0;)
        {
            text.append(buffer, n);
        }
        std::fclose(scratch);
        std::filesystem::remove(path);
        std::size_t const at = text.find("ERROR:");
        return at == std::string::npos ? text : text.substr(at);
    }

    /// Builds one of the five contexts that hold a unit over `unit`: 0 the dummy proc's, 1 the haste proc's, 2 the
    /// proc trigger's, 3 the spell damage taken's, 4 the melee damage taken's.
    void BuildContext(int which, Unit* unit)
    {
        SpellCooldownMgr cooldowns;
        Unit* victim = NULL;
        Unit* target = NULL;
        uint32 damage = 0;
        uint32 flags = 0;
        uint32 cooldown = 0;
        Aura* aura = NULL;
        SpellEntry const* spell = NULL;
        SpellClassOptionsEntry const* options = NULL;
        SpellEffectIndex effIndex = EFFECT_INDEX_0;
        int32 amount = 0;
        Item* castItem = NULL;
        uint32 triggered = 0;
        int32 basepoints[MAX_EFFECT_INDEX] = { 0, 0, 0 };
        float mod = 1.0f;
        std::list<Aura*> auras;
        std::list<Aura*>::const_iterator i = auras.begin();

        switch (which)
        {
            case 0:
            {
                DummyAuraProcContext ctx(unit, cooldowns, victim, damage, aura, spell, flags, cooldown, spell, effIndex,
                                         options, amount, castItem, triggered, target, basepoints);
                break;
            }
            case 1:
            {
                HasteAuraProcContext ctx(unit, victim, damage, triggered, target, amount);
                break;
            }
            case 2:
            {
                ProcTriggerSpellAuraProcContext ctx(unit, victim, damage, aura, spell, flags, flags, spell, options,
                                                    amount, triggered, target, basepoints, castItem);
                break;
            }
            case 3:
            {
                SpellDamageTakenContext ctx(unit, spell, mod, i);
                break;
            }
            case 4:
            {
                MeleeDamageTakenContext ctx(unit, mod, i);
                break;
            }
            default:
                break;
        }
    }

    /// Builds the two Unit-site contexts that hold no unit.
    void BuildTheContextsWithNoUnit()
    {
        Aura* aura = NULL;
        uint32 triggered = 0;
        Unit* target = NULL;
        int32 total = 0;
        std::list<Aura*> auras;
        std::list<Aura*>::const_iterator i = auras.begin();
        SpellCritChanceAuraProcContext crit(aura, triggered, target);
        SpellHealingDoneContext healing(total, i);
    }
}

TEST(Unit_TheSpellSeamCheckPassesOutsideTheMapPhaseAndOnTheThreadUpdatingTheUnitsMap)
{
    REQUIRE(!MapPhase::Active());
    InWorldCreature unit;
    ValuedCreature outside;

    // Outside the map phase the world thread owns every map: no violation, on this thread or another.
    CHECK_EQ(ViolationsOf([&]() { unit.AssertSpellSeamOwner(); }), 0u);
    CHECK_EQ(ViolationsOnAnotherThread([&]() { unit.AssertSpellSeamOwner(); }), 0u);

    OpenSeamMapPhase phase;
    {
        // The thread updating the unit's map owns it, a deck's nested update handing it back included. The
        // scope owns the unit's map, so the check looks up no vessel.
        MapPhase::Scope own(OwnMap());
        REQUIRE(MapPhase::Owns(OwnMap()));
        CHECK_EQ(ViolationsOf([&]() { unit.AssertSpellSeamOwner(); }), 0u);
        {
            MapPhase::Scope deck(OtherMap());
        }
        CHECK_EQ(ViolationsOf([&]() { unit.AssertSpellSeamOwner(); }), 0u);
        for (int which = 0; which < 5; ++which)
        {
            CHECK_EQ(ViolationsOf([&]() { BuildContext(which, &unit); }), 0u);
        }
    }
    {
        // A unit in no world is never checked, whichever map the thread updates.
        MapPhase::Scope other(OtherMap());
        CHECK_EQ(ViolationsOf([&]() { outside.AssertSpellSeamOwner(); }), 0u);
    }
    CHECK_EQ(ViolationsOnAnotherThread([&]() { outside.AssertSpellSeamOwner(); }), 0u);
}

#ifndef MANGOS_DEBUG
// Under MANGOS_DEBUG a violation asserts, so the cases that count one run in Release only.
TEST(Unit_TheSpellSeamAndTheMovementKernelEachNameThemselvesInTheViolationLog)
{
    // Only the first ten violations are logged, so the case needs two of them left.
    REQUIRE(MapPhase::Violations() <= 8);
    InWorldCreature unit;
    OpenSeamMapPhase phase;
    // No scope is open on this thread, so the check counts without looking up the unit's vessel.
    REQUIRE(!MapPhase::UpdatesAMap());
    std::string const guid = unit.GetGuidStr();

    uint32 const seam = MapPhase::Violations() + 1;
    std::string const seamLog = LoggedBy([&]() { unit.AssertSpellSeamOwner(); });
    CHECK_STR(seamLog, "ERROR:MapPhase: the spell seam of " + guid + " touched from outside its map's update (violation " +
              std::to_string(seam) + ")\n");

    uint32 const kernel = MapPhase::Violations() + 1;
    std::string const kernelLog = LoggedBy([&]() { unit.AssertMotionOwner(); });
    CHECK_STR(kernelLog, "ERROR:MapPhase: the movement kernel of " + guid +
              " touched from outside its map's update (violation " + std::to_string(kernel) + ")\n");
}

TEST(Unit_TheSpellSeamCheckCountsAThreadThatDoesNotUpdateTheUnitsMap)
{
    REQUIRE(!MapPhase::Active());
    InWorldCreature unit;
    OpenSeamMapPhase phase;
    // No scope is open on this thread, so the check counts without looking up the unit's vessel.
    REQUIRE(!MapPhase::UpdatesAMap());

    // A thread that updates no map, this one or another, counts one violation per check.
    CHECK_EQ(ViolationsOf([&]() { unit.AssertSpellSeamOwner(); }), 1u);
    CHECK_EQ(ViolationsOf([&]() { unit.AssertSpellSeamOwner(); unit.AssertSpellSeamOwner(); }), 2u);
    CHECK_EQ(ViolationsOnAnotherThread([&]() { unit.AssertSpellSeamOwner(); }), 1u);

    // The movement kernel's check counts on the same counter.
    CHECK_EQ(ViolationsOnAnotherThread([&]() { unit.AssertMotionOwner(); }), 1u);
}

TEST(UnitSiteContexts_EachContextThatHoldsTheUnitChecksItWhenBuilt)
{
    REQUIRE(!MapPhase::Active());
    InWorldCreature unit;
    OpenSeamMapPhase phase;
    // No scope is open on this thread, so the check counts without looking up the unit's vessel.
    REQUIRE(!MapPhase::UpdatesAMap());

    // Built on a thread that updates no map, this one or another, each of the five counts one violation; with no
    // unit, none.
    for (int which = 0; which < 5; ++which)
    {
        CHECK_EQ(ViolationsOnAnotherThread([&]() { BuildContext(which, &unit); }), 1u);
        CHECK_EQ(ViolationsOf([&]() { BuildContext(which, &unit); }), 1u);
        CHECK_EQ(ViolationsOnAnotherThread([&]() { BuildContext(which, NULL); }), 0u);
    }

    // The two Unit-site contexts that hold no unit check nothing.
    CHECK_EQ(ViolationsOnAnotherThread([&]() { BuildTheContextsWithNoUnit(); }), 0u);
}
#endif
