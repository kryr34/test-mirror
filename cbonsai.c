#include <stdlib.h>
#include <ncurses.h>
#include <panel.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

enum branchType {trunk, shootLeft, shootRight, dying, dead};

struct config {
	int live;
	int infinite;
	int screensaver;
	int printTree;
	int verbosity;
	int lifeStart;
	int multiplier;
	int baseType;
	int seed;
	int leavesSize;
	int save;
	int load;
	int targetBranchCount;

	double timeWait;
	double timeStep;

	char* message;
	char* leaves[64];
	char* saveFile;
	char* loadFile;
};

struct ncursesObjects {
	WINDOW* baseWin;
	WINDOW* treeWin;
	WINDOW* messageBorderWin;
	WINDOW* messageWin;

	PANEL* basePanel;
	PANEL* treePanel;
	PANEL* messageBorderPanel;
	PANEL* messagePanel;
};

struct counters {
	int branches;
	int shoots;
	int shootCounter;
	time_t timer;
};

void finish(void) {
	clear();
	refresh();
	endwin();	// delete ncurses screen
	curs_set(1);
}

void printHelp(const struct config *conf) {
	printf("Usage: cbonsai [OPTION]...\n");
	printf("\n");
	printf("cbonsai is a beautifully random bonsai tree generator.\n");
	printf("\n");
	printf("Options:\n");
	printf("  -l, --live             live mode: show each step of growth\n");
	printf("  -t, --time=TIME        in live mode, wait TIME secs between\n");
	printf("                           steps of growth (must be larger than 0) [default: %.2f]\n", conf->timeStep);
	printf("  -i, --infinite         infinite mode: keep growing trees\n");
	printf("  -w, --wait=TIME        in infinite mode, wait TIME between each tree\n");
	printf("                           generation [default: %.2f]\n", conf->timeWait);
	printf("  -S, --screensaver      screensaver mode; equivalent to -li and\n");
	printf("                           quit on any keypress\n");
	printf("  -m, --message=STR      attach message next to the tree\n");
	printf("  -b, --base=INT         ascii-art plant base to use, 0 is none\n");
	printf("  -c, --leaf=LIST        list of comma-delimited strings randomly chosen\n");
	printf("                           for leaves\n");
	printf("  -M, --multiplier=INT   branch multiplier; higher -> more\n");
	printf("                           branching (0-20) [default: %i]\n", conf->multiplier);
	printf("  -L, --life=INT         life; higher -> more growth (0-200) [default: %i]\n", conf->lifeStart);
	printf("  -p, --print            print tree to terminal when finished\n");
	printf("  -s, --seed=INT         seed random number generator\n");
	printf("  -C, --continue=STR     load progress from file [default: %s]\n", conf->loadFile);
	printf("  -W, --save=STR         save progress to file [default: %s]\n", conf->saveFile);
	printf("  -v, --verbose          increase output verbosity\n");
	printf("  -h, --help             show help	\n");
}

void drawBase(WINDOW* baseWin, int baseType) {
	// draw base art
	switch(baseType) {
	case 1:
		wattron(baseWin, A_BOLD | COLOR_PAIR(8));
		wprintw(baseWin, "%s", ":");
		wattron(baseWin, COLOR_PAIR(2));
		wprintw(baseWin, "%s", "___________");
		wattron(baseWin, COLOR_PAIR(11));
		wprintw(baseWin, "%s", "./~~~\\.");
		wattron(baseWin, COLOR_PAIR(2));
		wprintw(baseWin, "%s", "___________");
		wattron(baseWin, COLOR_PAIR(8));
		wprintw(baseWin, "%s", ":");

		mvwprintw(baseWin, 1, 0, "%s", " \\                           / ");
		mvwprintw(baseWin, 2, 0, "%s", "  \\_________________________/ ");
		mvwprintw(baseWin, 3, 0, "%s", "  (_)                     (_)");

		wattroff(baseWin, A_BOLD);
		break;
	case 2:
		wattron(baseWin, COLOR_PAIR(8));
		wprintw(baseWin, "%s", "(");
		wattron(baseWin, COLOR_PAIR(2));
		wprintw(baseWin, "%s", "---");
		wattron(baseWin, COLOR_PAIR(11));
		wprintw(baseWin, "%s", "./~~~\\.");
		wattron(baseWin, COLOR_PAIR(2));
		wprintw(baseWin, "%s", "---");
		wattron(baseWin, COLOR_PAIR(8));
		wprintw(baseWin, "%s", ")");

		mvwprintw(baseWin, 1, 0, "%s", " (           ) ");
		mvwprintw(baseWin, 2, 0, "%s", "  (_________)  ");
		break;
	}
}

