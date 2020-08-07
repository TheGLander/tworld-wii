/* tworld.c: The top-level module.
 *
 * Copyright (C) 2001 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"defs.h"
#include	"err.h"
#include	"series.h"
#include	"res.h"
#include	"play.h"
#include	"score.h"
#include	"solution.h"
#include	"help.h"
#include	"oshw.h"
#include	"cmdline.h"

/* The data needed to identify what is being played.
 */
typedef	struct gamespec {
    gameseries	series;		/* the complete set of levels */
    int		currentgame;	/* which level is currently selected */
    int		invalid;	/* TRUE if the current level is invalid */
    int		playback;	/* TRUE if in playback mode */
} gamespec;

/* Structure used to pass data back from initoptionswithcmdline().
 */
typedef	struct startupdata {
    char       *filename;	/* which data file to use */
    int		levelnum;	/* a selected initial level */ 
    int		listseries;	/* TRUE if the files should be listed */
    int		listscores;	/* TRUE if the scores should be listed */
} startupdata;

/* Online help.
 */
static char const *yowzitch = 
	"Usage: tworld [-hvls] [-DS DIR] [NAME] [LEVEL]\n"
	"   -D  Read shared data from DIR instead of the default\n"
	"   -S  Save games in DIR instead of the default\n"
	"   -l  Display the list of available data files and exit\n"
	"   -s  Display your scores for the selected data file and exit\n"
	"   -h  Display this help and exit\n"
	"   -v  Display version information and exit\n"
	"NAME specifies which data file to use.\n"
	"LEVEL specifies which level to start at.\n"
	"(Press ? during the game for further help.)\n";

/* Online version data.
 */
static char const *vourzhon =
	"TileWorld, version 0.7.0.\n\n"
	"Copyright (C) 2001 by Brian Raiter, under the terms of the GNU\n"
	"General Public License; either version 2 of the License, or at\n"
	"your option any later version.\n"
	"   This program is distributed without any warranty, express or\n"
	"implied. See COPYING for details.\n"
	"   (The author also requests that you voluntarily refrain from\n"
	"redistributing this version of the program, as it is still pretty\n"
	"rough around the edges.)\n"
	"   Please direct bug reports to breadbox@muppetlabs.com, or post\n"
	"them on annexcafe.chips.challenge.\n";

/*
 * The top-level user interface functions.
 */

/* A callback function for handling the keyboard while displaying a
 * scrolling list.
 */
static int scrollinputcallback(int *move)
{
    switch (input(TRUE)) {
      case CmdPrev10:		*move = ScrollHalfPageUp;	break;
      case CmdNorth:		*move = ScrollUp;		break;
      case CmdPrev:		*move = ScrollUp;		break;
      case CmdPrevLevel:	*move = ScrollUp;		break;
      case CmdSouth:		*move = ScrollDn;		break;
      case CmdNext:		*move = ScrollDn;		break;
      case CmdNextLevel:	*move = ScrollDn;		break;
      case CmdNext10:		*move = ScrollHalfPageDn;	break;
      case CmdProceed:		*move = TRUE;			return FALSE;
      case CmdQuitLevel:	*move = FALSE;			return FALSE;
      case CmdQuit:						exit(0);
    }
    return TRUE;
}

/* Display the user's current score.
 */
static void showscores(gamespec *gs)
{
    char      **texts;
    char const *header;
    int		listsize, n;

    if (!createscorelist(&gs->series, &texts, &listsize, &header)) {
	ding();
	return;
    }
    n = gs->currentgame;
    setsubtitle(NULL);
    if (displaylist("SCORES", header, (char const**)texts, listsize, &n,
		    scrollinputcallback))
	if (n < listsize - 1)
	    gs->currentgame = n;
    freescorelist(texts, listsize);
}

/*
 */
static void replaceablesolution(gamespec *gs)
{
    gs->series.games[gs->currentgame].replacebest =
			!gs->series.games[gs->currentgame].replacebest;
}

/* Get a keystroke from the user at the completion of the current
 * level.
 */
