#ifndef CUSTOM_AI_H
#define CUSTOM_AI_H
#include "../battle.h"

//#define BATTLE_DEBUG_OUTPUT 1

struct PACKED AI_sDamageCalc {
    u16 species;
    u16 hp;
    u16 maxhp;
    u16 percenthp;
    u16 dummy;
    u32 item;
    u32 item_held_effect;
    u32 item_power;

    u32 condition;
    u32 condition2;
    u8 isGrounded;

    u16 ability;
    u8 sex;
    u8 type1;
    u8 type2;
    u8 type3;

    u32 speed;

    u32 weight;

    u32 attack;
    u32 defense;
    u32 sp_attack;
    u32 sp_defense;
    s8 states[8];
    u8 level;
    u32 form;

    BOOL hasMoldBreaker;
    u32 effect_of_moves;
    BOOL flashFireActivated;
    u8 hiddenPowerType;

    u8 slowStartCount;
    u8 furyCutterCount;
    u8 metronomeTurns;
    u8 lastResortCount;
    u8 attackerHasMoveFailureLastTurn;
};

struct PACKED AIContext {
    struct AI_sDamageCalc attackerMon;
    struct AI_sDamageCalc defenderMon; //"player"
    struct AI_sDamageCalc defenderAlly;

    BOOL isAllyAlive;
    BOOL isDoubleBattle;
    BOOL isPartnerGrounded;

    BOOL defenderImmuneToPoison;
    BOOL defenderImmuneToParalysis;
    BOOL defenderImmuneToBurn;
    BOOL defenderImmuneToSleep;

    u8 attackerMovesFirst;
    u8 defenderMovesFirst;
    u8 isSpeedTie;

    int attacker;
    int defender;
    int defenderSide;
    int attackerSide;

    int attackerSpeed;
    int defenderSpeed;

    u8 partySizeAttacker;
    u8 livingMembersAttacker;
    u8 monWithMegaInParty;

    u8 partySizeDefender;
    u8 livingMembersDefender;

    u8 attackerMovesKnown;
    BOOL attackerHasAttackingMoves;

    u32 attackerLastUsedMove;
    u32 defenderLastUsedMove;
    u32 defenderLastUsedMoveEffect;
    u32 defenderTurnsOnField;
    u32 attackerTurnsOnField;

    u32 attackerMove;
    u32 attackerMoveEffect;

    BOOL isDefenderIncapacitated;
    BOOL defenderKnowsThawingMove;
    BOOL defenderHasAtleastOneUsefulSoundMove;
    BOOL defenderCanForceSwitching;

    u32 maxDamageReceived;

    BOOL defenderHasAtleastOnePhysicalMove;
    BOOL defenderHasAtleastOneSpecialMove;
    BOOL playerCanOneShotMonWithMove[4];
    BOOL playerCanOneShotMonWithAnyMove;
    BOOL monCanOneShotPlayerWithAnyMove;
    
    BOOL monCanOneShotPlayerWithMove[4];
    u32 attackerRolledMoveDamages[4];
    u32 effectivenessOnPlayer[4];
    u32 attackerRolledMaxDamage;
};

struct PACKED AI_damage {
    u32 damageRoll;
    u32 damageRange[16];
    u32 moveEffectiveness;
};

void LONG_CALL SetupStateVariables(struct BattleSystem *bsys, u32 attacker, u32 defender, struct AIContext *ai);

int LONG_CALL BattleAI_PostKOSwitchIn(struct BattleSystem *bsys, int attacker);
int LONG_CALL BattleAI_PostKOSwitchIn_Internal(struct BattleSystem *bsys, int attacker, int *score, BOOL calcWithHighestDamageHit);

u8 LONG_CALL BattleAI_CalcSpeed(void *bw, struct BattleStruct *sp, int client1, struct PartyPokemon *partyMon, int flag);

