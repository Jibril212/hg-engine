#include "../../include/battle.h"
#include "../../include/config.h"
#include "../../include/constants/ability.h"
#include "../../include/constants/file.h"
#include "../../include/constants/hold_item_effects.h"
#include "../../include/constants/item.h"
#include "../../include/constants/move_effects.h"
#include "../../include/constants/moves.h"
#include "../../include/constants/species.h"
#include "../../include/custom/custom_ai.h"
#include "../../include/debug.h"
#include "../../include/overlay.h"
#include "../../include/pokemon.h"
#include "../../include/q412.h"
#include "../../include/types.h"

// this has been moved to src/battle/other_battle_calculators.c so it can be used
extern u8 TypeEffectivenessTable[][3];

int GetHiddenPowerType(u32 hp_iv, u32 atk_iv, u32 def_iv, u32 spe_iv, u32 spatk_iv, u32 spdef_iv)
{
    int type = (hp_iv & 1) | ((atk_iv & 1) << 1) | ((def_iv & 1) << 2) | ((spe_iv & 1) << 3) | ((spatk_iv & 1) << 4) | ((spdef_iv & 1) << 5);
    type = (type * 15 / 63) + 1;
    if (type >= TYPE_MYSTERY) {
        type++;
    }
    return type;
}

void LONG_CALL FillDamageStructFromPartyMon(void *bw UNUSED, struct BattleStruct *sp, struct AI_sDamageCalc *monStruct, struct PartyPokemon *pp)
{
    monStruct->species = GetMonData(pp, MON_DATA_SPECIES, 0);
    monStruct->hp = GetMonData(pp, MON_DATA_HP, 0);
    monStruct->maxhp = GetMonData(pp, MON_DATA_MAXHP, 0);
    monStruct->percenthp = (monStruct->hp * 100) / monStruct->maxhp;
    monStruct->item = GetMonData(pp, MON_DATA_HELD_ITEM, 0);
    monStruct->item_held_effect = BattleItemDataGet(sp, monStruct->item, 1);
    monStruct->item_power = BattleItemDataGet(sp, monStruct->item, 2);

    monStruct->ability = GetMonData(pp, MON_DATA_ABILITY, 0);
    monStruct->sex = GetMonData(pp, MON_DATA_GENDER, 0);
    monStruct->type1 = GetMonData(pp, MON_DATA_TYPE_1, 0);
    monStruct->type2 = GetMonData(pp, MON_DATA_TYPE_2, 0);
    monStruct->type3 = TYPE_TYPELESS;


    monStruct->condition = GetMonData(pp, MON_DATA_STATUS, 0);
    monStruct->condition = 0;
    monStruct->isGrounded = IsPartyPokemonGrounded(sp, pp);

    monStruct->speed = GetMonData(pp, MON_DATA_SPEED, 0);
    monStruct->weight = 1;

    monStruct->attack = GetMonData(pp, MON_DATA_SPECIAL_ATTACK, 0);
    monStruct->defense = GetMonData(pp, MON_DATA_SPECIAL_ATTACK, 0);
    monStruct->sp_attack = GetMonData(pp, MON_DATA_SPECIAL_ATTACK, 0);
    monStruct->sp_defense = GetMonData(pp, MON_DATA_SPECIAL_ATTACK, 0);

    for (int i = 0; i < 8; i++) {
        monStruct->states[i] = 0; // Reset all states to 0
    }

    monStruct->level = GetMonData(pp, MON_DATA_LEVEL, 0);
    monStruct->form = GetMonData(pp, MON_DATA_FORM, 0);
    // ArchiveDataLoadOfs(&monStruct->weight, ARC_DEX_LISTS, 1, PokeOtherFormMonsNoGet(monStruct->species, monStruct->form) * sizeof(s32), sizeof(s32));

    monStruct->hasMoldBreaker = FALSE;
    if (monStruct->ability == ABILITY_MOLD_BREAKER || monStruct->ability == ABILITY_TERAVOLT || monStruct->ability == ABILITY_TURBOBLAZE) {
        monStruct->hasMoldBreaker = TRUE;
    }

    monStruct->effect_of_moves = 0;
    monStruct->flashFireActivated = FALSE;

    u32 hp_iv = GetMonData(pp, MON_DATA_HP_IV, 0);
    u32 atk_iv = GetMonData(pp, MON_DATA_ATK_IV, 0);
    u32 def_iv = GetMonData(pp, MON_DATA_DEF_IV, 0);
    u32 spe_iv = GetMonData(pp, MON_DATA_SPEED_IV, 0);
    u32 spatk_iv = GetMonData(pp, MON_DATA_SPATK_IV, 0);
    u32 spdef_iv = GetMonData(pp, MON_DATA_SPDEF_IV, 0);
    monStruct->hiddenPowerType = GetHiddenPowerType(hp_iv, atk_iv, def_iv, spe_iv, spatk_iv, spdef_iv);

    monStruct->slowStartCount = 0;
    monStruct->furyCutterCount = 0;
    monStruct->metronomeTurns = 0;
    monStruct->lastResortCount = 0;
    monStruct->attackerHasMoveFailureLastTurn = 0;
}

void LONG_CALL FillDamageStructFromBattleMon(void *bw, struct BattleStruct *sp, struct AI_sDamageCalc *monStruct, int numSlot)
{
    monStruct->species = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_SPECIES, NULL);
    monStruct->hp = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_HP, NULL);
    monStruct->maxhp = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_MAX_HP, NULL);
    monStruct->percenthp = (monStruct->hp * 100) / monStruct->maxhp;
    monStruct->item = GetBattleMonItem(sp, numSlot);
    monStruct->item_held_effect = BattleItemDataGet(sp, monStruct->item, 1);
    monStruct->item_power = BattleItemDataGet(sp, monStruct->item, 2);

    monStruct->condition = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_MAX_CONDITION, NULL);
    monStruct->condition2 = sp->battlemon[numSlot].condition2;
    monStruct->isGrounded = IsClientGrounded(sp, numSlot);

    monStruct->ability = GetBattlerAbility(sp, numSlot);
    monStruct->sex = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_SEX, NULL);
    monStruct->type1 = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_TYPE1, NULL);
    monStruct->type2 = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_TYPE2, NULL);
    monStruct->type3 = sp->battlemon[numSlot].type3;

    monStruct->speed = sp->effectiveSpeed[numSlot];
    monStruct->weight = GetPokemonWeight(bw, sp, numSlot, numSlot);

    monStruct->form = sp->battlemon[numSlot].form_no;
    monStruct->attack = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_ATK, NULL);
    monStruct->defense = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_DEF, NULL);
    monStruct->sp_attack = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_SPATK, NULL);
    monStruct->sp_defense = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_SPDEF, NULL);
    for (int i = 0; i < 8; i++) {
        monStruct->states[i] = 0; // Reset all states to 0
    }

    monStruct->states[STAT_ATTACK] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_ATK, NULL) - 6;
    monStruct->states[STAT_SPATK] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_SPATK, NULL) - 6;
    monStruct->states[STAT_DEFENSE] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_DEF, NULL) - 6;
    monStruct->states[STAT_SPDEF] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_SPDEF, NULL) - 6;
    monStruct->states[STAT_SPEED] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_SPE, NULL) - 6;
    monStruct->states[STAT_ACCURACY] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_ACCURACY, NULL) - 6;
    monStruct->states[STAT_EVASION] = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_STATE_EVASIVENESS, NULL) - 6;

    monStruct->level = BattlePokemonParamGet(sp, numSlot, BATTLE_MON_DATA_LEVEL, NULL);
    monStruct->form = sp->battlemon[numSlot].form_no;

    monStruct->hasMoldBreaker = FALSE;
    if (monStruct->ability == ABILITY_MOLD_BREAKER || monStruct->ability == ABILITY_TERAVOLT || monStruct->ability == ABILITY_TURBOBLAZE) {
        monStruct->hasMoldBreaker = TRUE;
    }

    monStruct->effect_of_moves = sp->battlemon[numSlot].effect_of_moves;
    monStruct->flashFireActivated = FALSE;
    if (sp->battlemon[numSlot].moveeffect.flashFire) {
        monStruct->flashFireActivated = TRUE;
    }
    monStruct->hiddenPowerType = GetHiddenPowerType(
        sp->battlemon[numSlot].hp_iv,
        sp->battlemon[numSlot].atk_iv,
        sp->battlemon[numSlot].def_iv,
        sp->battlemon[numSlot].spe_iv,
        sp->battlemon[numSlot].spatk_iv,
        sp->battlemon[numSlot].spdef_iv);

    monStruct->slowStartCount = (sp->total_turn - sp->battlemon[numSlot].moveeffect.slowStartTurns);
    monStruct->furyCutterCount = sp->battlemon[numSlot].moveeffect.furyCutterCount;
    monStruct->metronomeTurns = sp->battlemon[numSlot].moveeffect.metronomeTurns;
    monStruct->lastResortCount = sp->battlemon[numSlot].moveeffect.lastResortCount;
    monStruct->attackerHasMoveFailureLastTurn = sp->moveConditionsFlags[numSlot].moveFailureLastTurn;
}