static void endinput(gamespec *gs, int status)
{
    displayendmessage(status > 0);

    for (;;) {
	switch (input(TRUE)) {
	  case CmdPrev10:	gs->currentgame -= 10;		return;
	  case CmdPrevLevel:	--gs->currentgame;		return;
	  case CmdPrev:		--gs->currentgame;		return;
	  case CmdSameLevel:					return;
	  case CmdSame:						return;
	  case CmdNextLevel:	++gs->currentgame;		return;
	  case CmdNext:		++gs->currentgame;		return;
	  case CmdNext10:	gs->currentgame += 10;		return;
	  case CmdProceed:	if (status > 0) ++gs->currentgame; return;
	  case CmdPlayback:	gs->playback = !gs->playback;	return;
	  case CmdHelp:		gameplayhelp();			break;
	  case CmdSeeScores:	showscores(gs);			return;
	  case CmdKillSolution:	replaceablesolution(gs);	return;
	  case CmdQuitLevel:					exit(0);
	  case CmdQuit:						exit(0);
	}
    }
}

/* Play the current level. Return when the user completes the level or
 * requests something else.
 */
static void playgame(gamespec *gs)
{
    int	cmd, n;

    drawscreen();
    cmd = input(TRUE);
    switch (cmd) {
      case CmdNorth: case CmdWest:				break;
      case CmdSouth: case CmdEast:				break;
      case CmdPrev10:		gs->currentgame -= 10;		return;
      case CmdPrev:		--gs->currentgame;		return;
      case CmdPrevLevel:	--gs->currentgame;		return;
      case CmdNextLevel:	++gs->currentgame;		return;
      case CmdNext:		++gs->currentgame;		return;
      case CmdNext10:		gs->currentgame += 10;		return;
      case CmdPlayback:		gs->playback = TRUE;		return;
      case CmdSeeScores:	showscores(gs);			return;
      case CmdKillSolution:	replaceablesolution(gs);	return;
      case CmdHelp:		gameplayhelp();			return;
      case CmdQuitLevel:					exit(0);
      case CmdQuit:						exit(0);
      default:			cmd = CmdNone;			break;
    }

    setgameplaymode(BeginPlay);
    for (;;) {
	n = doturn(cmd);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	cmd = input(FALSE);
	if (cmd == CmdQuitLevel) {
	    n = -1;
	    break;
	}
	switch (cmd) {
	  case CmdNorth: case CmdWest:			break;
	  case CmdSouth: case CmdEast:			break;
	  case CmdPreserve:				break;
	  case CmdPrevLevel:		n = -1;		goto quitloop;
	  case CmdNextLevel:		n = +1;		goto quitloop;
	  case CmdSameLevel:		n = 0;		goto quitloop;
	  case CmdDebugCmd1:				break;
	  case CmdDebugCmd2:				break;
	  case CmdCheatNorth:     case CmdCheatWest:		break;
	  case CmdCheatSouth:     case CmdCheatEast:		break;
	  case CmdCheatHome:					break;
	  case CmdCheatKeyRed:    case CmdCheatKeyBlue:		break;
	  case CmdCheatKeyYellow: case CmdCheatKeyGreen:	break;
	  case CmdCheatBootsIce:  case CmdCheatBootsSlide:	break;
	  case CmdCheatBootsFire: case CmdCheatBootsWater:	break;
	  case CmdCheatICChip:					break;
	  case CmdQuit:					exit(0);
	  case CmdPauseGame:
	    setgameplaymode(SuspendPlay);
	    anykey();
	    setgameplaymode(ResumePlay);
	    cmd = CmdNone;
	    break;
	  case CmdHelp:
	    setgameplaymode(SuspendPlay);
	    gameplayhelp();
	    setgameplaymode(ResumePlay);
	    cmd = CmdNone;
	    break;
	  default:
	    cmd = CmdNone;
	    break;
	}
    }
    setgameplaymode(EndPlay);
    if (n > 0 && replacesolution())
	savesolutions(&gs->series);
    endinput(gs, n);
    return;

  quitloop:
    setgameplaymode(EndPlay);
    gs->currentgame += n;
}

/* A minimal interface for invalid levels that lets the user move
 * to another level.
 */
static void noplaygame(gamespec *gs)
{
    for (;;) {
	drawscreen();
	switch (input(TRUE)) {
	  case CmdPrev10:	gs->currentgame -= 10;	return;
	  case CmdPrev:		--gs->currentgame;	return;
	  case CmdPrevLevel:	--gs->currentgame;	return;
	  case CmdNextLevel:	++gs->currentgame;	return;
	  case CmdNext:		++gs->currentgame;	return;
	  case CmdNext10:	gs->currentgame += 10;	return;
	  case CmdSeeScores:	showscores(gs);		return;
	  case CmdHelp:		gameplayhelp();		return;
	  case CmdQuitLevel:				exit(0);
	  case CmdQuit:					exit(0);
	  default:		ding();			break;
	}
    }
}

