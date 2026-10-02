// hrcbot_version.h
// Central version / identity constants for the modernized Hurricane Bot.
//
// The original 1.3.4 binary identified itself as:
//   "Hurricane Bot ver 1.3.4-hl2sdk-amsdk20130427 (130501)".
// This tree is a from-the-binary reconstruction modernized for
// Metamod:Source 1.12 and the 64 bit capable hl2sdk (hl2dm) branch.
#ifndef HRCBOT_VERSION_H_
#define HRCBOT_VERSION_H_

#define HRCBOT_NAME        "Hurricane Bot"
#define HRCBOT_LOGTAG      "HRCBOT"
#define HRCBOT_AUTHOR      "Hurricane (original); modernized reconstruction"
#define HRCBOT_URL         "https://hurrikhan.eu/"
#define HRCBOT_LICENSE      "Freeware - see LICENCE.txt; must not be sold"
#define HRCBOT_DESCRIPTION "Automatic server-side bots for Half-Life 2: Deathmatch"

// Human readable version of the modernized tree.
#define HRCBOT_VERSION_MAJOR 2
#define HRCBOT_VERSION_MINOR 0
#define HRCBOT_VERSION_PATCH 0

// Original release this reconstruction targets (kept for log compatibility).
#define HRCBOT_LEGACY_VERSION "1.3.4-hl2sdk-amsdk20130427 (130501)"

#define HRCBOT_VERSION_STRING "2.0.0"
#define HRCBOT_VERSION_CVAR   "2.0.0-reconstructed"

#define HRCBOT_BANNER \
	"Hurricane Bot (HRCBot) " HRCBOT_VERSION_STRING \
	" - reconstructed for Metamod:Source 1.12 / 64-bit hl2dm"

#endif // HRCBOT_VERSION_H_