void drawWins(int baseType, struct ncursesObjects *objects) {
	int baseWidth = 0;
	int baseHeight = 0;
	int rows, cols;

	switch(baseType) {
	case 1:
		baseWidth = 31;
		baseHeight = 4;
		break;
	case 2:
		baseWidth = 15;
		baseHeight = 3;
		break;
	}

	// calculate where base should go
	getmaxyx(stdscr, rows, cols);
	int baseOriginY = (rows - baseHeight);
	int baseOriginX = (cols / 2) - (baseWidth / 2);

	// create windows
	objects->baseWin = newwin(baseHeight, baseWidth, baseOriginY, baseOriginX);
	objects->treeWin = newwin(rows - baseHeight, cols, 0, 0);

	// create/replace tree and base panels
	if (objects->basePanel)
		replace_panel(objects->basePanel, objects->baseWin);
	else
		objects->basePanel = new_panel(objects->baseWin);

	if (objects->treePanel)
		replace_panel(objects->treePanel, objects->treeWin);
	else
		objects->treePanel = new_panel(objects->treeWin);

	drawBase(objects->baseWin, baseType);
}

// roll (randomize) a given die
void roll(int *dice, int mod) { *dice = rand() % mod; }

// check for key press
void checkKeyPress(int screensaver) {
	if ((screensaver && wgetch(stdscr) != ERR) || (wgetch(stdscr) == 'q')) {
		finish();
		exit(0);
	}
}

// display changes
void updateScreen(float timeStep) {
	update_panels();
	doupdate();

	// convert given time into seconds and nanoseconds and sleep
	struct timespec ts;
	ts.tv_sec = timeStep / 1;
	ts.tv_nsec = (timeStep - ts.tv_sec) * 1000000000;
	nanosleep(&ts, NULL);	// sleep for given time
}

void saveToFile(char* fname, int seed, int counter) {
	FILE *fp = fopen(fname, "w");
	fprintf(fp, "%d %d", seed, counter);
	fclose(fp);
}

// based on type of tree, determine what color a branch should be
void chooseColor(enum branchType type, WINDOW* treeWin) {
	switch(type) {
	case trunk:
	case shootLeft:
	case shootRight:
		if (rand() % 2 == 0) wattron(treeWin, A_BOLD | COLOR_PAIR(11));
		else wattron(treeWin, COLOR_PAIR(3));
		break;

	case dying:
		if (rand() % 10 == 0) wattron(treeWin, A_BOLD | COLOR_PAIR(2));
		else wattron(treeWin, COLOR_PAIR(2));
		break;

	case dead:
		if (rand() % 3 == 0) wattron(treeWin, A_BOLD | COLOR_PAIR(10));
		else wattron(treeWin, COLOR_PAIR(10));
		break;
	}
}