u8 LONG_CALL BattleAI_UpdateTypeEffectiveness(u32 move_no, u32 held_effect UNUSED, u8 defender_type, u8 defaultEffectiveness)
{
    if (move_no == MOVE_FREEZE_DRY && defender_type == TYPE_WATER) {
        defaultEffectiveness = TYPE_MUL_SUPER_EFFECTIVE;
    }

    return defaultEffectiveness;
}

int LONG_CALL BattleAI_GetTypeEffectiveness(void *bw, struct BattleStruct *sp, int moveno, int move_type, u32 *flag UNUSED, struct AI_sDamageCalc *attacker, struct AI_sDamageCalc *defender)
{
    int typeTableEntryNo = 0; // Used to cycle through all (non-neutral) type interactions.

    // https://xcancel.com/Sibuna_Switch/status/1827463371383328877#m
    u8 defender_type_1 = GetSanitisedType(defender->type1);
    u8 defender_type_2 = GetSanitisedType(defender->type2);
    u8 defender_type_3 = GetSanitisedType(defender->type3);

    u32 type1Effectiveness = TYPE_MUL_NORMAL;
    u32 type2Effectiveness = TYPE_MUL_NORMAL;
    u32 type3Effectiveness = TYPE_MUL_NORMAL;

    // [0]: Attacking type
    // [1]: Defending type
    // [2]: TYPE_MUL
    // TODO: handle Ring Target, Thousand Arrows, Freeze-Dry, Flying Press
    while (TypeEffectivenessTable[typeTableEntryNo][0] != TYPE_ENDTABLE) {
        if (TypeEffectivenessTable[typeTableEntryNo][0] == TYPE_FORESIGHT) {
            if ((defender->condition2 & STATUS2_FORESIGHT)
                || (attacker->ability == ABILITY_SCRAPPY)
                || (attacker->ability == ABILITY_MINDS_EYE)) {
                break;
            } else {
                typeTableEntryNo++;
                continue;
            }
        }

        //TODO: terra
        if (TypeEffectivenessTable[typeTableEntryNo][0] == move_type)
        {
            if (TypeEffectivenessTable[typeTableEntryNo][1] == defender_type_1)
            {
                if (AI_ShouldUseNormalTypeEffCalc(sp, defender->item_held_effect, typeTableEntryNo)
                    && !StrongWindsShouldWeaken(bw, sp, typeTableEntryNo, defender_type_1))
                {
                    //no ring target
                    type1Effectiveness = UpdateTypeEffectiveness(moveno, HOLD_EFFECT_NONE, defender_type_1, TypeEffectivenessTable[typeTableEntryNo][2]);
                }
            } else if (TypeEffectivenessTable[typeTableEntryNo][1] == defender_type_2) {
                if (AI_ShouldUseNormalTypeEffCalc(sp, defender->item_held_effect, typeTableEntryNo)
                    && !StrongWindsShouldWeaken(bw, sp, typeTableEntryNo, defender_type_2)) {
                    type2Effectiveness = UpdateTypeEffectiveness(moveno, HOLD_EFFECT_NONE, defender_type_2, TypeEffectivenessTable[typeTableEntryNo][2]);
                }
            } else if (TypeEffectivenessTable[typeTableEntryNo][1] == defender_type_3) {
                if (AI_ShouldUseNormalTypeEffCalc(sp, defender->item_held_effect, typeTableEntryNo)
                    && !StrongWindsShouldWeaken(bw, sp, typeTableEntryNo, defender_type_3)) {
                    type3Effectiveness = UpdateTypeEffectiveness(moveno, HOLD_EFFECT_NONE, defender_type_3, TypeEffectivenessTable[typeTableEntryNo][2]);
                }
            }
        }
        typeTableEntryNo++;
    }
  
    int typeMul = type1Effectiveness * type2Effectiveness * type3Effectiveness;
    switch (typeMul) {
    case EFFECTIVENESS_MULT_TRIPLE_SUPER_EFFECTIVE:
        return TYPE_MUL_TRIPLE_SUPER_EFFECTIVE; // 40
    case EFFECTIVENESS_MULT_DOUBLE_SUPER_EFFECTIVE:
        return TYPE_MUL_DOUBLE_SUPER_EFFECTIVE; // 30
    case EFFECTIVENESS_MULT_SUPER_EFFECTIVE:
        return TYPE_MUL_SUPER_EFFECTIVE; // 20
    case EFFECTIVENESS_MULT_NORMAL:
        return TYPE_MUL_NORMAL; // 10
    case EFFECTIVENESS_MULT_NOT_EFFECTIVE:
        return TYPE_MUL_NOT_EFFECTIVE; // 5
    case EFFECTIVENESS_MULT_DOUBLE_NOT_EFFECTIVE:
        return TYPE_MUL_DOUBLE_NOT_EFFECTIVE; // 4
    case EFFECTIVENESS_MULT_TRIPLE_NOT_EFFECTIVE:
        return TYPE_MUL_TRIPLE_NOT_EFFECTIVE; // 3
    }
    return TYPE_MUL_NO_EFFECT; // 0
}



BOOL LONG_CALL BattleAI_IsContactBeingMade(struct BattleStruct *sp, u32 ability, u32 itemHoldEffect, u32 moveno)
{
    if (ability == ABILITY_LONG_REACH) {
        return FALSE;
    }

    if (itemHoldEffect == HOLD_EFFECT_PREVENT_CONTACT_EFFECTS || (itemHoldEffect == HOLD_EFFECT_INCREASE_PUNCHING_MOVE_DMG && IsMovePunchingMove(moveno))) {
        return FALSE;
    }

    if (itemHoldEffect == HOLD_EFFECT_PREVENT_CONTACT_EFFECTS) {
        return FALSE;
    }

    if (sp->moveTbl[moveno].flag & FLAG_CONTACT) {
        return TRUE;
    }

    return FALSE;
}

