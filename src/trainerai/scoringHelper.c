#include "../../include/types.h"
#include "../../include/config.h"
#include "../../include/battle.h"
#include "../../include/mega.h"
#include "../../include/pokemon.h"
#include "../../include/item.h"
#include "../../include/trainer_ai.h"
#include "../../include/constants/ability.h"
#include "../../include/constants/species.h"
#include "../../include/constants/battle_script_constants.h"
#include "../../include/constants/battle_message_constants.h"
#include "../../include/constants/move_effects.h"
#include "../../include/constants/item.h"
#include "../../include/custom/custom_ai.h"
#include "../../include/overlay.h"
#include "../../include/constants/file.h"


int LONG_CALL BattlerPositiveStatChangesSum(struct BattleSystem *bsys, u32 battler, struct AIContext *ai UNUSED)
{
    struct BattleStruct *ctx = bsys->sp;
    int statSum = 0;

    for (int stat = 0; stat < STAT_MAX; stat++) {
        if (ctx->battlemon[battler].states[stat] > 6) {
            statSum += ctx->battlemon[battler].states[stat];
        }
    }
    return statSum;
}

BOOL LONG_CALL BattlerKnowsMove(struct BattleSystem *bsys, u32 battler, u32 move, struct AIContext *ai UNUSED)
{
    BOOL knowsMove = FALSE;
    struct BattleStruct *ctx = bsys->sp;
    for (int i = 0; i < 4; i++) {
        u32 battler_move_check = ctx->battlemon[battler].move[i];
        if (battler_move_check == move) {
            knowsMove = TRUE;
            break;
        }
    }
    return knowsMove;
}

BOOL LONG_CALL BattlerHasSoundBasedMove(struct BattleSystem *bsys, u32 battler, struct AIContext *ai UNUSED)
{
    BOOL hasMove = FALSE;
    struct BattleStruct *ctx = bsys->sp;
    for (int i = 0; i < CLIENT_MAX; ++i) {
        u32 move = ctx->battlemon[battler].move[i];
        if (IsMoveSoundBased(move)) {
            hasMove = TRUE;
            break;
        }
    }
    return hasMove;
}

BOOL LONG_CALL BattlerKnowsFlinchingMove(struct BattleSystem *bsys, u32 battler, struct AIContext *ai UNUSED)
{
    BOOL knowsMove = 0;
    struct BattleStruct *ctx = bsys->sp;
    for (int i = 0; i < CLIENT_MAX; ++i) {
        u32 move = ctx->battlemon[battler].move[i];
        switch (move) {
        case MOVE_AIR_SLASH:
        case MOVE_ASTONISH:
        case MOVE_BITE:
        case MOVE_BONE_CLUB:
        case MOVE_DARK_PULSE:
        case MOVE_DOUBLE_IRON_BASH:
        case MOVE_DRAGON_RUSH:
        case MOVE_EXTRASENSORY:
        case MOVE_FIERY_WRATH:
        case MOVE_FIRE_FANG:
        case MOVE_FLOATY_FALL:
        case MOVE_HEADBUTT:
        case MOVE_HEART_STAMP:
        case MOVE_HYPER_FANG:
        case MOVE_ICE_FANG:
        case MOVE_ICICLE_CRASH:
        case MOVE_IRON_HEAD:
        case MOVE_LOW_KICK:
        case MOVE_MOUNTAIN_GALE:
        case MOVE_NEEDLE_ARM:
        case MOVE_ROCK_SLIDE:
        case MOVE_ROLLING_KICK:
        case MOVE_STEAMROLLER:
        case MOVE_STOMP:
        case MOVE_THUNDER_FANG:
        case MOVE_TRIPLE_ARROWS:
        case MOVE_TWISTER:
        case MOVE_WATERFALL:
        case MOVE_ZEN_HEADBUTT:
        case MOVE_ZING_ZAP:
            knowsMove = TRUE;
            break;
        default:
            break;
        }
    }
    return knowsMove;
}

BOOL LONG_CALL BattlerKnowsThawingMove(struct BattleSystem *bsys, u32 battler, struct AIContext *ai UNUSED)
{
    BOOL knowsMove = 0;
    struct BattleStruct *ctx = bsys->sp;
    for (int i = 0; i < CLIENT_MAX; ++i) {
        u32 move = ctx->battlemon[battler].move[i];
        switch (move) {
        case MOVE_FLAME_WHEEL:
        case MOVE_SACRED_FIRE:
        case MOVE_FLARE_BLITZ:
        case MOVE_FUSION_FLARE:
        case MOVE_SCALD:
        case MOVE_STEAM_ERUPTION:
        case MOVE_BURN_UP:
        case MOVE_PYRO_BALL:
        case MOVE_SCORCHING_SANDS:
        case MOVE_MATCHA_GOTCHA:
            knowsMove = TRUE;
            break;
        default:
            break;
        }
    }
    return knowsMove;
}