// determine change in X and Y coordinates of a given branch
void setDeltas(enum branchType type, int life, int age, int multiplier, int *returnDx, int *returnDy) {
	int dx = 0;
	int dy = 0;
	int dice;
	switch (type) {
	case trunk: // trunk

		// new or dead trunk
		if (age <= 2 || life < 4) {
			dy = 0;
			dx = (rand() % 3) - 1;
		}
		// young trunk should grow wide
		else if (age < (multiplier * 3)) {

			// every (multiplier * 0.8) steps, raise tree to next level
			if (age % (int) (multiplier * 0.5) == 0) dy = -1;
			else dy = 0;

			roll(&dice, 10);
			if (dice >= 0 && dice <=0) dx = -2;
			else if (dice >= 1 && dice <= 3) dx = -1;
			else if (dice >= 4 && dice <= 5) dx = 0;
			else if (dice >= 6 && dice <= 8) dx = 1;
			else if (dice >= 9 && dice <= 9) dx = 2;
		}
		// middle-aged trunk
		else {
			roll(&dice, 10);
			if (dice > 2) dy = -1;
			else dy = 0;
			dx = (rand() % 3) - 1;
		}
		break;

	case 1: // left shoot: trend left and little vertical movement
		roll(&dice, 10);
		if (dice >= 0 && dice <= 1) dy = -1;
		else if (dice >= 2 && dice <= 7) dy = 0;
		else if (dice >= 8 && dice <= 9) dy = 1;

		roll(&dice, 10);
		if (dice >= 0 && dice <=1) dx = -2;
		else if (dice >= 2 && dice <= 5) dx = -1;
		else if (dice >= 6 && dice <= 8) dx = 0;
		else if (dice >= 9 && dice <= 9) dx = 1;
		break;

	case 2: // right shoot: trend right and little vertical movement
		roll(&dice, 10);
		if (dice >= 0 && dice <= 1) dy = -1;
		else if (dice >= 2 && dice <= 7) dy = 0;
		else if (dice >= 8 && dice <= 9) dy = 1;

		roll(&dice, 10);
		if (dice >= 0 && dice <=1) dx = 2;
		else if (dice >= 2 && dice <= 5) dx = 1;
		else if (dice >= 6 && dice <= 8) dx = 0;
		else if (dice >= 9 && dice <= 9) dx = -1;
		break;

	case 3: // dying: discourage vertical growth(?); trend left/right (-3,3)
		roll(&dice, 10);
		if (dice >= 0 && dice <=1) dy = -1;
		else if (dice >= 2 && dice <=8) dy = 0;
		else if (dice >= 9 && dice <=9) dy = 1;

		roll(&dice, 15);
		if (dice >= 0 && dice <=0) dx = -3;
		else if (dice >= 1 && dice <= 2) dx = -2;
		else if (dice >= 3 && dice <= 5) dx = -1;
		else if (dice >= 6 && dice <= 8) dx = 0;
		else if (dice >= 9 && dice <= 11) dx = 1;
		else if (dice >= 12 && dice <= 13) dx = 2;
		else if (dice >= 14 && dice <= 14) dx = 3;
		break;

	case 4: // dead: fill in surrounding area
		roll(&dice, 10);
		if (dice >= 0 && dice <= 2) dy = -1;
		else if (dice >= 3 && dice <= 6) dy = 0;
		else if (dice >= 7 && dice <= 9) dy = 1;
		dx = (rand() % 3) - 1;
		break;
	}

	*returnDx = dx;
	*returnDy = dy;
}

