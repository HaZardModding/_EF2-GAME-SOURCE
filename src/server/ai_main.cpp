// Copyright (C) 1999-2000 Id Software, Inc.
//

/*****************************************************************************
 * name:		ai_main.c
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /EF2/Code/DLLs/game/ai_main.cpp $
 * $Author: Singlis $ 
 * $Revision: 18 $
 * $Modtime: 9/24/03 3:01p $
 * $Date: 9/26/03 2:35p $
 *
 *****************************************************************************/


#include "g_local.h"
#include <qcommon/q_shared.h>
#include "botlib.h"		//bot lib interface
#include "be_aas.h"
#include "be_ea.h"
#include "be_ai_char.h"
#include "be_ai_chat.h"
#include "be_ai_gen.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"
#include "be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_vcmd.h"

//
#include "chars.h"
#include "inv.h"
#include "syn.h"

#include "player.h"

#define MAX_PATH		144


//bot states
bot_state_t	*botstates[MAX_CLIENTS];
//number of bots
int numbots;
//floating point time
float floattime;
//time to do a regular update
float regularupdate_time;
//
int bot_interbreed;
int bot_interbreedmatchcount;
//
vmCvar_t bot_thinktime;
vmCvar_t bot_memorydump;
vmCvar_t bot_saveroutingcache;
vmCvar_t bot_pause;
vmCvar_t bot_report;
vmCvar_t bot_testsolid;
vmCvar_t bot_testclusters;
vmCvar_t bot_developer;
vmCvar_t bot_interbreedchar;
vmCvar_t bot_interbreedbots;
vmCvar_t bot_interbreedcycle;
vmCvar_t bot_interbreedwrite;


/*
==================
BotAI_Print
==================
*/
//--------------------------------------------------------------
// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] for BotRandomWeaponName - chrissstrahl
//--------------------------------------------------------------
void QDECL BotAI_Print(int type, const char *fmt, ...) {
	char str[2048];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(str, fmt, ap);
	va_end(ap);

	switch(type) {
		case PRT_MESSAGE: {
			gi.Printf("%s", str);
			break;
		}
		case PRT_WARNING: {
			gi.Printf( S_COLOR_YELLOW "Warning: %s", str );
			break;
		}
		case PRT_ERROR: {
			gi.Printf( S_COLOR_RED "Error: %s", str );
			break;
		}
		case PRT_FATAL: {
			gi.Printf( S_COLOR_RED "Fatal: %s", str );
			break;
		}
		case PRT_EXIT: {
			gi.Printf( S_COLOR_RED "Exit: %s", str );
			break;
		}
		default: {
			gi.Printf( "unknown print type\n" );
			break;
		}
	}
}


/*
==================
BotAI_Trace
==================
*/
void BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask) {
	trace_t trace;

//	AAS_ClearShownDebugLines();
//	AAS_DebugLine(ms->origin, goal->origin, LINECOLOR_RED);
	gi.trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse);
	//copy the trace information
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy(trace.endpos, bsptrace->endpos);
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy(trace.plane.normal, bsptrace->plane.normal);
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}

/*
==================
BotAI_GetClientState
==================
*/
int BotAI_GetClientState( int clientNum, playerState_t *state ) {
	gentity_t	*ent;

	ent = &g_entities[clientNum];
	if ( !ent->inuse ) {
		return qfalse;
	}
	if ( !ent->client ) {
		return qfalse;
	}

	memcpy( state, &ent->client->ps, sizeof(playerState_t) );
	return qtrue;
}

/*
==================
BotAI_GetEntityState
==================
*/
int BotAI_GetEntityState( int entityNum, entityState_t *state ) {
	gentity_t	*ent;

	ent = &g_entities[entityNum];
	memset( state, 0, sizeof(entityState_t) );
	if (!ent->inuse) return qfalse;
	if (!ent->linked) return qfalse;
	if (ent->svflags & SVF_NOCLIENT) return qfalse;
	memcpy( state, &ent->s, sizeof(entityState_t) );
	return qtrue;
}

/*
==================
BotAI_GetSnapshotEntity
==================
*/
int BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state ) {
	int		entNum;

	entNum = gi.BotGetSnapshotEntity( clientNum, sequence );
	if ( entNum == -1 ) {
		memset(state, 0, sizeof(entityState_t));
		return -1;
	}

	BotAI_GetEntityState( entNum, state );

	return sequence + 1;
}

/*
==================
BotAI_BotInitialChat
==================
*/
//--------------------------------------------------------------
// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] - chrissstrahl
//--------------------------------------------------------------
void QDECL BotAI_BotInitialChat(bot_state_t* bs, const char* type, ...) {
	int		i, mcontext;
	va_list	ap;

	//--------------------------------------------------------------
	// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] - chrissstrahl
	//--------------------------------------------------------------
	const char* p;
	const char* vars[MAX_MATCHVARIABLES];


	memset(vars, 0, sizeof(vars));
	va_start(ap, type);


	//--------------------------------------------------------------
	// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] - chrissstrahl
	//--------------------------------------------------------------
	p = va_arg(ap, const char*);


	for (i = 0; i < MAX_MATCHVARIABLES; i++) {
		if( !p ) {
			break;
		}
		vars[i] = p;


		//--------------------------------------------------------------
		// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] - chrissstrahl
		//--------------------------------------------------------------
		p = va_arg(ap, const char*);
	}
	va_end(ap);

	mcontext = BotSynonymContext(bs);

	gi.BotInitialChat( bs->cs, type, mcontext, vars[0], vars[1], vars[2], vars[3], vars[4], vars[5], vars[6], vars[7] );
}


/*
==================
BotTestAAS
==================
*/
void BotTestAAS(vec3_t origin) {
	int areanum;
	aas_areainfo_t info;

	gi.Cvar_Update(&bot_testsolid);
	gi.Cvar_Update(&bot_testclusters);
	if (bot_testsolid.integer) {
		if (!gi.AAS_Initialized()) return;
		areanum = BotPointAreaNum(origin);
		if (areanum) BotAI_Print(PRT_MESSAGE, "\remtpy area");
		else BotAI_Print(PRT_MESSAGE, "\r^1SOLID area");
	}
	else if (bot_testclusters.integer) {
		if (!gi.AAS_Initialized()) return;
		areanum = BotPointAreaNum(origin);
		if (!areanum)
			BotAI_Print(PRT_MESSAGE, "\r^1Solid!                              ");
		else {
			gi.AAS_AreaInfo(areanum, &info);
			BotAI_Print(PRT_MESSAGE, "\rarea %d, cluster %d       ", areanum, info.cluster);
		}
	}
}