BOOL IsMoveBoostedBySheerForce(u32 moveno, u32 moveeffect)
{
    BOOL isBoosted = FALSE;
    switch (moveeffect) {
    case MOVE_EFFECT_FLINCH_HIT:
    case MOVE_EFFECT_RAISE_ALL_STATS_HIT:
    case MOVE_EFFECT_BLIZZARD:
    case MOVE_EFFECT_PARALYZE_HIT:
    case MOVE_EFFECT_LOWER_SPEED_HIT:
    case MOVE_EFFECT_RAISE_SP_ATK_HIT:
    case MOVE_EFFECT_CONFUSE_HIT:
    case MOVE_EFFECT_LOWER_DEFENSE_HIT:
    case MOVE_EFFECT_LOWER_SP_DEF_HIT:
    case MOVE_EFFECT_BURN_HIT:
    case MOVE_EFFECT_FLINCH_BURN_HIT:
    case MOVE_EFFECT_RAISE_SPEED_HIT:
    case MOVE_EFFECT_POISON_HIT:
    case MOVE_EFFECT_FREEZE_HIT:
    case MOVE_EFFECT_FLINCH_FREEZE_HIT:
    case MOVE_EFFECT_RAISE_ATTACK_HIT:
    case MOVE_EFFECT_LOWER_ACCURACY_HIT:
    case MOVE_EFFECT_BADLY_POISON_HIT:
    case MOVE_EFFECT_SECRET_POWER:
    case MOVE_EFFECT_LOWER_SP_ATK_HIT:
    case MOVE_EFFECT_THUNDER:
    case MOVE_EFFECT_HURRICANE:
    case MOVE_EFFECT_FLINCH_PARALYZE_HIT:
    case MOVE_EFFECT_FLINCH_DOUBLE_DAMAGE_FLY_OR_BOUNCE:
    case MOVE_EFFECT_LOWER_SP_DEF_2_HIT:
    case MOVE_EFFECT_LOWER_ATTACK_HIT:
    case MOVE_EFFECT_THAW_AND_BURN_HIT:
    case MOVE_EFFECT_CHATTER:
    case MOVE_EFFECT_FLINCH_MINIMIZE_DOUBLE_HIT:
    case MOVE_EFFECT_RANDOM_PRIMARY_STATUS_HIT:
    case MOVE_EFFECT_PREVENT_HEALING_HIT: // Psychic Noise
    case MOVE_EFFECT_POISON_MULTI_HIT: // twineedle
    case MOVE_EFFECT_HIGH_CRITICAL_BURN_HIT: // blaze kick
    case MOVE_EFFECT_HIGH_CRITICAL_POISON_HIT: // cross poison
    case MOVE_EFFECT_RECOIL_BURN_HIT: // flare blitz
    case MOVE_EFFECT_RECOIL_PARALYZE_HIT:
        isBoosted = TRUE;
        break;
    default:
        break;
    }

    switch (moveno) {
    case MOVE_SPARKLING_ARIA:
    case MOVE_SPIRIT_SHACKLE:
    case MOVE_ANCHOR_SHOT:
    case MOVE_CEASELESS_EDGE:
    case MOVE_STONE_AXE:
    case MOVE_ELECTRO_SHOT:
        isBoosted = TRUE;
        break;
    default:
        break;
    }
    return isBoosted;
}

BOOL IsMoveUsefulSoundMove(u32 moveno)
{
    switch (moveno)
    {
    case MOVE_ALLURING_VOICE:
    case MOVE_BOOMBURST:
    case MOVE_BUG_BUZZ:
    case MOVE_CHATTER:
    case MOVE_CLANGING_SCALES:
    //case MOVE_CLANGOROUS_SOUL:
    case MOVE_CLANGOROUS_SOULBLAZE:
    //case MOVE_CONFIDE:
    //case MOVE_DISARMING_VOICE:
    //case MOVE_ECHOED_VOICE:
    case MOVE_EERIE_SPELL:
    case MOVE_GRASS_WHISTLE:
    //case MOVE_GROWL:
    //case MOVE_HEAL_BELL:
    //case MOVE_HOWL:
    case MOVE_HYPER_VOICE:
    //case MOVE_METAL_SOUND:
    //case MOVE_NOBLE_ROAR:
    case MOVE_OVERDRIVE:
    //case MOVE_PARTING_SHOT:
    case MOVE_PERISH_SONG:
    case MOVE_PSYCHIC_NOISE:
    case MOVE_RELIC_SONG:
    case MOVE_ROAR:
   //case MOVE_ROUND:
    //case MOVE_SCREECH:
    // MOVE_SHADOW_PANIC:
    case MOVE_SING:
    case MOVE_SNARL:
    case MOVE_SNORE:
    case MOVE_SPARKLING_ARIA:
    // case MOVE_SUPERSONIC:
    case MOVE_TORCH_SONG:
    case MOVE_UPROAR:
        return TRUE;
    default:
        return FALSE;
    }
}

BOOL IsMoveForceSwitching(u32 moveno)
{
    switch (moveno) {
    case MOVE_ROAR:
    case MOVE_WHIRLWIND:
    case MOVE_DRAGON_TAIL:
    case MOVE_CIRCLE_THROW:
        return TRUE;
    default:
        return FALSE;
    }
}

int LONG_CALL BattleAI_AdjustUnusualMoveDamage(u32 attackerLevel, u32 attackerHP, u32 defenderHP, u32 damage, u32 moveEffect, u32 attackerAbility, u32 attackerItem)
{
    // struct BattleStruct* ctx = bsys->sp;
    switch (moveEffect) {
    case MOVE_EFFECT_UP_TO_10_HITS:
        if (attackerAbility == ABILITY_SKILL_LINK) {
            return damage * 10;
        } else if (attackerItem == ITEM_LOADED_DICE) {
            return damage *= 5; // 4-10
        }
        return damage *= 3;
    case MOVE_EFFECT_HIT_THREE_TIMES_ALWAYS_CRITICAL: // surge Strikes
    case MOVE_EFFECT_HIT_THREE_TIMES: // triple dive
    case MOVE_EFFECT_HIT_THREE_TIMES_INCREMENT_BASE_POWER_10: // triple kick
        return damage *= 3;
    case MOVE_EFFECT_HIT_THREE_TIMES_INCREMENT_BASE_POWER_20: // triple axel
        return damage *= 6;
    case MOVE_EFFECT_MULTI_HIT: // 2-5 hit moves
        if (attackerAbility == ABILITY_SKILL_LINK) {
            return damage * 5;
        } else if (attackerItem == ITEM_LOADED_DICE) {
            return damage *= 4; // 4-5
        }
        return damage *= 3;
    case MOVE_EFFECT_LEVEL_DAMAGE_FLAT: // night shade, seismic toss
    case MOVE_EFFECT_RANDOM_DAMAGE_1_TO_150_LEVEL: // psywave
        return attackerLevel;
    case MOVE_EFFECT_10_DAMAGE_FLAT: // sonic boom
        return 20;
    case MOVE_EFFECT_40_DAMAGE_FLAT: // dragon rage
        return 40;
    case MOVE_EFFECT_HIT_TWICE_AND_FLINCH: // double Iron bash
    case MOVE_EFFECT_POISON_MULTI_HIT: // twinneedle
    case MOVE_EFFECT_HIT_TWICE: // double hit, dual wingbeat, etc...
        return damage *= 2;
    case MOVE_EFFECT_HALVE_HP: // super fang, nature's madness
        return defenderHP / 2;
    case MOVE_EFFECT_AVERAGE_HP: // pain split
    {
        if (attackerHP < defenderHP) {
            return defenderHP - ((attackerHP + defenderHP) / 2);
        } else {
            return 0;
        }
    }
    case MOVE_EFFECT_SET_HP_EQUAL_TO_USER: // endeavor
    {
        if (attackerHP < defenderHP) {
            return defenderHP - attackerHP;
        } else {
            return 0;
        }
    }
    }
    return damage;
}

BOOL LONG_CALL BattleAI_IsKnockOffPoweredUp(struct AI_sDamageCalc *defender)
{
    if (defender->item == ITEM_NONE) {
        return FALSE;
    }
    if (defender->species == SPECIES_SLOWBRO && defender->item == ITEM_SLOWBRONITE && defender->form == 2) {
        return TRUE;
    } else {
        return CanItemBeRemovedFromSpecies(defender->species, defender->item); // incorrectly does not see MAIL, but who cares?
    }
}