char* chooseString(const struct config *conf, enum branchType type, int life, int dx, int dy) {
	char* branchStr;

	branchStr = malloc(32 * sizeof(char));
	strcpy(branchStr, "?");	// fallback character

	if (life < 4) type = dying;

	switch(type) {
	case trunk:
		if (dy == 0) strcpy(branchStr, "/~");
		else if (dx < 0) strcpy(branchStr, "\\|");
		else if (dx == 0) strcpy(branchStr, "/|\\");
		else if (dx > 0) strcpy(branchStr, "|/");
		break;
	case shootLeft:
		if (dy > 0) strcpy(branchStr, "\\");
		else if (dy == 0) strcpy(branchStr, "\\_");
		else if (dx < 0) strcpy(branchStr, "\\|");
		else if (dx == 0) strcpy(branchStr, "/|");
		else if (dx > 0) strcpy(branchStr, "/");
		break;
	case shootRight:
		if (dy > 0) strcpy(branchStr, "/");
		else if (dy == 0) strcpy(branchStr, "_/");
		else if (dx < 0) strcpy(branchStr, "\\|");
		else if (dx == 0) strcpy(branchStr, "/|");
		else if (dx > 0) strcpy(branchStr, "/");
		break;
	case dying:
	case dead:
		strncpy(branchStr, conf->leaves[rand() % conf->leavesSize], sizeof(branchStr) - 1);
		branchStr[sizeof(branchStr) - 1] = '\0';
	}

	return branchStr;
}

void branch(const struct config *conf, struct ncursesObjects *objects, struct counters *myCounters, int y, int x, enum branchType type, int life) {
	myCounters->branches++;
	int dx = 0;
	int dy = 0;
	int age = 0;
	int shootCooldown = conf->multiplier;

	while (life > 0) {
		checkKeyPress(conf->screensaver);
		life--;		// decrement remaining life counter
		age = conf->lifeStart - life;

		setDeltas(type, life, age, conf->multiplier, &dx, &dy);

		int maxY = getmaxy(objects->treeWin);
		if (dy > 0 && y > (maxY - 2)) dy--; // reduce dy if too close to the ground

		// near-dead branch should branch into a lot of leaves
		if (life < 3)
			branch(conf, objects, myCounters, y, x, dead, life);

		// dying trunk should branch into a lot of leaves
		else if (type == 0 && life < (conf->multiplier + 2))
			branch(conf, objects, myCounters, y, x, dying, life);

		// dying shoot should branch into a lot of leaves
		else if ((type == shootLeft || type == shootRight) && life < (conf->multiplier + 2))
			branch(conf, objects, myCounters, y, x, dying, life);

		// trunks should re-branch if not close to ground AND either randomly, or upon every <multiplier> steps
		/* else if (type == 0 && ( \ */
		/* 		(rand() % (conf.multiplier)) == 0 || \ */
		/* 		(life > conf.multiplier && life % conf.multiplier == 0) */
		/* 		) ) { */
		else if (type == trunk && (((rand() % 3) == 0) || (life % conf->multiplier == 0))) {

			// if trunk is branching and not about to die, create another trunk with random life
			if ((rand() % 8 == 0) && life > 7) {
				shootCooldown = conf->multiplier * 2;	// reset shoot cooldown
				branch(conf, objects, myCounters, y, x, trunk, life + (rand() % 5 - 2));
			}

			// otherwise create a shoot
			else if (shootCooldown <= 0) {
				shootCooldown = conf->multiplier * 2;	// reset shoot cooldown

				int shootLife = (life + conf->multiplier);

				// first shoot is randomly directed
				myCounters->shoots++;
				myCounters->shootCounter++;
				if (conf->verbosity) mvwprintw(objects->treeWin, 4, 5, "shoots: %02d", myCounters->shoots);

				// create shoot
				branch(conf, objects, myCounters, y, x, (myCounters->shootCounter % 2) + 1, shootLife);
			}
		}
		shootCooldown--;

		if (conf->verbosity > 0) {
			mvwprintw(objects->treeWin, 5, 5, "dx: %02d", dx);
			mvwprintw(objects->treeWin, 6, 5, "dy: %02d", dy);
			mvwprintw(objects->treeWin, 7, 5, "type: %d", type);
			mvwprintw(objects->treeWin, 8, 5, "shootCooldown: % 3d", shootCooldown);
		}

		// move in x and y directions
		x += dx;
		y += dy;

		chooseColor(type, objects->treeWin);

		// choose string to use for this branch
		char *branchStr = chooseString(conf, type, life, dx, dy);

		mvwprintw(objects->treeWin, y, x, "%s", branchStr);
		wattroff(objects->treeWin, A_BOLD);
		free(branchStr);

		// if live, show progress
		if (conf->live) {
			if (conf->targetBranchCount > myCounters->branches) {
				// we have not yet cought up to loaded state
				// fast forward
				updateScreen(0.001);
			} else if ((time(NULL) - myCounters->timer) > 10) {
				saveToFile(conf->saveFile, conf->seed, myCounters->branches);
				myCounters->timer = time(NULL);
				updateScreen(conf->timeStep);
			} else {
				updateScreen(conf->timeStep);
			}
		}
	}
}

