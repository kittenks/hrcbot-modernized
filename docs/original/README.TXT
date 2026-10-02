Hurricane Bot version 1.3.4
===========================

Fixed a broken change that was affecting the last Windows build.
Fixed the "unknown error 38" error message (removed an old sdk issue workaround that has since been fixed)

Hurricane Bot version 1.3.3
===========================

The linux build now clears up irrelevant code/data/meta like the windows build naturally does.
This subtancially reduced the size of the linux binary.

Hurricane Bot version 1.3.2
===========================

A couple of fixes but mostly : built with Allied Modders SDK 2009.
This releases contains a fix for the linux version (broken with an update at the end of October 2012.
The Windows version has not been edited based on the rule : "If it works, don't fix it".

Hurricane Bot version 1.3.1
===========================

Changes since 1.3.0
-------------------
Removed debug log lines
Disabled irrelevant cvars
Build is done using AlliedModders's SDK.

Note
====

Please don't ask me questions about something already in this file, like the usual : "how do I change their strenght, number, ...".

Please don't ask me how to install the bot.  I don't know more that what I wrote here.  I delete any mail with this question.


Readme / Issues
===============

This version has been built with AlliedModders's 2009SDK.

I don't know exactly why the linux binary is much bigger than the win32 one.  I guess using whole-program-optimization would squeeze if only because
of all the useless symbols.  It does not really matters.

If I were you I'd keep the old .hrcbot map files because they are still valid. (If it works : don't fix it)

If the server crashes, please ensure first that it's because of the bot : disable all other modules and see it crash again.
If you can reproduce the error that way, the map and the server configuration (without your passwords please) can help. (I only have the original maps)
If you are IT : a stacktrace, whatever.  Well, you know what can help.

Manual Installation
===================

Just copy the content of this folder in your "half-life 2 deathmatch" directory.

ie: /home/steam/orangebox/hl2mp/addons

More than that, I can't really help you : I don't play and the only server I have is to test the binary is running.
There are many guys out there who can tell you how to perfectly tune your servers.  I'm just not one of them.

And just because it does not seems clear enough : do not ask to install the bot.  I delete any mail with this question.


Commands
========

hrcbot_enabled N
	If set to 0, the bots will cease all activity and the map will not be analysed at load time.
	If you re-enable the bot in a level with an unanalysed map, you will have to change the level again to get it analyzed.
	This feature has been suggested in order to disable to bot for badly-handled maps.

	ie: hrcbot_enabled 1
	

hrcbot_minplayers N
	Fills the server with bots in order to have a minimum of N acvite players

	Since version 1.0.57 the bots will never fill up the last available slot (so a new player can always connect)
	This will also fixe cases where hrcbot_minplayers is bigger than the maximum number of players allowed on the server/map.

	ie: hrcbot_minplayers 4

hrcbot_maxplayers N
	If there are at least N players (human and bot both) in game, then no more bot will be added.
	The bots will also be balanced down to match this N value.

	ie: hrcbot_maxplayers 16

hrcbot_preferredcount N
	If this value is set to an integer value bigger than 0, the plugin will try to have this number of bots playing, as long as
	the minplayers and maxplayers conditions are verified.

	ie: hrcbot_preferredcount 4

hrcbot_forceteam N
	Force the bot into a preset team.

	In HL2DM N can be:

		-1 = balanced teams (default)
                 2 = combine
                 3 = rebels

		 Do not use

		 0 = automatic
                 1 = spectator

	ie: hrcbot_forceteam -1

hrcbot_autobalancebots N
	Set this to 0 to disable usage of hrcbot_minplayers, hrcbot_maxplayers, hrcbot_preferredcount and hrcbot_forceteam.
	hrcbot_add will only work if N is set to 0

	ie: hrcbot_autobalancebots 1
	
hrcbot_add
hrcbot_add T
	This command can only work if hrcbot_autobalancebots is set to 0
	Without parameter, the command adds a bot.
	With a parameter, adds a bot to the team T

	In HL2DM T can be:

                 2 = combine
                 3 = rebels

		 Do not use

		 0 = automatic
                 1 = spectator


	ie: hrcbot_add
	ie: hrcbot_add 2

hrcbot_kick
hrcbot_kick T
	This command can only work if hrcbot_autobalancebots is set to 0
	Without parameter, the command removes a bot.
	With a parameter, removes a bot from the team T

	In HL2DM T can be:

                 2 = combine
                 3 = rebels

		 Do not use

		 0 = automatic
                 1 = spectator

	ie: hrcbot_kick
	ie: hrcbot_kick 2

hrcbot_waitforplayers 0
	The bots will only be added after the first player connects.

hrcbot_freezeifnoplayers 0
	When the last human player leaves the game, the bots are disabled.  They are enabled again when a player connects.

	ie: hrcbot_freezeifnoplayers 1

hrcbot_player_spawnweapon NAME
	Sets the default spawn weapon of the bot.
	Supported values are : smg1 pistol 357 crossbow shotgun ar2 rpg
	Avoid the 357 if you want to have fun
	
	ie: hrcbot_player_spawnweapon smg1