/*
==================
BotReportStatus
==================
*/
void BotReportStatus(bot_state_t *bs) {
	char goalname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char flagstatus[32];


	//--------------------------------------------------------------
	// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] - chrissstrahl
	//--------------------------------------------------------------
	const char *leader;
	
	
	//
	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) == 0) leader = "L";
	else leader = " ";

	strcpy(flagstatus, "  ");
	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs)) {
			if (BotTeam(bs) == TEAM_RED) strcpy(flagstatus, S_COLOR_RED"F ");
			else strcpy(flagstatus, S_COLOR_BLUE"F ");
		}
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs)) {
			if (BotTeam(bs) == TEAM_RED) strcpy(flagstatus, S_COLOR_RED"F ");
			else strcpy(flagstatus, S_COLOR_BLUE"F ");
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs)) {
			if (BotTeam(bs) == TEAM_RED) Com_sprintf(flagstatus, sizeof(flagstatus), S_COLOR_RED"%2d", bs->inventory[INVENTORY_REDCUBE]);
			else Com_sprintf(flagstatus, sizeof(flagstatus), S_COLOR_BLUE"%2d", bs->inventory[INVENTORY_BLUECUBE]);
		}
	}
#endif

	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: helping %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: accompanying %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_DEFENDKEYAREA:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: defending %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_GETITEM:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: getting item %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_KILL:
		{
			ClientName(bs->teamgoal.entitynum, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: killing %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: camping\n", netname, leader, flagstatus);
			break;
		}
		case LTG_PATROL:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: patrolling\n", netname, leader, flagstatus);
			break;
		}
		case LTG_GETFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: capturing flag\n", netname, leader, flagstatus);
			break;
		}
		case LTG_RUSHBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: rushing base\n", netname, leader, flagstatus);
			break;
		}
		case LTG_RETURNFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: returning flag\n", netname, leader, flagstatus);
			break;
		}
		case LTG_ATTACKENEMYBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: attacking the enemy base\n", netname, leader, flagstatus);
			break;
		}
		case LTG_HARVEST:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: harvesting\n", netname, leader, flagstatus);
			break;
		}
		default:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: roaming\n", netname, leader, flagstatus);
			break;
		}
	}
}

/*
==================
BotTeamplayReport
==================
*/
void BotTeamplayReport(void) {
	int i;
	char buf[MAX_INFO_STRING];

	BotAI_Print(PRT_MESSAGE, S_COLOR_RED"RED\n");
	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		//
		if ( !botstates[i] || !botstates[i]->inuse ) continue;
		//
		strncpy(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));


		//--------------------------------------------------------------
		// GAMEFIX - Fixed: Warning: C6053 Due to the previous call to strncpy, the string ? may not be null-terminated. - chrissstrahl
		//--------------------------------------------------------------
		buf[MAX_INFO_STRING - 1] = '\0';


		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_RED) {
			BotReportStatus(botstates[i]);
		}
	}
	BotAI_Print(PRT_MESSAGE, S_COLOR_BLUE"BLUE\n");
	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		//
		if ( !botstates[i] || !botstates[i]->inuse ) continue;
		//
		strncpy(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));


		//--------------------------------------------------------------
		// GAMEFIX - Fixed: Warning: C6053 Due to the previous call to strncpy, the string ? may not be null-terminated. - chrissstrahl
		//--------------------------------------------------------------
		buf[MAX_INFO_STRING - 1] = '\0';


		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		//skip spectators
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_BLUE) {
			BotReportStatus(botstates[i]);
		}
	}
}

/*
==================
BotSetInfoConfigString
==================
*/
void BotSetInfoConfigString(bot_state_t *bs) {
	char goalname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char action[MAX_MESSAGE_SIZE];
	char carrying[32];
	bot_goal_t goal;


	//--------------------------------------------------------------
	// GAMEFIX - Fixed: warning: ISO C++ forbids converting a string constant to char * [-Wwrite - strings] - chrissstrahl
	//--------------------------------------------------------------
	const char *leader;

	
	//
	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) == 0) leader = "L";
	else leader = " ";

	strcpy(carrying, "  ");
	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs)) {
			strcpy(carrying, "F ");
		}
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs)) {
			strcpy(carrying, "F ");
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs)) {
			if (BotTeam(bs) == TEAM_RED) Com_sprintf(carrying, sizeof(carrying), "%2d", bs->inventory[INVENTORY_REDCUBE]);
			else Com_sprintf(carrying, sizeof(carrying), "%2d", bs->inventory[INVENTORY_BLUECUBE]);
		}
	}
#endif

	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "helping %s", goalname);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "accompanying %s", goalname);
			break;
		}
		case LTG_DEFENDKEYAREA:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "defending %s", goalname);
			break;
		}
		case LTG_GETITEM:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "getting item %s", goalname);
			break;
		}
		case LTG_KILL:
		{
			ClientName(bs->teamgoal.entitynum, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "killing %s", goalname);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			Com_sprintf(action, sizeof(action), "camping");
			break;
		}
		case LTG_PATROL:
		{
			Com_sprintf(action, sizeof(action), "patrolling");
			break;
		}
		case LTG_GETFLAG:
		{
			Com_sprintf(action, sizeof(action), "capturing flag");
			break;
		}
		case LTG_RUSHBASE:
		{
			Com_sprintf(action, sizeof(action), "rushing base");
			break;
		}
		case LTG_RETURNFLAG:
		{
			Com_sprintf(action, sizeof(action), "returning flag");
			break;
		}
		case LTG_ATTACKENEMYBASE:
		{
			Com_sprintf(action, sizeof(action), "attacking the enemy base");
			break;
		}
		case LTG_HARVEST:
		{
			Com_sprintf(action, sizeof(action), "harvesting");
			break;
		}
		default:
		{
			gi.BotGetTopGoal(bs->gs, &goal);
			gi.BotGoalName(goal.number, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "roaming %s", goalname);
			break;
		}
	}
  	gi.setConfigstring (CS_BOTINFO + bs->client, va("l\\%s\\c\\%s\\a\\%s",
				leader,
				carrying,
				action));
}

/*
==============
BotUpdateInfoConfigStrings
==============
*/
void BotUpdateInfoConfigStrings(void) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		//
		if ( !botstates[i] || !botstates[i]->inuse )
			continue;
		//
		strncpy(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));


		//--------------------------------------------------------------
		// GAMEFIX - Fixed: Warning: C6053 Due to the previous call to strncpy, the string ? may not be null-terminated. - chrissstrahl
		//--------------------------------------------------------------
		buf[MAX_INFO_STRING - 1] = '\0';


		//if no config string or no name
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name")))
			continue;
		BotSetInfoConfigString(botstates[i]);
	}
}