void addSpaces(WINDOW* messageWin, int count, int *linePosition, int maxWidth) {
	// add spaces if there's enough space
	if (*linePosition < (maxWidth - count)) {
		/* if (verbosity) mvwprintw(treeWin, 12, 5, "inserting a space: linePosition: %02d", *linePosition); */

		// add spaces up to width
		for (int j = 0; j < count; j++) {
			wprintw(messageWin, "%s", " ");
			(*linePosition)++;
		}
	}
}

// create ncurses windows to contain message and message box
void createMessageWindows(struct ncursesObjects *objects, char* message) {
	int maxY, maxX;
	getmaxyx(stdscr, maxY, maxX);

	int boxWidth = 0;
	int boxHeight = 0;

	if (strlen(message) + 3 <= (0.25 * maxX)) {
		boxWidth = strlen(message) + 1;
		boxHeight = 1;
	} else {
		boxWidth = 0.25 * maxX;
		boxHeight = (strlen(message) / boxWidth) + (strlen(message) / boxWidth);
	}

	// create separate box for message border
	objects->messageBorderWin = newwin(boxHeight + 2, boxWidth + 4, (maxY * 0.7) - 1, (maxX * 0.7) - 2);
	objects->messageWin = newwin(boxHeight, boxWidth + 1, maxY * 0.7, maxX * 0.7);

	// draw box
	wattron(objects->messageBorderWin, COLOR_PAIR(8) | A_BOLD);
	wborder(objects->messageBorderWin, '|', '|', '-', '-', '+', '+', '+', '+');

	// create/replace message panels
	if (objects->messageBorderPanel)
		replace_panel(objects->messageBorderPanel, objects->messageBorderWin);
	else
		objects->messageBorderPanel = new_panel(objects->messageBorderWin);

	if (objects->messagePanel)
		replace_panel(objects->messagePanel, objects->messageWin);
	else
		objects->messagePanel = new_panel(objects->messageWin);
}