BOOL LONG_CALL MonDiesFromResidualDamage(struct BattleStruct *ctx, u32 attacker, u32 attackerCondition, BOOL isSeeded)
{
    // TODO order with heal
    // https://pokemondb.net/pokebase/409424/at-the-end-of-each-turn-what-goes-first-healing-or-damaging
    BOOL diesFromResidual = FALSE;
    u32 hp = ctx->battlemon[attacker].hp;
    u32 maxHp = ctx->battlemon[attacker].maxhp;
    u32 ability = ctx->battlemon[attacker].ability;
    u32 item = ctx->battlemon[attacker].item;
    int damageReceived = 0;

    if (ctx->field_condition & WEATHER_SANDSTORM_ANY) {
        if (!HasType(ctx, attacker, TYPE_ROCK) && !HasType(ctx, attacker, TYPE_GROUND) && !HasType(ctx, attacker, TYPE_STEEL)
            && ability != ABILITY_SAND_FORCE && ability != ABILITY_SAND_RUSH && ability != ABILITY_SAND_VEIL
            && ability != ABILITY_OVERCOAT && ability != ABILITY_MAGIC_GUARD
            && item != ITEM_SAFETY_GOGGLES) {
            damageReceived += maxHp / 16;
        }
    } else if (ctx->field_condition & WEATHER_HAIL_ANY || ctx->field_condition & WEATHER_SNOW_ANY) {
        if (ability == ABILITY_ICE_BODY) {
            damageReceived -= maxHp / 16;
        } else if (SNOW_WARNING_GENERATION < GEN_LATEST) {
            if (!HasType(ctx, attacker, TYPE_ICE)
                && ability != ABILITY_ICE_BODY && ability != ABILITY_SNOW_CLOAK && ability != ABILITY_SLUSH_RUSH
                && ability != ABILITY_OVERCOAT && ability != ABILITY_MAGIC_GUARD
                && item != ITEM_SAFETY_GOGGLES) {
                damageReceived += maxHp / 16;
            }
        }
    } else if (ctx->field_condition & WEATHER_SUNNY_ANY) {
        if (ability == ABILITY_DRY_SKIN || ability == ABILITY_SOLAR_POWER) {
            damageReceived += maxHp / 8;
        }
    } else if (ctx->field_condition & WEATHER_RAIN_ANY) {
        if (ability == ABILITY_RAIN_DISH) {
            damageReceived -= maxHp / 16;
        }
    }
    if (item == ITEM_LEFTOVERS || (HasType(ctx, attacker, TYPE_POISON) && item == ITEM_BLACK_SLUDGE)) {
        damageReceived -= maxHp / 16;
    }
    if (ctx->terrainOverlay.type == GRASSY_TERRAIN && ctx->terrainOverlay.numberOfTurnsLeft > 0) {
        damageReceived -= maxHp / 16;
    }
    // if (ctx->battlemon[attacker].effect_of_moves & MOVE_EFFECT_FLAG_INGRAIN || ctx->battlemon[attacker].effect_of_moves & MOVE_EFFECT_FLAG_AQUA_RING)
    {
    }
    if (isSeeded) {
        damageReceived += maxHp / 8;
    }
    if (ctx->battlemon[attacker].condition2 & STATUS2_CURSE) {
        damageReceived += maxHp / 4;
    }

    if (attackerCondition & STATUS_BURN) {
        damageReceived += maxHp / 16;
    } else if (attackerCondition & STATUS_POISON) {
        if (ability == ABILITY_POISON_HEAL) {
            damageReceived -= maxHp / 8;
        } else {
            damageReceived += maxHp / 8;
        }
    } else if (attackerCondition & STATUS_BAD_POISON) {
        if (ability == ABILITY_POISON_HEAL) {
            damageReceived -= maxHp / 8;
        } else {
            if ((attackerCondition & STATUS_POISON_COUNT) != STATUS_POISON_COUNT) {
                attackerCondition += 1 << 8;
            }
            int toxicDamageTicks = ((attackerCondition & STATUS_POISON_COUNT) >> 8); // TODO: check this
            damageReceived += (toxicDamageTicks * maxHp / 16);
        }
    }

    if (damageReceived > 0 && damageReceived >= (int)hp) {
        diesFromResidual = TRUE;
    }
    return diesFromResidual;
}

BOOL LONG_CALL IsMonInflictedWithAnyNegativeStatus(struct BattleStruct *ctx, u32 attacker)
{
    if (ctx->battlemon[attacker].ability == ABILITY_POISON_HEAL && ctx->battlemon[attacker].condition & STATUS_POISON_ALL) {
        return FALSE;
    }
    if (ctx->battlemon[attacker].condition & (STATUS_POISON_ALL | STATUS_BURN | STATUS_PARALYSIS)) {
        return TRUE;
    }
    if (ctx->battlemon[attacker].effect_of_moves & (MOVE_EFFECT_YAWN_COUNTER | MOVE_EFFECT_FLAG_LEECH_SEED_ACTIVE | MOVE_EFFECT_FLAG_PERISH_SONG_ACTIVE)) {
        return TRUE;
    }
    if (ctx->battlemon[attacker].condition2 & (STATUS2_ATTRACT | STATUS2_CURSE)) {
        return TRUE;
    }
    return FALSE;
}
