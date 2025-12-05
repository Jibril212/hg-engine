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

#define BATTLE_DEBUG_OUTPUT 1
// #define ATTRACT_WORK_ON_ALL_SEXES 1

#define IMPOSSIBLE_MOVE   40
#define NEVER_USE_MOVE_20 20

void LONG_CALL SetupContexts(struct BattleSystem *bsys, u32 attacker, struct AIContext *ai1, struct AIContext *ai2, int damages[4][4]);
int LONG_CALL ScoreMovesAgainstDefender(struct BattleSystem *bsys, u32 attacker, u32 target, int moveScores[4][4], struct AIContext *ai);
int LONG_CALL ScoreMovesAgainstAlly(struct BattleSystem *bsys, u32 attacker, u32 target, int moveScores[4][4], struct AIContext *ai);

int LONG_CALL BasicScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai);
int LONG_CALL DamagingMoveScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai);
int LONG_CALL SetupScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai);
int LONG_CALL RecoveryScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai);
int LONG_CALL HarassmentScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai);

int LONG_CALL OffensiveSetup(struct BattleSystem *bsys UNUSED, u32 attacker UNUSED, int i UNUSED, struct AIContext *ai);
int LONG_CALL DefensiveSetup(struct BattleSystem *bsys UNUSED, u32 attacker UNUSED, int i UNUSED, struct AIContext *ai);

BOOL LONG_CALL HasMovePriority(struct BattleSystem *bsys, u8 attacker, u32 attackerMove, u32 attackerAbility, u8 defender);
BOOL LONG_CALL HasMovePranksterPriority(struct BattleSystem *bsys, u8 attacker, u32 attackerMove, u32 attackerAbility, u8 defender);

enum AIActionChoice __attribute__((section(".init"))) TrainerAI_Main(struct BattleSystem *bsys, u32 attacker)
{
#ifdef BATTLE_DEBUG_OUTPUT
    debug_printf("TrainerAI_Main: %d\n", attacker);
#endif // BATTLE_DEBUG_OUTPUT

    int score = 0;
    if (attacker >= 10) {
        return BattleAI_PostKOSwitchIn_Internal(bsys, attacker - 10, &score, FALSE);
    }

    struct BattleStruct *ctx = bsys->sp;
    struct AIContext aiContextOp1 = { 0 };
    struct AIContext aiContextOp2 = { 0 };
    struct AIContext *ai1 = &aiContextOp1;
    struct AIContext *ai2 = &aiContextOp2;
    enum AIActionChoice result = AI_ENEMY_ATTACK_1;

    int highestScoredMove = 0;
    int highestScoredMoveAcross = 0;
    int moveScores[4][4] = { 0 }; // account for BATTLER_OPPONENT (0), attacker (1), BATTLER_ACROSS(2), BATTLER_ALLY(3),  4 moves each or
                                  // account for BATTLER_OPPONENT (2), attacker (3), BATTLER_ACROSS(0), BATTLER_ALLY(1),  4 moves each
    int damages[4][4] = { 0 };    // rolled damage for each move against each target
    int targets[4] = { 0 };
    int tiedMoveIndices[4] = { 0 };
    u32 target = 0;

    u32 defender = BATTLER_OPPONENT(attacker); // default for singles
    target = defender;

    if (BattleTypeGet(bsys) & (BATTLE_TYPE_MULTI | BATTLE_TYPE_DOUBLE | BATTLE_TYPE_TAG))
    {
#ifdef BATTLE_DEBUG_OUTPUT
        debug_printf("att %d(%d), ally %d(%d), defendOp %d(%d), defendCross %d(%d)\n", 
            attacker,
            ctx->battlemon[attacker].species,
            BATTLER_ALLY(attacker),
            ctx->battlemon[BATTLER_ALLY(attacker)].species,
            BATTLER_OPPONENT(attacker),
            ctx->battlemon[BATTLER_OPPONENT(attacker)].species,
            BATTLER_ACROSS(attacker),
            ctx->battlemon[BATTLER_ACROSS(attacker)].species);
        //debug_printf("att %d, ally %d, defendOp %d, defendCross %d\n", ctx->battlemon[attacker].species, ctx->battlemon[BATTLER_ALLY(attacker)].species, ctx->battlemon[BATTLER_OPPONENT(attacker)].species, ctx->battlemon[BATTLER_ACROSS(attacker)].species);
#endif // BATTLE_DEBUG_OUTPUT

        SetupContexts(bsys, attacker, ai1, ai2, damages);
        if (ctx->battlemon[defender].hp > 0)
            highestScoredMove = ScoreMovesAgainstDefender(bsys, attacker, target, moveScores, ai1);

        defender = BATTLER_ACROSS(attacker);
        if (ctx->battlemon[defender].hp > 0)
        {
            target = defender;
            highestScoredMoveAcross = ScoreMovesAgainstDefender(bsys, attacker, target, moveScores, ai2);
            if (highestScoredMoveAcross > highestScoredMove)
                highestScoredMove = highestScoredMoveAcross;
        }

        defender = BATTLER_ALLY(attacker);
        target = defender;
        highestScoredMoveAcross = ScoreMovesAgainstAlly(bsys, attacker, target, moveScores, ai1);
        if (highestScoredMoveAcross > highestScoredMove)
            highestScoredMove = highestScoredMoveAcross;

        int targetsSize = 0;
        for (u8 k = 0; k < 4; k++) // find targets with highestScoredMove
        {
            for (u8 i = 0; i < 4; i++) // movesScore
            {
                if (moveScores[k][i] == highestScoredMove) {
                    debug_printf("found target %d with score %d, dmg %d\n", k, highestScoredMove, damages[k][i]);
                    targets[targetsSize] = k;
                    targetsSize++;
                    break;
                }
            }
        }

        target = targets[(BattleRand(bsys) % targetsSize)];

#ifdef BATTLE_DEBUG_OUTPUT
        debug_printf("picked target slot %d\n", target);
#endif // BATTLE_DEBUG_OUTPUT
    } else // single battles
    {
        // BATTLER_OPPONENT
        SetupStateVariables(bsys, attacker, defender, ai1);
        highestScoredMove = ScoreMovesAgainstDefender(bsys, attacker, target, moveScores, ai1);
    }
    ctx->aiWorkTable.ai_dir_select_client[attacker] = target;

#ifdef BATTLE_DEBUG_OUTPUT
    u8 j = 0;
    for (int k = 0; k < 4; k++) {
        for (u8 i = 0; i < 4; i++) // movesScore
        {
            debug_printf("%4d/%4d  ", moveScores[k][i], damages[k][i]);
        }

        if (targets[j] == k) {
            j++;
            debug_printf("x");
        }
        debug_printf("\n");
    }
#endif // BATTLE_DEBUG_OUTPUT

    for (u8 i = 0; i < 4; i++) {
        if (moveScores[target][i] == highestScoredMove) {
            result = i;
        }
    }

#ifdef BATTLE_DEBUG_OUTPUT
    for (u8 i = 0; i < 4; i++) {
        debug_printf("%i ", moveScores[target][i]);
    }
    debug_printf("-> highest %i:%i, dmg %d\n", result, highestScoredMove, damages[target][result]);
#endif // BATTLE_DEBUG_OUTPUT

    int tieMoveCount = 0;

    for (u8 i = 0; i < 4; i++) { // check for ties
        if (moveScores[target][i] == highestScoredMove) {
            tiedMoveIndices[tieMoveCount] = i;
            tieMoveCount++;
        }
    }
    u8 tieMoveIndex = (BattleRand(bsys) % tieMoveCount);

    result = tiedMoveIndices[tieMoveIndex % 4]; // randomly pick a move among the tie
#ifdef BATTLE_DEBUG_OUTPUT
    debug_printf("got tieMoveIndex %d -> Resulting move: %d\n", tieMoveIndex, result);
#endif // BATTLE_DEBUG_OUTPUT
    return result;
}

void LONG_CALL SetupContexts(struct BattleSystem *bsys, u32 attacker, struct AIContext *ai1, struct AIContext *ai2, int damages[4][4])
{
    struct BattleStruct *ctx = bsys->sp;

    u32 defender = BATTLER_OPPONENT(attacker);
    if (ctx->battlemon[defender].hp > 0)
    {
        SetupStateVariables(bsys, attacker, defender, ai1);
        for (u8 i = 0; i < 4; i++)
        {
            damages[defender][i] = ai1->attackerRolledMoveDamages[i];
        }
    }

    defender = BATTLER_ACROSS(attacker);
    if (ctx->battlemon[defender].hp > 0)
    {
        SetupStateVariables(bsys, attacker, defender, ai2);
        for (u8 j = 0; j < 4; j++)
        {
            damages[defender][j] = ai2->attackerRolledMoveDamages[j];
        }
    }
    if (ai1->playerCanOneShotMonWithAnyMove) {
        ai2->playerCanOneShotMonWithAnyMove = TRUE;
    }
    if (ai2->playerCanOneShotMonWithAnyMove) {
        ai1->playerCanOneShotMonWithAnyMove = TRUE;
    }
}