/* Play back the user's best solution for the current level.
 */
static void playbackgame(gamespec *gs)
{
    int	n;

    drawscreen();

    setgameplaymode(BeginPlay);
    for (;;) {
	n = doturn(CmdNone);
	drawscreen();
	if (n)
	    break;
	waitfortick();
	switch (input(FALSE)) {
	  case CmdPrevLevel:	--gs->currentgame;		goto quitloop;
	  case CmdNextLevel:	++gs->currentgame;		goto quitloop;
	  case CmdSameLevel:					goto quitloop;
	  case CmdPlayback:	gs->playback = FALSE;		goto quitloop;
	  case CmdQuitLevel:	gs->playback = FALSE;		goto quitloop;
	  case CmdQuit:						exit(0);
	  case CmdPauseGame:
	    setgameplaymode(SuspendPlay);
	    anykey();
	    setgameplaymode(ResumePlay);
	    break;
	  case CmdHelp:
	    setgameplaymode(SuspendPlay);
	    gameplayhelp();
	    setgameplaymode(ResumePlay);
	    break;
	}
    }
    setgameplaymode(EndPlay);
    gs->playback = FALSE;
    endinput(gs, n);
    return;

  quitloop:
    setgameplaymode(EndPlay);
    gs->playback = FALSE;
}

/*
 * Ancillary top-level functions.
 */

/* Assign values to the different directories that the program uses.
 */
static void initdirs(char const *root, char const *save)
{
    unsigned int	maxpath = getpathbufferlen() - 1;
    char const	       *dir;

    if (!save && (dir = getenv("TWORLDSAVEDIR")) && *dir) {
	if (strlen(dir) < maxpath)
	    save = dir;
	else
	    warn("Value of environment variable TWORLDSAVEDIR is too long");
    }

    if (!root) {
	if ((dir = getenv("TWORLDDIR")) && *dir) {
	    if (strlen(dir) < maxpath - 8)
		root = dir;
	    else
		warn("Value of environment variable TWORLDDIR is too long");
	}
	if (!root) {
#ifdef ROOTDIR
	    root = ROOTDIR;
#else
	    root = ".";
#endif
	}
    }

    resdir = getpathbuffer();
    strcpy(resdir, root);

    seriesdir = getpathbuffer();
    strcpy(seriesdir, root);
    combinepath(seriesdir, "data");

    savedir = getpathbuffer();
    if (!save) {
#ifdef SAVEDIR
	save = SAVEDIR;
#else
	if ((dir = getenv("HOME")) && *dir && strlen(dir) < maxpath - 8) {
	    strcpy(savedir, dir);
	    combinepath(savedir, ".tworld");
	} else {
	    strcpy(savedir, root);
	    combinepath(savedir, "save");
	}
#endif
    } else {
	strcpy(savedir, save);
    }
}

/* Parse the command-line options and arguments, and initialize the
 * user-controlled options.
 */
static int initoptionswithcmdline(int argc, char *argv[], startupdata *start)
{
    cmdlineinfo	opts;
    char const *optrootdir = NULL;
    char const *optsavedir = NULL;
    int		ch, n;

    start->filename = getpathbuffer();
    *start->filename = '\0';
    start->levelnum = 0;
    start->listseries = FALSE;
    start->listscores = FALSE;

    initoptions(&opts, argc - 1, argv + 1, "D:S:hlsv");
    while ((ch = readoption(&opts)) >= 0) {
	switch (ch) {
	  case 0:
	    if (*start->filename && start->levelnum) {
		fprintf(stderr, "too many arguments: %s\n", opts.val);
		fputs(yowzitch, stderr);
		exit(EXIT_FAILURE);
	    }
	    if (sscanf(opts.val, "%d", &n) == 1)
		start->levelnum = n;
	    else
		strncpy(start->filename, opts.val, getpathbufferlen() - 1);
	    break;
	  case 'D':	optrootdir = opts.val;				break;
	  case 'S':	optsavedir = opts.val;				break;
	  case 'l':	start->listseries = TRUE;			break;
	  case 's':	start->listscores = TRUE;			break;
	  case 'h':	fputs(yowzitch, stdout); 	   exit(EXIT_SUCCESS);
	  case 'v':	fputs(vourzhon, stdout); 	   exit(EXIT_SUCCESS);
	  case ':':
	    fprintf(stderr, "option requires an argument: -%c\n%s",
			    opts.opt, yowzitch);
	    return FALSE;
	  case '?':
	    fprintf(stderr, "unrecognized option: -%c\n%s",
			    opts.opt, yowzitch);
	    return FALSE;
	  default:
	    fputs(yowzitch, stderr);
	    return FALSE;
	}
    }

    if ((start->listscores || start->levelnum) && !*start->filename)
	strcpy(start->filename, "chips.dat");
    start->filename[getpathbufferlen() - 1] = '\0';

    initdirs(optrootdir, optsavedir);

    return TRUE;
}