/*
==============
BotInterbreedBots
==============
*/
void BotInterbreedBots(void) {
	float ranks[MAX_CLIENTS];
	int parent1, parent2, child;
	int i;

	// get rankings for all the bots
	for (i = 0; i < maxclients->integer; i++) {
		if ( botstates[i] && botstates[i]->inuse ) {
			ranks[i] = botstates[i]->num_kills * 2 - botstates[i]->num_deaths;
		}
		else {
			ranks[i] = -1;
		}
	}

	if (gi.GeneticParentsAndChildSelection(MAX_CLIENTS, ranks, &parent1, &parent2, &child)) {


		//--------------------------------------------------------------
		// GAMEFIX - Fixed: Warning: C28182 Dereferencing NULL-Pointer ?, contains same NULL-Value as ? - chrissstrahl
		//--------------------------------------------------------------
		if (botstates[parent1] && botstates[parent1]->inuse &&
			botstates[parent2] && botstates[parent2]->inuse &&
			botstates[child] && botstates[child]->inuse)
		{
			gi.BotInterbreedGoalFuzzyLogic(botstates[parent1]->gs, botstates[parent2]->gs, botstates[child]->gs);
			gi.BotMutateGoalFuzzyLogic(botstates[child]->gs, 1);
		}
	}
	// reset the kills and deaths
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (botstates[i] && botstates[i]->inuse) {
			botstates[i]->num_kills = 0;
			botstates[i]->num_deaths = 0;
		}
	}
}

/*
==============
BotWriteInterbreeded
==============
*/
void BotWriteInterbreeded(char *filename) {
	float rank, bestrank;
	int i, bestbot;

	bestrank = 0;
	bestbot = -1;
	// get the best bot
	for (i = 0; i < MAX_CLIENTS; i++) {
		if ( botstates[i] && botstates[i]->inuse ) {
			rank = botstates[i]->num_kills * 2 - botstates[i]->num_deaths;
		}
		else {
			rank = -1;
		}
		if (rank > bestrank) {
			bestrank = rank;
			bestbot = i;
		}
	}
	if (bestbot >= 0) {
		//write out the new goal fuzzy logic
		gi.BotSaveGoalFuzzyLogic(botstates[bestbot]->gs, filename);
	}
}

/*
==============
BotInterbreedEndMatch

add link back into ExitLevel?
==============
*/
void BotInterbreedEndMatch(void) {

	if (!bot_interbreed) return;
	bot_interbreedmatchcount++;
	if (bot_interbreedmatchcount >= bot_interbreedcycle.integer) {
		bot_interbreedmatchcount = 0;
		//
		gi.Cvar_Update(&bot_interbreedwrite);
		if (strlen(bot_interbreedwrite.string)) {
			BotWriteInterbreeded(bot_interbreedwrite.string);
			gi.cvar_set("bot_interbreedwrite", "");
		}
		BotInterbreedBots();
	}
}

int BotAIShutdownClient(int client, qboolean restart);
void G_ExitLevel(void);
/*
==============
BotInterbreeding
==============
*/
void BotInterbreeding(void) {
	int i;

	gi.Cvar_Update(&bot_interbreedchar);
	if (!strlen(bot_interbreedchar.string)) return;
	//make sure we are in tournament mode
	if (gametype != GT_TOURNAMENT) {
		gi.cvar_set("g_gametype", va("%d", GT_TOURNAMENT));
		G_ExitLevel();
		return;
	}
	//shutdown all the bots
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (botstates[i] && botstates[i]->inuse) {
			BotAIShutdownClient(botstates[i]->client, qfalse);
		}
	}
	//make sure all item weight configs are reloaded and Not shared
	gi.BotLibVarSet("bot_reloadcharacters", "1");
	//add a number of bots using the desired bot character
	for (i = 0; i < bot_interbreedbots.integer; i++) {
		gi.SendConsoleCommand( va("addbot %s 4 free %i %s%d\n",
						bot_interbreedchar.string, i * 50, bot_interbreedchar.string, i) ); // EXEC_INSERT, 
	}
	//
	gi.cvar_set("bot_interbreedchar", "");
	bot_interbreed = qtrue;
}

/*
==============
BotEntityInfo
==============
*/
void BotEntityInfo(int entnum, aas_entityinfo_t *info) {
	gi.AAS_EntityInfo(entnum, info);

	if ( !g_entities[info->number].entity )
		info->valid = qfalse;
}

/*
==============
NumBots
==============
*/
int NumBots(void) {
	return numbots;
}

/*
==============
BotTeamLeader
==============
*/
int BotTeamLeader(bot_state_t *bs) {
	int leader;

	leader = ClientFromName(bs->teamleader);
	if (leader < 0) return qfalse;
	if (!botstates[leader] || !botstates[leader]->inuse) return qfalse;
	return qtrue;
}

/*
==============
AngleDifference
==============
*/
float AngleDifference(float ang1, float ang2) {
	float diff;

	diff = ang1 - ang2;
	if (ang1 > ang2) {
		if (diff > 180.0) diff -= 360.0;
	}
	else {
		if (diff < -180.0) diff += 360.0;
	}
	return diff;
}

/*
==============
BotChangeViewAngle
==============
*/
float BotChangeViewAngle(float angle, float ideal_angle, float speed) {
	float move;

	angle = AngleMod(angle);
	ideal_angle = AngleMod(ideal_angle);
	if (angle == ideal_angle) return angle;
	move = ideal_angle - angle;
	if (ideal_angle > angle) {
		if (move > 180.0) move -= 360.0;
	}
	else {
		if (move < -180.0) move += 360.0;
	}
	if (move > 0) {
		if (move > speed) move = speed;
	}
	else {
		if (move < -speed) move = -speed;
	}
	return AngleMod(angle + move);
}

/*
==============
BotChangeViewAngles
==============
*/
void BotChangeViewAngles(bot_state_t *bs, float thinktime) {
	float diff, factor, maxchange, anglespeed, disired_speed;
	int i;

	if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
	//
	if (bs->enemy >= 0) {
		factor = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_VIEW_FACTOR, 0.01f, 1);
		maxchange = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_VIEW_MAXCHANGE, 1, 1800);
	}
	else {
		factor = 0.05f;
		maxchange = 360;
	}
	if (maxchange < 240) maxchange = 240;
	maxchange *= thinktime;
	for (i = 0; i < 2; i++) {
		//
		if (bot_challenge.integer) {
			//smooth slowdown view model
			diff = abs((long)AngleDifference(bs->viewangles[i], (long)bs->ideal_viewangles[i]));
			anglespeed = diff * factor;
			if (anglespeed > maxchange) anglespeed = maxchange;
			bs->viewangles[i] = BotChangeViewAngle(bs->viewangles[i],
											bs->ideal_viewangles[i], anglespeed);
		}
		else {
			//over reaction view model
			bs->viewangles[i] = AngleMod(bs->viewangles[i]);
			bs->ideal_viewangles[i] = AngleMod(bs->ideal_viewangles[i]);
			diff = AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]);
			disired_speed = diff * factor;
			bs->viewanglespeed[i] += (bs->viewanglespeed[i] - disired_speed);
			if (bs->viewanglespeed[i] > 180) bs->viewanglespeed[i] = maxchange;
			if (bs->viewanglespeed[i] < -180) bs->viewanglespeed[i] = -maxchange;
			anglespeed = bs->viewanglespeed[i];
			if (anglespeed > maxchange) anglespeed = maxchange;
			if (anglespeed < -maxchange) anglespeed = -maxchange;
			bs->viewangles[i] += anglespeed;
			bs->viewangles[i] = AngleMod(bs->viewangles[i]);
			//demping
			bs->viewanglespeed[i] *= 0.45 * (1 - factor);
		}
		//BotAI_Print(PRT_MESSAGE, "ideal_angles %f %f\n", bs->ideal_viewangles[0], bs->ideal_viewangles[1], bs->ideal_viewangles[2]);`
		//bs->viewangles[i] = bs->ideal_viewangles[i];
	}
	//bs->viewangles[PITCH] = 0;
	if (bs->viewangles[PITCH] > 180) bs->viewangles[PITCH] -= 360;
	//elementary action: view
	gi.EA_View(bs->client, bs->viewangles);
}