int drawMessage(const struct config *conf, struct ncursesObjects *objects, char* message) {
	if (!message) return 1;

	createMessageWindows(objects, message);

	int maxWidth = getmaxx(objects->messageWin) - 2;

	// word wrap message as it is written
	unsigned int i = 0;
	int linePosition = 0;
	int wordLength = 0;
	char wordBuffer[512] = {'\0'};
	char thisChar;
	while (true) {
		thisChar = message[i];
		if (conf->verbosity) {
			mvwprintw(objects->treeWin, 9, 5, "index: %03d", i);
			mvwprintw(objects->treeWin, 10, 5, "linePosition: %02d", linePosition);
		}

		// append this character to word buffer,
		// if it's not space or NULL and it can fit
		if (!(isspace(thisChar) || thisChar == '\0') && wordLength < (int) (sizeof(wordBuffer) / sizeof(wordBuffer[0]))) {
			strncat(wordBuffer, &thisChar, 1);
			wordLength++;
			linePosition++;
		}

		// if char is space or null char
		else if (isspace(thisChar) || thisChar == '\0') {

			// if current line can fit word, add word to current line
			if (linePosition <= maxWidth) {
				wprintw(objects->messageWin, "%s", wordBuffer);	// print word
				wordLength = 0;		// reset word length
				wordBuffer[0] = '\0';	// clear word buffer

				switch (thisChar) {
				case ' ':
					addSpaces(objects->messageWin, 1, &linePosition, maxWidth);
					break;
				case '\t':
					addSpaces(objects->messageWin, 1, &linePosition, maxWidth);
					break;
				case '\n':
					waddch(objects->messageWin, thisChar);
					linePosition = 0;
					break;
				}

			}

			// if word can't fit within a single line, just print it
			else if (wordLength > maxWidth) {
				wprintw(objects->messageWin, "%s ", wordBuffer);	// print word
				wordLength = 0;		// reset word length
				wordBuffer[0] = '\0';	// clear word buffer

				// our line position on this new line is the x coordinate
				int y;
				(void) y;
				getyx(objects->messageWin, y, linePosition);
			}

			// if current line can't fit word, go to next line
			else {
				if (conf->verbosity) mvwprintw(objects->treeWin, (i / 24) + 28, 5, "couldn't fit word. linePosition: %02d, wordLength: %02d", linePosition, wordLength);
				wprintw(objects->messageWin, "\n%s ", wordBuffer); // print newline, then word
				linePosition = wordLength;	// reset line position
				wordLength = 0;		// reset word length
				wordBuffer[0] = '\0';	// clear word buffer
			}
		}
		else {
			printf("%s", "Error while parsing message");
			return 1;
		}

		if (conf->verbosity >= 2) {
			updateScreen(1);
			mvwprintw(objects->treeWin, 11, 5, "word buffer: |% 15s|", wordBuffer);
		}
		if (thisChar == '\0') break;	// quit when we reach the end of the message
		i++;
	}
	return 0;
}

void init(const struct config *conf, struct ncursesObjects *objects) {
	savetty();	// save terminal settings
	initscr();	// init ncurses screen
	noecho();	// don't echo input to screen
	curs_set(0);	// make cursor invisible
	cbreak();	// don't wait for new line to grab user input
	nodelay(stdscr, TRUE);	// force getch to be a non-blocking call

	// if terminal has color capabilities, use them
	if (has_colors()) {
		start_color();

		// use native background color when possible
		int bg = COLOR_BLACK;
		if (use_default_colors() != ERR) bg = -1;

		// define color pairs
		for(int i=0; i<16; i++){
			init_pair(i, i, bg);
		}

		// restrict color pallete in non-256color terminals (e.g. screen or linux)
		if (COLORS < 256) {
			init_pair(8, 7, bg);	// gray will look white
			init_pair(9, 1, bg);
			init_pair(10, 2, bg);
			init_pair(11, 3, bg);
			init_pair(12, 4, bg);
			init_pair(13, 5, bg);
			init_pair(14, 6, bg);
			init_pair(15, 7, bg);
		}
	} else {
		printf("%s", "Warning: terminal does not have color support.\n");
	}

	// define and draw windows, then create panels
	drawWins(conf->baseType, objects);
	drawMessage(conf, objects, conf->message);
}

void growTree(const struct config *conf, struct ncursesObjects *objects) {
	int maxY, maxX;
	getmaxyx(objects->treeWin, maxY, maxX);

	struct counters myCounters  = { 0, 0, rand(), time(NULL) };

	if (conf->verbosity > 0) {
		mvwprintw(objects->treeWin, 2, 5, "maxX: %03d, maxY: %03d", maxX, maxY);
	}

	// recursively grow tree trunk and branches
	branch(conf, objects, &myCounters, maxY - 1, (maxX / 2), trunk, conf->lifeStart);

	// display changes
	update_panels();
	doupdate();
}