int LONG_CALL BattleAI_GetDynamicMoveType(struct BattleSystem *bsys, struct BattleStruct *ctx, struct AI_sDamageCalc *attacker, int moveNo)
{
    int type = ctx->moveTbl[moveNo].type;

    switch (moveNo) {
    case MOVE_NATURAL_GIFT:
        //https://github.com/pret/pokeheartgold/blob/29282f7bb45946dee63475022a8d506092bc3748/src/battle/overlay_12_0224E4FC.c#L4600
        //type = GetNaturalGiftType(ctx, battlerId);
        type = GetItemData(attacker->item, ITEM_PARAM_NATURAL_POWER_TYPE, 5); //TODO: check heap
        break;
    case MOVE_JUDGMENT:
        switch (attacker->item_held_effect) {
        case HOLD_EFFECT_ARCEUS_FIGHTING:
            type = TYPE_FIGHTING;
            break;
        case HOLD_EFFECT_ARCEUS_FLYING:
            type = TYPE_FLYING;
            break;
        case HOLD_EFFECT_ARCEUS_POISON:
            type = TYPE_POISON;
            break;
        case HOLD_EFFECT_ARCEUS_GROUND:
            type = TYPE_GROUND;
            break;
        case HOLD_EFFECT_ARCEUS_ROCK:
            type = TYPE_ROCK;
            break;
        case HOLD_EFFECT_ARCEUS_BUG:
            type = TYPE_BUG;
            break;
        case HOLD_EFFECT_ARCEUS_GHOST:
            type = TYPE_GHOST;
            break;
        case HOLD_EFFECT_ARCEUS_STEEL:
            type = TYPE_STEEL;
            break;
        case HOLD_EFFECT_ARCEUS_FIRE:
            type = TYPE_FIRE;
            break;
        case HOLD_EFFECT_ARCEUS_WATER:
            type = TYPE_WATER;
            break;
        case HOLD_EFFECT_ARCEUS_GRASS:
            type = TYPE_GRASS;
            break;
        case HOLD_EFFECT_ARCEUS_ELECTRIC:
            type = TYPE_ELECTRIC;
            break;
        case HOLD_EFFECT_ARCEUS_PSYCHIC:
            type = TYPE_PSYCHIC;
            break;
        case HOLD_EFFECT_ARCEUS_ICE:
            type = TYPE_ICE;
            break;
        case HOLD_EFFECT_ARCEUS_DRAGON:
            type = TYPE_DRAGON;
            break;
        case HOLD_EFFECT_ARCEUS_DARK:
            type = TYPE_DARK;
            break;
        case HOLD_EFFECT_ARCEUS_FAIRY:
            type = TYPE_FAIRY;
            break;
        // TODO: handle Blank Plate, Legend Plate, Z-Crystals
        default:
            type = TYPE_NORMAL;
            break;
        }
        break;
    case MOVE_HIDDEN_POWER:
        type = attacker->hiddenPowerType;
        break;
    case MOVE_WEATHER_BALL:
        if (!CheckSideAbility(bsys, ctx, CHECK_ABILITY_ALL_HP, 0, ABILITY_CLOUD_NINE) && !CheckSideAbility(bsys, ctx, CHECK_ABILITY_ALL_HP, 0, ABILITY_AIR_LOCK)) {
            if (ctx->field_condition & FIELD_CONDITION_WEATHER) {
                if (ctx->field_condition & WEATHER_RAIN_ANY) {
                    type = TYPE_WATER;
                }
                if (ctx->field_condition & WEATHER_SANDSTORM_ANY) {
                    type = TYPE_ROCK;
                }
                if (ctx->field_condition & WEATHER_SUNNY_ANY) {
                    type = TYPE_FIRE;
                }
                if (ctx->field_condition & WEATHER_HAIL_ANY) {
                    type = TYPE_ICE;
                }
                // BUG: If the weather is foggy, then type doesn't get set properly before being returned
                // BUGFIX
                if (ctx->field_condition & FIELD_STATUS_FOG) {
                    type = TYPE_NORMAL;
                }
                if (ctx->field_condition & WEATHER_SHADOWY_AURA_ANY) {
                    type = TYPE_TYPELESS;
                }
            }
        }
        break;
    case MOVE_TECHNO_BLAST:
        switch (attacker->item_held_effect) {
        case HOLD_EFFECT_BURN_DRIVE:
            type = TYPE_FIRE;
            break;
        case HOLD_EFFECT_DOUSE_DRIVE:
            type = TYPE_WATER;
            break;
        case HOLD_EFFECT_SHOCK_DRIVE:
            type = TYPE_ELECTRIC;
            break;
        case HOLD_EFFECT_CHILL_DRIVE:
            type = TYPE_ICE;
            break;
        default:
            type = TYPE_NORMAL;
            break;
        }
        break;
    case MOVE_REVELATION_DANCE:
        //TODO
        break;
    case MOVE_MULTI_ATTACK:
        switch (attacker->item_held_effect) {
        case HOLD_EFFECT_FIGHTING_MEMORY:
            type = TYPE_FIGHTING;
            break;
        case HOLD_EFFECT_FLYING_MEMORY:
            type = TYPE_FLYING;
            break;
        case HOLD_EFFECT_POISON_MEMORY:
            type = TYPE_POISON;
            break;
        case HOLD_EFFECT_GROUND_MEMORY:
            type = TYPE_GROUND;
            break;
        case HOLD_EFFECT_ROCK_MEMORY:
            type = TYPE_ROCK;
            break;
        case HOLD_EFFECT_BUG_MEMORY:
            type = TYPE_BUG;
            break;
        case HOLD_EFFECT_GHOST_MEMORY:
            type = TYPE_GHOST;
            break;
        case HOLD_EFFECT_STEEL_MEMORY:
            type = TYPE_STEEL;
            break;
        case HOLD_EFFECT_FIRE_MEMORY:
            type = TYPE_FIRE;
            break;
        case HOLD_EFFECT_WATER_MEMORY:
            type = TYPE_WATER;
            break;
        case HOLD_EFFECT_GRASS_MEMORY:
            type = TYPE_GRASS;
            break;
        case HOLD_EFFECT_ELECTRIC_MEMORY:
            type = TYPE_ELECTRIC;
            break;
        case HOLD_EFFECT_PSYCHIC_MEMORY:
            type = TYPE_PSYCHIC;
            break;
        case HOLD_EFFECT_ICE_MEMORY:
            type = TYPE_ICE;
            break;
        case HOLD_EFFECT_DRAGON_MEMORY:
            type = TYPE_DRAGON;
            break;
        case HOLD_EFFECT_DARK_MEMORY:
            type = TYPE_DARK;
            break;
        case HOLD_EFFECT_FAIRY_MEMORY:
            type = TYPE_FAIRY;
            break;
        default:
            type = TYPE_NORMAL;
            break;
        }
        break;
    case MOVE_AURA_WHEEL:
        if (attacker->species == SPECIES_MORPEKO) {
            switch (attacker->form) {
            // SPECIES_MORPEKO
            case 0:
                type = TYPE_ELECTRIC;
                break;
            // SPECIES_MORPEKO_HANGRY
            case 1:
                type = TYPE_DARK;
                break;

            default:
                // Aura Wheel can only be successfully used by Morpeko (or a Pokémon that has transformed into Morpeko). This line does not prevent the move from being used!!!
                type = TYPE_TYPELESS;
                break;
            }
        } else {
            // Aura Wheel can only be successfully used by Morpeko (or a Pokémon that has transformed into Morpeko). This line does not prevent the move from being used!!!
            type = TYPE_TYPELESS;
        }
        break;
    case MOVE_TERRAIN_PULSE:
        type = TYPE_NORMAL;
        if (ctx->terrainOverlay.numberOfTurnsLeft > 0) {
            switch (ctx->terrainOverlay.type) {
            case GRASSY_TERRAIN:
                type = TYPE_GRASS;
                break;
            case ELECTRIC_TERRAIN:
                type = TYPE_ELECTRIC;
                break;
            case MISTY_TERRAIN:
                type = TYPE_FAIRY;
                break;
            case PSYCHIC_TERRAIN:
                type = TYPE_PSYCHIC;
                break;
            default:
                break;
            }
        }
        break;
    case MOVE_TERA_BLAST:
    case MOVE_TERA_STARSTORM:
        //TODO
        break;
    case MOVE_RAGING_BULL:
        if (attacker->species == SPECIES_TAUROS) {
            switch (attacker->form) {
            // SPECIES_TAUROS_COMBAT
            case 1:
                type = TYPE_FIGHTING;
                break;
            // SPECIES_TAUROS_BLAZE
            case 2:
                type = TYPE_FIRE;
                break;
            // SPECIES_TAUROS_AQUA
            case 3:
                type = TYPE_WATER;
                break;

            default:
                type = TYPE_NORMAL;
                break;
            }
        } else {
            type = TYPE_NORMAL;
        }
        break;
    case MOVE_IVY_CUDGEL:
        if (attacker->species == SPECIES_OGERPON) {
            switch (attacker->form) {
            // SPECIES_OGERPON
            case 0:
                type = TYPE_GRASS;
                break;
            // SPECIES_OGERPON_WELLSPRING_MASK
            case 1:
                type = TYPE_WATER;
                break;
            // SPECIES_OGERPON_HEARTHFLAME_MASK
            case 2:
                type = TYPE_FIRE;
                break;
            // SPECIES_OGERPON_CORNERSTONE_MASK
            case 3:
                type = TYPE_ROCK;
                break;

            default:
                type = TYPE_GRASS;
                break;
            }
        } else {
            type = TYPE_GRASS;
        }
        break;
    default:
        type = TYPE_NORMAL;
        break;
    }

    u32 typeLocal;

    if (attacker->ability == ABILITY_NORMALIZE) {
        typeLocal = TYPE_NORMAL;
    } else if (ctx->moveTbl[moveNo].type == TYPE_NORMAL && MoveIsAffectedByNormalizeVariants(moveNo)) {
        if (attacker->ability == ABILITY_PIXILATE) {
            typeLocal = TYPE_FAIRY;
        } else if (attacker->ability == ABILITY_REFRIGERATE) {
            typeLocal = TYPE_ICE;
        } else if (attacker->ability == ABILITY_AERILATE) {
            typeLocal = TYPE_FLYING;
        } else if (attacker->ability == ABILITY_GALVANIZE) {
            typeLocal = TYPE_ELECTRIC;
        } else // needs to be for sure initialized
        {
            typeLocal = TYPE_NORMAL;
        }
    } else if (type) {
        typeLocal = type;
    } else {
        typeLocal = ctx->moveTbl[moveNo].type;
    }

     if (attacker->ability == ABILITY_LIQUID_VOICE && IsMoveSoundBased(moveNo)) {
        typeLocal = TYPE_WATER;
    }

    return typeLocal;
}