/*
==============
BotInputToUserCommand
==============
*/
void BotInputToUserCommand(bot_input_t *bi, usercmd_t *ucmd, int delta_angles[3], int time) {
	vec3_t angles, forward, right;
	short temp;
	int j;

	//clear the whole structure
	memset(ucmd, 0, sizeof(usercmd_t));
	//
	//Com_Printf("dir = %f %f %f speed = %f\n", bi->dir[0], bi->dir[1], bi->dir[2], bi->speed);
	//the duration for the user command in milli seconds
	ucmd->serverTime = time;
	//
	if (bi->actionflags & ACTION_DELAYEDJUMP) {
		bi->actionflags |= ACTION_JUMP;
		bi->actionflags &= ~ACTION_DELAYEDJUMP;
	}
	//set the buttons

	if (bi->actionflags & ACTION_RESPAWN) ucmd->buttons = (BUTTON_ATTACKRIGHT | BUTTON_ATTACKLEFT);
	if (bi->actionflags & ACTION_ATTACK) ucmd->buttons |= BUTTON_ATTACKLEFT;
	if (bi->actionflags & ACTION_ATTACKRIGHT) ucmd->buttons |= BUTTON_ATTACKRIGHT;
	if (bi->actionflags & ACTION_TALK) ucmd->buttons |= BUTTON_TALK;
//	if (bi->actionflags & ACTION_GESTURE) ucmd->buttons |= BUTTON_GESTURE;
	if (bi->actionflags & ACTION_USE) ucmd->buttons |= BUTTON_USE;
	if (!(bi->actionflags & ACTION_WALK))
		ucmd->buttons |= BUTTON_RUN;
//	if (bi->actionflags & ACTION_WALK) ucmd->buttons |= BUTTON_WALKING;  // will probably need to hook these back up
//	if (bi->actionflags & ACTION_AFFIRMATIVE) ucmd->buttons |= BUTTON_AFFIRMATIVE;
//	if (bi->actionflags & ACTION_NEGATIVE) ucmd->buttons |= BUTTON_NEGATIVE;
//	if (bi->actionflags & ACTION_GETFLAG) ucmd->buttons |= BUTTON_GETFLAG;
//	if (bi->actionflags & ACTION_GUARDBASE) ucmd->buttons |= BUTTON_GUARDBASE;
//	if (bi->actionflags & ACTION_PATROL) ucmd->buttons |= BUTTON_PATROL;
//	if (bi->actionflags & ACTION_FOLLOWME) ucmd->buttons |= BUTTON_FOLLOWME;
	//
	ucmd->weapon = bi->weapon;
	//set the view angles
	//NOTE: the ucmd->angles are the angles WITHOUT the delta angles
	ucmd->angles[PITCH] = ANGLE2SHORT(bi->viewangles[PITCH]);
	ucmd->angles[YAW] = ANGLE2SHORT(bi->viewangles[YAW]);
	ucmd->angles[ROLL] = ANGLE2SHORT(bi->viewangles[ROLL]);
	//subtract the delta angles
	for (j = 0; j < 3; j++) {
		temp = ucmd->angles[j] - delta_angles[j];
		/*NOTE: disabled because temp should be mod first
		if ( j == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if ( temp > 16000 ) temp = 16000;
			else if ( temp < -16000 ) temp = -16000;
		}
		*/
		ucmd->angles[j] = temp;
	}
	//NOTE: movement is relative to the REAL view angles
	//get the horizontal forward and right vector
	//get the pitch in the range [-180, 180]
	if (bi->dir[2]) 
		angles[PITCH] = bi->viewangles[PITCH];
	else 
		angles[PITCH] = 0;
	angles[YAW] = bi->viewangles[YAW];
	angles[ROLL] = 0;
	AngleVectors(angles, forward, right, NULL);
	//bot input speed is in the range [0, 400]
	bi->speed = bi->speed * 127 / sv_maxspeed->value;
	//set the view independent movement
	ucmd->forwardmove = (signed char) (DotProduct(forward, bi->dir) * bi->speed);
	ucmd->rightmove = (signed char) (0 - DotProduct(right, bi->dir) * bi->speed);
	ucmd->upmove = (signed char) (abs((long)forward[2] * (long)bi->dir[2] * (long)bi->speed));
	//normal keyboard movement
	if (bi->actionflags & ACTION_MOVEFORWARD) ucmd->forwardmove += 127;
	if (bi->actionflags & ACTION_MOVEBACK) ucmd->forwardmove -= 127;
	if (bi->actionflags & ACTION_MOVELEFT) ucmd->rightmove -= 127;
	if (bi->actionflags & ACTION_MOVERIGHT) ucmd->rightmove += 127;
	//jump/moveup
	if ((bi->actionflags & ACTION_MOVEUP) || (bi->actionflags & ACTION_JUMP)) ucmd->upmove += 127;
	//crouch/movedown
	if (bi->actionflags & ACTION_CROUCH) ucmd->upmove -= 127;
	//
	//Com_Printf("forward = %d right = %d up = %d\n", ucmd.forwardmove, ucmd.rightmove, ucmd.upmove);
	//Com_Printf("ucmd->serverTime = %d\n", ucmd->serverTime);
}

/*
==============
BotUpdateInput
==============
*/
void BotUpdateInput(bot_state_t *bs, int time, int elapsed_time) {
	bot_input_t bi;
	int j;

	//add the delta angles to the bot's current view angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	//change the bot view angles
	BotChangeViewAngles(bs, (float) elapsed_time / 1000);
	//retrieve the bot input
	gi.EA_GetInput(bs->client, (float) time / 1000, &bi);
	//respawn hack
	if (bi.actionflags & ACTION_RESPAWN) {
		if (bs->lastucmd.buttons & (BUTTON_ATTACKRIGHT | BUTTON_ATTACKLEFT)) bi.actionflags &= ~(ACTION_RESPAWN|ACTION_ATTACK);
	}
	//convert the bot input to a usercmd
	BotInputToUserCommand(&bi, &bs->lastucmd, bs->cur_ps.delta_angles, time);
	//subtract the delta angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
}