int LONG_CALL ScoreMovesAgainstDefender(struct BattleSystem *bsys, u32 attacker, u32 target, int moveScores[4][4], struct AIContext *ai)
{
    struct BattleStruct *ctx = bsys->sp;
    int highestScoredMove = 0;
    for (int i = 0; i < GetBattlerLearnedMoveCount(bsys, ctx, attacker); i++)
    {
        u32 attackerMove = ctx->battlemon[attacker].move[i];
        if (attackerMove != MOVE_NONE)
        {
            if (attackerMove == ctx->battlemon[attacker].moveeffect.moveNoChoice
                || attackerMove == ctx->battlemon[attacker].moveeffect.encoredMove) // if the attacker has a move that is forced, use it
            {
#ifdef BATTLE_DEBUG_OUTPUT
                debug_printf("Attacker has choiced move %d:%d\n", i, ctx->battlemon[attacker].moveeffect.moveNoChoice);
#endif // BATTLE_DEBUG_OUTPUT
                moveScores[target][i] += 1000;
            }

             moveScores[target][i] += 1000; // don't want to get negative numbers, so start high at 1000, MOVE_NONE will stay at 0
             moveScores[target][i] += BasicScoring(bsys, attacker, i, ai);
             moveScores[target][i] += DamagingMoveScoring(bsys, attacker, i, ai);
             moveScores[target][i] += SetupScoring(bsys, attacker, i, ai);
             moveScores[target][i] += RecoveryScoring(bsys, attacker, i, ai);
             moveScores[target][i] += HarassmentScoring(bsys, attacker, i, ai);

            if (highestScoredMove < moveScores[target][i]) {
                highestScoredMove = moveScores[target][i];
            }
        }
    }

    return highestScoredMove;
}

int LONG_CALL ScoreMovesAgainstAlly(struct BattleSystem *bsys, u32 attacker, u32 target, int moveScores[4][4], struct AIContext *ai)
{
    struct BattleStruct *ctx = bsys->sp;
    if (!ai->isDoubleBattle || !ai->isAllyAlive) {
        return 0;
    }
    int highestScoredMove = 0;
    switch (bsys->trainerId[BATTLER_ENEMY]) {
    case 66: // trainer ID
    {

        u8 skillSwapPosition = 5;
        for (int j = 0; j < GetBattlerLearnedMoveCount(bsys, ctx, attacker); j++) {
            if (ctx->battlemon[attacker].move[j] == MOVE_SKILL_SWAP) {
                skillSwapPosition = j;
                break;
            }
        }

        if (skillSwapPosition < 5) {
            if ((ai->attackerMon.ability == ABILITY_FLASH_FIRE && ai->defenderAlly.ability != ABILITY_FLASH_FIRE && ai->defenderAlly.species == SPECIES_DURANT)
                || (ai->attackerMon.ability == ABILITY_TECHNICIAN && ai->defenderAlly.ability != ABILITY_TECHNICIAN && ai->defenderAlly.species == SPECIES_DURANT)
                || (ai->attackerMon.ability == ABILITY_PRANKSTER && ai->defenderAlly.ability != ABILITY_PRANKSTER && ai->defenderAlly.species == SPECIES_BRELOOM)) {
                highestScoredMove = 1000;
                highestScoredMove += 12;
                moveScores[target][skillSwapPosition] += highestScoredMove;
            }
        }
        break;
    }
    default:
        break;
    }

    return highestScoredMove;
}