int LONG_CALL BattleAI_CalcBaseDamage(void *bw, struct BattleStruct *sp, int moveno, u32 side_cond, u32 field_cond, u16 pow, u8 type, u8 critical, u8 attackerSlot, u8 defenderSlot, struct AI_sDamageCalc *attacker, struct AI_sDamageCalc *defender);
int LONG_CALL BattleAI_CalcDamage(void *bw, struct BattleStruct *sp, int moveno, u32 side_cond, u32 field_cond, u16 pow, u8 type, u8 critical, u8 attackerSlot, u8 defenderSlot, struct AI_damage *damages, struct AI_sDamageCalc *attacker, struct AI_sDamageCalc *defender);

void LONG_CALL FillDamageStructFromPartyMon(void *bw UNUSED, struct BattleStruct *sp, struct AI_sDamageCalc *monStruct, struct PartyPokemon *pp);
void LONG_CALL FillDamageStructFromBattleMon(void *bw, struct BattleStruct *sp, struct AI_sDamageCalc *monStruct, int numSlot);

BOOL LONG_CALL IsMoveBoostedBySheerForce(u32 moveno, u32 moveeffect);
BOOL LONG_CALL BattleAI_IsContactBeingMade(struct BattleStruct *sp, u32 ability, u32 itemHoldEffect, u32 moveno);
int LONG_CALL BattleAI_GetTypeEffectiveness(void *bw, struct BattleStruct *sp, int moveno, int move_type, u32 *flag, struct AI_sDamageCalc *attacker, struct AI_sDamageCalc *defender);

BOOL LONG_CALL BattleAI_AttackerHasOnlyIneffectiveMoves(struct BattleStruct *ctx, u32 attacker, int knownMoves, u32 effectiveness[4]);

int LONG_CALL BattleAI_AdjustUnusualMoveDamage(u32 attackerLevel, u32 attackerHP, u32 defenderHP, u32 damage, u32 moveEffect, u32 attackerAbility, u32 attackerItem);
int LONG_CALL BattleAI_GetDynamicMoveType(struct BattleSystem *bsys, struct BattleStruct *ctx, struct AI_sDamageCalc *attacker, int moveno);
BOOL LONG_CALL BattleAI_IsKnockOffPoweredUp(struct AI_sDamageCalc *defender);

BOOL LONG_CALL CanAttackerOneShotDefender(u32 attackerHighestDamage, u8 split, u32 moveno, struct AI_sDamageCalc *attacker, struct AI_sDamageCalc *defender);
BOOL LONG_CALL BattlerKnowsThawingMove(struct BattleSystem *bsys, u32 battler, struct AIContext *ai);

BOOL LONG_CALL IsChoicedMoveConsidedUseless(u32 moveno, u8 split);

BOOL LONG_CALL IsBattleMonSlowerThanOpposition(struct BattleSystem *bsys, u8 slot, BOOL isDoubleBattle);

BOOL LONG_CALL IsPartyPokemonGrounded(struct BattleStruct *sp, struct PartyPokemon *pp);



int LONG_CALL BattlerPositiveStatChangesSum(struct BattleSystem *bsys, u32 battler, struct AIContext *ai UNUSED);
BOOL LONG_CALL MonDiesFromResidualDamage(struct BattleStruct *ctx, u32 attacker, u32 attackerCondition, BOOL isSeeded);
BOOL LONG_CALL IsMonInflictedWithAnyNegativeStatus(struct BattleStruct *ctx, u32 attacker);

BOOL LONG_CALL BattlerHasSoundBasedMove(struct BattleSystem *bsys, u32 battler, struct AIContext *ai);
BOOL LONG_CALL BattlerKnowsFlinchingMove(struct BattleSystem *bsys, u32 battler, struct AIContext *ai);
BOOL LONG_CALL BattlerKnowsMove(struct BattleSystem *bsys, u32 battler, u32 move, struct AIContext *ai);
#endif // !CUSTOM_AI_H