BOOL LONG_CALL CanAttackerOneShotDefender(u32 attackerDamage, u8 split, u32 moveno, struct AI_sDamageCalc *attacker, struct AI_sDamageCalc *defender)
{
    BOOL isMoveMultihit = IsMultiHitMove(moveno);
    BOOL canOneShot = TRUE;

    if (attackerDamage >= defender->hp) {
        if (defender->hp == defender->maxhp) {

            if (!isMoveMultihit
                && (defender->item == ITEM_FOCUS_SASH
                    || (!attacker->hasMoldBreaker && defender->ability == ABILITY_STURDY))) {
                canOneShot = FALSE;
            }

            if (!attacker->hasMoldBreaker && defender->ability == ABILITY_DISGUISE) { // SPECIES_MIMIKYU
                canOneShot = FALSE;
            }
        }

    } else {
        canOneShot = FALSE;
    }

    if (!attacker->hasMoldBreaker && defender->ability == ABILITY_ICE_FACE && defender->form == 0 && split == SPLIT_PHYSICAL) { // SPECIES_EISCUE
        canOneShot = FALSE;
    }

    return canOneShot;
}

BOOL BattleAI_AttackerHasOnlyIneffectiveMoves(struct BattleStruct *ctx, u32 attacker, int knownMoves, u32 effectiveness[4])
{
    BOOL onlyIneffectiveMoves = TRUE;
    for (int k = 0; k < knownMoves; ++k) {
        u32 attackerMoveno = ctx->battlemon[attacker].move[k];
        struct BattleMove attackerMove = ctx->moveTbl[attackerMoveno];
        if (attackerMove.split != SPLIT_STATUS && attackerMove.power > 0 && ctx->battlemon[attacker].pp[k]) {
            switch (effectiveness[k]) {
            case TYPE_MUL_NORMAL:
            case TYPE_MUL_SUPER_EFFECTIVE:
            case TYPE_MUL_DOUBLE_SUPER_EFFECTIVE:
            case TYPE_MUL_TRIPLE_SUPER_EFFECTIVE:
                onlyIneffectiveMoves = FALSE;
                break;
            default:
                break;
            }
        }
    }
    return onlyIneffectiveMoves;
}

BOOL LONG_CALL IsChoicedMoveConsidedUseless(u32 moveno, u8 split)
{
    BOOL isUseless = FALSE;
    if (split == SPLIT_STATUS) {
        isUseless = TRUE;
    }

    switch (moveno) {
    case MOVE_FAKE_OUT:
    case MOVE_FIRST_IMPRESSION:
    default:
        break;
    }
    return isUseless;
}

BOOL LONG_CALL IsBattleMonSlowerThanOpposition(struct BattleSystem *bsys, u8 slot, BOOL isDoubleBattle)
{
    struct BattleStruct *ctx = bsys->sp;
    BOOL isSlower = FALSE;
    if (ctx->effectiveSpeed[slot] < ctx->effectiveSpeed[BATTLER_OPPONENT(slot)]) {
        isSlower = TRUE;
    }
    if (isDoubleBattle && ctx->effectiveSpeed[slot] < ctx->effectiveSpeed[BATTLER_ACROSS(slot)]) {
        isSlower = TRUE;
    }
    return isSlower;
}

BOOL LONG_CALL IsPartyPokemonGrounded(struct BattleStruct *sp, struct PartyPokemon *pp)
{
    u16 item = GetMonData(pp, MON_DATA_HELD_ITEM, 0);

    u8 holdeffect = BattleItemDataGet(sp, item, 1);

    if ((GetMonData(pp, MON_DATA_ABILITY, 0) != ABILITY_LEVITATE && holdeffect != HOLD_EFFECT_UNGROUND_DESTROYED_ON_HIT // not holding Air Balloon
            && !(GetMonData(pp, MON_DATA_TYPE_1, 0) == TYPE_FLYING) && !(GetMonData(pp, MON_DATA_TYPE_2, 0) == TYPE_FLYING))
        || (holdeffect == HOLD_EFFECT_SPEED_DOWN_GROUNDED // holding Iron Ball
            || (sp->field_condition & FIELD_STATUS_GRAVITY))) {
        return TRUE;
    }

    return FALSE;
}