/*
==============
BotAIRegularUpdate
==============
*/
void BotAIRegularUpdate(void) {
	if (regularupdate_time < FloatTime()) {
		gi.BotUpdateEntityItems();
		regularupdate_time = FloatTime() + 0.3;
	}
}

/*
==============
RemoveColorEscapeSequences
==============
*/
void RemoveColorEscapeSequences( char *text ) {
	int i, l;

	l = 0;
	for ( i = 0; text[i]; i++ ) {
		if (Q_IsColorString(&text[i])) {
			i++;
			continue;
		}
		if (text[i] > 0x7E)
			continue;
		text[l++] = text[i];
	}
	text[l] = '\0';
}

/*
==============
BotAI
==============
*/
int BotAI(int client, float thinktime) {
	bot_state_t *bs;
	char buf[1024], *args,cmd[1024], *ptr, *ptr2;
	int j;
	vec3_t tmpv;

	// Make sure we are still a valid bot

	if ( !g_entities[ client ].inuse || !g_entities[ client ].entity || !g_entities[ client ].client )
	{
		BotAIShutdownClient( client, qfalse );
		return qfalse;
	}

	gi.EA_ResetInput(client);
	//
	bs = botstates[client];
	if (!bs || !bs->inuse) {
		BotAI_Print(PRT_FATAL, "BotAI: client %d is not setup\n", client);
		return qfalse;
	}

	//retrieve the current client state
	BotAI_GetClientState( client, &bs->cur_ps );

	//retrieve any waiting server commands
	while( gi.BotGetConsoleMessage(client, buf, sizeof(buf)) ) {
		//have buf point to the command and args to the command arguments
		args = strchr( buf, ':');
		if (!args) continue;
		*args++;// = '\0';
		*args++;

		RemoveColorEscapeSequences( args );

		ptr = args;
		args = strchr(args,' ');
		strncpy(cmd,ptr,++args - ptr );
		cmd[(args - ptr) - 1] = 0;

//		*args++ = '\0';

		//remove color espace sequences from the arguments

		if (!Q_stricmp(buf, "cp "))
			{ /*CenterPrintf*/ }
#ifdef MISSIONPACK
		else if (!strncmp(cmd, "vchat",5)) {
			BotVoiceChatCommand(bs, SAY_ALL, ptr);
		}
		else if (!strncmp(cmd, "vtchat",6)) {
			BotVoiceChatCommand(bs, SAY_TEAM, ptr);
		}
		else if (!strncmp(cmd, "vtell",5)) {
			BotVoiceChatCommand(bs, SAY_TELL, ptr);
		}
#endif
		else if (!Q_stricmp(buf, "cs"))
			{ /*ConfigStringModified*/ }
		else if (!strncmp(buf, "hudprint",8)) {
			ptr2= strchr(buf,':');
			memcpy(cmd,ptr2,strlen(ptr2)+1);
			memcpy(ptr2+1,cmd,strlen(cmd)+1);
			*ptr2 = 25;
			gi.BotQueueConsoleMessage(bs->cs, CMS_NORMAL, strchr(buf,'"'));
		}
		else if (!strncmp(buf, "hudsay",6)) {
			ptr2= strchr(buf,':');
			memcpy(cmd,ptr2,strlen(ptr2)+1);
			memcpy(ptr2+1,cmd,strlen(cmd)+1);
			*ptr2 = 25;
			gi.BotQueueConsoleMessage(bs->cs, CMS_CHAT, strchr(buf,'"'));
		}
		else if (!Q_stricmp(buf, "tchat")) {
//			remove first and last quote from the chat message
			memmove(args, args+1, strlen(args));
			args[strlen(args)-1] = '\0';
			gi.BotQueueConsoleMessage(bs->cs, CMS_CHAT, args);
		}
		else if (!Q_stricmp(buf, "scores"))
			{ /*FIXME: parse scores?*/ }
		else if (!Q_stricmp(buf, "clientLevelShot"))
			{ /*ignore*/ }


		//--------------------------------------------------------------
		// GAMEFIX - Fixed: Warning C4996 stricmp: The POSIX name for this item is deprecated. Using Q_stricmp instead. - chrissstrahl
		//--------------------------------------------------------------
		else if ( Q_stricmp(buf, "disconnect") == 0 ) { 
			int i = 0;
			i++;
		}
	}
	//add the delta angles to the bot's current view angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	//increase the local time of the bot
	bs->ltime += thinktime;
	//
	bs->thinktime = thinktime;
	//origin of the bot
	VectorCopy(bs->cur_ps.origin, bs->origin);
	//eye coordinates of the bot
	VectorCopy(bs->cur_ps.origin, bs->eye);
	bs->eye[2] += (bs->cur_ps.viewheight);
	//get the area the bot is in
	VectorCopy(bs->origin,tmpv);
	tmpv[2] += 20;
	bs->areanum = BotPointAreaNum(bs->origin);
	//the real AI
	BotDeathmatchAI(bs, thinktime);
	//set the weapon selection every AI frame
	gi.EA_SelectWeapon(bs->client, bs->weaponnum);
	//subtract the delta angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	//everything was ok
	return qtrue;
}

/*
==================
BotScheduleBotThink
==================
*/
void BotScheduleBotThink(void) {
	int i, botnum;

	botnum = 0;

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		//initialize the bot think residual time
		botstates[i]->botthink_residual = bot_thinktime.integer * botnum / numbots;
		botnum++;
	}
}

/*
==============
BotWriteSessionData
==============
*/
void BotWriteSessionData(bot_state_t *bs) {
	const char	*s;
	const char	*var;

	s = va(
			"%i %i %i %i %i %i %i %i"
			" %f %f %f"
			" %f %f %f"
			" %f %f %f",
		bs->lastgoal_decisionmaker,
		bs->lastgoal_ltgtype,
		bs->lastgoal_teammate,
		bs->lastgoal_teamgoal.areanum,
		bs->lastgoal_teamgoal.entitynum,
		bs->lastgoal_teamgoal.flags,
		bs->lastgoal_teamgoal.iteminfo,
		bs->lastgoal_teamgoal.number,
		bs->lastgoal_teamgoal.origin[0],
		bs->lastgoal_teamgoal.origin[1],
		bs->lastgoal_teamgoal.origin[2],
		bs->lastgoal_teamgoal.mins[0],
		bs->lastgoal_teamgoal.mins[1],
		bs->lastgoal_teamgoal.mins[2],
		bs->lastgoal_teamgoal.maxs[0],
		bs->lastgoal_teamgoal.maxs[1],
		bs->lastgoal_teamgoal.maxs[2]
		);

	var = va( "botsession%i", bs->client );

	gi.cvar_set( var, s );
}