/* Initialize the other modules.
 */
static int initializesystem(void)
{
    if (!oshwinitialize())
	return FALSE;
    setsubtitle(NULL);
    setkeyboardrepeat(TRUE);
    return TRUE;
}

/* Initialize the program. The list of available data files is drawn
 * up; if only one is found, it is selected automatically. Otherwise
 * the list is presented to the user, and their selection determines
 * which one is used. printseriesandquit causes the list of data files
 * to be displayed on stdout before initializing graphic output.
 * printscoresandquit causes the scores for a single data file to be
 * displayed on stdout before initializing graphic output (which
 * requires that only one data file is considered for selection).
 * Once a data file is selected, the first level for which the user
 * has no saved solution is found and selected, or the first level if
 * the user has solved every level.
 */
static int startup(gamespec *gs, startupdata const *start)
{
    gameseries *serieslist;
    char      **texts;
    char const *header;
    int		listsize, n;

    if (!createserieslist(start->filename, &serieslist,
			  &texts, &listsize, &header))
	return FALSE;
    if (listsize < 1)
	return FALSE;

    if (start->listseries) {
	puts(header);
	for (n = 0 ; n < listsize ; ++n)
	    puts(texts[n]);
	exit(EXIT_SUCCESS);
    }

    if (listsize == 1) {
	gs->series = serieslist[0];
	if (!readseriesfile(&gs->series))
	    return FALSE;
	if (start->listscores) {
	    freeserieslist(texts, listsize);
	    if (!createscorelist(&gs->series, &texts, &listsize, &header))
		return FALSE;
	    puts(header);
	    for (n = 0 ; n < listsize ; ++n)
		puts(texts[n]);
	    exit(EXIT_SUCCESS);
	}
	if (!initializesystem())
	    return FALSE;
    } else {
	if (!initializesystem())
	    return FALSE;
	n = 0;
	if (!displaylist("Welcome to Tile World. Select your destination.",
			 header, (char const**)texts, listsize, &n,
			 scrollinputcallback))
	    exit(EXIT_SUCCESS);
	if (n < 0 || n >= listsize)
	    return FALSE;
	gs->series = serieslist[n];
	if (!readseriesfile(&gs->series))
	    return FALSE;
    }

    freeserieslist(texts, listsize);
    free(serieslist);

    if (start->levelnum > 0 && start->levelnum <= gs->series.total) {
	gs->currentgame = start->levelnum - 1;
    } else {
	gs->currentgame = 0;
	for (n = 0 ; n < gs->series.total ; ++n) {
	    if (!hassolution(gs->series.games + n)) {
		gs->currentgame = n;
		break;
	    }
	}
    }
    gs->playback = FALSE;
    return TRUE;
}

/*
 * main().
 */

/* Initialize the system and enter an infinite loop of displaying a
 * playing the selected level.
 */
int main(int argc, char *argv[])
{
    startupdata	start;
    gamespec	spec;

    if (!initoptionswithcmdline(argc, argv, &start))
	return EXIT_FAILURE;

    if (!startup(&spec, &start))
	return EXIT_FAILURE;

    for (;;) {
	if (spec.currentgame < 0) {
	    spec.currentgame = 0;
	    ding();
	}
	if (spec.currentgame >= spec.series.total) {
	    spec.currentgame = spec.series.total - 1;
	    ding();
	}
	if (spec.playback
		&& !hassolution(spec.series.games + spec.currentgame)) {
	    spec.playback = FALSE;
	    ding();
	}

	spec.invalid = !initgamestate(&spec.series, spec.currentgame,
						    spec.playback);
	setsubtitle(spec.series.games[spec.currentgame].name);
	if (spec.invalid)
	    noplaygame(&spec);
	else if (spec.playback)
	    playbackgame(&spec);
	else
	    playgame(&spec);
	endgamestate();
    }

    return EXIT_FAILURE;
}