void LONG_CALL SetupStateVariables(struct BattleSystem *bsys, u32 attacker, u32 defender, struct AIContext *ai)
{
    struct BattleStruct *ctx = bsys->sp;
    u8 critical = 0;
    u8 speedCalc;
    u32 effectivenessFlag = 0;

    FillDamageStructFromBattleMon(bsys, ctx, &ai->attackerMon, attacker);
    FillDamageStructFromBattleMon(bsys, ctx, &ai->defenderMon, defender);
    ai->isDoubleBattle = FALSE;
    if (BattleTypeGet(bsys) & (BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_TAG)) {
        ai->isDoubleBattle = TRUE;
    }
    ai->isAllyAlive = FALSE;
    if (ai->isDoubleBattle && ctx->battlemon[BATTLER_ALLY(attacker)].hp) {
        ai->isAllyAlive = TRUE;
        FillDamageStructFromBattleMon(bsys, ctx, &ai->defenderAlly, BATTLER_ALLY(attacker));
    }

    ai->attacker = attacker;
    ai->defender = defender;
    ai->attackerSide = BATTLER_IS_ENEMY(ai->attacker);
    ai->defenderSide = BATTLER_IS_ENEMY(ai->defender);

    ai->attackerLastUsedMove = ctx->waza_no_old[ai->attacker];
    ai->defenderLastUsedMove = ctx->waza_no_old[ai->defender];
    ai->defenderLastUsedMoveEffect = ctx->moveTbl[ai->defenderLastUsedMove].effect;
    ai->defenderTurnsOnField = ctx->total_turn - ctx->battlemon[ai->defender].moveeffect.fakeOutCount;
    ai->attackerTurnsOnField = ctx->total_turn - ctx->battlemon[attacker].moveeffect.fakeOutCount;

    ai->defenderMovesFirst = 0;
    ai->attackerMovesFirst = 0;
    ai->isSpeedTie = 0;
    ai->maxDamageReceived = 0;
    ai->attackerRolledMaxDamage = 0;

    ai->defenderKnowsThawingMove = BattlerKnowsThawingMove(bsys, ai->defender, ai);

    ai->isPartnerGrounded = FALSE;
    if (ai->isDoubleBattle && ctx->battlemon[BATTLER_ALLY(attacker)].hp) {
        ai->isPartnerGrounded = IsClientGrounded(ctx, BATTLER_ALLY(attacker));
    }

    ai->defenderHasAtleastOneUsefulSoundMove = FALSE;
    ai->defenderCanForceSwitching = FALSE;

    speedCalc = CalcSpeed(bsys, ctx, defender, attacker, CALCSPEED_FLAG_NO_PRIORITY); // checks actual turn order with field state considered
    // evaluates to 0 if ai->defender > attacker (false)
    // and 1 if ai->defender < attacker (true)
    // if speed tie, then 2.

    if (speedCalc == 0) {
        ai->defenderMovesFirst = 1;
    } else {
        ai->attackerMovesFirst = 1;
        if (speedCalc == 2) {
            ai->isSpeedTie = 1;
        }
    }
    ai->attackerMon.speed = ctx->effectiveSpeed[attacker];
    ai->defenderMon.speed = ctx->effectiveSpeed[defender];

#ifdef BATTLE_DEBUG_OUTPUT
    debug_printf("SpeedCalc %d, defMovesFirst %d, attMovesFirst %d \n", speedCalc, ai->defenderMovesFirst, ai->attackerMovesFirst);
#endif // BATTLE_DEBUG_OUTPUT

    ai->isDefenderIncapacitated = FALSE;
    if ((ai->defenderMon.condition & STATUS_SLEEP)
        || ((ai->defenderMon.condition & STATUS_FREEZE) && !ai->defenderKnowsThawingMove)
        || (ai->defenderMon.condition2 & STATUS2_RECHARGE)
        || ST_CheckIfInTruant(ctx, defender)) {
        ai->isDefenderIncapacitated = TRUE;
    }

    BOOL isDefenderImmuneToAnyStatus = FALSE;
    if ((ai->defenderMon.condition & STATUS_ALL)
        || (ai->defenderMon.ability == ABILITY_GOOD_AS_GOLD) || (ai->defenderMon.ability == ABILITY_COMATOSE) || (ai->defenderMon.ability == ABILITY_PURIFYING_SALT)
        || (ai->defenderMon.ability == ABILITY_SHIELDS_DOWN && ai->defenderMon.percenthp > 50)
        || (ai->defenderMon.ability == ABILITY_LEAF_GUARD && ctx->field_condition & WEATHER_SUNNY_ANY)
        || (ai->defenderMon.ability == ABILITY_HYDRATION && ctx->field_condition & WEATHER_RAIN_ANY)
        || (ctx->side_condition[ai->defenderSide] & SIDE_STATUS_SAFEGUARD)) {
        isDefenderImmuneToAnyStatus = TRUE;
    }

    ai->defenderImmuneToPoison = FALSE;
    if (isDefenderImmuneToAnyStatus
        || ai->defenderMon.ability == ABILITY_MAGIC_GUARD || ai->defenderMon.ability == ABILITY_IMMUNITY || ai->defenderMon.ability == ABILITY_POISON_HEAL
        || (ai->defenderMon.isGrounded && ctx->terrainOverlay.type == MISTY_TERRAIN)
        || ((HasType(ctx, ai->defender, TYPE_POISON) || HasType(ctx, ai->defender, TYPE_STEEL)) && ai->attackerMon.ability != ABILITY_CORROSION)) {
        ai->defenderImmuneToPoison = TRUE;
    }

    ai->defenderImmuneToParalysis = FALSE;
    if (isDefenderImmuneToAnyStatus || ai->defenderMon.ability == ABILITY_LIMBER || (ai->defenderMon.isGrounded && ctx->terrainOverlay.type == MISTY_TERRAIN) || HasType(ctx, ai->defender, TYPE_ELECTRIC)) {
        ai->defenderImmuneToParalysis = TRUE;
    }

    ai->defenderImmuneToBurn = FALSE;
    if (isDefenderImmuneToAnyStatus
        || ai->defenderMon.ability == ABILITY_WATER_VEIL || ai->defenderMon.ability == ABILITY_THERMAL_EXCHANGE
        || ai->defenderMon.ability == ABILITY_MAGIC_GUARD || ai->defenderMon.ability == ABILITY_WATER_BUBBLE
        || (ai->defenderMon.isGrounded && ctx->terrainOverlay.type == MISTY_TERRAIN)
        || HasType(ctx, ai->defender, TYPE_FIRE)) {
        ai->defenderImmuneToBurn = TRUE;
    }

    ai->defenderImmuneToSleep = FALSE;
    if (isDefenderImmuneToAnyStatus
        || ai->defenderMon.ability == ABILITY_VITAL_SPIRIT || ai->defenderMon.ability == ABILITY_INSOMNIA
        || (ai->defenderMon.isGrounded && (ctx->terrainOverlay.type == MISTY_TERRAIN || ctx->terrainOverlay.type == ELECTRIC_TERRAIN))) {
        ai->defenderImmuneToSleep = TRUE;
    }

    ai->partySizeAttacker = Battle_GetClientPartySize(bsys, attacker);
    ai->livingMembersAttacker = 0;
    ai->partySizeDefender = Battle_GetClientPartySize(bsys, ai->defender);
    ai->livingMembersDefender = 0;

    ai->monWithMegaInParty = FALSE;
    for (int i = 0; i < ai->partySizeAttacker; i++) {
        struct PartyPokemon *current = Battle_GetClientPartyMon(bsys, attacker, i);
        if (!(GetMonData(current, MON_DATA_HP, 0) == 0 || GetMonData(current, MON_DATA_SPECIES_OR_EGG, 0) == 0 || GetMonData(current, MON_DATA_SPECIES_OR_EGG, 0) == SPECIES_EGG)) {
            u32 item = GetMonData(current, MON_DATA_HELD_ITEM, 0);
            if (IS_ITEM_MEGA_STONE(item)) {
                ai->monWithMegaInParty = TRUE;
            }
            ai->livingMembersAttacker++;
        }
    }

    for (int i = 0; i < ai->partySizeDefender; i++) {
        struct PartyPokemon *current = Battle_GetClientPartyMon(bsys, ai->defender, i);
        if (!(GetMonData(current, MON_DATA_HP, 0) == 0 || GetMonData(current, MON_DATA_SPECIES_OR_EGG, 0) == 0 || GetMonData(current, MON_DATA_SPECIES_OR_EGG, 0) == SPECIES_EGG)) {
            ai->livingMembersDefender++;
        }
    }

    ai->attackerMovesKnown = GetBattlerLearnedMoveCount(bsys, ctx, attacker);

#ifdef BATTLE_DEBUG_OUTPUT
    int highestDamageMoveIndex = 0;
#endif
    ai->playerCanOneShotMonWithAnyMove = FALSE;
    for (int k = 0; k < GetBattlerLearnedMoveCount(bsys, ctx, ai->defender); k++) {
        struct AI_damage damages = { 0 };
        u32 defenderMoveno = ctx->battlemon[defender].move[k];
        struct BattleMove defenderMove = ctx->moveTbl[defenderMoveno];

        if (defenderMove.split != SPLIT_STATUS && defenderMove.power && ctx->battlemon[defender].pp[k]) {
            damages.damageRoll = BattleAI_CalcDamage(bsys, ctx, defenderMoveno, ctx->side_condition[BATTLER_IS_ENEMY(defender)], ctx->field_condition, defenderMove.power, defenderMove.type, critical, defender, attacker, &damages, &ai->defenderMon, &ai->attackerMon);

            damages.damageRoll = BattleAI_AdjustUnusualMoveDamage(ai->defenderMon.level, ai->defenderMon.hp, ai->attackerMon.hp, damages.damageRoll, defenderMove.effect, ai->defenderMon.ability, ai->defenderMon.item);
            for (int u = 0; u < 16; u++) {
                damages.damageRange[u] = BattleAI_AdjustUnusualMoveDamage(ai->defenderMon.level, ai->defenderMon.hp, ai->attackerMon.hp, damages.damageRange[u], defenderMove.effect, ai->defenderMon.ability, ai->defenderMon.item);
            }

            BOOL playerCanOneShotAiMon = CanAttackerOneShotDefender(damages.damageRoll, defenderMove.split, defenderMoveno, &ai->defenderMon, &ai->attackerMon);
            if (playerCanOneShotAiMon) {
                ai->playerCanOneShotMonWithAnyMove = TRUE;
                ai->playerCanOneShotMonWithMove[k] = TRUE;
            }

            if (damages.damageRoll > ai->maxDamageReceived) {
                ai->maxDamageReceived = damages.damageRoll;
#ifdef BATTLE_DEBUG_OUTPUT
                highestDamageMoveIndex = k;
#endif
            }
            if (defenderMove.split == SPLIT_SPECIAL) {
                ai->defenderHasAtleastOneSpecialMove = 1;
            } else {
                ai->defenderHasAtleastOnePhysicalMove = 1;
            }
            if (IsMoveUsefulSoundMove(defenderMoveno) && (defenderMove.split == SPLIT_STATUS || damages.moveEffectiveness >= TYPE_MUL_NORMAL)) {
                ai->defenderHasAtleastOneUsefulSoundMove = TRUE;
            }
            if (IsMoveForceSwitching(defenderMoveno)) {
                ai->defenderCanForceSwitching = TRUE;
            }

            debug_printf("Receiving from move %d: %3d is [%4d-%4d], roll %4d > att.HP %d\n", k, defenderMoveno, damages.damageRange[0], damages.damageRange[15], damages.damageRoll, ai->attackerMon.hp);
        }
    }

#ifdef BATTLE_DEBUG_OUTPUT
    debug_printf("Overall Max damage received from %i:%i is %d > %d att.HP\n", highestDamageMoveIndex, ctx->battlemon[ai->defender].move[highestDamageMoveIndex], ai->maxDamageReceived, ai->attackerMon.hp);
#endif // BATTLE_DEBUG_OUTPUT

    ai->attackerHasAttackingMoves = FALSE;
    ai->monCanOneShotPlayerWithAnyMove = FALSE;
    for (int j = 0; j < ai->attackerMovesKnown; j++) {
        struct AI_damage damages = { 0 };
        u32 attackerMoveno = ctx->battlemon[attacker].move[j];
        struct BattleMove attackerMove = ctx->moveTbl[attackerMoveno];
        if (attackerMove.split == SPLIT_STATUS && ctx->battlemon[attacker].pp[j]) {
            u8 movetype = GetAdjustedMoveTypeBasics(ctx, attackerMoveno, ai->attackerMon.ability, attackerMove.type);
            ai->effectivenessOnPlayer[j] = BattleAI_GetTypeEffectiveness(bsys, ctx, attackerMoveno, movetype, &effectivenessFlag, &ai->attackerMon, &ai->defenderMon);
        } else if (attackerMove.power && ctx->battlemon[attacker].pp[j]) {
            ai->attackerHasAttackingMoves = TRUE;
            damages.damageRoll = BattleAI_CalcDamage(bsys, ctx, attackerMoveno, ctx->side_condition[BATTLER_IS_ENEMY(attacker)], ctx->field_condition, attackerMove.power, attackerMove.type, critical, attacker, defender, &damages, &ai->attackerMon, &ai->defenderMon);
            ai->effectivenessOnPlayer[j] = damages.moveEffectiveness;

            damages.damageRoll = BattleAI_AdjustUnusualMoveDamage(ai->attackerMon.level, ai->attackerMon.hp, ai->defenderMon.hp, damages.damageRoll, attackerMove.effect, ai->attackerMon.ability, ai->attackerMon.item);
            for (int u = 0; u < 16; u++) {
                damages.damageRange[u] = BattleAI_AdjustUnusualMoveDamage(ai->attackerMon.level, ai->attackerMon.hp, ai->defenderMon.hp, damages.damageRange[u], attackerMove.effect, ai->attackerMon.ability, ai->attackerMon.item);
            }

            BOOL aiMonCanOneshotPlayer = CanAttackerOneShotDefender(damages.damageRoll, attackerMove.split, attackerMoveno, &ai->attackerMon, &ai->defenderMon);

            ai->attackerRolledMoveDamages[j] = damages.damageRoll;
            if (aiMonCanOneshotPlayer) {
                ai->monCanOneShotPlayerWithAnyMove = TRUE;
                ai->monCanOneShotPlayerWithMove[j] = TRUE;
                ai->attackerRolledMoveDamages[j] = ai->defenderMon.hp; // cap killing move's damage at defender HP, so that all killing moves are treated equally as "highest damage"
            }
            debug_printf("Dealing with move %d: %3d is [%4d-%4d], roll %4d > def.HP %d\n", j, attackerMoveno, damages.damageRange[0], damages.damageRange[15], damages.damageRoll, ai->defenderMon.hp);

            if (ai->attackerRolledMoveDamages[j] > ai->attackerRolledMaxDamage) {
                ai->attackerRolledMaxDamage = ai->attackerRolledMoveDamages[j];
            }
        }
    }
}