/*
==============
BotReadSessionData
==============
*/
void BotReadSessionData(bot_state_t *bs) {
	char	s[MAX_STRING_CHARS];
	const char	*var;

	var = va( "botsession%i", bs->client );
	gi.Cvar_VariableStringBuffer( var, s, sizeof(s) );


	//--------------------------------------------------------------
	// GAMEFIX - Fixed: Warning C6031 Return value ignored: sscanf. - chrissstrahl
	//--------------------------------------------------------------
	if (!sscanf(s,
		"%i %i %i %i %i %i %i %i"
		" %f %f %f"
		" %f %f %f"
		" %f %f %f",
		&bs->lastgoal_decisionmaker,
		&bs->lastgoal_ltgtype,
		&bs->lastgoal_teammate,
		&bs->lastgoal_teamgoal.areanum,
		&bs->lastgoal_teamgoal.entitynum,
		&bs->lastgoal_teamgoal.flags,
		&bs->lastgoal_teamgoal.iteminfo,
		&bs->lastgoal_teamgoal.number,
		&bs->lastgoal_teamgoal.origin[0],
		&bs->lastgoal_teamgoal.origin[1],
		&bs->lastgoal_teamgoal.origin[2],
		&bs->lastgoal_teamgoal.mins[0],
		&bs->lastgoal_teamgoal.mins[1],
		&bs->lastgoal_teamgoal.mins[2],
		&bs->lastgoal_teamgoal.maxs[0],
		&bs->lastgoal_teamgoal.maxs[1],
		&bs->lastgoal_teamgoal.maxs[2]
	)) {
		gi.Error(ERR_FATAL, "BotReadSessionData(bot_state_t *bs) - Invalid Format");
	}
}

/*
==============
BotAISetupClient
==============
*/
int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart) {
	char filename[MAX_PATH], name[MAX_PATH], gender[MAX_PATH];
	bot_state_t *bs;
	int errnum;

	if (!botstates[client]) botstates[client] = (bot_state_t *)gi.Malloc(sizeof(bot_state_t)); // G_Alloc
	bs = botstates[client];

	memset(bs,0,sizeof(bot_state_t)); // this isn't right, but gi.Malloc() doesn't zero out memory, FIXME find one that does
	if (bs && bs->inuse) {
		BotAI_Print(PRT_FATAL, "BotAISetupClient: client %d already setup\n", client);
		return qfalse;
	}

	if (!gi.AAS_Initialized()) {
		BotAI_Print(PRT_FATAL, "AAS not initialized\n");
		return qfalse;
	}


	//--------------------------------------------------------------
	// GAMEFIX - Fixed: Warning: C6011 Dereferencing NULL-Pointer. - chrissstrahl
	//--------------------------------------------------------------
	if (!bs) {
		return 0;
	}


	//load the bot character
	bs->character = gi.BotLoadCharacter(settings->characterfile, settings->skill);
	if (!bs->character) {
		BotAI_Print(PRT_FATAL, "couldn't load skill %f from %s\n", settings->skill, settings->characterfile);
		return qfalse;
	}
	//copy the settings
	memcpy(&bs->settings, settings, sizeof(bot_settings_t));
	//allocate a goal state
	bs->gs = gi.BotAllocGoalState(client);
	//load the item weights
	gi.Characteristic_String(bs->character, CHARACTERISTIC_ITEMWEIGHTS, filename, MAX_PATH);
	errnum = gi.BotLoadItemWeights(bs->gs, filename);
	if (errnum != BLERR_NOERROR) {
		gi.BotFreeGoalState(bs->gs);
		return qfalse;
	}
	//allocate a weapon state
	bs->ws = gi.BotAllocWeaponState();
	//load the weapon weights
	gi.Characteristic_String(bs->character, CHARACTERISTIC_WEAPONWEIGHTS, filename, MAX_PATH);
	errnum = gi.BotLoadWeaponWeights(bs->ws, filename);
	if (errnum != BLERR_NOERROR) {
		gi.BotFreeGoalState(bs->gs);
		gi.BotFreeWeaponState(bs->ws);
		return qfalse;
	}
	//allocate a chat state
	bs->cs = gi.BotAllocChatState();
	//load the chat file
	gi.Characteristic_String(bs->character, CHARACTERISTIC_CHAT_FILE, filename, MAX_PATH);
	gi.Characteristic_String(bs->character, CHARACTERISTIC_CHAT_NAME, name, MAX_PATH);
	errnum = gi.BotLoadChatFile(bs->cs, filename, name);
	if (errnum != BLERR_NOERROR) {
		gi.BotFreeChatState(bs->cs);
		gi.BotFreeGoalState(bs->gs);
		gi.BotFreeWeaponState(bs->ws);
		return qfalse;
	}
	//get the gender characteristic
	gi.Characteristic_String(bs->character, CHARACTERISTIC_GENDER, gender, MAX_PATH);
	//set the chat gender
	if (*gender == 'f' || *gender == 'F') gi.BotSetChatGender(bs->cs, CHAT_GENDERFEMALE);
	else if (*gender == 'm' || *gender == 'M') gi.BotSetChatGender(bs->cs, CHAT_GENDERMALE);
	else gi.BotSetChatGender(bs->cs, CHAT_GENDERLESS);

	bs->inuse = qtrue;
	bs->client = client;
	bs->entitynum = client;
	bs->setupcount = 4;
	bs->entergame_time = FloatTime();
	bs->ms = gi.BotAllocMoveState();
	bs->walker = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_WALKER, 0, 1);
	numbots++;

	if (gi.Cvar_VariableIntegerValue("bot_testichat")) {
		gi.BotLibVarSet("bot_testichat", "1");
		BotChatTest(bs);
	}
	//NOTE: reschedule the bot thinking
	BotScheduleBotThink();
	//if interbreeding start with a mutation
	if (bot_interbreed) {
		gi.BotMutateGoalFuzzyLogic(bs->gs, 1);
	}
	// if we kept the bot client
	if (restart) {
		BotReadSessionData(bs);
	}
	//bot has been setup succesfully
	return qtrue;
}

