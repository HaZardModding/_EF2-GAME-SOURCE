//--------------------------------------------------------------
// GAMEFIX - ELITE FORCE 2 - API - STRINGS
// 
// Elite Force 2 Specific STRINGS USED FOR TEXT THAT IS SPECIFIC TO ELITE FORCE 2
//--------------------------------------------------------------

#pragma once

//--- GAMEFIX ---
//-> Strings related to gamefix.cpp/gamefix.hpp are in gamefix_strings.hpp
//---------------


//--- API --- api_stef2.cpp/api_stef2.hpp SPECIFIC
constexpr auto _GFixAPI_YOUR_LANG_WAS_SET_TO_ENG = "Your Language was set to English\n";
constexpr auto _GFixAPI_YOUR_LANG_WAS_SET_TO_DEU = "Ihre Sprache wurde auf deutsch festgelegt\n";


//--- GAME CODE --- regular gamecode files SPECIFIC
constexpr auto _GFixEF2_INFO_FUNC_InitGame = "==== GAMEFIX %s ====\n==== Compiled %s @ %s====\n";
constexpr auto _GFixEF2_INFO_FUNC_ClientBegin = "level_ai - ON - Server no longer empty!\n";
constexpr auto _GFixEF2_INFO_FUNC_ClientDisconnect = "level_ai - OFF - Server is now empty!\n";

constexpr auto _GFixEF2_MSG_FUNC_FinishMissionFailed = "^1=/\\=^3 Mission Failed ^1=/\\=\n";

constexpr auto _GFixEF2_WARN_EVENT_ACTOR_attack_FAILED = "Actor::AttackEntity - $%s.attack($entity) failed, entity does not exist\n";
constexpr auto _GFixEF2_WARN_EVENT_CTHREAD_triggerEntity_FAILED = "triggerEntity($...) - given entity does not exist\n";
constexpr auto _GFixEF2_WARN_EVENT_CTHREAD_fakePlayer_ONLYSP = "WARNING: fakeplayer script command can only be used in Singleplayer\n";
constexpr auto _GFixEF2_WARN_EVENT_CTHREAD_sendClientCommand_FAILED = "SendClientCommand: Given Entity does not exist!\n";
constexpr auto _GFixEF2_WARN_EVENT_CTHREAD_getNumFreeRelSVCmds_FAILED = "GetNumFreeReliableServerCommands: Given Entity does not exist!\n";

constexpr auto _GFixEF2_ERR_FUNC_SensoryInFov_act_NULL = "SensoryPerception::InFOV -- actor is NULL";