// print stdscr to terminal window
void printstdscr(void) {
	int maxY, maxX, color, attribs;
	getmaxyx(stdscr, maxY, maxX);

	// loop through each character on stdscr
	for (int y = 0; y < maxY; y++) {
		for (int x = 0; x < maxX; x++) {
			// get attributes of this character
			color = mvwinch(stdscr, y, x) & A_COLOR;
			attribs = (mvwinch(stdscr, y, x) & A_ATTRIBUTES) - color;
			color /= 256;

			// enable bold if needed
			if ((attribs) == A_BOLD) printf("%s", "\033[1m");
			else printf("%s", "\033[0m");

			// enable correct color
			if (color == 0) printf("%s", "\033[0m");
			else if (color <= 7) printf("\033[3%im", color);
			else if (color >= 8) printf("\033[9%im", color - 8);

			// print character
			// mvwinch returns chtype which depends on machine, so we type cast
			printf("%c", (char) mvwinch(stdscr, y, x));
		}
	}
	printf("%s\n", "\033[0m");
}

// find homedir for default file location
void getFilePath(char **path) {
	char* postfix = "/.cache/bonsai";
	char* homedir = getenv("HOME");

	if (homedir == NULL) {
		homedir = getpwuid(getuid())->pw_dir;
	}
	if (homedir == NULL) {
		printf("error: unable to find homedir. Set save path manually\n");
		exit(1);
	}

	// fit paths plus "/" and null byte
	size_t n = strlen(homedir) + strlen(postfix) + sizeof(char);
	*path = (char *) malloc(n);
	snprintf(*path, n, "%s%s", homedir, postfix);
}

// load seed and counter from file
void loadFromFile(struct config *conf) {
	FILE* fp = fopen(conf->loadFile, "r");
	char seed[1024];	// buffer for seed
	char state[1024];	// buffer for state counter

	if (access(conf->loadFile, F_OK) != 0 ) {
		printf("State file not found. %s\n", conf->loadFile);
		return;
	}
	if (fscanf(fp, "%1023s %1023s", seed, state) != 2) {
		printf("Error in save file format\n");
		exit(1);
	}

	conf->seed = strtod(seed, NULL);
	conf->targetBranchCount = strtod(state, NULL);

	fclose(fp);
}