/*
==============
BotAIShutdownClient
==============
*/
int BotAIShutdownClient(int client, qboolean restart) {
	bot_state_t *bs;
	qboolean characterStillBeingUsed;
	int i;

	bs = botstates[client];
	if (!bs || !bs->inuse) {
		//BotAI_Print(PRT_ERROR, "BotAIShutdownClient: client %d already shutdown\n", client);
		return qfalse;
	}

	if (restart) {
		BotWriteSessionData(bs);
	}

	if (BotChat_ExitGame(bs)) {
		gi.BotEnterChat(bs->cs, bs->client, CHAT_ALL);
	}

	gi.BotFreeMoveState(bs->ms);
	//free the goal state`			
	gi.BotFreeGoalState(bs->gs);
	//free the chat file
	gi.BotFreeChatState(bs->cs);
	//free the weapon weights
	gi.BotFreeWeaponState(bs->ws);
	//free the bot character

	characterStillBeingUsed = qfalse;

	for( i = 0 ; i < maxclients->integer ; i++ )
	{
		bot_state_t *otherBotState;

		otherBotState = botstates[ i ];

		if ( !otherBotState || !otherBotState->inuse || otherBotState == bs )
			continue;

		if ( otherBotState->character == bs->character )
			characterStillBeingUsed = qtrue;
		
	}

	if ( !characterStillBeingUsed )
		gi.BotFreeCharacter(bs->character);

	//
	BotFreeWaypoints(bs->checkpoints);
	BotFreeWaypoints(bs->patrolpoints);
	//clear activate goal stack
	BotClearActivateGoalStack(bs);
	//clear the bot state
	memset(bs, 0, sizeof(bot_state_t));
	//set the inuse flag to qfalse
	bs->inuse = qfalse;
	//there's one bot less
	numbots--;
	//everything went ok
	return qtrue;
}

/*
==============
BotResetState

called when a bot enters the intermission or observer mode and
when the level is changed
==============
*/
void BotResetState(bot_state_t *bs) {
	int client, entitynum, inuse;
	int movestate, goalstate, chatstate, weaponstate;
	bot_settings_t settings;
	int character;
	playerState_t ps;							//current player state
	float entergame_time;

	//save some things that should not be reset here
	memcpy(&settings, &bs->settings, sizeof(bot_settings_t));
	memcpy(&ps, &bs->cur_ps, sizeof(playerState_t));
	inuse = bs->inuse;
	client = bs->client;
	entitynum = bs->entitynum;
	character = bs->character;
	movestate = bs->ms;
	goalstate = bs->gs;
	chatstate = bs->cs;
	weaponstate = bs->ws;
	entergame_time = bs->entergame_time;
	//free checkpoints and patrol points
	BotFreeWaypoints(bs->checkpoints);
	BotFreeWaypoints(bs->patrolpoints);
	//reset the whole state
	memset(bs, 0, sizeof(bot_state_t));
	//copy back some state stuff that should not be reset
	bs->ms = movestate;
	bs->gs = goalstate;
	bs->cs = chatstate;
	bs->ws = weaponstate;
	memcpy(&bs->cur_ps, &ps, sizeof(playerState_t));
	memcpy(&bs->settings, &settings, sizeof(bot_settings_t));
	bs->inuse = inuse;
	bs->client = client;
	bs->entitynum = entitynum;
	bs->character = character;
	bs->entergame_time = entergame_time;
	//reset several states
	if (bs->ms) gi.BotResetMoveState(bs->ms);
	if (bs->gs) gi.BotResetGoalState(bs->gs);
	if (bs->ws) gi.BotResetWeaponState(bs->ws);
	if (bs->gs) gi.BotResetAvoidGoals(bs->gs);
	if (bs->ms) gi.BotResetAvoidReach(bs->ms);
}

/*
==============
BotAILoadMap
==============
*/
int BotAILoadMap( int restart ) {
	int			i;
	vmCvar_t	mapname;

	if (!restart) {
		gi.Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		gi.BotLibLoadMap( mapname.string );
	}

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (botstates[i] && botstates[i]->inuse) {
			BotResetState( botstates[i] );
			botstates[i]->setupcount = 4;
		}
	}

	BotSetupDeathmatchAI();

	return qtrue;
}

#ifdef MISSIONPACK
void ProximityMine_Trigger( gentity_t *trigger, gentity_t *other, trace_t *trace );
#endif

void G_CheckBotSpawn( void );


extern Event EV_Player_UseItem;

/*
==================
BotAIStartFrame
==================
*/
static int local_time;
static int botlib_residual;
static int lastbotthink_time;

int BotAIStartFrame(int time) {
	int i;
	gentity_t	*ent;
	bot_entitystate_t state;
	int elapsed_time, thinktime;

	G_CheckBotSpawn();

	gi.Cvar_Update(&bot_rocketjump);
	gi.Cvar_Update(&bot_grapple);
	gi.Cvar_Update(&bot_fastchat);
	gi.Cvar_Update(&bot_nochat);
	gi.Cvar_Update(&bot_testrchat);
	gi.Cvar_Update(&bot_thinktime);
	gi.Cvar_Update(&bot_memorydump);
	gi.Cvar_Update(&bot_saveroutingcache);
	gi.Cvar_Update(&bot_pause);
	gi.Cvar_Update(&bot_report);

	if (bot_report.integer) {
//		BotTeamplayReport();
//		gi.cvar_set("bot_report", "0");
		BotUpdateInfoConfigStrings();
	}

	if (bot_pause.integer) {
		// execute bot user commands every frame
		for( i = 0; i < MAX_CLIENTS; i++ ) {
			if( !botstates[i] || !botstates[i]->inuse ) {
				continue;
			}
			if( g_entities[i].client->pers.enterTime) { // connected != CON_CONNECTED ) {
				continue;
			}
			botstates[i]->lastucmd.forwardmove = 0;
			botstates[i]->lastucmd.rightmove = 0;
			botstates[i]->lastucmd.upmove = 0;
			botstates[i]->lastucmd.buttons = 0;
			botstates[i]->lastucmd.serverTime = time;
			gi.BotUserCommand(botstates[i]->client, &botstates[i]->lastucmd);
		}
		return qtrue;
	}

	if (bot_memorydump.integer) {
		gi.BotLibVarSet("memorydump", "1");
		gi.cvar_set("bot_memorydump", "0");
	}
	if (bot_saveroutingcache.integer) {
		gi.BotLibVarSet("saveroutingcache", "1");
		gi.cvar_set("bot_saveroutingcache", "0");
	}
	//check if bot interbreeding is activated
	BotInterbreeding();
	//cap the bot think time
	if (bot_thinktime.integer > 200) {
		gi.cvar_set("bot_thinktime", "200");
	}
	//if the bot think time changed we should reschedule the bots
	if (bot_thinktime.integer != lastbotthink_time) {
		lastbotthink_time = bot_thinktime.integer;
		BotScheduleBotThink();
	}

	elapsed_time = time - local_time;
	local_time = time;

	botlib_residual += elapsed_time;

	if (elapsed_time > bot_thinktime.integer) thinktime = elapsed_time;
	else thinktime = bot_thinktime.integer;

	// update the bot library
	if ( botlib_residual >= thinktime ) {
		botlib_residual -= thinktime;

		gi.BotLibStartFrame((float) time / 1000);

		if (!gi.AAS_Initialized()) return qfalse;

		//update entities in the botlib
		for (i = 0; i < MAX_GENTITIES; i++) {
			ent = &g_entities[i];
			if (!ent->inuse) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			if (!ent->linked) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			if (ent->svflags & SVF_NOCLIENT) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			// do not update missiles
			if (ent->s.eType == ET_MISSILE) { //  && ent->s.weapon != WP_GRAPPLING_HOOK) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			// do not update event only entities
			if (ent->s.eType > ET_EVENTS) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
#ifdef MISSIONPACKBOTTODO
			// never link prox mine triggers
			if (ent->contents == CONTENTS_TRIGGER) {
				if (ent->touch == ProximityMine_Trigger) {
					gi.BotLibUpdateEntity(i, NULL);
					continue;
				}
			}
#endif
			//
			memset(&state, 0, sizeof(bot_entitystate_t));
			//
			VectorCopy(ent->currentOrigin, state.origin);
			VectorCopy(ent->currentAngles, state.angles);
			VectorCopy(ent->s.origin2, state.old_origin);
			VectorCopy(ent->mins, state.mins);
			VectorCopy(ent->maxs, state.maxs);
			state.type = ent->s.eType;
			state.flags = ent->s.eFlags;
			if (ent->bmodel) state.solid = SOLID_BSP;
			else state.solid = SOLID_BBOX;
			state.groundent = ent->s.groundEntityNum;
			state.modelindex = ent->s.modelindex;
//			state.modelindex2 = ent->s.modelindex2;
			state.frame = ent->s.frame;
//			state.event = ent->s.event;
//			state.eventParm = ent->s.eventParm;
//			state.powerups = ent->s.powerups;
//			state.legsAnim = ent->s.legsAnim;
//			state.torsoAnim = ent->s.torsoAnim;
//			state.weapon = ent->s.weapon;
			//
			gi.BotLibUpdateEntity(i, &state);
		}

		BotAIRegularUpdate();
	}

	floattime = gi.AAS_Time();

	// execute scheduled bot AI
	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		//
		botstates[i]->botthink_residual += elapsed_time;
		//
		if ( botstates[i]->botthink_residual >= thinktime ) {
			botstates[i]->botthink_residual -= thinktime;

			if (!gi.AAS_Initialized()) return qfalse;

			if (g_entities[i].client->pers.enterTime) { //  connected == CON_CONNECTED) {
				BotAI(i, (float) thinktime / 1000);
			}
		}
	}


	// execute bot user commands every frame
	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		if( !g_entities[i].client->pers.enterTime) { // connected != CON_CONNECTED ) {
			continue;
		}

		BotUpdateInput(botstates[i], time, elapsed_time);
		gi.BotUserCommand(botstates[i]->client, &botstates[i]->lastucmd);
	}

	return qtrue;
}

