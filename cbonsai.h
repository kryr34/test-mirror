#ifndef CBONSAI_H
#define CBONSAI_H

#include <ncurses.h>
#include <panel.h>

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

	double timeWait;
	double timeStep;

	char* message;
	char* leavesInput;
	char* leaves[100];
};

void finish(void);
void printHelp(struct config conf);
void drawWins(int baseType, WINDOW* *baseWinPtr, WINDOW* *treeWinPtr);
void roll(int *dice, int mod);
void checkKeyPress(int screensaver);
void branch(struct config conf, int y, int x, int type, int life);
void addSpaces(int count, int *linePosition, int maxWidth);
int drawMessage(struct config conf);
void init(struct config conf);
void growTree(struct config conf);

#endif