int main(int argc, char* argv[]) {
	struct config conf = {
		.live = 0,
		.infinite = 0,
		.screensaver = 0,
		.printTree = 0,
		.verbosity = 0,
		.lifeStart = 32,
		.multiplier = 5,
		.baseType = 1,
		.seed = 0,
		.leavesSize = 0,
		.save = 0,
		.load = 0,
		.targetBranchCount = 0,

		.timeWait = 4,
		.timeStep = 0.03,

		.message = NULL,
		.leaves = {0},
		.saveFile = "~/.cache/cbonsai",
		.loadFile = "~/.cache/cbonsai",
	};

	struct option long_options[] = {
		{"live", no_argument, NULL, 'l'},
		{"time", required_argument, NULL, 't'},
		{"infinite", no_argument, NULL, 'i'},
		{"wait", required_argument, NULL, 'w'},
		{"screensaver", no_argument, NULL, 'S'},
		{"message", required_argument, NULL, 'm'},
		{"base", required_argument, NULL, 'b'},
		{"leaf", required_argument, NULL, 'c'},
		{"multiplier", required_argument, NULL, 'M'},
		{"life", required_argument, NULL, 'L'},
		{"print", required_argument, NULL, 'p'},
		{"seed", required_argument, NULL, 's'},
		{"continue", optional_argument, NULL, 'C'},
		{"save", optional_argument, NULL, 'W'},
		{"verbose", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};

	struct ncursesObjects objects = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

	char leavesInput[128] = "&";

	// parse arguments
	int option_index = 0;
	int c;
	while ((c = getopt_long(argc, argv, "lt:iw:Sm:b:c:M:L:ps:C::W::vh", long_options, &option_index)) != -1) {
		switch (c) {
		case 'l':
			conf.live = 1;
			break;
		case 't':
			if (strtold(optarg, NULL) != 0) conf.timeStep = strtod(optarg, NULL);
			else {
				printf("error: invalid step time: '%s'\n", optarg);
				exit(1);
			}
			if (conf.timeStep < 0) {
				printf("error: invalid step time: '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'i':
			conf.infinite = 1;
			break;
		case 'w':
			if (strtold(optarg, NULL) != 0) conf.timeWait = strtod(optarg, NULL);
			else {
				printf("error: invalid wait time: '%s'\n", optarg);
				exit(1);
			}
			if (conf.timeWait < 0) {
				printf("error: invalid wait time: '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'S':
			conf.live = 1;
			conf.infinite = 1;
			conf.screensaver = 1;
			break;
		case 'm':
			conf.message = optarg;
			break;
		case 'b':
			if (strtold(optarg, NULL) != 0) conf.baseType = strtod(optarg, NULL);
			else {
				printf("error: invalid base index: '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'c':
			strncpy(leavesInput, optarg, sizeof(leavesInput) - 1);
			leavesInput[sizeof(leavesInput) - 1] = '\0';
			break;
		case 'M':
			if (strtold(optarg, NULL) != 0) conf.multiplier = strtod(optarg, NULL);
			else {
				printf("error: invalid multiplier: '%s'\n", optarg);
				exit(1);
			}
			if (conf.multiplier < 0) {
				printf("error: invalid multiplier: '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'L':
			if (strtold(optarg, NULL) != 0) conf.lifeStart = strtod(optarg, NULL);
			else {
				printf("error: invalid initial life: '%s'\n", optarg);
				exit(1);
			}
			if (conf.lifeStart < 0) {
				printf("error: invalid initial life: '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'p':
			conf.printTree = 1;
			break;
		case 's':
			if (strtold(optarg, NULL) != 0) conf.seed = strtod(optarg, NULL);
			else {
				printf("error: invalid seed: '%s'\n", optarg);
				exit(1);
			}
			if (conf.seed < 0) {
				printf("error: invalid seed: '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'C':
			if (optarg != NULL) {
				conf.loadFile = optarg;
			} else {
			// build default path
				getFilePath(&conf.loadFile);
			}
			conf.load = 1;
			break;
		case 'W':
			if (optarg != NULL) {
				conf.saveFile = optarg;
			} else {
				// build default path
				getFilePath(&conf.saveFile);
			}
			conf.save = 1;
			break;
		case 'v':
			conf.verbosity++;
			break;

		// '?' represents unknown option. Treat it like --help.
		case '?':
		case 'h':
			printHelp(&conf);
			return 0;
			break;
		}
	}

	// delimit leaves on "," and add each token to the leaves[] list
	char *token = strtok(leavesInput, ",");
	while (token != NULL) {
		if (conf.leavesSize < 100) conf.leaves[conf.leavesSize] = token;
		token = strtok(NULL, ",");
		conf.leavesSize++;
	}

	if (conf.load) {
		loadFromFile(&conf);
	}

	// seed random number generator
	if (conf.seed == 0) conf.seed = time(NULL);
	srand(conf.seed);

	do {
		init(&conf, &objects);
		growTree(&conf, &objects);
		if (conf.infinite) {
			timeout(conf.timeWait * 1000);
			checkKeyPress(conf.screensaver);

			// seed random number generator
			srand(time(NULL));
		}
	} while (conf.infinite);

	if (conf.printTree) {
		finish();

		// overlay all windows onto stdscr
		overlay(objects.baseWin, stdscr);
		overlay(objects.treeWin, stdscr);
		overlay(objects.messageBorderWin, stdscr);
		overlay(objects.messageWin, stdscr);

		printstdscr();
	} else {
		wgetch(objects.treeWin);
		finish();
	}

	// free window memory
	delwin(objects.baseWin);
	delwin(objects.treeWin);
	delwin(objects.messageBorderWin);
	delwin(objects.messageWin);

	return 0;
}