/*
==============
BotInitLibrary
==============
*/
int BotInitLibrary(void) {
	char buf[144];

	//set the maxclients->integer and maxentities library variables before calling BotSetupLibrary
	gi.Cvar_VariableStringBuffer("sv_maxclients->integer", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "8");
	gi.BotLibVarSet("maxclients->integer", buf);
	Com_sprintf(buf, sizeof(buf), "%d", MAX_GENTITIES);
	gi.BotLibVarSet("maxentities", buf);
	//bsp checksum
	gi.Cvar_VariableStringBuffer("sv_mapChecksum", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("sv_mapChecksum", buf);
	//maximum number of aas links
	gi.Cvar_VariableStringBuffer("max_aaslinks", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("max_aaslinks", buf);
	//maximum number of items in a level
	gi.Cvar_VariableStringBuffer("max_levelitems", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("max_levelitems", buf);
	//game type
	gi.Cvar_VariableStringBuffer("g_gametype", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "0");
	gi.BotLibVarSet("g_gametype", buf);
	//bot developer mode and log file
	gi.BotLibVarSet("bot_developer", bot_developer.string);
	gi.BotLibVarSet("log", buf);
	//no chatting
	gi.Cvar_VariableStringBuffer("bot_nochat", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("nochat", "0");
	//visualize jump pads
	gi.Cvar_VariableStringBuffer("bot_visualizejumppads", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("bot_visualizejumppads", buf);
	//forced clustering calculations
	gi.Cvar_VariableStringBuffer("bot_forceclustering", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("forceclustering", buf);
	//forced reachability calculations
	gi.Cvar_VariableStringBuffer("bot_forcereachability", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("forcereachability", buf);
	//force writing of AAS to file
	gi.Cvar_VariableStringBuffer("bot_forcewrite", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("forcewrite", buf);
	//no AAS optimization
	gi.Cvar_VariableStringBuffer("bot_aasoptimize", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("aasoptimize", buf);
	//
	gi.Cvar_VariableStringBuffer("bot_saveroutingcache", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("saveroutingcache", buf);
	//reload instead of cache bot character files
	gi.Cvar_VariableStringBuffer("bot_reloadcharacters", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "0");
	gi.BotLibVarSet("bot_reloadcharacters", buf);
	//base directory
	gi.Cvar_VariableStringBuffer("fs_basepath", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("basedir", buf);
	//game directory
	gi.Cvar_VariableStringBuffer("fs_game", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("gamedir", buf);
	//cd directory
	gi.Cvar_VariableStringBuffer("fs_cdpath", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("cddir", buf);
	//
#ifdef MISSIONPACKBOTTODO
	gi.BotLibDefine("MISSIONPACK");
#endif
	//setup the bot library
	return gi.BotLibSetup();
}

/*
==============
BotAISetup
==============
*/
int BotAISetup( int restart ) {
	int			errnum;

	local_time = 0;
	botlib_residual = 0;
	lastbotthink_time = 0;

	floattime = 0;
	regularupdate_time = 0;

	gi.Cvar_Register(&bot_thinktime, "bot_thinktime", "100", CVAR_CHEAT);
	gi.Cvar_Register(&bot_memorydump, "bot_memorydump", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_saveroutingcache, "bot_saveroutingcache", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_pause, "bot_pause", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_report, "bot_report", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_testsolid, "bot_testsolid", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_testclusters, "bot_testclusters", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_developer, "bot_developer", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_interbreedchar, "bot_interbreedchar", "", 0);
	gi.Cvar_Register(&bot_interbreedbots, "bot_interbreedbots", "10", 0);
	gi.Cvar_Register(&bot_interbreedcycle, "bot_interbreedcycle", "20", 0);
	gi.Cvar_Register(&bot_interbreedwrite, "bot_interbreedwrite", "", 0);

	//if the game is restarted for a tournament
	if (restart) {
		return qtrue;
	}

	//initialize the bot states
	memset( botstates, 0, sizeof(botstates) );

	errnum = BotInitLibrary();
	if (errnum != BLERR_NOERROR) return qfalse;
	return qtrue;
}

/*
==============
BotAIShutdown
==============
*/
int BotAIShutdown( int restart ) {

	int i;

	//if the game is restarted for a tournament
	if ( restart ) {
		//shutdown all the bots in the botlib
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (botstates[i] && botstates[i]->inuse) {
				BotAIShutdownClient(botstates[i]->client, restart);
			}
		}
		//don't shutdown the bot library
	}
	else {
		gi.BotLibShutdown();
	}
	return qtrue;
}

