//--------------------------------------------------------------
// GAMEFIX - GENERAL CODE USED FOR GAME-FIXES
// 
// Elite Force 2 Specific Functions and classes go to: api_stef2.cpp
//--------------------------------------------------------------


#include "gamefix.hpp"
#include "api_stef2.hpp"


//--------------------------------------------------------------
// GAMEFIX - Added: Function to read contents of a file into a container, each line will be one object
//--------------------------------------------------------------
Container<str> gamefix_fileContentTokenized;

//--------------------------------------------------------------
// GAMEFIX - Added: Support for Delayed serverCommands for players - daggolin
//--------------------------------------------------------------
gamefix_pendingServerCommand* pendingServerCommandList[MAX_CLIENTS];

//--------------------------------------------------------------
// GAMEFIX - Added: Information list for all standard levels - chrissstrahl
//--------------------------------------------------------------
gamefix_defaultMaps_s gamefix_defaultMaps_t[gamefix_defaultMapsSize];

//--------------------------------------------------------------
//Added: Information we want to persist over level changes and restarts - multiplayer only - See g_local.h for the struct - chrissstrahl
//--------------------------------------------------------------
gamefix_client_persistant_s gamefix_client_persistant_t[MAX_CLIENTS];

//--------------------------------------------------------------
//Added: Extra Information we can attach to entities - multiplayer only - chrissstrahl
//--------------------------------------------------------------
gamefix_entity_extraData_s gamefix_entity_extraData_t[MAX_GENTITIES];