int LONG_CALL BattleAI_PostKOSwitchIn_Internal(struct BattleSystem *bsys, int attacker, int *score, BOOL calcWithHighestDamageHit)
{
    debug_printf("BattleAI_PostKOSwitchIn_Internal %d\n", attacker);

    struct BattleStruct *ctx = bsys->sp;
    int battleType = BattleTypeGet(bsys);
    struct PartyPokemon *mon;

    struct AI_sDamageCalc attackerMon = { 0 };
    struct AI_sDamageCalc defenderMon = { 0 };

    u8 critical = 0;

    u8 speedCalc;
    u32 defender = BATTLER_OPPONENT(attacker); // default for singles -- updated in the doubles section
    u8 slot1, slot2;
    u16 moveno = 0;
    u32 monDealsRolledDamage[6] = { 0 };
    u32 monHighestDamageMoveno = 0;
    u32 monReceivingHighestDamageMoveno = 0;
    u32 monReceivesDamage[6] = { 0 };
    u16 switchInScore[6] = { 0 };
    int partySize = 0;
    int picked = 6; // in Order

    slot1 = attacker;
    slot2 = slot1;
    debug_printf("Targeting defender in slot %d(%d) with hp %d\n", defender, ctx->battlemon[defender].species, ctx->battlemon[defender].hp);

    if (battleType & (BATTLE_TYPE_TAG | BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE)) {
        slot2 = BATTLER_ALLY(attacker);
        if (ctx->battlemon[defender].hp == 0) {
            debug_printf("No target\n");
            return 6;
        }
    }
    
    FillDamageStructFromBattleMon(bsys, ctx, &defenderMon, defender);

    partySize = Battle_GetClientPartySize(bsys, attacker);
    for (int i = 0; i < partySize; i++) {
        mon = Battle_GetClientPartyMon(bsys, attacker, i);
        attackerMon.species = GetMonData(mon, MON_DATA_SPECIES_OR_EGG, 0);
        debug_printf("Slot %d:%d hp:%d,\n", i, attackerMon.species, GetMonData(mon, MON_DATA_HP, 0));
        debug_printf("sel_m1 %d, sel_m2 %d, switchSl1 %d, switchSl1 %d\n", ctx->sel_mons_no[slot1], ctx->sel_mons_no[slot2], ctx->aiSwitchedPartySlot[slot1], ctx->aiSwitchedPartySlot[slot2]);

        if (attackerMon.species != SPECIES_NONE && attackerMon.species != SPECIES_EGG && GetMonData(mon, MON_DATA_HP, 0)
            && i != ctx->sel_mons_no[slot1]
            && i != ctx->sel_mons_no[slot2]
            && i != ctx->aiSwitchedPartySlot[slot1]
            && i != ctx->aiSwitchedPartySlot[slot2]) {
            switchInScore[i] = 100;

            FillDamageStructFromPartyMon(bsys, ctx, &attackerMon, mon);

            speedCalc = BattleAI_CalcSpeed(bsys, ctx, defender, mon, CALCSPEED_FLAG_NO_PRIORITY); // checks actual turn order with field state considered

            for (u8 j = 0; j < CLIENT_MAX; ++j) {
                struct AI_damage damages = { 0 };
                moveno = GetMonData(mon, MON_DATA_MOVE1 + j, NULL);
                u8 pp = GetMonData(mon, MON_DATA_MOVE1PP + j, NULL);
                if (moveno != MOVE_NONE && pp) {
                    struct BattleMove attackerMove = ctx->moveTbl[moveno];

                    if (attackerMove.split != SPLIT_STATUS && attackerMove.power) {
                        damages.damageRoll = BattleAI_CalcDamage(bsys, ctx, moveno, ctx->side_condition[BATTLER_IS_ENEMY(attacker)], ctx->field_condition, attackerMove.power, attackerMove.type, critical, attacker, defender, &damages, &attackerMon, &defenderMon);

                        damages.damageRoll = BattleAI_AdjustUnusualMoveDamage(attackerMon.level, attackerMon.hp, defenderMon.hp, damages.damageRoll, attackerMove.effect, attackerMon.ability, attackerMon.item);
                        for (int u = 0; u < 16; u++) {
                            damages.damageRange[u] = BattleAI_AdjustUnusualMoveDamage(attackerMon.level, attackerMon.hp, defenderMon.hp, damages.damageRange[u], attackerMove.effect, attackerMon.ability, attackerMon.item);
                        }

                        if (damages.damageRoll > monDealsRolledDamage[i]) {
                            monHighestDamageMoveno = moveno;
                            monDealsRolledDamage[i] = damages.damageRoll;
                        }
                    }
                    debug_printf("Dealing with move %d: %3d deals [%4d-%4d], roll %4d > def.HP %d\n", j, moveno, damages.damageRange[0], damages.damageRange[15], damages.damageRoll, defenderMon.hp);
                }
            }

            for (int k = 0; k < GetBattlerLearnedMoveCount(bsys, ctx, defender); ++k) {
                struct AI_damage damages = { 0 };
                u32 defenderMoveno = ctx->battlemon[defender].move[k];
                struct BattleMove defenderMove = ctx->moveTbl[defenderMoveno];

                if (defenderMove.split != SPLIT_STATUS && defenderMove.power && ctx->battlemon[defender].pp[k]) {
                    damages.damageRoll = BattleAI_CalcDamage(bsys, ctx, defenderMoveno, ctx->side_condition[BATTLER_IS_ENEMY(defender)], ctx->field_condition, defenderMove.power, defenderMove.type, critical, defender, attacker, &damages, &defenderMon, &attackerMon);

                    damages.damageRoll = BattleAI_AdjustUnusualMoveDamage(defenderMon.level, defenderMon.hp, attackerMon.hp, damages.damageRoll, defenderMove.effect, defenderMon.ability, defenderMon.item);
                    for (int u = 0; u < 16; u++) {
                        damages.damageRange[u] = BattleAI_AdjustUnusualMoveDamage(defenderMon.level, defenderMon.hp, attackerMon.hp, damages.damageRange[u], defenderMove.effect, defenderMon.ability, defenderMon.item);
                    }

                    if (damages.damageRoll > monReceivesDamage[i]) {
                        monReceivingHighestDamageMoveno = defenderMoveno;
                        monReceivesDamage[i] = damages.damageRoll;
                    }
                }
                debug_printf("Receiving from move %d: %3d is [%4d-%4d], roll %4d > att.HP %d\n", k, defenderMoveno, damages.damageRange[0], damages.damageRange[15], damages.damageRoll, attackerMon.hp);
            }

            // TODO stealth rocks, spikes, toxic spikes, etc...
            u8 aiMonCanOneshotPlayer = CanAttackerOneShotDefender(monDealsRolledDamage[i], ctx->moveTbl[monHighestDamageMoveno].split, monHighestDamageMoveno, &attackerMon, &defenderMon);
            u8 playerCanOneShotAiMon = CanAttackerOneShotDefender(monReceivesDamage[i], ctx->moveTbl[monReceivingHighestDamageMoveno].split, monReceivingHighestDamageMoveno, &defenderMon, &attackerMon);
            u16 partyMonPercentDamageDealt = (100 * monDealsRolledDamage[i] / defenderMon.hp);
            u16 partyMonPercentDamageReceived = (100 * monReceivesDamage[i] / attackerMon.hp);

            debug_printf("SwitchScore: SpeedCalc %d. Attacker %d deals %dpct to defender %d. Receives %dpct", speedCalc, attacker, (defenderMon.hp > 0 ? (100 * monDealsRolledDamage[i] / defenderMon.hp) : 0), defender, (100 * monReceivesDamage[i] / attackerMon.hp));
            if (calcWithHighestDamageHit) {
                if (partyMonPercentDamageReceived >= 100) {
                    switchInScore[i] -= 10;
                } else if (speedCalc == 0 && (2 * partyMonPercentDamageReceived >= 100)) {
                    switchInScore[i] -= 10;
                }
                if (IS_ITEM_MEGA_STONE(attackerMon.item)) {
                    switchInScore[i] -= 10;
                }
            }

            if (!playerCanOneShotAiMon && (attackerMon.species == SPECIES_WYNAUT || attackerMon.species == SPECIES_WOBBUFFET)) {
                switchInScore[i] += 2;
            }

            if (speedCalc > 0) {
                if (aiMonCanOneshotPlayer) {
                    switchInScore[i] += 5;
                } else if (partyMonPercentDamageDealt >= partyMonPercentDamageReceived) {
                    switchInScore[i] += 3;
                } else {
                    switchInScore[i] += 1;
                }

                if (attackerMon.species == SPECIES_DITTO) {
                    switchInScore[i] += 2;
                }
            } else {
                if (aiMonCanOneshotPlayer && !playerCanOneShotAiMon) {
                    switchInScore[i] += 4;
                } else if (partyMonPercentDamageDealt > partyMonPercentDamageReceived) {
                    switchInScore[i] += 2;
                } else if (playerCanOneShotAiMon) {
                    switchInScore[i] -= 1;
                }
            }
            debug_printf(": %d\n", switchInScore[i]);
            // default += 0;
        }
    }

    u16 currentScore = switchInScore[0];
    for (int i = 0; i < partySize; i++) {
        if (switchInScore[i] > currentScore) {
            picked = i;
            *score = switchInScore[i];
            currentScore = switchInScore[i];
        }
    }
    for (int i = 0; i < partySize; i++) {
        debug_printf("%i ", switchInScore[i]);
    }
    debug_printf("-> picked %i\n", picked);

    return picked;
}