/*Heavily penalize stupid decisions that would fail, do nothing, or objectively hurt the user.*/
int LONG_CALL BasicScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai)
{
    int moveScore = 0;
    struct BattleStruct *ctx = bsys->sp;

    ai->attackerMove = ctx->battlemon[attacker].move[i];
    ai->attackerMoveEffect = ctx->moveTbl[ai->attackerMove].effect;

    if (ctx->battlemon[ai->attacker].pp[i] == 0) {
        moveScore -= IMPOSSIBLE_MOVE;
    }
    if (ctx->battlemon[ai->attacker].moveeffect.disabledTurns && ai->attackerMove == ctx->battlemon[ai->attacker].moveeffect.disabledMove) {
        moveScore -= IMPOSSIBLE_MOVE;
    }
    if (ctx->battlemon[ai->attacker].moveeffect.tauntTurns > 0 && ctx->moveTbl[ai->attackerMove].split == SPLIT_STATUS) {
        moveScore -= IMPOSSIBLE_MOVE; // taunted, so no status moves
    }
    if ((IsPowderMove(ai->attackerMove) || ai->attackerMove == MOVE_LEECH_SEED) && HasType(ctx, ai->defender, TYPE_GRASS)) {
        moveScore -= IMPOSSIBLE_MOVE;
    }

    // if (ctx->clientPriority[ctx->attack_client] > 0 && GetBattlerAbility(ctx, ctx->attack_client) == ABILITY_PRANKSTER && HasType(ctx, defender, TYPE_DARK) && (ctx->attack_client & 1) != (defender & 1)) // used on an enemy
    if (HasMovePranksterPriority(bsys, ai->attacker, ai->attackerMove, ai->attackerMon.ability, ai->defender) && HasType(ctx, ai->defender, TYPE_DARK))
    {
        moveScore -= IMPOSSIBLE_MOVE; // TODO check
    }

    if (ai->effectivenessOnPlayer[i] == TYPE_MUL_NO_EFFECT) // immunity
    {
        moveScore -= IMPOSSIBLE_MOVE;
    }

    if ((ctx->terrainOverlay.type == PSYCHIC_TERRAIN && ctx->terrainOverlay.numberOfTurnsLeft > 0)
        && ai->defenderMon.isGrounded
        && HasMovePriority(bsys, ai->attacker, ai->attackerMove, ai->attackerMon.ability, ai->defender)) {
        moveScore -= IMPOSSIBLE_MOVE;
    }

    switch (ai->attackerMoveEffect) {
    case MOVE_EFFECT_STEALTH_ROCK:
        if (ctx->side_condition[ai->defenderSide] & SIDE_STATUS_STEALTH_ROCK || ai->livingMembersDefender == 1) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_SET_SPIKES:
        if (ctx->scw[ai->defenderSide].spikesLayers >= 3) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_TOXIC_SPIKES:
        if (ctx->scw[ai->defenderSide].toxicSpikesLayers >= 2) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_STICKY_WEB:
        if (ctx->side_condition[ai->defenderSide] & SIDE_STATUS_STICKY_WEB) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
        //case MOVE_STEEL_ROLLER && ctx->terrainOverlay.type == TERRAIN_NONE //TODO
    case MOVE_EFFECT_APPLY_TERRAINS: {
        switch (ai->attackerMove) {
        case MOVE_GRASSY_TERRAIN:
            if (ctx->terrainOverlay.type == GRASSY_TERRAIN && ctx->terrainOverlay.numberOfTurnsLeft > 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        case MOVE_MISTY_TERRAIN:
            if (ctx->terrainOverlay.type == MISTY_TERRAIN && ctx->terrainOverlay.numberOfTurnsLeft > 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        case MOVE_ELECTRIC_TERRAIN:
            if (ctx->terrainOverlay.type == ELECTRIC_TERRAIN && ctx->terrainOverlay.numberOfTurnsLeft > 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        case MOVE_PSYCHIC_TERRAIN:
            if (ctx->terrainOverlay.type == PSYCHIC_TERRAIN && ctx->terrainOverlay.numberOfTurnsLeft > 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        }
        break;
    }
    case MOVE_EFFECT_PARALYZE_HIT:
        if (ctx->moveTbl[ai->attackerMove].secondaryEffectChance == 100) // nuzzle
        {
            if (ai->defenderImmuneToParalysis || ai->attackerRolledMoveDamages[i] == 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
        }
        break;
    case MOVE_EFFECT_STATUS_PARALYZE:
        if (ai->defenderImmuneToParalysis) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_STATUS_BURN:
        if (ai->defenderImmuneToBurn) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_SWITCH_HELD_ITEMS:
        if (ai->attackerMon.item == ITEM_NONE && ai->defenderMon.item == ITEM_NONE) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_STATUS_SLEEP:
    case MOVE_EFFECT_STATUS_SLEEP_NEXT_TURN:
        if (ai->defenderImmuneToSleep || (ai->defenderMon.effect_of_moves & MOVE_EFFECT_YAWN_COUNTER)) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_STATUS_POISON:
    case MOVE_EFFECT_STATUS_BADLY_POISON:
        if (ai->defenderImmuneToPoison) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_STATUS_NIGHTMARE:
        if (ctx->battlemon[ai->defender].condition2 & STATUS2_NIGHTMARE || ai->defenderMon.ability == ABILITY_MAGIC_GUARD) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_FAIL_IF_NOT_USED_ALL_OTHER_MOVES:
        if (ctx->battlemon[attacker].moveeffect.lastResortCount < ai->attackerMovesKnown - 1) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_WEATHER_SANDSTORM:
        if (ctx->field_condition & WEATHER_SANDSTORM_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_WEATHER_HAIL:
        if (ctx->field_condition & WEATHER_HAIL_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_WEATHER_SNOW:
        if (ctx->field_condition & WEATHER_SNOW_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_WEATHER_RAIN:
        if (ctx->field_condition & WEATHER_RAIN_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_WEATHER_SUN:
        if (ctx->field_condition & WEATHER_SUNNY_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    default:
        break;
    }

    if (ai->attackerMove == MOVE_STEEL_ROLLER && ctx->terrainOverlay.type == TERRAIN_NONE) {
        moveScore -= IMPOSSIBLE_MOVE;
    }

    return moveScore;
}

BOOL LONG_CALL isMoveSpecialAiAttackingMove(u32 attackerMove)
{
    BOOL isSpecialAIMove = FALSE;
    switch (attackerMove) {
    case MOVE_SELF_DESTRUCT:
    case MOVE_EXPLOSION:
    case MOVE_MISTY_EXPLOSION:
    case MOVE_FINAL_GAMBIT:
    case MOVE_ROLLOUT:
    case MOVE_METEOR_BEAM:
    case MOVE_ELECTRO_SHOT:
    case MOVE_SAND_TOMB:
    case MOVE_BIND:
    case MOVE_CLAMP:
    case MOVE_FIRE_SPIN:
    case MOVE_WRAP:
    case MOVE_WHIRLPOOL:
    case MOVE_INFESTATION:
        isSpecialAIMove = TRUE;
        break;
    default:
        break;
    }
    return isSpecialAIMove;
}

int LONG_CALL SpecialAiAttackingMove(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai)
{
    int moveScore = 0;
    struct BattleStruct *ctx = bsys->sp;
    ai->attackerMove = ctx->battlemon[attacker].move[i];

    switch (ai->attackerMove) {
    case MOVE_SELF_DESTRUCT:
    case MOVE_EXPLOSION:
    case MOVE_MISTY_EXPLOSION:
        if ((ai->defenderMon.ability != ABILITY_DAMP || ai->attackerMon.hasMoldBreaker) && ai->attackerRolledMoveDamages[i] > 0) /*TODO: immunity handled? */
        {
            if (ai->attackerMon.percenthp < 10) {
                moveScore += 10;
            } else if (ai->attackerMon.percenthp < 33) {
                if (BattleRand(bsys) % 10 < 7) { // 70%
                    moveScore += 8;
                }
            } else if (ai->attackerMon.percenthp < 66) {
                if (BattleRand(bsys) % 2) { // 50%
                    moveScore += 7;
                }
            } else {
                if (BattleRand(bsys) % 20 == 0) { // 5%
                    moveScore += 7;
                }
            }

            if (ai->livingMembersAttacker == 1 && ai->livingMembersDefender > 1) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            if (ai->livingMembersAttacker == 1 && ai->livingMembersDefender == 1) {
                moveScore -= 1;
            }
        }
        break;
    case MOVE_FINAL_GAMBIT:
        if (ai->attackerMovesFirst && ai->attackerMon.hp >= ai->defenderMon.hp) {
            moveScore += 8;
        } else if (ai->attackerMovesFirst && ai->playerCanOneShotMonWithAnyMove) {
            moveScore += 7;
        } else {
            moveScore += 6;
        }
        break;
    case MOVE_ROLLOUT:
        moveScore += 7;
        break;
    case MOVE_METEOR_BEAM:
        if (ai->attackerMon.item == ITEM_POWER_HERB) {
            moveScore += 9;
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_ELECTRO_SHOT:
        if (ai->attackerMon.item == ITEM_POWER_HERB || ctx->field_condition & WEATHER_RAIN_ANY) {
            moveScore += 9;
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_SAND_TOMB:
        if (!ai->defenderMon.isGrounded) {
            break;
        }
        FALLTHROUGH;
    case MOVE_BIND:
    case MOVE_CLAMP:
    case MOVE_FIRE_SPIN:
    case MOVE_WRAP:
    case MOVE_WHIRLPOOL:
    case MOVE_INFESTATION:
        if (ctx->binding_turns[ai->defender] != 0 || HasType(ctx, ai->defender, TYPE_GHOST) || ai->attackerMon.item == ITEM_SHED_SHELL) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        else {
            moveScore += 6;
            if (BattleRand(bsys) % 10 < 2) {
                moveScore += 2;
            }
        }
        break;
    default:
        break;
    }

    return moveScore;
}

int LONG_CALL DamagingMoveScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai)
{
    int moveScore = 0;
    struct BattleStruct *ctx = bsys->sp;
    BOOL isMoveHighestDamage = FALSE;

    if (ctx->moveTbl[ai->attackerMove].split == SPLIT_STATUS) {
        return 0;
    }

    ai->attackerMove = ctx->battlemon[attacker].move[i];
    ai->attackerMoveEffect = ctx->moveTbl[ai->attackerMove].effect;

    if (isMoveSpecialAiAttackingMove(ai->attackerMove)) {
        moveScore += SpecialAiAttackingMove(bsys, attacker, i, ai);
    } else if (ai->attackerMoveEffect == MOVE_EFFECT_ONE_HIT_KO) {
        switch (ai->attackerMove) {
        case MOVE_SHEER_COLD:
            if (HasType(ctx, ai->defender, TYPE_ICE)) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        case MOVE_GUILLOTINE:
        case MOVE_HORN_DRILL:
            if (HasType(ctx, ai->defender, TYPE_GHOST) && ai->attackerMon.ability != ABILITY_SCRAPPY) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        case MOVE_FISSURE:
            if (ai->attackerMove == MOVE_FISSURE && (ai->defenderMon.item == ITEM_AIR_BALLOON || HasType(ctx, ai->defender, TYPE_FLYING) || (!ai->attackerMon.hasMoldBreaker && ai->defenderMon.ability == ABILITY_LEVITATE))) {
                moveScore -= NEVER_USE_MOVE_20;
            }
            break;
        default:
            break;
        }
        if (ai->attackerMon.level > ai->defenderMon.level) {
            if (BattleRand(bsys) % 3 == 0) {
                moveScore += 9;
            }
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
    } else if (ai->attackerRolledMaxDamage == ai->attackerRolledMoveDamages[i]) {
        isMoveHighestDamage = TRUE;
        moveScore += 6;
        if (BattleRand(bsys) % 10 < 2) {
            moveScore += 2;
        }
    }

    if (ai->monCanOneShotPlayerWithMove[i]) // if ai sees kill with this move
    {
        if (ai->attackerMovesFirst || (ai->defenderMovesFirst && ctx->moveTbl[ai->attackerMove].priority > 0)) {
            moveScore += 6; // and is faster/has priority
        } else if (ai->defenderMovesFirst) {
            moveScore += 3;
        }
        if (ai->attackerMon.ability == ABILITY_MOXIE || ai->attackerMon.ability == ABILITY_BEAST_BOOST || ai->attackerMon.ability == ABILITY_CHILLING_NEIGH || ai->attackerMon.ability == ABILITY_GRIM_NEIGH) {
            moveScore += 1;
        }
    }

    if (ai->attackerMoveEffect == MOVE_EFFECT_HIGH_CRITICAL && (ai->effectivenessOnPlayer[i] == TYPE_MUL_SUPER_EFFECTIVE || ai->effectivenessOnPlayer[i] == TYPE_MUL_DOUBLE_SUPER_EFFECTIVE || ai->effectivenessOnPlayer[i] == TYPE_MUL_TRIPLE_SUPER_EFFECTIVE)) {
        if (BattleRand(bsys) % 2 == 0) {
            moveScore += 2;
        }
    }

    if (ai->defenderMovesFirst && ai->playerCanOneShotMonWithAnyMove)
    {
        if (HasMovePriority(bsys, ai->attacker, ai->attackerMove, ai->attackerMon.ability, ai->defender)) {
            moveScore += 11;
        }
    }
    if (!isMoveHighestDamage && ai->attackerMon.ability == ABILITY_CONTRARY) // no Unaware check
    {
        switch (ai->attackerMove) {
        case MOVE_OVERHEAT:
        case MOVE_LEAF_STORM:
        case MOVE_DRACO_METEOR:
        case MOVE_CLOSE_COMBAT:
        case MOVE_SUPERPOWER:
        case MOVE_V_CREATE:
            if (ai->isDefenderIncapacitated) {
                moveScore += 3;
            }
            if (ai->attackerMovesFirst) {
                moveScore += 3;
            }
            if (3 * ai->maxDamageReceived < ai->attackerMon.hp) {
                moveScore += 3;
                if (ai->attackerMovesFirst) {
                    moveScore += 1;
                }
            }
            break;
        default:
            break;
        }
    }

    if (!isMoveHighestDamage 
        && ai->attackerMoveEffect == MOVE_EFFECT_LOWER_SPEED_HIT 
        && ctx->moveTbl[ai->attackerMove].secondaryEffectChance == 100) {
        if (ai->defenderMovesFirst 
            && ((ai->defenderMon.ability != ABILITY_CLEAR_BODY 
                && ai->defenderMon.ability != ABILITY_WHITE_SMOKE 
                && ai->defenderMon.ability != ABILITY_CONTRARY) 
                || ai->attackerMon.hasMoldBreaker)) {
            moveScore += 6;
        } else {
            moveScore += 5;
        }
        if (ai->isDoubleBattle) {
            if (ai->attackerMove == MOVE_ICY_WIND || ai->attackerMove == MOVE_ELECTROWEB) {
                moveScore += 1;
            }
        }
    }

    if (!isMoveHighestDamage && ctx->moveTbl[ai->attackerMove].secondaryEffectChance == 100
        && ((ai->attackerMoveEffect == MOVE_EFFECT_LOWER_SP_ATK_HIT && ai->defenderHasAtleastOneSpecialMove)
            || (ai->attackerMoveEffect == MOVE_EFFECT_LOWER_ATTACK_HIT && ai->defenderHasAtleastOnePhysicalMove))) {
        if ((ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_CONTRARY) || ai->attackerMon.hasMoldBreaker) {
            moveScore += 6;
        } else {
            moveScore += 5;
        }
        if (ai->isDoubleBattle) {
            if (ctx->moveTbl[ai->attackerMove].target == RANGE_ADJACENT_OPPONENTS) {
                moveScore += 1;
            }
        }
    }

    if (!isMoveHighestDamage && ai->attackerMoveEffect == MOVE_EFFECT_SWITCH_HIT) {
        u8 switchThreshold = 1;
        if (ai->monWithMegaInParty) {
            switchThreshold = 2;
        }
        if (ai->livingMembersAttacker > switchThreshold && ai->attackerRolledMoveDamages[i] > 0) // no immunity
        {
            moveScore += 6;
            if (ai->playerCanOneShotMonWithAnyMove && ai->attackerMovesFirst) {
                moveScore += 1;
            }
            if (BattleRand(bsys) % 10 < 2) {
                moveScore += 1;
            }
        }
        /*
        if (ai->attackerMon.ability == ABILITY_REGENERATOR && ai->attackerMon.percenthp < 67)
            moveScore += 1;
        */
    }

    switch (ai->attackerMove) {
    case MOVE_RELIC_SONG: // TODO
        break;
    case MOVE_FUTURE_SIGHT:
        if (ai->attackerMovesFirst && ai->playerCanOneShotMonWithAnyMove) {
            moveScore += 8;
        } else {
            moveScore += 6;
        }
        break;
    case MOVE_ACID_SPRAY: {
        if ((ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_CONTRARY && ai->defenderMon.ability != ABILITY_BULLETPROOF) || ai->attackerMon.hasMoldBreaker) {
            moveScore += 6;
        }
        break;
    }
    case MOVE_SUCKER_PUNCH: {
        if (ai->attackerLastUsedMove == MOVE_SUCKER_PUNCH) {
            if (BattleRand(bsys) % 2 == 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
        }
        break;
    }
    case MOVE_FIRST_IMPRESSION: {
        if (ai->attackerTurnsOnField == 0) {
            moveScore += 9;
        } else {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        break;
    }
    case MOVE_FAKE_OUT: {
        if (ai->attackerTurnsOnField == 0 && ((ai->defenderMon.ability != ABILITY_SHIELD_DUST && ai->defenderMon.ability != ABILITY_INNER_FOCUS) || ai->attackerMon.hasMoldBreaker)) {
            moveScore += 9;
        } else {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        break;
    }
    case MOVE_PURSUIT: {
        if (ai->attackerMovesFirst) {
            moveScore += 3;
        }

        if (ai->monCanOneShotPlayerWithMove[i]) {
            moveScore += 10;
        } else if (ai->defenderMon.percenthp < 20) {
            moveScore += 10;
        } else if (ai->defenderMon.percenthp < 40) {
            if (BattleRand(bsys) % 2 == 0) {
                moveScore += 8;
            }
        }
        break;
    }
    case MOVE_FELL_STINGER: {
        if (ctx->battlemon[ai->attacker].states[STAT_ATTACK] < 12 && ai->monCanOneShotPlayerWithMove[i]) {
            if (ai->attackerMovesFirst) {
                moveScore += 10;
            }
            // TODO
        }
        break;
    }
    case MOVE_EARTHQUAKE:
    case MOVE_MAGNITUDE: {
        if (ai->isDoubleBattle) {
            if (ai->isPartnerGrounded) // or about to use magnetrise && faster
            {
                if (ctx->battlemon[BATTLER_ALLY(ai->attacker)].hp) {
                    if (HasType(ctx, BATTLER_ALLY(ai->attacker), TYPE_POISON) || HasType(ctx, BATTLER_ALLY(ai->attacker), TYPE_STEEL) || HasType(ctx, BATTLER_ALLY(ai->attacker), TYPE_FIRE) || HasType(ctx, BATTLER_ALLY(ai->attacker), TYPE_ROCK)) {
                        moveScore -= 10;
                    } else {
                        moveScore -= 3;
                    }
                }
            } else {
                moveScore += 2;
            }
        }
    }
    default:
        break;
    }

    return moveScore;
}


int LONG_CALL OffensiveSetup(struct BattleSystem *bsys UNUSED, u32 attacker UNUSED, int i UNUSED, struct AIContext *ai)
{
    debug_printf("Off: incap %d, movesFirst %d, is1H %d, maxRec %d, hp %d", ai->isDefenderIncapacitated, ai->attackerMovesFirst, ai->playerCanOneShotMonWithAnyMove, ai->maxDamageReceived, ai->attackerMon.hp);
    int moveScore = 0;
    if (ai->isDefenderIncapacitated) {
        moveScore += 3;
    }
    if (ai->attackerMovesFirst) {
        if (!ai->playerCanOneShotMonWithAnyMove) {
            moveScore += 3;
        }
    } else {
        if (2 * ai->maxDamageReceived >= ai->attackerMon.hp) {
            moveScore -= 5;
        }
    }
    if (3 * ai->maxDamageReceived < ai->attackerMon.hp) {
        moveScore += 3;
        if (ai->attackerMovesFirst) {
            moveScore += 1;
        }
    }
    debug_printf(", score %d\n", moveScore);
    return moveScore;
}
int LONG_CALL DefensiveSetup(struct BattleSystem *bsys UNUSED, u32 attacker UNUSED, int i UNUSED, struct AIContext *ai)
{
    debug_printf("Def: incap %d, movesFirst %d, is1H %d, maxRec %d, hp %d", ai->isDefenderIncapacitated, ai->attackerMovesFirst, ai->playerCanOneShotMonWithAnyMove, ai->maxDamageReceived, ai->attackerMon.hp);
    
    int moveScore = 0;
    if (ai->isDefenderIncapacitated) {
        moveScore += 2;
    }
    if (ai->defenderMovesFirst && 2 * ai->maxDamageReceived >= ai->attackerMon.hp) {
        moveScore -= 5;
    }
    if (3 * ai->maxDamageReceived < ai->attackerMon.hp) {
        moveScore += 3;
        if (ai->attackerMovesFirst) {
            moveScore += 1;
        }
    }
    return moveScore;
}

int LONG_CALL SetupScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai)
{
    int moveScore = 0;
    struct BattleStruct *ctx = bsys->sp;
    BOOL hasSitrus = (ai->attackerMon.item == ITEM_SITRUS_BERRY);
    BOOL shouldSetup = TRUE;
    BOOL isSetupMove = TRUE;

    ai->attackerMove = ctx->battlemon[attacker].move[i];
    ai->attackerMoveEffect = ctx->moveTbl[ai->attackerMove].effect;

    if (ai->playerCanOneShotMonWithAnyMove || ai->defenderCanForceSwitching) {
        shouldSetup = FALSE;
    }
    if (ai->defenderMon.ability == ABILITY_UNAWARE && (BattleRand(bsys) % 4 > 0)) {
        shouldSetup = FALSE;
    }

    switch (ai->attackerMoveEffect) {
    case MOVE_EFFECT_WEATHER_RAIN:
        if (ctx->field_condition & WEATHER_RAIN_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            if (ai->attackerMon.ability == ABILITY_SWIFT_SWIM
                || ai->attackerMon.ability == ABILITY_RAIN_DISH
                || ai->attackerMon.ability == ABILITY_HYDRATION
                || ai->attackerMon.ability == ABILITY_DRY_SKIN) {
                moveScore += 9;
            }
            if (ai->attackerMon.item == ITEM_DAMP_ROCK) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_WEATHER_HAIL:
    case MOVE_EFFECT_WEATHER_SNOW:
        // case MOVE_EFFECT_SNOWSCAPE:
        if (ctx->field_condition & (WEATHER_HAIL_ANY | WEATHER_SNOW_ANY)) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            if (HasType(ctx, attacker, TYPE_ICE)
                || ai->attackerMon.ability == ABILITY_ICE_BODY || ai->attackerMon.ability == ABILITY_SNOW_CLOAK
                || ai->attackerMon.ability == ABILITY_SLUSH_RUSH) {
                moveScore += 9;
            }
            if (ai->attackerMon.item == ITEM_ICY_ROCK) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_WEATHER_SUN:
        if (ctx->field_condition & WEATHER_SUNNY_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            if (HasType(ctx, attacker, TYPE_FIRE) || ai->attackerMon.ability == ABILITY_SOLAR_POWER) { //|| ai->attackerMon.ability == ABILITY_CHLOROPHYL
                moveScore += 9;
            }
            if (ai->attackerMon.item == ITEM_HEAT_ROCK) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_WEATHER_SANDSTORM:
        if (ctx->field_condition & WEATHER_SANDSTORM_ANY) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            if (HasType(ctx, attacker, TYPE_ROCK) || HasType(ctx, attacker, TYPE_GROUND) || HasType(ctx, attacker, TYPE_STEEL)
                || ai->attackerMon.ability == ABILITY_SAND_FORCE || ai->attackerMon.ability == ABILITY_SAND_RUSH || ai->attackerMon.ability == ABILITY_SAND_VEIL) {
                moveScore += 9;
            }
            if (ai->attackerMon.item == ITEM_SMOOTH_ROCK) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_COPY_STAT_CHANGES: {
        u8 sumStatChange = BattlerPositiveStatChangesSum(bsys, ai->defender, ai);
        if (sumStatChange > 0) {
            moveScore += 5;
        }
        if (sumStatChange > 1) {
            moveScore += sumStatChange;
        }
        break;
    }
    case MOVE_EFFECT_EVA_UP:
    case MOVE_EFFECT_EVA_UP_2:
    case MOVE_EFFECT_EVA_UP_3:
    case MOVE_EFFECT_EVA_UP_2_MINIMIZE:
        moveScore += 6;
        if (ai->defenderMon.ability == ABILITY_NO_GUARD || ctx->battlemon[attacker].states[STAT_EVASION] >= 12) {
            moveScore -= NEVER_USE_MOVE_20;
        } 
        if (ctx->battlemon[attacker].states[STAT_EVASION] >= 7) {
            moveScore -= 1;
        } 
        else if (2 * ai->maxDamageReceived < ai->attackerMon.hp) {
            moveScore += 2;
        }
        if (ai->isDefenderIncapacitated) {
            moveScore += 2;
        }
        break;
    case MOVE_EFFECT_REMOVE_HAZARDS_AND_BINDING: // rapid_spin
    case MOVE_EFFECT_RAISE_SPEED_HIT: // trailblaze
        if (ctx->moveTbl[ai->attackerMove].secondaryEffectChance < 100 || ai->attackerRolledMaxDamage == ai->attackerRolledMoveDamages[i]) // if other move (like ...), or highest damage
        {
            isSetupMove = FALSE;
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_SPEED_UP_2:
    case MOVE_EFFECT_AUTOTOMIZE:
        if (ai->defenderMovesFirst) {
            moveScore += 7;
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_CRIT_UP_2:
        if (ai->defenderMon.ability == ABILITY_SHELL_ARMOR || ai->defenderMon.ability == ABILITY_BATTLE_ARMOR || ctx->battlemon[ctx->defence_client].condition2 & STATUS2_FOCUS_ENERGY) {
            moveScore -= NEVER_USE_MOVE_20;
        } else if (ai->attackerMon.ability == ABILITY_SUPER_LUCK || ai->attackerMon.ability == ABILITY_SNIPER || ai->attackerMon.item == ITEM_SCOPE_LENS) { // or hight crit moves
            moveScore += 7;
        } else {
            moveScore += 5;
        }
        break;
    // case MOVE_EFFECT_ATK_SP_ATK_SPEED_UP_2_LOSE_HALF_MAX_HP: //filet away
    case MOVE_EFFECT_MAX_ATK_LOSE_HALF_MAX_HP: // belly drum
        moveScore += 4;
        if (ai->isDefenderIncapacitated) {
            moveScore += 5;
        } else if (2 * ai->attackerMon.hp > ai->attackerMon.maxhp) {
            if (hasSitrus) {
                if (2 * ai->maxDamageReceived < (u32)(25 * ai->attackerMon.hp / 10 - ai->attackerMon.maxhp)) { // D < 1.125*Hp - 0.5maxHp
                    moveScore += 4;
                }
            } else if (2 * ai->maxDamageReceived < (u32)(2 * ai->attackerMon.hp - ai->attackerMon.maxhp)) { // D < Hp - 0.5maxHp
                moveScore += 4;
            }
        }
        break;
    case MOVE_EFFECT_RAISE_ALL_STATS_LOSE_THIRD_MAX_HP: // clangorous soul
    case MOVE_EFFECT_ATK_SP_ATK_SPEED_UP_2_DEF_SP_DEF_DOWN: // shell smash
        moveScore += 6;
        if (ai->isDefenderIncapacitated) {
            moveScore += 3;
        }
        if (ai->attackerMovesFirst) // if ai is slower, no defense drop for hit
        {
            if (ai->attackerMon.item == ITEM_WHITE_HERB || 2 * ai->maxDamageReceived < ai->attackerMon.hp) { // simple heuristic to not calc exact dmg.
                moveScore += 2;
            } else {
                moveScore -= 2;
            }
        }
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 7 || ctx->battlemon[attacker].states[STAT_SPATK] > 7) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_RAISE_SP_ATK_HIT: // charge beam
    case MOVE_EFFECT_RAISE_ATTACK_HIT: // PuP
        if (ctx->moveTbl[ai->attackerMove].secondaryEffectChance < 100 || ai->attackerRolledMaxDamage == ai->attackerRolledMoveDamages[i]) // if other move (like Metal Claw), or highest damage
        {
            isSetupMove = FALSE;
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_SP_ATK_UP: // growth
    case MOVE_EFFECT_SP_ATK_UP_2: // nasty plot
    case MOVE_EFFECT_SP_ATK_UP_3: // tail glow
    case MOVE_EFFECT_ATK_SP_ATK_UP: // work up
    case MOVE_EFFECT_ATK_ACC_UP: // hone claws
    case MOVE_EFFECT_ATK_UP: // howl
    case MOVE_EFFECT_ATK_UP_2: // swords dance
    case MOVE_EFFECT_ATK_UP_3:
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 11 || ctx->battlemon[attacker].states[STAT_SPATK] > 11) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 7 || ctx->battlemon[attacker].states[STAT_SPATK] > 7) {
            moveScore -= 2;
        }
        moveScore += 6;
        moveScore += OffensiveSetup(bsys, attacker, i, ai);
        break;
    case MOVE_EFFECT_ATK_SPEED_UP: // dragon dance
    case MOVE_EFFECT_SPEED_UP_2_ATK_UP: // shift gear
    case MOVE_EFFECT_TIDY_UP: // tidy up is basically ddance
    case MOVE_EFFECT_ATK_DEF_SPEED_UP: // victory dance
    case MOVE_EFFECT_RANDOM_STAT_UP_2: // accupressure
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 11 || ctx->battlemon[attacker].states[STAT_SPATK] > 11) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 7 || ctx->battlemon[attacker].states[STAT_SPATK] > 7 || ai->attackerMovesFirst) {
            moveScore -= 2;
        }
        moveScore += 6;
        moveScore += OffensiveSetup(bsys, attacker, i, ai);
        break;
    case MOVE_EFFECT_GROUND_TRAP_USER_CONTINUOUS_HEAL: // Ingrain
        moveScore += 6;
        if (ctx->battlemon[attacker].effect_of_moves & MOVE_EFFECT_FLAG_INGRAIN) {
            moveScore -= IMPOSSIBLE_MOVE;
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_RESTORE_HP_EVERY_TURN: // Aqua Ring
        moveScore += 6;
        if (ctx->battlemon[attacker].effect_of_moves & MOVE_EFFECT_FLAG_AQUA_RING) {
            moveScore -= IMPOSSIBLE_MOVE;
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_DEF_UP:  //harden
    case MOVE_EFFECT_DEF_UP_2: //iron defense
    case MOVE_EFFECT_DEF_UP_3: //cotton guard
    case MOVE_EFFECT_SP_DEF_UP:
    case MOVE_EFFECT_SP_DEF_UP_2: //amnesia
    case MOVE_EFFECT_SP_DEF_UP_3:
    case MOVE_EFFECT_DEF_UP_DOUBLE_ROLLOUT_POWER: //defense curl
    case MOVE_EFFECT_SP_DEF_UP_DOUBLE_ELECTRIC_POWER: // Charge
    case MOVE_EFFECT_DEF_SP_DEF_UP: // cosmic power
        // case MOVE_EFFECT_STUFF_CHEEKS:
        if (ctx->battlemon[attacker].states[STAT_DEFENSE] > 11 || ctx->battlemon[attacker].states[STAT_SPDEF] > 11) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (ctx->battlemon[attacker].states[STAT_DEFENSE] > 7 || ctx->battlemon[attacker].states[STAT_SPDEF] > 7) {
            moveScore -= 3;
        }
        moveScore += 6;
        moveScore += DefensiveSetup(bsys, attacker, i, ai);
        break;
    case MOVE_EFFECT_STOCKPILE:
        if (ctx->battlemon[attacker].states[STAT_DEFENSE] > 8 || ctx->battlemon[attacker].states[STAT_SPDEF] > 8) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (ctx->battlemon[attacker].states[STAT_DEFENSE] > 7 || ctx->battlemon[attacker].states[STAT_SPDEF] > 7) {
            moveScore -= 3;
        }
        moveScore += 6;
        moveScore += DefensiveSetup(bsys, attacker, i, ai);
        break;
    case MOVE_EFFECT_SP_ATK_SP_DEF_SPEED_UP: // quiver dance
    case MOVE_EFFECT_SP_ATK_SP_DEF_UP: // Calm Mind
        moveScore += 6;
        if (ctx->battlemon[attacker].states[STAT_SPATK] > 11 || ctx->battlemon[attacker].states[STAT_SPDEF] > 11) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (ctx->battlemon[attacker].states[STAT_SPATK] > 7 || ctx->battlemon[attacker].states[STAT_SPDEF] > 7) {
            moveScore -= 2;
        }
        if (ai->defenderHasAtleastOnePhysicalMove == FALSE && ai->defenderHasAtleastOneSpecialMove) {
            moveScore += DefensiveSetup(bsys, attacker, i, ai);
        } else {
            moveScore += OffensiveSetup(bsys, attacker, i, ai);
        }
        break;
    case MOVE_EFFECT_CURSE:
        if (HasType(ctx, attacker, TYPE_GHOST)) {
            isSetupMove = FALSE;
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_ATK_DEF_UP: // Bulk Up
    case MOVE_EFFECT_ATK_DEF_ACC_UP: // coil
        // case MOVE_EFFECT_NO_RETREAT: // No Retreat
        moveScore += 6;
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 11 || ctx->battlemon[attacker].states[STAT_DEFENSE] > 11) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (ctx->battlemon[attacker].states[STAT_ATTACK] > 7 || ctx->battlemon[attacker].states[STAT_DEFENSE] > 7) {
            moveScore -= 2;
        }
        if (ai->defenderHasAtleastOnePhysicalMove && ai->defenderHasAtleastOneSpecialMove == FALSE) {
            moveScore += DefensiveSetup(bsys, attacker, i, ai);
        } else {
            moveScore += OffensiveSetup(bsys, attacker, i, ai);
        }
        break;
    case MOVE_EFFECT_CHARGE_TURN_ATK_SP_ATK_SPEED_UP_2: // geomancy
        if (ai->attackerMon.item == ITEM_POWER_HERB) {
            moveScore += 9;
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    default:
        isSetupMove = FALSE;
        break;
    }
    if (!shouldSetup) {
        moveScore = 0;
    }
    if (!isSetupMove) {
        moveScore = 0;
    }

    return moveScore;
}

int LONG_CALL HarassmentScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai)
{
    int moveScore = 0;
    BOOL sharesMoves = FALSE;
    struct BattleStruct *ctx = bsys->sp;

    ai->attackerMove = ctx->battlemon[attacker].move[i];
    ai->attackerMoveEffect = ctx->moveTbl[ai->attackerMove].effect;

    switch (ai->attackerMoveEffect) {
    case MOVE_EFFECT_TAUNT: //TODO
        if (ctx->battlemon[ai->defender].moveeffect.tauntTurns > 0)
        {
            moveScore -= NEVER_USE_MOVE_20;
        }
        else if (ai->defenderMon.ability == ABILITY_OBLIVIOUS || ai->defenderMon.ability == ABILITY_AROMA_VEIL
            || (ai->isDoubleBattle && ctx->battlemon[BATTLER_ALLY(attacker)].hp > 0 && ctx->battlemon[BATTLER_ALLY(attacker)].ability == ABILITY_AROMA_VEIL))
        {
            moveScore -= NEVER_USE_MOVE_20;
        }
        else if (BattlerKnowsMove(bsys, ai->defender, MOVE_DEFOG, ai) == TRUE && ai->attackerMovesFirst)
        {
            moveScore += 9;
        }
        else
        {
            moveScore += 5;
        }
        break;
    case MOVE_EFFECT_STEALTH_ROCK:
    case MOVE_EFFECT_SET_SPIKES:
    case MOVE_EFFECT_TOXIC_SPIKES:
        if (ai->attackerTurnsOnField == 0) {
            moveScore += 8;
        } else {
            moveScore += 6;
        }
        if (BattleRand(bsys) % 4 != 0) {
            moveScore += 1;
        }
        if (ctx->scw[ai->defenderSide].spikesLayers > 0 && ai->attackerMoveEffect == MOVE_EFFECT_SET_SPIKES) {
            moveScore -= 1;
        }
        if (ctx->scw[ai->defenderSide].toxicSpikesLayers > 0 && ai->attackerMoveEffect == MOVE_EFFECT_TOXIC_SPIKES) {
            moveScore -= 1;
        }
        break;
    case MOVE_EFFECT_STICKY_WEB:
        if (ai->attackerTurnsOnField == 0) {
            moveScore += 9;
        } else {
            moveScore += 6;
        }
        if (BattleRand(bsys) % 4 != 0) {
            moveScore += 3;
        }
        break;
    case MOVE_EFFECT_SURVIVE_WITH_1_HP: // endure
    {
        BOOL monDiesEndTurnEndure = MonDiesFromResidualDamage(ctx, ai->attacker, ai->attackerMon.condition, (ctx->battlemon[ai->attacker].effect_of_moves & MOVE_EFFECT_FLAG_LEECH_SEED_ACTIVE));

        if (IsMonInflictedWithAnyNegativeStatus(ctx, attacker)) {
            moveScore -= 1;
        }
        if (ai->defenderMovesFirst && ai->playerCanOneShotMonWithAnyMove) {
            moveScore += 6;
            if (IS_ITEM_BERRY(ai->attackerMon.item)) {
                moveScore += 2;
            }
        }
        if (ctx->protectSuccessTurns[ai->attacker] == 1) {
            if (BattleRand(bsys) % 2 == 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
        }
        if (ctx->protectSuccessTurns[ai->attacker] > 1 || monDiesEndTurnEndure) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        break;
    }
    // case MOVE_EFFECT_KINGS_SHIELD:
    // case MOVE_EFFECT_OBSTRUCT:
    // case MOVE_EFFECT_SPIKEY_SHIELD:
    case MOVE_EFFECT_PROTECT: // TODO
    {
        BOOL monDiesEndTurn = MonDiesFromResidualDamage(ctx, ai->attacker, ai->attackerMon.condition, (ctx->battlemon[ai->attacker].effect_of_moves & MOVE_EFFECT_FLAG_LEECH_SEED_ACTIVE));
        moveScore += 6;
        if (IsMonInflictedWithAnyNegativeStatus(ctx, attacker)) {
            moveScore -= 1;
        }
        if (IsMonInflictedWithAnyNegativeStatus(ctx, ai->defender)) {
            moveScore += 1;
        }
        if (ai->attackerTurnsOnField == 0 && ai->isDoubleBattle) {
            moveScore -= 1;
        }
        if (ctx->protectSuccessTurns[ai->attacker] == 1) {
            if (BattleRand(bsys) % 2 == 0) {
                moveScore -= NEVER_USE_MOVE_20;
            }
        } else if (ctx->protectSuccessTurns[ai->attacker] > 1) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        if (monDiesEndTurn) {
            moveScore -= IMPOSSIBLE_MOVE;
        }
        break;
    }
    case MOVE_EFFECT_MAKE_SHARED_MOVES_UNUSEABLE:
        if (ctx->battlemon[ai->defender].effect_of_moves & MOVE_EFFECT_FLAG_IMPRISONED) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            for (int j = 0; j < CLIENT_MAX; ++j) {
                for (int k = 0; k < CLIENT_MAX; ++k) {
                    if (ctx->battlemon[ai->attacker].move[k] == ctx->battlemon[ai->defender].move[j] && ctx->battlemon[ai->defender].move[j] != MOVE_NONE) {
                        sharesMoves = TRUE;
                        break;
                    }
                }
                if (sharesMoves == TRUE) {
                    break;
                }
            }
            if (sharesMoves == TRUE) {
                moveScore += 9;
            }
        }
        break;
    case MOVE_EFFECT_PASS_STATS_AND_STATUS:
        if ((ai->isDoubleBattle && ai->livingMembersAttacker > 2) || (!ai->isDoubleBattle && ai->livingMembersAttacker > 1)) {
            if ((BattlerPositiveStatChangesSum(bsys, ai->attacker, ai) >= 1) || ctx->battlemon[ai->attacker].condition2 & STATUS2_SUBSTITUTE) {
                moveScore += 14;
            }
        }
        if (ai->livingMembersAttacker == 1) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_DOUBLE_SPEED_3_TURNS:
        if (IsBattleMonSlowerThanOpposition(bsys, attacker, ai->isDoubleBattle)
            || (ai->isDoubleBattle && ctx->battlemon[BATTLER_ALLY(attacker)].hp > 0 && IsBattleMonSlowerThanOpposition(bsys, BATTLER_ALLY(attacker), ai->isDoubleBattle))) { // or partner is slower
            moveScore += 9;
        } else {
            moveScore += 5;
        }
        if (ctx->side_condition[ai->attackerSide] & SIDE_STATUS_TAILWIND) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_TRICK_ROOM:
        if (IsBattleMonSlowerThanOpposition(bsys, attacker, ai->isDoubleBattle)
            || (ai->isDoubleBattle && ctx->battlemon[BATTLER_ALLY(attacker)].hp > 0 && IsBattleMonSlowerThanOpposition(bsys, BATTLER_ALLY(attacker), ai->isDoubleBattle))) { // or partner is slower
            moveScore += 10;
        } else {
            moveScore += 5;
        }

        if (ctx->field_condition & FIELD_STATUS_TRICK_ROOM) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_APPLY_TERRAINS:
        if (ai->attackerMon.item == ITEM_TERRAIN_EXTENDER) {
            moveScore += 9;
        } else {
            moveScore += 8;
        }
        break;
    case MOVE_EFFECT_SET_REFLECT:
        if (ctx->side_condition[ai->attackerSide] & SIDE_STATUS_REFLECT) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
            if (ai->defenderHasAtleastOnePhysicalMove) {
                if (ai->attackerMon.item == ITEM_LIGHT_CLAY) {
                    moveScore += 1;
                }
                if (BattleRand(bsys) % 2 == 0) {
                    moveScore += 1;
                }
            }
        }
        break;
    case MOVE_EFFECT_SET_LIGHT_SCREEN:
        if (ctx->side_condition[ai->attackerSide] & SIDE_STATUS_LIGHT_SCREEN) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
            if (ai->defenderHasAtleastOneSpecialMove) {
                if (ai->attackerMon.item == ITEM_LIGHT_CLAY) {
                    moveScore += 1;
                }
                if (BattleRand(bsys) % 2 == 0) {
                    moveScore += 1;
                }
            }
        }
        break;
    case MOVE_EFFECT_SET_AURORA_VEIL:
        if (ctx->side_condition[ai->attackerSide] & SIDE_STATUS_AURORA_VEIL || (ctx->field_condition & WEATHER_SNOW_ANY) == 0) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
            if (ai->attackerMon.item == ITEM_LIGHT_CLAY) {
                moveScore += 1;
            }
            if (BattleRand(bsys) % 2 == 0) {
                moveScore += 2;
            }
        }
        break;
    case MOVE_EFFECT_BOOST_ALLY_POWER_BY_50_PERCENT: // TODO
    case MOVE_EFFECT_MAKE_GLOBAL_TARGET:
        if (ai->isDoubleBattle && ai->isAllyAlive) {
            moveScore += 6;
            if (BattleRand(bsys) % 10 < 2) {
                moveScore += 2;
            }
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_SET_SUBSTITUTE:
        moveScore += 6;
        if (ai->attackerMovesFirst && (ctx->battlemon[ai->defender].effect_of_moves & MOVE_EFFECT_FLAG_LEECH_SEED_ACTIVE)) {
            moveScore += 2;
        }
        if (ai->defenderMon.condition & STATUS_SLEEP) {
            moveScore += 2;
        }
        if (BattleRand(bsys) % 2 == 0) {
            moveScore -= 1;
        }
        if (ai->defenderMon.ability == ABILITY_INFILTRATOR || ai->attackerMon.percenthp < 50) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        if (ai->defenderHasAtleastOneUsefulSoundMove && (ai->defenderMon.hasMoldBreaker || ai->attackerMon.ability != ABILITY_SOUNDPROOF)) {
            moveScore -= 8;
        }
        break;
    case MOVE_EFFECT_FAINT_AND_ATK_SP_ATK_DOWN_2:
        moveScore += 6;
        if (ai->attackerMon.percenthp < 10) {
            moveScore += 10;
        } else if (ai->attackerMon.percenthp < 33) {
            if (BattleRand(bsys) % 10 < 7) { // 70%
                moveScore += 8;
            }
        } else if (ai->attackerMon.percenthp < 66) {
            if (BattleRand(bsys) % 2) { // 50%
                moveScore += 7;
            }
        } else {
            if (BattleRand(bsys) % 20 == 0) { // 5%
                moveScore += 7;
            }
        }

        if (ai->livingMembersAttacker == 1) {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_PARALYZE_HIT:
        if (ctx->moveTbl[ai->attackerMove].secondaryEffectChance < 100 || ai->attackerRolledMoveDamages[i] > 0 /*no immunity*/) { // nuzzle
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_STATUS_PARALYZE:
        if (ai->defenderMovesFirst || // and slower after para
            (BattlerKnowsMove(bsys, attacker, MOVE_HEX, ai) == TRUE) || (BattlerKnowsFlinchingMove(bsys, attacker, ai) == TRUE) || ctx->battlemon[ai->defender].condition2 & STATUS2_ATTRACT || ctx->battlemon[ai->defender].condition2 & STATUS2_CONFUSION) {
            moveScore += 8;
        } else {
            moveScore += 7;
        }
        if (BattleRand(bsys) % 2) { // 50%
            moveScore -= 1;
        }
        break;
    case MOVE_EFFECT_STATUS_BURN:
        moveScore += 6;
        if (BattleRand(bsys) % 3 == 0) {
            if ((BattlerKnowsMove(bsys, attacker, MOVE_HEX, ai) == TRUE)
                || (ai->isDoubleBattle && BattlerKnowsMove(bsys, BATTLER_ALLY(attacker), MOVE_HEX, ai))) { // or partner
                moveScore += 1;
            }
            if (ai->defenderHasAtleastOnePhysicalMove) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_STATUS_SLEEP:
    case MOVE_EFFECT_STATUS_SLEEP_NEXT_TURN:
        moveScore += 6;
        if (!ai->monCanOneShotPlayerWithAnyMove && (BattleRand(bsys) % 4 == 0)) // no kill
        {
            if (!ai->defenderImmuneToSleep) {
                moveScore += 1;
            }
            if (((BattlerKnowsMove(bsys, attacker, MOVE_DREAM_EATER, ai) == TRUE) || (BattlerKnowsMove(bsys, attacker, MOVE_NIGHTMARE, ai) == TRUE))
                && (BattlerKnowsMove(bsys, attacker, MOVE_SNORE, ai) == FALSE) && (BattlerKnowsMove(bsys, attacker, MOVE_SLEEP_TALK, ai) == FALSE)) {
                moveScore += 1;
            }
            if ((BattlerKnowsMove(bsys, attacker, MOVE_HEX, ai) == TRUE)
                || (ai->isDoubleBattle && BattlerKnowsMove(bsys, BATTLER_ALLY(attacker), MOVE_HEX, ai))) // or partner
            {
                moveScore += 1;
            }
        }
        if (ai->attackerMon.ability == ABILITY_PRANKSTER) {
            moveScore += 2;
        }
        break;
    case MOVE_EFFECT_STATUS_POISON:
    case MOVE_EFFECT_STATUS_BADLY_POISON:
        moveScore += 6;
        if (!ai->monCanOneShotPlayerWithAnyMove && (BattleRand(bsys) % 3 == 0)) // no kill
        {
            if (!ai->defenderImmuneToPoison && ai->defenderMon.percenthp > 20) {
                if (ai->maxDamageReceived < ai->attackerMon.hp / 2 && (BattlerKnowsMove(bsys, attacker, MOVE_HEX, ai) || BattlerKnowsMove(bsys, attacker, MOVE_VENOM_DRENCH, ai) || BattlerKnowsMove(bsys, attacker, MOVE_VENOSHOCK, ai) || ai->attackerMon.ability == ABILITY_MERCILESS)) {
                    moveScore += 2;
                }
            }
        }
        break;
    case MOVE_EFFECT_SWITCH_HELD_ITEMS:
        moveScore += 5;
        if ((ai->attackerMon.item == ITEM_TOXIC_ORB && !ai->defenderImmuneToPoison) || (ai->attackerMon.item == ITEM_FLAME_ORB && !ai->defenderImmuneToBurn) || (ai->attackerMon.item == ITEM_BLACK_SLUDGE && !HasType(ctx, ai->defender, TYPE_POISON))) {
            moveScore += 1;
            if (BattleRand(bsys) % 2) { // 50%
                moveScore += 1;
            }
        } else if (ai->attackerMon.item == ITEM_IRON_BALL || ai->attackerMon.item == ITEM_LAGGING_TAIL || ai->attackerMon.item == ITEM_STICKY_BARB) {
            moveScore += 2;
        }
        break;
    case MOVE_EFFECT_SP_ATK_UP_CAUSE_CONFUSION:
    case MOVE_EFFECT_ATK_UP_2_STATUS_CONFUSION:
    case MOVE_EFFECT_STATUS_CONFUSE:
        if ((ctx->battlemon[ai->defender].condition2 & STATUS2_CONFUSION)
            || (ctx->side_condition[ai->defenderSide] & SIDE_STATUS_SAFEGUARD)
            || (!ai->attackerMon.hasMoldBreaker && ai->defenderMon.ability == ABILITY_OWN_TEMPO)) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
            if (ai->defenderMon.condition & STATUS_PARALYSIS || ctx->battlemon[ai->defender].condition2 & STATUS2_ATTRACT) {
                moveScore += 2;
            }
        }
        break;
    case MOVE_EFFECT_INFATUATE:
        if (ctx->battlemon[ai->defender].condition2 & STATUS2_ATTRACT
            || (!ai->attackerMon.hasMoldBreaker && ai->defenderMon.ability == ABILITY_OBLIVIOUS)
#ifndef ATTRACT_WORK_ON_ALL_SEXES
            || ai->attackerMon.sex != POKEMON_GENDER_UNKNOWN
            || ai->attackerMon.sex == ai->defenderMon.sex
#endif // !ATTRACT_WORK_ON_ALL_SEXES
        )

        {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
            if (ai->defenderMon.condition & STATUS_PARALYSIS || ctx->battlemon[ai->defender].condition2 & STATUS2_CONFUSION) {
                moveScore += 2;
            }
        }
        break;
    case MOVE_EFFECT_STATUS_LEECH_SEED:
        if ((ctx->battlemon[ai->defender].effect_of_moves & MOVE_EFFECT_FLAG_LEECH_SEED_ACTIVE) || HasType(ctx, ai->defender, TYPE_GRASS) || ai->defenderMon.ability == ABILITY_MAGIC_GUARD) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
            if (ai->playerCanOneShotMonWithAnyMove == FALSE && ai->defenderMon.percenthp > 20) {
                moveScore += 1;
                if (BattleRand(bsys) % 2) { // 50%
                    moveScore += 1;
                }
            }
            if (ctx->battlemon[ai->defender].condition2 & STATUS2_ATTRACT || ctx->battlemon[ai->defender].condition2 & STATUS2_CONFUSION) {
                moveScore += 1;
            }
            if (ai->defenderMon.condition & (STATUS_POISON_ALL | STATUS_BURN | STATUS_PARALYSIS)) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_USE_RANDOM_LEARNED_MOVE_SLEEP:
    case MOVE_EFFECT_DAMAGE_WHILE_ASLEEP:
        if (ctx->battlemon[attacker].condition & STATUS_SLEEP) {
            moveScore += 6;
            if (ai->attackerLastUsedMove == MOVE_REST) {
                moveScore += 10;
            }
            if (BattleRand(bsys) % 2 == 0) {
                moveScore += 1;
            }
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_CURSE:
        if (!HasType(ctx, attacker, TYPE_GHOST)) {
            break;
        }
        FALLTHROUGH;
    case MOVE_EFFECT_KO_MON_THAT_DEFEATED_USER:
        if (ai->playerCanOneShotMonWithAnyMove && ai->attackerMovesFirst) {
            moveScore += 6;
            if (BattleRand(bsys) % 5 == 0) {
                moveScore += 1;
            }
        } else if (ai->defenderMovesFirst) {
            moveScore += 5;
            if (BattleRand(bsys) % 2) {
                moveScore += 1;
            }
        }
        break;
    case MOVE_EFFECT_CHANGE_TO_WATER_TYPE:
        if (HasType(ctx, ai->defender, TYPE_WATER)) {
            moveScore -= NEVER_USE_MOVE_20;
        } else {
            moveScore += 6;
        }
        break;
    case MOVE_EFFECT_ATK_DOWN:
    case MOVE_EFFECT_ATK_DOWN_2:
    case MOVE_EFFECT_ATK_DOWN_3:
    case MOVE_EFFECT_ATK_DEF_DOWN:
        if (ai->defenderHasAtleastOnePhysicalMove) {
            moveScore += 5;
            if (ai->attackerMon.hasMoldBreaker || (ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_CONTRARY && ai->defenderMon.ability != ABILITY_HYPER_CUTTER)) {
                moveScore += 1;
            }
            if (ai->defenderMovesFirst) {
                moveScore -= 1;
            }
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_SP_ATK_DOWN:
    case MOVE_EFFECT_SP_ATK_DOWN_2:
    case MOVE_EFFECT_SP_ATK_DOWN_3:
    case MOVE_EFFECT_SP_ATK_DOWN_2_OPPOSITE_GENDER:
    case MOVE_EFFECT_ATK_SP_ATK_DOWN:
        if (ai->defenderHasAtleastOneSpecialMove) {
            moveScore += 5;
            if (ai->attackerMon.hasMoldBreaker || (ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_CONTRARY)) {
                moveScore += 1;
            }
            if (ai->defenderMovesFirst) {
                moveScore -= 1;
            }
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_DEF_DOWN:
    case MOVE_EFFECT_DEF_DOWN_2:
    case MOVE_EFFECT_DEF_DOWN_3:
        if (2 * ai->maxDamageReceived < ai->attackerMon.hp && 2 * ai->attackerRolledMaxDamage < ai->defenderMon.hp) {
            if (ai->attackerMon.hasMoldBreaker || (ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_BIG_PECKS && ai->defenderMon.ability != ABILITY_CONTRARY)) {
                moveScore += 7;
            }
        }
        break;
    case MOVE_EFFECT_SP_DEF_DOWN:
    case MOVE_EFFECT_SP_DEF_DOWN_2:
    case MOVE_EFFECT_SP_DEF_DOWN_3:
        if (2 * ai->maxDamageReceived < ai->attackerMon.hp && 2 * ai->attackerRolledMaxDamage < ai->defenderMon.hp) {
            if (ai->attackerMon.hasMoldBreaker || (ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_CONTRARY)) {
                moveScore += 7;
            }
        }
        break;
    case MOVE_EFFECT_EVA_DOWN:
    case MOVE_EFFECT_EVA_DOWN_2:
    case MOVE_EFFECT_EVA_DOWN_3:
    case MOVE_EFFECT_ACC_DOWN:
    case MOVE_EFFECT_ACC_DOWN_2:
    case MOVE_EFFECT_ACC_DOWN_3:
        moveScore += 5;
        if (ai->attackerMon.hasMoldBreaker || (ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_KEEN_EYE && ai->defenderMon.ability != ABILITY_CONTRARY)) {
            moveScore += 1;
        }
        if (ai->defenderMovesFirst) {
            moveScore -= 1;
        }
        break;
    case MOVE_EFFECT_VENOM_DRENCH:
        if (ai->defenderMon.condition & STATUS_POISON_ALL) {
            if ((ai->defenderHasAtleastOnePhysicalMove && ctx->battlemon[ai->defender].states[STAT_ATTACK] >= 6) || (ai->defenderHasAtleastOneSpecialMove && ctx->battlemon[ai->defender].states[STAT_SPATK] >= 6)) {
                moveScore += 5;
                if (ai->attackerMon.hasMoldBreaker || (ai->defenderMon.ability != ABILITY_CLEAR_BODY && ai->defenderMon.ability != ABILITY_WHITE_SMOKE && ai->defenderMon.ability != ABILITY_CONTRARY)) {
                    moveScore += 1;
                }
                if (ai->defenderMovesFirst) {
                    moveScore -= 1;
                }
            } else {
                moveScore -= NEVER_USE_MOVE_20;
            }
        } else {
            moveScore -= NEVER_USE_MOVE_20;
        }
        break;
    case MOVE_EFFECT_MIRROR_COAT:
        if (ai->attackerMovesFirst) {
            moveScore -= 1;
        }
        if (ai->defenderHasAtleastOneSpecialMove && !ai->playerCanOneShotMonWithAnyMove)
        {
            moveScore += 6;
        }
        break;
    case MOVE_EFFECT_COUNTER:
        if (ai->attackerMovesFirst) {
            moveScore -= 1;
        }
        if (ai->defenderHasAtleastOnePhysicalMove && !ai->playerCanOneShotMonWithAnyMove) {
            moveScore += 6;
        }
        break;
    default:
        break;
    }

    return moveScore;
}

u32 LONG_CALL GetRecoverAmountPercent(struct BattleSystem *bsys, u32 attackerMove, u32 attackerMoveEffect)
{
    struct BattleStruct *ctx = bsys->sp;
    u32 recoverAmountPercent = 50;
    switch (attackerMoveEffect) {
    case MOVE_EFFECT_RECOVER_HEALTH_AND_SLEEP:
        recoverAmountPercent = 100;
        break;
    case MOVE_EFFECT_HEAL_HALF_DIFFERENT_IN_WEATHER:
        if ((attackerMove == MOVE_SHORE_UP && (ctx->field_condition & WEATHER_SANDSTORM_ANY))
            || (attackerMove != MOVE_SHORE_UP && (ctx->field_condition & WEATHER_SUNNY_ANY))) {
            recoverAmountPercent = 67;
        } else if (ctx->field_condition & WEATHER_RAIN_ANY) {
            recoverAmountPercent = 25;
        }
        break;
    default:
        break;
    }
    return recoverAmountPercent;
}

BOOL LONG_CALL shouldRecover(struct BattleSystem *bsys, u32 attacker UNUSED, u32 attackerMoveEffect, struct AIContext *ai)
{
    u32 recoverAmountPercent = GetRecoverAmountPercent(bsys, ai->attackerMove, attackerMoveEffect);

    u32 recoverAmountHP = recoverAmountPercent * ai->attackerMon.maxhp / 100;
    if ((recoverAmountHP + ai->attackerMon.hp) > ai->attackerMon.maxhp) {
        recoverAmountHP = ai->attackerMon.maxhp - ai->attackerMon.hp; // prevent overheal
    }

    if (ai->attackerMon.condition & STATUS_BAD_POISON) {
        return FALSE;
    }
    if (ai->maxDamageReceived > recoverAmountHP) {
        return FALSE;
    }
    if (ai->attackerMovesFirst) {
        if (ai->playerCanOneShotMonWithAnyMove && ai->maxDamageReceived < (ai->attackerMon.hp + recoverAmountHP)) { // cannot kill after heal
            return TRUE;
        }
        if (!ai->playerCanOneShotMonWithAnyMove) {
            if (ai->attackerMon.percenthp < 40) {
                return TRUE;
            } else if (ai->attackerMon.percenthp < 66) {
                return FALSE + (BattleRand(bsys) % 2);
            }
        }
    } else {
        if (ai->attackerMon.percenthp < 50) {
            return TRUE;
        } else if (ai->attackerMon.percenthp < 70 && (BattleRand(bsys) % 4) != 0) { // 75%
            return TRUE;
        }
    }

    return FALSE;
}

int LONG_CALL RecoveryScoring(struct BattleSystem *bsys, u32 attacker, int i, struct AIContext *ai)
{
    int moveScore = 0;
    struct BattleStruct *ctx = bsys->sp;
    BOOL isHealingMove = TRUE;

    ai->attackerMove = ctx->battlemon[attacker].move[i];
    ai->attackerMoveEffect = ctx->moveTbl[ai->attackerMove].effect;

    BOOL aiShouldRecover = shouldRecover(bsys, attacker, ai->attackerMoveEffect, ai);
    if (ai->attackerMon.percenthp >= 85) {
        moveScore -= 6;
    }

    switch (ai->attackerMoveEffect) {
    // case MOVE_EFFECT_HIT_STRENGTH_SAP //TODO
    case MOVE_EFFECT_HEAL_HALF_REMOVE_FLYING_TYPE:
    case MOVE_EFFECT_RESTORE_HALF_HP:
        if (aiShouldRecover) {
            moveScore += 7;
        } else {
            moveScore += 5;
        }
        break;
    case MOVE_EFFECT_HEAL_HALF_DIFFERENT_IN_WEATHER: {
        u32 recoverAmount = GetRecoverAmountPercent(bsys, ai->attackerMove, MOVE_EFFECT_HEAL_HALF_DIFFERENT_IN_WEATHER);
        if (aiShouldRecover && recoverAmount > 50) {
            moveScore += 7;
        } else if (recoverAmount == 50 && shouldRecover(bsys, attacker, MOVE_EFFECT_RESTORE_HALF_HP, ai)) {
            moveScore += 7;
        } else {
            moveScore += 5;
        }
        break;
    }
    case MOVE_EFFECT_RECOVER_HEALTH_AND_SLEEP:
        if (aiShouldRecover) {
            if (ai->attackerMon.item == ITEM_CHESTO_BERRY || ai->attackerMon.item == ITEM_LUM_BERRY
                || ai->attackerMon.ability == ABILITY_EARLY_BIRD || ai->attackerMon.ability == ABILITY_SHED_SKIN
                || BattlerKnowsMove(bsys, attacker, MOVE_SLEEP_TALK, ai) || BattlerKnowsMove(bsys, attacker, MOVE_SNORE, ai)
                || (ai->attackerMon.ability == ABILITY_HYDRATION && (ctx->field_condition & WEATHER_RAIN_ANY))) {
                moveScore += 8;
            } else {
                moveScore += 7;
            }
        } else {
            moveScore += 5;
        }
        break;
    default:
        isHealingMove = FALSE;
        break;
    }

    if (!isHealingMove) {
        moveScore = 0;
    } else if (ai->attackerMon.percenthp == 100) {
        moveScore = 0 - NEVER_USE_MOVE_20;
    }

    return moveScore;
}



BOOL LONG_CALL HasMovePriority(struct BattleSystem *bsys, u8 attacker, u32 attackerMove, u32 attackerAbility, u8 defender)
{
    struct BattleStruct *ctx = bsys->sp;
    struct BattleMove attackerMoveStruct = ctx->moveTbl[attackerMove];
    BOOL hasPriority = FALSE;
    if (attackerMove == MOVE_GRASSY_GLIDE 
        && ctx->terrainOverlay.type == GRASSY_TERRAIN 
        && ctx->terrainOverlay.numberOfTurnsLeft > 0)
    {
        hasPriority = TRUE;
    } 
    else if (HasMovePranksterPriority(bsys, attacker, attackerMove, attackerAbility, defender))
    {
        hasPriority = TRUE;
    } 
    else if (attackerMoveStruct.priority)
    {
        hasPriority = TRUE;
    }
    return hasPriority;
}

BOOL LONG_CALL HasMovePranksterPriority(struct BattleSystem *bsys, u8 attacker, u32 attackerMove, u32 attackerAbility, u8 defender)
{
    struct BattleStruct *ctx = bsys->sp;
    struct BattleMove attackerMoveStruct = ctx->moveTbl[attackerMove];

    if (attackerAbility == ABILITY_PRANKSTER
        && attackerMoveStruct.split == SPLIT_STATUS
        && ctx->clientPriority[attacker] > 0
        && (attackerMoveStruct.target == RANGE_ADJACENT_OPPONENTS
            || (attackerMoveStruct.target == RANGE_SINGLE_TARGET
                && BATTLERS_ON_DIFFERENT_SIDE(attacker, defender))))
    {
        return TRUE;
    }
    return FALSE;
}