hrcbot_handicap N
	The bot strenght (aim accuracy) can now be reduced with this.
	At 0, you will just have your luck to survive and your eyes to cry. (Bots will have NO handicap)
	With higher values (10 or more), even your little sister (the one in the cradle) will be able to fight them.
	Values equal or above 0 only.
	

	ie: hrcbot_handicap 5

hrcbot_mute N
	Silent the bot by setting this parameter to 1.  Let him speak by setting it to 0

	ie: hrcbot_mute 1

hrcbot_motd N
	Set this to 0 to disable the "advertising" of the bot.

	ex: hrcbot_motd 0

hrcbot_dialogmsg N
	Set this to 0 in order to disable the displaying of the version of the bot on each client.

	ex: hrcbot_dialogmsg 1

hrcbot_spawnprotectiontime N
	When a player spawn, the bot will leave him alone for until this time has elapsed (1/60 seconds units)

	ie: hrcbot_spawnprotectiontime 120
	    This will protect the spawned player (from the bots) for about 2 seconds

hrcbot_spawnprotectionseconds N
	When a player spawn, the bot will leave him alone for until this time has elapsed (Units are seconds)	
	This is not a "var" but a "function". It changes the value of hrcbot_spawnprotectiontime

	ie: hrcbot_spawnprotectionseconds 30
	
	Will be equivalent to

	    hrcbot_spawnprotectiontime 1800

	And will protect a spawned player (from the bots) for about 30 seconds.

hrcbot_clan NAME
	Each new bot get NAME prefixed to its name.

	ie: hrcbot_clan [HRC]

hrcbot_namesfile NAME
	NAME is a text file with a bot name on each line.
	The file is parsed until the first empty line is found.
	The default location of the file is "hl2mp/addons/hrcbot_server_plugin/hrcbot_names.txt".
	Look into the file to understand the format.

	ie: hrcbot_namesfile "hl2mp/addons/hrcbot_server_plugin/hrcbot_names.txt"

hrcbot_crowbarmaniacs N
	Enabling this will give a crowbar to the bot.  And a bot loves a crowbar more than a Doom Space Marine loves a BFG-9000

	ie: hrcbot_crowbarmaniacs 1

hrcbot_autoweaponswitch N
	Setting this to 0 will prevent the bots from changing weapons when they get a new one.
	
	ie: hrcbot_autoweaponswitch 0

hrcbot_spawnprotectedhealth N
	New versions of "Spawn & Chat Protection" will allow the bot to know when a player is protected. (http://www.esteticas.se/catman.php)
	Meanwhile, the bot will assume that anyone having at least N health is protected.	

	ie: hrcbot_spawnprotectedhealth 500

hrcbot_playermodel T
	All the bots will use the specified model T.  Use "*" for the default random choice.

	HL2DM default models are:

	"models/Combine_Soldier.mdl"
	"models/Combine_Super_Soldier.mdl"
	"models/Police.mdl"

	"models/Humans/Group03/Female_01.mdl"
	"models/Humans/Group03/Female_02.mdl"
	"models/Humans/Group03/Female_03.mdl"
	"models/Humans/Group03/Female_04.mdl"
	"models/Humans/Group03/Female_05.mdl"
	"models/Humans/Group03/Female_06.mdl"
	"models/Humans/Group03/Female_07.mdl"

	"models/Humans/Group03/Male_01.mdl"
	"models/Humans/Group03/male_02.mdl"
	"models/Humans/Group03/male_03.mdl"
	"models/Humans/Group03/Male_04.mdl"
	"models/Humans/Group03/Male_05.mdl"
	"models/Humans/Group03/male_06.mdl"
	"models/Humans/Group03/male_07.mdl"
	"models/Humans/Group03/male_08.mdl"
	"models/Humans/Group03/male_09.mdl" 

	ie: hrcbot_playermodel *
	ie: hrcbot_playermodel models/Humans/Group03/male_09.mdl


hrcbot_do "bot name" command
	This command used to be only available for debugging.
	The bot will execute the command as himself. '\"' are mandatory for composed names."

	WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

	Please note that this command has to be used with care as some parameters (status change) can break the bot.

	WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

	ie: hrcbot_do "[HRC]Zeus" say Hello
	ie: hrcbot_do "[HRC]Zeus" joinclass 1

	ie (could be dangerous because the bot is not supposed to switch team): hrcbot_do "[HRC]Zeus" jointeam 3


Special Commands
================

hrcbot_notifycriticals 1
	The plugin will log critical sections of the code into the hrcbot_log.txt file.
	The critical sections are the parts helping me to solve an issue I'm working on.
	Set this to 0 if you are not working with me to solve an issue on your server.

hrcbot_info
	This command will dump relevant status of the plugin into the console and into the hrcbot_log.txt file.
	Resize your console, lines are pretty wide.


Team Deathmatch
===============

In order to set the game in team deathmatch mode, set mp_teamplay to 1

============

Have fun,

Hurricane

www.hurrikhan.eu

Please send feed back to: hurricane.bot@gmail.com
