// vim: noai:et:ts=2:sw=2:sts=2
// tvi.c troi's smallish vimish editor
//
// clang-format off
//
// started from my pass through the tutorial
// at https://viewsourcecode.org/snaptoken/kilo/
// kilo originally by antirez http://antirez.com/news/108
// the original work is theirs, I'm just futzing around.
//
// kilo released under BSD 2-Clause license
// https://github.com/antirez/kilo/blob/master/LICENSE 
//
// tutorial released under
// CC BY 4.0 https://creativecommons.org/licenses/by/4.0/
//
// As I said, I'm just futzing around. Anything I've done here
// is free to use if you think it's useful. Use at your own
// risk and honor the original authors.
//
// Troy Brumley, October 2020.
//
// clang-format on

#ifndef FILE_TVI_H_SEEN
#define FILE_TVI_H_SEEN

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TVI_VERSION "0.0.1"
#define TVI_TAB_STOP 8
#define TVI_QUIT_TIMES 3

///////////////////////////////////////////////////////////
// modes
enum editorMode { EM_NORMAL, EM_VISUAL, EM_INSERT, EM_COMMAND };

///////////////////////////////////////////////////////////
// keyboard
#define CTRL_KEY(k) ((k)&0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

///////////////////////////////////////////////
// highlighting
enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_OPERATOR,
  HL_MATCH,
  HL_PUNCTUATION
};

#define HL_HIGHLIGHT_DONT (1 << 0)
#define HL_HIGHLIGHT_NUMBERS (1 << 1)
#define HL_HIGHLIGHT_STRINGS (1 << 2)
#define HL_HIGHLIGHT_COMMENT (1 << 3)
#define HL_HIGHLIGHT_KEYWORDS (1 << 4)
#define HL_HIGHLIGHT_OPERATORS (1 << 5)
#define HL_HIGHLIGHT_PUNCTUATION (1 << 6)

///////////////////////////////////////////////// 
// syntax highlighting declaration
struct editorSyntax {
  char *filetype;
  char **extensions;
  char **keywords;
  int keywordCount;
  int keywordsCaseSensitive;
  char *lineCommentStart;
  char *blockCommentStart;
  char *blockCommentEnd;
  int flags;
};

////////////////////////////////////////
// text and screen state
typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int highlighting;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct editorSyntax *syntax;
  struct termios orig_termios;
  int mode;
  int findForward;  // boolean search direction, true forward, false backward
  char *findString; // last used find string
};

extern struct editorConfig E;

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT { NULL, 0 }


////////////////////////////////
// prototypes for foward references
// TODO: some of these need renames
// TODO: and those not in tvi need to be moved
//       to the appropriate header
void initializeKeywordTables();
void editorSelectSyntaxHighlight();
int editorSyntaxToColor(int);
void editorUpdateSyntax(erow *row);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void die(const char *s);

#endif // !FILE_TVI_H_SEEN