//--------------------------------------------------------------
// GAMEFIX - Added: Function to return the singleplayer spawnspot if no multiplayer spawn spot was found - chrissstrahl
//--------------------------------------------------------------
Entity* gamefix_returnInfoPlayerStart(str info)
{
	gi.Printf(va(_GFix_INFO_FAILSAFE, info.c_str()));
	return G_FindClass(NULL, "info_player_start");
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function grabbing given entity, like in scripts - chrissstrahl
//--------------------------------------------------------------
Entity* gamefix_getEntity(str& name)
{
	return gameFixAPI_getEntity(name);
}

//--------------------------------------------------------------
// GAMEFIX - Return Player by client number - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getPlayer(int index)
{
	gentity_t* ed;

	if (index > gameFixAPI_maxClients())
		return nullptr;

	ed = &g_entities[index];

	if (!ed || !ed->inuse || !ed->entity)
		return nullptr;

	return (Player*)g_entities[index].entity;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning number of player on the server - chrissstrahl
//--------------------------------------------------------------
int gamefix_getPlayers(bool state)
{
	return gameFixAPI_getPlayers(state);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to get player that is inside of the given entity boundingbox - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getPlayerInsideOfEntity(Entity* eTheBox)
{
	Player* player = nullptr;
	Player* playerReturn = nullptr;

	if (!eTheBox || gameFixAPI_isDead(eTheBox) || gameFixAPI_isSpectator_stef2(player)) {
		return nullptr;
	}


	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);

		if (!player || gameFixAPI_isDead((Entity*)player) || gameFixAPI_isSpectator_stef2(eTheBox)) {
			continue;
		}

		Entity* ePlayer = (Entity*)player;
		if ((ePlayer->absmin[0] > eTheBox->absmax[0]) ||
			(ePlayer->absmin[1] > eTheBox->absmax[1]) ||
			(ePlayer->absmin[2] > eTheBox->absmax[2]) ||
			(ePlayer->absmax[0] < eTheBox->absmin[0]) ||
			(ePlayer->absmax[1] < eTheBox->absmin[1]) ||
			(ePlayer->absmax[2] < eTheBox->absmin[2]))
		{
			continue;
		}

		playerReturn = player;
	}
	return playerReturn;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function used to check if entity is inside boundingbox of other-entity - chrissstrahl
//--------------------------------------------------------------
bool gamefix_checkEntityInsideOfEntity(Entity* eCheck, Entity* eTheBox)
{
	if (!eCheck || !eTheBox || eCheck == eTheBox) {
		return false;
	}

	if (gameFixAPI_isDead(eCheck) || gameFixAPI_isDead(eTheBox)) {
		return false;
	}

	if (gameFixAPI_isSpectator_stef2(eCheck) || gameFixAPI_isSpectator_stef2(eTheBox)) {
		return false;
	}

	if ((eCheck->absmin[0] > eTheBox->absmax[0]) ||
		(eCheck->absmin[1] > eTheBox->absmax[1]) ||
		(eCheck->absmin[2] > eTheBox->absmax[2]) ||
		(eCheck->absmax[0] < eTheBox->absmin[0]) ||
		(eCheck->absmax[1] < eTheBox->absmin[1]) ||
		(eCheck->absmax[2] < eTheBox->absmin[2]))
	{
		return false;
	}
	return true;
}

//--------------------------------------------------------------
// GAMEFIX - Checks if any other player is targeting the given entity - chrissstrahl
//--------------------------------------------------------------
bool gamefix_targetedByOtherPlayer(Player* player, Entity* entity)
{
	if (player && !gameFixAPI_inSingleplayer()) {
		for (int i = 0; i < gameFixAPI_maxClients(); ++i) {
			gentity_t* ent = g_entities + i;
			if (!ent || !ent->inuse || !ent->client || !ent->entity || player->entnum == i || gameFixAPI_isDead(ent->entity) || gameFixAPI_isSpectator_stef2(ent->entity)) {
				continue;
			}

			Entity* curTarget = gameFixAPI_getTargetedEntity(player);
			if (curTarget && curTarget == entity) {
				return true;
			}
		}
	}
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Returns closest player to given entity - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getClosestPlayer(Entity* entity)
//NO SPECTATOR, NO DEAD
{
	return gamefix_getClosestPlayer(entity, true, true, false, 0, 0);
}

Player* gamefix_getClosestPlayerSamePlane(Entity* entity)
//NO SPECTATOR, NO DEAD, SAME PLANE, MAX Z-Difference (to match plane), MAX X-Difference (to match plane)
{
	return gamefix_getClosestPlayer(entity, true, true, true, 196, 1024);
}

Player* gamefix_getClosestPlayer(Entity* entity,bool noSpectator, bool noDead,bool samePlane,int planeMaxVertDiff, int planeMaxRange)
//RETURNS nullptr IF CRITIRA (spec/dead/notarget) DON'T MATCH
//GIVES PLAYER FROM OTHER PLANES (bigger Z-AXIS differences) IF NONE ON THE SAME PLANE BEFORE GIVING UP AND RETURNING nullptr
{
	if (gameFixAPI_inSingleplayer()) {
		return gamefix_getPlayer(0);
	}

	if (!entity) {
		return nullptr;
	}

	Player* playerClosest = nullptr;
	float distanceClosest = 999999;

	Player* playerClosestSamePlane = nullptr;
	float distanceClosestSamePlane = 999999;

	Player* player = nullptr;

	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);

		if (!player) {
			continue;
		}

		if (noDead && gameFixAPI_isDead((Entity*)player) || noSpectator && gameFixAPI_isSpectator_stef2((Entity*)player)) {
			continue;
		}

		if (gamefix_checkNotarget((Entity*)player)) {
			continue;
		}

		float distanceCurrent = VectorLength(player->centroid - entity->centroid);
		
		//always grab player closest as a backup
		if (distanceClosest > distanceCurrent) {
			distanceClosest = distanceCurrent;
			playerClosest = player;
		}

		//have limited range for same plane check
		if (samePlane) {
			Vector zAxiOther = Vector(0, 0, 0);
			Vector zAxiPlayer = Vector(0, 0, 0);
			zAxiOther[2] = entity->origin[2];
			zAxiPlayer[2] = player->origin[2];
			if (distanceCurrent <= planeMaxRange && VectorLength(zAxiOther - zAxiPlayer) < planeMaxVertDiff && distanceClosestSamePlane > distanceCurrent) {
				distanceClosestSamePlane = distanceCurrent;
				playerClosestSamePlane = player;
			}
		}
	}

	//if we don't have player on same plane within range, fallback to our backup
	if (!playerClosestSamePlane) {
		playerClosestSamePlane = playerClosest;
	}

	return playerClosestSamePlane;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to get best closest Player to follow - chrissstrahl
// //--------------------------------------------------------------
Player* gamefix_getClosestPlayerToFollow(Actor* actor)
{
	if (!actor) { return nullptr; }
	Entity* ent = nullptr;

	ent = gamefix_getActorFollowTarget(actor);

	if (gamefix_EntityValid(ent) && ent->isSubclassOf(Player)) {
		return (Player*)ent;
	}

	return gamefix_getClosestPlayerSamePlane((Entity*)actor);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning closest player that given actor can see - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getClosestPlayerActorCanSee(Actor* actor, qboolean useFOV)
{
	if (!actor || gameFixAPI_isDead(actor)) {
		return nullptr;
	}

	Player* playerClosest = nullptr;
	float distanceClosest = 999999;

	Player* player = nullptr;

	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);

		if (!gamefix_EntityValid((Entity*)player) || !gamefix_actorCanSee(actor, (Entity*)player, useFOV, qtrue)) {
			continue;
		}

		float distanceCurrent = VectorLength(player->centroid - actor->centroid);

		//always grab player closest as a backup
		if (distanceClosest > distanceCurrent) {
			distanceClosest = distanceCurrent;
			playerClosest = player;
		}
	}

	return playerClosest;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning current enemy if player or the closest player actor cansee - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getClosestPlayerCanseeIfNoCurEnemy(Actor* actor)
{
	Entity* ent = gamefix_actorGetCurrentEnemy(actor);
	if (gamefix_EntityValid(ent) && ent->isSubclassOf(Player)) {
		return (Player*)ent;
	}
	return gamefix_getClosestPlayerActorCanSee(actor, qfalse);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning activator or the closest player - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getActivatorOrClosestPlayer(Entity* entity)
{
	Entity* activator = gameFixAPI_getActivator(entity);
	if (!activator || !activator->isSubclassOf(Player)) {
		activator = gamefix_getClosestPlayerSamePlane(entity);
	}
	return (Player*)activator;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning activator or the closest player actor cansee - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getActivatorOrClosestPlayerCansee(Actor* actor)
{
	Entity* activator = gameFixAPI_getActivator(actor);
	if (!activator || !activator->isSubclassOf(Player)) {
		activator = gamefix_getClosestPlayerActorCanSee(actor, qfalse);
	}
	return (Player*)activator;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function that returns any player preferably not dead or spectator - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getAnyPlayerPreferably()
{
	return gamefix_getAnyPlayerPreferably(true,true);
}

Player* gamefix_getAnyPlayerPreferably(bool noDead,bool noSpectator)
{
	if (gameFixAPI_inSingleplayer()) {
		return gamefix_getPlayer(0);
	}

	Player* player = nullptr;
	Player* playerDead = nullptr;
	Player* playerSpec = nullptr;

	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);

		if (!player) {
			continue;
		}

		if (gamefix_checkNotarget((Entity*)player)) {
			continue;
		}

		if (noDead && gameFixAPI_isDead((Entity*)player)) {
			if (!playerDead) {
				playerDead = player;
			}
			continue;
		}
		
		if(noSpectator && gameFixAPI_isSpectator_stef2((Entity*)player)) {
			if (!playerSpec) {
				playerSpec = player;
			}
			continue;
		}
		//yay a player is a match
		break;
	}

	//we didn't get the prefered player, now take any avialable
	if (!player) {
		if (!playerDead) {
			player = playerDead;
		}
		else if(!playerSpec){
			player = playerSpec;
		}
	}

	return player;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to return interger value from cVar - chrissstrahl
//--------------------------------------------------------------
int gamefix_getCvarInt(str cvarName)
{
	cvar_t* cvar = gi.cvar_get(cvarName.c_str());
	return (cvar ? cvar->integer : 0);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to return float value from cVar - chrissstrahl
//--------------------------------------------------------------
float gamefix_getCvarFloat(str cvarName)
{
	cvar_t* cvar = gi.cvar_get(cvarName.c_str());
	return (cvar ? cvar->value : 0.0f);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to return string value from cVar - chrissstrahl
//--------------------------------------------------------------
str gamefix_getCvar(str cvarName)
{
	cvar_t* cvar = gi.cvar_get(cvarName.c_str());
	return (cvar ? cvar->string : "");
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to check quicky if GIVEN entity exists,alive,nospec,targetable - chrissstrahl
//--------------------------------------------------------------
bool gamefix_EntityValid(Entity* entity)
{
	if (!gameFixAPI_isDead(entity) && !gameFixAPI_isSpectator_stef2(entity) && !gamefix_checkNotarget(entity)) {
		return true;
	}
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to check quicky if ANY player is alive,nospec,targetable - chrissstrahl
//--------------------------------------------------------------
bool gamefix_PlayerValid()
{
	Player* player = nullptr;
	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);
		if (gamefix_EntityValid(player)) {
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if entity is in notarget - chrissstrahl
//--------------------------------------------------------------
bool gamefix_checkNotarget(Entity* entity) {
	if (!entity) { return false; }
	return (entity->flags & FL_NOTARGET);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning actor follow target entity - chrissstrahl
//--------------------------------------------------------------
Entity* gamefix_getActorFollowTarget(Actor* actor) {
	return gameFixAPI_getActorFollowTargetEntity(actor);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if actor can actually see the given entity - chrissstrahl
//--------------------------------------------------------------
bool gamefix_actorCanSee(Actor* actor, Entity* entity, qboolean useFOV, qboolean useVisionDistance)
{
	return gameFixAPI_actorCanSee(actor,entity,useFOV,useVisionDistance);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning current enemy of actor - chrissstrahl
//--------------------------------------------------------------
Entity* gamefix_actorGetCurrentEnemy(Actor* actor)
{
	return gameFixAPI_actorGetCurrentEnemy(actor);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if actor hates given entity - chrissstrahl
//--------------------------------------------------------------
bool gamefix_actorHates(Actor* actor, Entity* entity)
{
	if (actor && entity && entity->isSubclassOf(Sentient)) {
		return gameFixAPI_actorHates(actor, (Sentient*)entity);
	}
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if given player uses currently a ranged weapon - chrissstrahl
//--------------------------------------------------------------
bool gamefix_checkPlayerRanged(Actor* actor,Player* player)
{
	if (!actor || !player || !gamefix_EntityValid((Entity*)player)) { return false; }
	return (actor->EntityHasFireType((Entity*)player, FT_BULLET) == qboolean(qtrue) || actor->EntityHasFireType((Entity*)player, FT_PROJECTILE) == qboolean(qtrue));
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if GIVEN player has ranged weapon + valid,cansee - chrissstrahl
//--------------------------------------------------------------
bool gamefix_checkPlayerRangedCanidate(Actor* actor,Player* player)
{
	if (actor && gamefix_EntityValid((Entity*)player) &&
		gamefix_checkPlayerRanged(actor, player) &&
		//within vision distance 360-FOV check if ai could see the given player
		gamefix_actorCanSee(actor, (Entity*)player, qfalse, qtrue))
	{
		return true;
	}
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if ANY player has ranged weapon + curEnemy,followTarget,valid,cansee - chrissstrahl
//--------------------------------------------------------------
bool gamefix_checkPlayerRanged(Actor* actor, bool closestOnly)
{
	if (!actor) { return false; }

	if (gameFixAPI_inSingleplayer()) {
		return gamefix_checkPlayerRanged(actor,GetPlayer(0));
	}
	
	Player* player = nullptr;
	if (closestOnly) {
		return gamefix_checkPlayerRangedCanidate(actor,gamefix_getClosestPlayerActorCanSee(actor,qfalse));
	}
	
	Entity* ent = gamefix_actorGetCurrentEnemy(actor);
	if (ent && ent->isSubclassOf(Player) && gamefix_checkPlayerRanged(actor,(Player*)ent)) {
		return true;
	}

	//meant for teammates - but this might also applay to enemies, I didn't bother to check
	ent = gamefix_getActorFollowTarget(actor);
	if (ent && ent->isSubclassOf(Player) && gamefix_checkPlayerRanged(actor, (Player*)ent)) {
		return true;
	}
	
	//if all else fails, we get to the expensive part
	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);
		if (gamefix_checkPlayerRangedCanidate(actor, player)){
			return true;
		}
	}
	
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function checking if given player uses given weapon - chrissstrahl
//--------------------------------------------------------------
bool gamefix_checkPlayerUsingWeaponNamed(Player* player, const str& weaponNameOfPlayer)
{
	return gameFixAPI_checkPlayerUsingWeaponNamed(player,weaponNameOfPlayer);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning player by giventargetname - chrissstrahl
//--------------------------------------------------------------
Player* gamefix_getPlayerByTargetname(const str& targetname)
{
	Player* player = nullptr;
	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);
		if (player && Q_stricmp(player->targetname.c_str(),targetname.c_str())) {
			return player;
		}
	}
	return nullptr;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function retrieving Server Language - chrissstrahl
//--------------------------------------------------------------
str gamefix_getServerLanguage()
{
	return gameFixAPI_getServerLanguage();
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function retrieving Player Language - chrissstrahl
//--------------------------------------------------------------
str gamefix_getLanguage(Player* player)
{
	return gameFixAPI_getLanguage(player);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function that allows Language selection English - chrissstrahl
//--------------------------------------------------------------
qboolean gamefix_languageEng(const gentity_t* ent)
{
	return gameFixAPI_languageEng(ent);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function that allows Language selection German - chrissstrahl
//--------------------------------------------------------------
qboolean gamefix_languageDeu(const gentity_t* ent)
{
	return gameFixAPI_languageDeu(ent);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning player by giventargetname - chrissstrahl
//--------------------------------------------------------------
str gamefix_getLocalizedString(Player* player,const str sEnglish,const str sGerman)
{
	if (player) {
		if (gameFixAPI_getLanguage(player) == "Deu") {
			return sGerman;
		}
	}
	return sEnglish;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function making the client send the contents of his local_language cvar as console-command - chrissstrahl
// Expected return values: Eng/Deu need to be catched in gamecmds.cpp -> consolecmd_t G_ConsoleCmds[] add:
// {"Eng",gamefix_languageEng,true},
// {"Deu",gamefix_languageDeu,true},
//--------------------------------------------------------------
void gamefix_vstrLocalLanguage(gentity_t* ent)
{
	if (ent && gameFixAPI_inMultiplayer()) {
		//this does not always work if sv_floodprotect (clientcommand is stalled) is enabled
		gi.SendServerCommand(ent->client->ps.clientNum, "stufftext \"vstr local_language\n\"");
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to turn OFF LEVEL AI if server empty, prevent issues with ai - chrissstrahl
//--------------------------------------------------------------
void gamefix_aiTurnOff()
{
	if (level.ai_on && gameFixAPI_inMultiplayer() && gamefix_getPlayers(true) <= 0) {
		level.ai_on = false;
		gi.Printf(_GFixEF2_INFO_FUNC_ClientDisconnect);
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to turn ON LEVEL AI if server no longer empty - chrissstrahl
//--------------------------------------------------------------
void gamefix_aiTurnOn()
{
	if (!level.ai_on && gameFixAPI_inMultiplayer() && gamefix_getPlayers(true) <= 0) {
		level.ai_on = true;
		gi.Printf(_GFixEF2_INFO_FUNC_ClientBegin);
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to print text to all players - chrissstrahl
//--------------------------------------------------------------
void gamefix_printAllClients(const str text)
{
	gameFixAPI_hudPrintAllClients(text);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function manage level related fixes - chrissstrahl
//--------------------------------------------------------------
void gamefix_levelFixes()
{
	gameFixAPI_levelFixes();
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function handling player game event - chrissstrahl
//--------------------------------------------------------------
void gamefix_playerSpectator(Player* player)
{
	gameFixAPI_playerSpectator(player);
}
void gamefix_playerChangedTeam(Player* player,const str &realTeamName)
{
	gameFixAPI_playerChangeTeam(player,realTeamName);
}
void gamefix_playerKilled(Player* player)
{
	gameFixAPI_playerKilled(player);
}
void gamefix_playerEntered(Player* player)
{
	gameFixAPI_playerEntered(player);
}
void gamefix_playerSpawn(Player* player)
{
	gameFixAPI_playerSpawn(player);
}
void gamefix_playerModelChanged(Player* player)
{
	gameFixAPI_playerModelChanged(player);
}
void gamefix_playerUseItem(Player* player, const char* name)
{
	gameFixAPI_playerUseItem(player, name);
}
void gamefix_playerScore(Player* player)
{
	gameFixAPI_playerScore(player);
}
void gamefix_playerClientBegin(gentity_t* ent)
{
	//gi.Printf("gamefix_playerClientBegin\n");

	//--------------------------------------------------------------
	// GAMEFIX - Added: sv_floodprotect disable to fix various issues in multiplayer - chrissstrahl
	//--------------------------------------------------------------
	gamefix_svFloodProtectDisable();

	//--------------------------------------------------------------
	// GAMEFIX - Added: Turn ON LEVEL AI if server no longer empty, is turned off again if server is empty - chrissstrahl
	//--------------------------------------------------------------
	gamefix_aiTurnOn();

	//--------------------------------------------------------------
	// GAMEFIX - Added: Detection of Player local_language cvar, for Eng/Deu Language detection - chrissstrahl
	//--------------------------------------------------------------
	gamefix_vstrLocalLanguage(ent);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function handling Dialog in Multiplayer - chrissstrahl
//--------------------------------------------------------------
void gamefix_dialogSetupPlayers(Actor* speaker,char unlocalDialogName[MAX_QPATH], bool headDisplay)
{
	gameFixAPI_dialogSetupPlayers(speaker, unlocalDialogName, headDisplay);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning the localized path for player, depending on their language Eng/Deu - chrissstrahl
//--------------------------------------------------------------
str gamefix_localizeStringForPlayer(Player* player,char unlocal[MAX_QPATH])
{
	if (player) {
		//localize if it is unlocalized
		if (Q_stricmpn(unlocal, "localization/", 13) == 0) {
			gamefix_replaceSubstring(unlocal, "localization/", "");
			str sLoc =  va("loc/%s/%s", gamefix_getLanguage(player).c_str(), unlocal);
			gi.Printf("gamefix_localizeStringForPlayer - %s\n", sLoc.c_str());
			return sLoc;
		}
		//if it is not unlocalized, assume the path is eigther already localized or there is no localization for it
		return unlocal;
	}
	return "";
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning the higher dialog playtime of Eng/Deu - chrissstrahl
//--------------------------------------------------------------
float gamefix_dialogGetSoundlength(char realDialogName[MAX_QPATH])
{
	float fLength = 0.0f;
	str sLoc = realDialogName;

	//get english and german version of the file and compare them - if their *.vlp exist and it is unlocalized
	if (Q_stricmpn(realDialogName,"localization/",13) == 0) {
		gamefix_replaceSubstring(realDialogName,"localization/","");
		fLength = max(gi.SoundLength(va("loc/Eng/%s",realDialogName)), gi.SoundLength(va("loc/Deu/%s",realDialogName)));
	}

	//get real dialog length - usually starting with localization/... - this will always work
	//if it is not unlocalized, assume the path is eigther already localized or there is no localization for it
	fLength = max(gi.SoundLength(sLoc.c_str()), fLength);

	return fLength;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function replacing specific part of a string - chrissstrahl
//--------------------------------------------------------------
void gamefix_replaceSubstring(char* str, const char* find, const char* replace)
{
	char buffer[1024];  // Ensure buffer is sufficiently large
	char* result = buffer;  // Write result to the buffer
	char* pos;
	int find_len = strlen(find);
	int replace_len = strlen(replace);

	// Temporary pointer for non-destructive scanning
	char* current = str;

	while ((pos = strstr(current, find)) != NULL) {
		// Calculate the length of the initial segment
		size_t len = pos - current;

		// Copy the part of the original string before the found substring
		memcpy(result, current, len);
		result += len;

		// Append the replacement substring
		memcpy(result, replace, replace_len);
		result += replace_len;

		// Move the current pointer past the found substring
		current = pos + find_len;
	}

	// Append the rest of the string after the last match
	strcpy(result, current);

	// Copy the buffer back to the original string
	strcpy(str, buffer);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function finding first occurence of given single char, returning its position - chrissstrahl
//--------------------------------------------------------------
int gamefix_findChar(const char* str, const char find)
{
	if (str) {
		for (int i = 0; str[i] != '\0'; ++i) {
			if (str[i] == find) {
				return i;
				break;
			}
		}
	}
	return -1;
}
int gamefix_findChar(const char* str, const char find, int start)
{
	if (str && start >= 0) {
		for (int i = start; str[i] != '\0'; ++i) {
			if (str[i] == find) {
				return i;
			}
		}
	}
	return -1;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function finding first occurence of given any of the single chars, returning its position - chrissstrahl
//--------------------------------------------------------------
int gamefix_findChars(const char* str, const char* find)
{
	if (str) {
		if (strlen(find)) {
			for (int i = 0; str[i] != '\0'; ++i) {
				for (int j = 0; find[j] != '\0'; ++j) {
					if (str[i] == find[j]) {
						return i;
					}
				}
			}
		}
	}
	return -1;
}
int gamefix_findChars(const char* str, const char* find, int start) {
	if (str && start >= 0) {
		int strLength = strlen(str);
		int findLength = strlen(find);

		if (findLength > 0 && start < strLength) {
			for (int i = start; str[i] != '\0'; ++i) {
				for (int j = 0; find[j] != '\0'; ++j) {
					if (str[i] == find[j]) {
						return i;
					}
				}
			}
		}
	}
	return -1;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function finding the last occurrence of any of the single chars, returning its position - chrissstrahl
// Basically the same as gamefix_findChars but starting from the end of the string
//--------------------------------------------------------------
int gamefix_findCharsReverse(const char* str, const char* find)
{
	if (str) {
		int len_str = strlen(str);
		int len_find = strlen(find);
		if (len_find) {
			for (int i = len_str - 1; i >= 0; --i) {
				for (int j = 0; j < len_find; ++j) {
					if (str[i] == find[j]) {
						return i;
					}
				}
			}
		}
	}
	return -1;
}
//by ChatGTP 4o
int gamefix_findCharsReverse(const char* str, const char* find, int startPos, int endPos)
{
	if (str) {
		int len_find = strlen(find);
		if (len_find) {
			for (int i = endPos - 1; i >= startPos; --i) {
				for (int j = 0; j < len_find; ++j) {
					if (str[i] == find[j]) {
						return i;
					}
				}
			}
		}
	}
	return -1;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function finding first occurence of given string, returning its position - chrissstrahl
// Basically a substitude for: .find .contains .find_first_of .IndexOf and strpos strstr
//--------------------------------------------------------------
int gamefix_findString(const char* str, const char* find)
{
	const char* pos = strstr(str, find);

	if (pos != nullptr) {
		return pos - str;
	}
	return -1;  // NoMatch
}
int gamefix_findStringCase(const str& latinumstack,const str& find)
{
	return gamefix_findStringCase(latinumstack, find, false);
}
int gamefix_findStringCase(const str& latinumstack, const str& find, bool wholeWord)
{
	int latinumLength = latinumstack.length();
	int findLength = find.length();

	if (findLength == 0 || latinumLength < findLength) {
		return -1;
	}

	for (int latinumIndex = 0; latinumIndex <= latinumLength - findLength; ++latinumIndex) {
		int findIndex = 0;
		while (findIndex < findLength && tolower(latinumstack[latinumIndex + findIndex]) == tolower(find[findIndex])) {
			++findIndex;
		}
		if (findIndex == findLength) {
			if (!wholeWord) {
				return latinumIndex;
			}
			// Check if preceding character is a separator or start of the string
			if ((latinumIndex == 0 || !isalnum(latinumstack[latinumIndex - 1])) &&
				// Check if next character is a separator or end of the string
				(latinumIndex + findLength == latinumLength || !isalnum(latinumstack[latinumIndex + findLength]))) {
				return latinumIndex;
			}
		}
		/*
			//check if next char is not a letter, but any kind of seperator
			if (latinumIndex + 1 <= latinumLength) {
				gi.Printf("---- Found, now checking for whole word ------\n");
				static char seperators[] = { ' ','|','.',':',';' ,',' ,'#' ,'~' ,'^', '\n','\0','\r','-','_','+','*','?','=','&','"','\'','!','%' };
				int seperatorsSize = sizeof(seperators) / sizeof(seperators[0]);
				for (int seperatorIndex = 0; seperatorIndex < seperatorsSize; seperatorIndex++) {
					if (tolower(latinumstack[latinumIndex + findIndex] + 1) == seperators[seperatorIndex]) {
						return latinumIndex;
					}
				}
			}
		*/
	}
	return -1;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function finding first occurence of given char, returning string prior to its occurence - chrissstrahl
//--------------------------------------------------------------
str gamefix_getStringUntilChar(const str& source, const char& delimiter)
{
	str result = "";

	if (delimiter) {
		for (int i = 0; i < source.length(); ++i) {
			if (source[i] == delimiter) {
				break;
			}
			result += source[i];
		}
	}

	return result;
}
//--------------------------------------------------------------
// GAMEFIX - Added: Function finding first occurence of given char, returning string prior to its occurence - chrissstrahl
//--------------------------------------------------------------
char* gamefix_getStringUntilChar(const char* source, const char& delimiter)
{
	if (!delimiter) {
		return (char*)source;
	}
	
	// Get length
	int length = 0;
	while (source[length] != '\0' && source[length] != delimiter) {
		++length;
	}

	// Allocate memory
	char* result = new char[length + 1];

	// Copy and terminate
	Q_strncpyz(result, source, length + 1);

	return result;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function returning a substr / part of a string - chrissstrahl
//--------------------------------------------------------------
str gamefix_getStringUntil(const str& sString, const int iStart, const int iEnd)
{
	const int iLength = sString.length();
	if (iStart >= iLength) {
		throw("gamefix_getStringUntil: start pos > than strlen");
	}

	int actualEnd = iEnd;
	if (iEnd > iLength) {
		actualEnd = iLength;
	}
	else if (iStart + iEnd > iLength) {
		actualEnd = iLength;
	}

	str result;
	for (int i = iStart; i < actualEnd; ++i) {
		result += sString[i];
	}

	return result;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function counting occurence of given char in a string - chrissstrahl
//--------------------------------------------------------------
int gamefix_countCharOccurrences(const char* str,const char& ch)
{
	if (!ch) {
		return 0;
	}

	int count = 0;
	while (*str) {
		if (*str == ch) {
			++count;
		}
		++str;
	}
	return count;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to allow spawning Object, pretty much like in the scripts
//--------------------------------------------------------------
Entity* gamefix_spawn(char const* className, char const* model, char const* origin, char const* targetname, const int spawnflags)
{
	Entity* ent;
	ClassDef* cls;

	// create a new entity
	SpawnArgs args;

	cls = getClassForID(className);
	if (!cls) {
		cls = getClass(className);
	}

	args.setArg("classname", className);
	if (strlen(model)) {
		args.setArg("model", model);
	}
	args.setArg("origin", origin);
	if (strlen(targetname)) {
		args.setArg("targetname", targetname);
	}

	cls = args.getClassDef();
	if (!cls)
	{
		gi.Printf("'%s' is not a valid entity name", className);
		return nullptr;
	}
	level.spawnflags = spawnflags;
	ent = args.Spawn();
	if (!ent) { return nullptr; }
	return ent;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to disable  sv_floodprotect as it creates many issues in mp with command not being detected - Chrissstrahl
// Known Problems with sv_floodProtect enabled:
// player disconnect, huds and menus not being added to client (if teams are switched fast)
// gamefix has its own floodfilter in place, allowing important commands like "disconnect" to pass at any time
// 
// see also: G_ClientCommand
//--------------------------------------------------------------
void gamefix_svFloodProtectDisable()
{
	if (gameFixAPI_inSingleplayer()) { return; }

	cvar_t* cvar = gi.cvar_get("sv_floodprotect");
	if (cvar && cvar->integer == 1 || !cvar) {
		gi.Printf(_GFix_INFO_APPLIED, _GFix_MSG_sv_floodProtect);
		gi.cvar_set("sv_floodprotect", "0");
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function kick all bots from server - chrissstrahl
//--------------------------------------------------------------
void gamefix_kickBots()
{
	Player* player = nullptr;
	for (int i = 0; i < gameFixAPI_maxClients(); i++) {
		player = gamefix_getPlayer(i);

		if (!player || !gameFixAPI_isBot(player)) {
			continue;
		}
		gi.SendConsoleCommand(va("kick %d\n", i));
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Support for Delayed serverCommands for players - This adds a serverCommand to a player's list of delayed commands - daggolin
//--------------------------------------------------------------
void gamefix_playerDelayedServerCommand(int entNum, const char* commandText)
{
	if (entNum < 0 || entNum >= game.maxclients) {
		return;
	}

	gentity_t* edict = &g_entities[entNum];
	if (!edict || !edict->inuse || !edict->client) {
		return;
	}

	Player* player = (Player*)edict->entity;
	if (!player) {
		return;
	}

	gamefix_pendingServerCommand* command = (gamefix_pendingServerCommand*)malloc(sizeof(gamefix_pendingServerCommand));
	if (command == NULL) {
		gi.Printf("gamefix_playerDelayedServerCommand: Couldn't allocate memory for new pendingServerCommand -> Dropping command.\n");
		return;
	}
	// Ensuring initialization - to stop MS-VS from nagging - chrissstrahl
	command->command = NULL;
	command->next = NULL; 

	int commandLength = strlen(commandText) + 1;
	command->command = (char*)malloc(commandLength * sizeof(char));
	if (command->command == NULL) {
		gi.Printf("gamefix_playerDelayedServerCommand: Couldn't allocate memory for new pendingServerCommandText -> Dropping command.\n");
		free(command);
		return;
	}

	Q_strncpyz(command->command, commandText, commandLength);
	command->next = NULL;

	gamefix_pendingServerCommand* temp = pendingServerCommandList[entNum];
	if (temp == NULL) {
		pendingServerCommandList[entNum] = command;
	}
	else {
		while (temp->next) {
			temp = temp->next;
		}
		//gi.Printf("gamefix_playerDelayedServerCommand (%s): %s\n", player->client->pers.netname, command->command);
		temp->next = command;
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Support for Delayed serverCommands for players - This handles the still delayed serverCommands of all players. - daggolin
//--------------------------------------------------------------
void gamefix_playerHandleDelayedServerCommand(void)
{
	for (int i = 0; i < maxclients->integer; i++) {
		gentity_t* edict = &g_entities[i];
		if (!edict->inuse || !edict->entity || !edict->client) {
			continue;
		}
		Player* player = (Player*)edict->entity;

		gamefix_pendingServerCommand* pendingCommand = pendingServerCommandList[i];
		while (pendingCommand) {
			if (gi.GetNumFreeReliableServerCommands(player->entnum) > 90) {
				str sCmd;
				str sNewText = "";
				int foundSpace = gamefix_findChar(pendingCommand->command, ' ');
				str sText = pendingCommand->command + foundSpace + 1;

				if (Q_stricmpn("hudprint ", pendingCommand->command, foundSpace) == 0 ||
					Q_stricmp("status", pendingCommand->command) == 0 ||
					Q_stricmp("score", pendingCommand->command) == 0)
				{
					sCmd = gamefix_getStringUntilChar(pendingCommand->command, ' ');
				}
				else {
					sCmd = "stufftext";
					sText = pendingCommand->command;
				}
				sCmd += " \"" + sText + "\"\n";

				if (sText.length() > 287) {
					sNewText = gamefix_getStringUntil(sText, 0, 286);
					gi.Printf("gamefix_playerDelayedServerCommand: String too long, was cut down to 286\n");
				}
				else {
					sNewText = sText;
				}
				sCmd += sNewText + "\"\n";

				gi.SendServerCommand(i, sCmd.c_str());

				pendingServerCommandList[i] = pendingCommand->next;
				free(pendingCommand->command);
				free(pendingCommand);
				pendingCommand = pendingServerCommandList[i];
			}
			else {
				break;
			}
		}
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Support for Delayed serverCommands for players - This clears the still delayed serverCommands of a player. - daggolin
//--------------------------------------------------------------
void gamefix_playerClearDelayedServerCommand(int entNum)
{
    gamefix_pendingServerCommand* current = pendingServerCommandList[entNum];
    while (current != NULL) {
        gamefix_pendingServerCommand* temp = current;
        current = current->next;
		free(temp->command);
        free(temp);
    }
    pendingServerCommandList[entNum] = NULL;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to read contents of a file into a container, each line will be one object - chrissstrahl
//--------------------------------------------------------------
bool gamefix_getFileContents(str sFile, str& contents, bool tokenize) {
	contents = "";
	if (!gi.FS_Exists(sFile.c_str())) {
		gi.Printf("gamefix_getFileContents: Couldn't find file: %s\n", sFile.c_str());
		return 0;
	}
	static str bufferOld = "";
	static str buffer = "";

	Script bufferInternal;
	bufferInternal.LoadFile(sFile.c_str());
	long lSize = bufferInternal.length;

	if (bufferInternal.buffer == NULL || !strlen(bufferInternal.buffer)) {
		bufferInternal.Close();
		return 0;
	}

	bufferOld = bufferInternal.buffer;
	bufferInternal.Close();

	if (bufferOld[lSize - 1] != '\0') {
		bufferOld += '\0';
	}

	// Convert Umlauts if the file was ENCODED as UTF8 instead of ANSI
	buffer = gamefix_convertUtf8UmlautsToAnsi(bufferOld);

	char* ptr;
	char* token;
	ptr = const_cast<char*>(buffer.c_str());
	//int lineNum = 0;
	//int tokenNumTotal = 0;
	
	//split up into tookens - basically a hard clensing
	if (tokenize) {
		while (ptr && *ptr != '\0') {
			str lineContents = "";
			int tokenNum = 0;		
			while (true) {
				token = COM_ParseExt(&ptr, false);
				if (!token[0]) {
					break;
				}

				// When putting the line together, separate the tokens with a whitespace
				if (tokenNum != 0) {
					lineContents += " ";
				}
				lineContents += token;
				//tokenNumTotal += ++tokenNum;
			}

			if (lineContents.length() > 0) {
				contents += lineContents;
				contents += "\n";
			}
			//lineNum++;
		}
	}
	//get raw content - usually the best choice if we want to update contents of a file
	else{
		contents = ptr;
	}
	//gi.Printf("Lines: %d, Tokens: %d, Size: %d, File: %s\n", lineNum, tokenNumTotal, lSize, sFile.c_str());
	//gi.Printf("%s\n", contents.c_str());
	return bool(contents.length());
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to check if text contains non ANSI Chars - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
bool gamefix_containsNonANSI(const unsigned char* buffer, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (buffer[i] >= 0x80) {
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to convert UTF-8 Umlaut and others withing ASCII bounds to ANSI - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
char* gamefix_convertUtf8UmlautsToAnsi(const char* utf8_str)
{
	const unsigned char* src = (const unsigned char*)utf8_str;
	size_t utf8_len = strlen(utf8_str);
	size_t ansi_size = utf8_len + 1; // Add 1 for null-termination

	// Allocate sufficient memory for ANSI string
	char* ansi_str = (char*)malloc(ansi_size);
	if (!ansi_str) {
		gi.Printf("Error: malloc");
		return NULL;
	}

	unsigned char* dst = (unsigned char*)ansi_str;
	size_t len = 0;

	while (*src) {
		// Check for specific UTF-8 umlauts and other characters
		if (src[0] == 0xC3) {
			switch (src[1]) {
			case 0xA4: // a-uml
				*dst++ = 0xE4;
				src += 2;
				len++;
				continue;
			case 0xB6: // o-uml
				*dst++ = 0xF6;
				src += 2;
				len++;
				continue;
			case 0xBC: // u-uml
				*dst++ = 0xFC;
				src += 2;
				len++;
				continue;
			case 0x84: // A-uml
				*dst++ = 0xC4;
				src += 2;
				len++;
				continue;
			case 0x96: // O-uml
				*dst++ = 0xD6;
				src += 2;
				len++;
				continue;
			case 0x9C: // U-uml
				*dst++ = 0xDC;
				src += 2;
				len++;
				continue;
			case 0x9F: // Sharp-S
				*dst++ = 0xDF;
				src += 2;
				len++;
				continue;
			}
		}
		else if (src[0] == 0xC2) {
			switch (src[1]) {
			case 0xA9: // Copyright Symbol
				*dst++ = 0xA9;
				src += 2;
				len++;
				continue;
			case 0xAE: // Registered Symbol
				*dst++ = 0xAE;
				src += 2;
				len++;
				continue;
			case 0xB1: // Plus Minus unified Symbol
				*dst++ = 0xB1;
				src += 2;
				len++;
				continue;
			case 0xBC: // One Fourth Symbol
				*dst++ = 0xBC;
				src += 2;
				len++;
				continue;
			}
		}
		else if (src[0] == 0xE2) {
			if (src[1] == 0x80) {
				switch (src[2]) {
				case 0x98: // left single quotation mark
				case 0x99: // right single quotation mark
					*dst++ = '\'';
					src += 3;
					len++;
					continue;
				case 0x9C: // left double quotation mark
				case 0x9D: // right bouble quotation mark
					*dst++ = '"';
					src += 3;
					len++;
					continue;
				}
			}
		}
		// If not a specific character, just copy the byte
		*dst++ = *src++;
		len++;
	}

	*dst = '\0'; // Null-terminate the output string

	return ansi_str;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to trim whitespace from a string - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
char* gamefix_trimWhitespace(char* input)
{
	return gamefix_trimWhitespace(input, false);
}
char* gamefix_trimWhitespace(char* input, bool dontTrimNewLine)
{
	char* start = input;
	char* end = nullptr;

	// Find the first non-whitespace character
	if (dontTrimNewLine) {
		while (isspace((unsigned char)*start) && *start != '\n') start++;
	}
	else {
		while (isspace((unsigned char)*start)) start++;
	}
	

	// If the input is all spaces, return it
	if (*start == 0) {
		return start;
	}

	// Find the last non-whitespace character
	end = start + strlen(start) - 1;
	if (dontTrimNewLine) {
		while (end > start && isspace((unsigned char)*end) && *end != '\n') end--;
	}
	else {
		while (end > start && isspace((unsigned char)*end)) end--;
	}

	// Write new null terminator
	end[1] = '\0';

	return start;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to trim whitespace from a string - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
str gamefix_trimWhitespace(const str& input)
{
	return gamefix_trimWhitespace(input,false);
}
str gamefix_trimWhitespace(const str& input, bool dontTrimNewLine)
{
	const char* c_str = input.c_str();
	int start = 0;
	int end = input.length() - 1;

	// Trim leading whitespace
	if (dontTrimNewLine) {
		while (start <= end && isspace(static_cast<unsigned char>(c_str[start])) && c_str[start] != '\n') {
			start++;
		}
	}
	else {
		while (start <= end && isspace(static_cast<unsigned char>(c_str[start]))) {
			start++;
		}
	}

	// Trim trailing whitespace
	if (dontTrimNewLine) {
		while (end >= start && isspace(static_cast<unsigned char>(c_str[end])) && c_str[start] != '\n') {
			end--;
		}
	}
	else {
		while (end >= start && isspace(static_cast<unsigned char>(c_str[end]))) {
			end--;
		}
	}

	// Create a new str with trimmed content
	return str(input, start, end + 1);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to list seperated values in a str container - chrissstrahl
//--------------------------------------------------------------
void gamefix_listSeperatedItems(Container<str>& container, const str& source, const str& seperator)
{
	container.FreeObjectList();
	if (!source.length() || !seperator.length()) {
		return;
	}

	str item = "";
	for (int i = 0; i < source.length(); i++) {
		bool add = false;
		for (int idx = 0; idx < seperator.length(); idx++) {
			if (source[i] == seperator[idx]) {
				add = true;
			}
		}

		if (!add) {
			item += source[i];
		}

		if (add || i == (source.length() - 1)) {
			if (item.length()) {
				container.AddObject(item);
			}
			item = "";
			continue;
		}
	}
}







//--------------------------------------------------------------
// GAMEFIX - Added: Function to get file extension from string - taken from q_shared - chrissstrahl
//--------------------------------------------------------------
str gamefix_getExtension(const str& in)
{
	static char exten[8];
	int i;

	const char* c_in = in.c_str();
	while (*c_in && (*c_in != '.'))
		c_in++;
	if (!*c_in)
		return str("");
	c_in++;
	for (i = 0; i < 7 && *c_in; i++, c_in++)
		exten[i] = *c_in;
	exten[i] = 0;
	return str(exten);
}

//--------------------------------------------------------------
// GAMEFIX - Added: Support for ini-files - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
gamefix_iniFileSection* gamefix_iniFileParseSections(const str& file, const char* data, int* section_count) {
	*section_count = 0;
	gamefix_iniFileSection* sections = (gamefix_iniFileSection*)malloc(MAX_SECTIONS * sizeof(gamefix_iniFileSection));
	if (!sections) {
		gi.Error(ERR_DROP, "iniFileParseSections - Failed to allocate memory for: %s!\n", file.c_str());
		return NULL;
	}

	for (int i = 0; i < MAX_SECTIONS; ++i) {
		sections[i].line_count = 0;
	}

	char* modifiable_data = gamefix_duplicateString(data);
	if (!modifiable_data) {
		gi.Error(ERR_DROP, "iniFileParseSections - Failed to dublicate string for: %s!\n", file.c_str());
		free(sections);
		return NULL;
	}

	char* line = strtok(modifiable_data, "\n");
	gamefix_iniFileSection* current_section = NULL;

	while (line != NULL) {
		char* trimmed_line = gamefix_trimWhitespace(line,true);

		if (trimmed_line[0] == '\0' || trimmed_line[0] == ';' || trimmed_line[0] == '#') {
			line = strtok(NULL, "\n");
			continue;
		}

		if (trimmed_line[0] == '[') {
			int end_pos = gamefix_findChar(trimmed_line, ']');
			if (end_pos != -1) {
				if (*section_count >= MAX_SECTIONS) {
					gi.Error(ERR_DROP,"%s exceeds limit of %d sections!\n", file.c_str(), MAX_SECTIONS);
					free(modifiable_data);
					free(sections);
					break;
				}
				current_section = &sections[(*section_count)++];
				strncpy(current_section->section, trimmed_line + 1, end_pos - 1);
				current_section->section[end_pos - 1] = '\0';
			}
		}
		else if (current_section && current_section->line_count < MAX_LINES) {
			strncpy(current_section->lines[current_section->line_count++], trimmed_line, MAX_LINE_LENGTH - 1);
			current_section->lines[current_section->line_count - 1][MAX_LINE_LENGTH - 1] = '\0';
		}

		line = strtok(NULL, "\n");
	}

	free(modifiable_data);
	return sections;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to return contents given section if it exists  - generated by ChatGTP 4o - chrissstrahl
// Added: Support for ini-files
//--------------------------------------------------------------
str gamefix_iniFileGetSection(const str& file, const str& data, const char* section_name)
{
	int section_count;
	gamefix_iniFileSection* sections = gamefix_iniFileParseSections(file, data, &section_count);
	str result = "";

	if (!sections) {
		gi.Printf("gamefix_iniFileGetSection: Failed to parse sections of %s!\n", file.c_str());
		return result;
	}
	
	for (int i = 0; i < section_count; i++) {
		if (Q_stricmp(sections[i].section, section_name) == 0) {
			for (int j = 0; j < sections[i].line_count; j++) {
				if (!result.length()) {
					result += "\n";
				}
				result += sections[i].lines[j];
				result += "\n";
			}
			break;
		}
	}

	free(sections); // Free the allocated sections array

	return result;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to get the value of the given key of the given section - chrissstrahl
// Added: Support for ini-files
//--------------------------------------------------------------
str gamefix_iniFileGetValueFromKey(const str& file, const str& section_contents, const str& key)
{
	return gamefix_iniFileGetValueFromKey(file, section_contents, key,"");
}
str gamefix_iniFileGetValueFromKey(const str& file, const str& section_contents, const str& key, const str& altVal)
{
	int length = section_contents.length();
	int lengthKey = key.length();

	if (lengthKey > 32) {
		gi.Printf("%s - Key-Name exceeded 32 length -> %s...\n",file.c_str(), gamefix_getStringUntil(key, 0, 24).c_str());
		return altVal;
	}

	//empty section, leave
	//empty key-name, leave
	if (!length || !lengthKey) {
		return altVal;
	}

	int foundKeyAt = gamefix_findStringCase(section_contents, key,true);
	
	//not found, leave
	if (foundKeyAt == -1) {
		return altVal;
	}

	//no actual value, leave
	if (length < foundKeyAt + lengthKey + 2) {
		return altVal;
	}
	
	bool valueStarted = false;
	str valueLines = "";
	for (int i = (foundKeyAt + lengthKey); i < length; i++) {
		if (section_contents[i] == '\n' || section_contents[i] == '\r') {
			break;
		}
		if (!valueStarted){
			if (section_contents[i] == '=' || isspace(section_contents[i])) {
				continue;
			}else{
				valueStarted = true;
			}
		}
		valueLines += section_contents[i];
	}
	valueLines = gamefix_trimWhitespace(valueLines,true);
	if (valueLines.length() > 256) {
		valueLines = gamefix_getStringUntil(valueLines, 0, 255);
		gi.Printf("Value exceeded 256 length for key: %s\n", key.c_str());
	}
	return valueLines;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to set the value of the given key of the given section - generated by ChatGTP 4o - chrissstrahl
// Added: Support for ini-files
// Replaces or adds key in the given section
//--------------------------------------------------------------
str gamefix_iniFileSetValueInSection(const str& section_contents, const str& key, const str& value) {
	str modified_section = section_contents;
	int key_length = key.length();
	int value_length = value.length();

	if (key_length == 0) {
		gi.Printf("Key-Name cannot be empty.\n");
		return modified_section;
	}

	// find key
	int found_key_at = gamefix_findStringCase(modified_section, key, true);

	if (found_key_at != -1) {
		// found key, replave value
		int end_of_key = found_key_at + key_length;

		// find '='
		int equal_sign_pos = gamefix_findChar(modified_section.c_str(), '=', end_of_key);
		if (equal_sign_pos == -1) {
			return modified_section; // no '=' found
		}

		// skip '=' and all following whitespaces
		int value_start_pos = equal_sign_pos + 1;
		while (isspace(modified_section[value_start_pos])) {
			value_start_pos++;
		}

		// check if a value is encapsulated within quotes
		char quote_char = '\0';
		if (modified_section[value_start_pos] == '"' || modified_section[value_start_pos] == '\'') {
			quote_char = modified_section[value_start_pos];
			value_start_pos++;
		}

		// find end pos of old value
		int end_of_value = value_start_pos;
		bool escape = false;
		while (end_of_value < modified_section.length()) {
			char current_char = modified_section[end_of_value];

			if (escape) {
				escape = false;
			}
			else if (current_char == '\\') {
				escape = true;
			}
			else if (quote_char != '\0' && current_char == quote_char) {
				quote_char = '\0';
			}
			else if (quote_char == '\0' && current_char == '\n') {
				break;
			}

			end_of_value++;
		}

		// Extract part before kex and part after old value
		str before_key = gamefix_getStringUntil(modified_section, 0, found_key_at);
		str after_key = gamefix_getStringUntil(modified_section, end_of_value, modified_section.length());

		// handle quotes
		str new_value = ""; // value;
		if ((value[0] == '"' && value[value_length - 1] == '"') || (value[0] == '\'' && value[value_length - 1] == '\'')) {
			new_value = value;
		}
		else {
			new_value = "\"" + value + "\"";
		}

		// modify
		modified_section = before_key + key + "=" + new_value + "\n" + after_key;
	}
	else {
		// no key found, append at end
		if (modified_section[modified_section.length() - 1] != '\n') {
			modified_section += "\n";
		}

		// handle quotes
		str new_value = value;
		if ((value[0] == '"' && value[value_length - 1] == '"' || (value[0] == '\'' && value[value_length - 1] == '\''))) {
			new_value = value;
		}
		else {
			new_value = "\"" + value + "\"";
		}
		
		modified_section += key + "=" + new_value + "\n";
	}
	return modified_section;
}


//--------------------------------------------------------------
// GAMEFIX - Added: Function to get all section names from INI - chrissstrahl
// Added: Support for ini-files
//--------------------------------------------------------------
void gamefix_iniFileGetSectionNames(const str& file, Container<str> &sectionList, const str& contents)
{
	str section_names = "";
	char* data = gamefix_duplicateString(contents.c_str());
	char* line = strtok(data, "\n");

	sectionList.FreeObjectList();

	while (line != nullptr) {
		// Trim whitespace from the line
		while (isspace(static_cast<unsigned char>(*line))) line++;
		char* endLn = line + strlen(line) - 1;
		while (endLn > line && isspace(static_cast<unsigned char>(*endLn))) endLn--;
		*(endLn + 1) = '\0';

		// Check if the line is a section header
		if (line[0] == '[') {
			char* end = strchr(line, ']');
			if (end) {
				*end = '\0';  // Remove closing bracket
				str section_name = str(line + 1);  // Skip opening bracket

				if (sectionList.IndexOfObject(section_name)) {
					gi.Printf("Dublicated section name %s in %s\n", section_name.c_str(), file.c_str());
				}

				sectionList.AddObject(section_name);
			}
		}

		line = strtok(nullptr, "\n");
	}

	free(data);  // Free the duplicated string
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to extract two integers from a str representing a min/max range with various separators - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
void gamefix_extractIntegerRange(const str& input, int& first, int& second)
{
	const char* c_str = input.c_str();
	char* sep = const_cast<char*>(c_str);

	// Find the first whitespace or dash character
	while (*sep && !isspace(static_cast<unsigned char>(*sep)) && *sep != '-') {
		sep++;
	}

	if (*sep) {
		char* next = sep;

		if (*sep == '-') {
			next++;
			int length = 0; //make sure we don't go outside the safe atoi/atof bounds with any returned number
			while (*next && !isdigit(static_cast<unsigned char>(*next)) && *next != '-' && *next != '+' && *next != '.' && length < 11) {
				next++;
				length++;
			}
			if (*next) {
				sep = next - 1;
			}
		}

		*sep = '\0';  // Temporarily terminate the first part of the string
		first = atoi(c_str);  // Convert the first part to a int

		next = sep + 1;
		// Skip over any non-digit and non-sign characters
		int length = 0; //make sure we don't go outside the safe atoi/atof bounds with any returned number
		while (*next && !isdigit(static_cast<unsigned char>(*next)) && *next != '-' && *next != '+' && *next != '.' && length < 11) {
			next++;
			length++;
		}
		second = atoi(next);  // Convert the second part to a int

		if (first > second) {
			gi.Printf("Invalid range: %.2f to %.2f (min value must be the beginning first value)\n", first, second);
		}
	}
	else {
		//make sure we don't go outside the safe atoi/atof bounds with any returned number
		str snum = "";
		if (strlen(c_str) > 10) {
			snum = gamefix_getStringUntil(c_str, 0, 9);
		}
		else {
			snum = c_str;
		}
		// Handle cases where no separator is found
		first = atoi(snum);
		second = atoi(snum);
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to extract two floats from a str representing a min/max range with various separators - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
void gamefix_extractFloatRange(const str input, float& first, float& second)
{
	const char* c_str = input.c_str();
	char* sep = const_cast<char*>(c_str);

	// Find the first whitespace or dash character
	while (*sep && !isspace(static_cast<unsigned char>(*sep)) && *sep != '-') {
		sep++;
	}

	if (*sep) {
		char* next = sep;

		if (*sep == '-') {
			next++;
			int length = 0; //make sure we don't go outside the safe atoi/atof bounds with any returned number
			while (*next && !isdigit(static_cast<unsigned char>(*next)) && *next != '-' && *next != '+' && *next != '.' && length < 11) {
				next++;
				length++;
			}
			if (*next) {
				sep = next - 1;
			}
		}

		*sep = '\0';  // Temporarily terminate the first part of the string
		first = atof(c_str);  // Convert the first part to a float

		next = sep + 1;
		// Skip over any non-digit and non-sign characters
		int length = 0; //make sure we don't go outside the safe atoi/atof bounds with any returned number
		while (*next && !isdigit(static_cast<unsigned char>(*next)) && *next != '-' && *next != '+' && *next != '.' && length < 11) {
			next++;
			length++;
		}
		second = atof(next);  // Convert the second part to a float

		if (first > second) {
			gi.Printf("Invalid range: %.2f to %.2f (min value must be the beginning first value)\n", first, second);
		}
	}
	else {
		//make sure we don't go outside the safe atoi/atof bounds with any returned number
		str snum = "";
		if (strlen(c_str) > 10) {
			snum = gamefix_getStringUntil(c_str, 0, 9);
		}
		else {
			snum = c_str;
		}
		// Handle cases where no separator is found
		first = atof(snum);
		second = atof(snum);
	}
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to duplicate a string  - generated by ChatGTP 4o - chrissstrahl
// manual implementation of strdup
//--------------------------------------------------------------
char* gamefix_duplicateString(const char* source)
{
	size_t len = strlen(source);
	char* dest = (char*)malloc(len + 1); // Allocate memory for the string
	if (dest) {
		strcpy(dest, source); // Copy the source string into the allocated memory
	}
	return dest;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to return cleaned up mapname - chrissstrahl
//--------------------------------------------------------------
str gamefix_cleanMapName(const str& mapname)
{
	str cleanMapName = mapname;
	if (cleanMapName.length()) {
		//clean leading slash, that might remained from a path
		if (cleanMapName[0] == '/') {
			cleanMapName = gamefix_getStringUntil(mapname, 1, mapname.length());
		}

		//filter spawnspot/target
		int filth = gamefix_findChar(mapname, '$');
		if (filth != -1) {
			cleanMapName.CapLength(filth);
		}

		//filter illegal potentially harfful char
		filth = gamefix_findChar(mapname, '%');
		if (filth != -1) {
			cleanMapName.CapLength(filth);
		}		
	}
	return cleanMapName;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to clean string of given illegal chars - chrissstrahl
//--------------------------------------------------------------
str gamefix_filterChars(const str filthy, str illegal)
{
	int filthyLength = filthy.length();
	int illegalLength = illegal.length();
	if (filthy && illegal && filthyLength && illegalLength) {
		str sNew = "";
		for(int i = 0; i < filthyLength && filthy[i] != '\0'; i++) {
			bool add = true;
			for (int j = 0; j < illegalLength;j++) {
				if (filthy[i] == illegal[j]) {
					add = false;
					break;
				}
			}
			if (add) {
				sNew += filthy[i];
			}
		}
		return sNew;
	}
	return filthy;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to clean string of given chars if they are dublicated - chrissstrahl
//--------------------------------------------------------------
str gamefix_stripDoubleChar(const str filthy, str illegal)
{
	//strip double slashes
	int i = 0;
	str clean = "";
	for (i = 0; i < filthy.length(); i++) {
		bool add = true;
		for (int j = 0; j < illegal.length(); j++) {
			if (i < (filthy.length() - 1) && filthy[i] == illegal[j] && filthy[i + 1] == illegal[j]) {
				add = false;
				break;
			}
		}

		if (!add) {
			continue;
		}

		clean += filthy[i];
	}
	return clean;
}


//--------------------------------------------------------------
// GAMEFIX - Added: Function to handle each frame call - chrissstrahl
//--------------------------------------------------------------
void gamefix_runFrame(int levelTime, int frameTime)
{
	gamefix_playerHandleDelayedServerCommand();
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to manage game shutdown - chrissstrahl
// Executed if game is shut down - killserver, quit, HOST-disconnect
//--------------------------------------------------------------
void gamefix_shutdownGame()
{
	gameFixAPI_shutdownGame();
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to manage game startup - chrissstrahl
// Executed ONLY if game is started for first time
//--------------------------------------------------------------
void gamefix_initGame()
{
	gameFixAPI_initGame();
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to manage game shutdown - chrissstrahl
// Executed if level is exited/changed/restarted - but not on first load/game start
//--------------------------------------------------------------
void gamefix_cleanupGame(qboolean restart)
{
	gi.Printf("==== CleanupGame ====\n");
	gameFixAPI_cleanupGame(restart);
}

/*
NOT IN USE - BUT MIGHT COME IN HANDY ONE DAY - THIS SHOULD PROBABLY BE PUT INTO A SEPERATE FILE
*
* *
* * *
//--------------------------------------------------------------
// GAMEFIX - Added: Function to check if text contains UTF8 BOM - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
bool gamefix_hasUTF8BOM(const unsigned char* buffer, size_t length) {
	return length >= 3 &&
		buffer[0] == 0xEF &&
		buffer[1] == 0xBB &&
		buffer[2] == 0xBF;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to check if text is likley UTF8 - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
bool gamefix_isLikelyUTF8(const unsigned char* buffer, size_t length) {
	size_t i = 0;
	while (i < length) {
		if (buffer[i] <= 0x7F) {
			// ASCII byte
			i++;
		}
		else if (buffer[i] >= 0xC2 && buffer[i] <= 0xDF) {
			// 2-byte sequence
			if (i + 1 < length &&
				(buffer[i + 1] & 0xC0) == 0x80) {
				i += 2;
			}
			else {
				return false;
			}
		}
		else if (buffer[i] >= 0xE0 && buffer[i] <= 0xEF) {
			// 3-byte sequence
			if (i + 2 < length &&
				(buffer[i + 1] & 0xC0) == 0x80 &&
				(buffer[i + 2] & 0xC0) == 0x80) {
				i += 3;
			}
			else {
				return false;
			}
		}
		else if (buffer[i] >= 0xF0 && buffer[i] <= 0xF4) {
			// 4-byte sequence
			if (i + 3 < length &&
				(buffer[i + 1] & 0xC0) == 0x80 &&
				(buffer[i + 2] & 0xC0) == 0x80 &&
				(buffer[i + 3] & 0xC0) == 0x80) {
				i += 4;
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}
	return true;
}

//--------------------------------------------------------------
// GAMEFIX - Added: Function to check Char is a Umlaut - generated by ChatGTP 4o - chrissstrahl
//--------------------------------------------------------------
bool gamefix_isUmlaut(char c) {
	return c == '\xE4' || c == '\xF6' || c == '\xFC' ||  // �, �, �
		c == '\xC4' || c == '\xD6' || c == '\xDC' ||  // �, �, �
		c == '\xDF';                                  // �
}

const char* gamefix_getExtension(const char* in)
{
	static char exten[8];
	int		i;

	while (*in && (*in != '.'))
		in++;
	if (!*in)
		return "";
	in++;
	for (i = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

*/
