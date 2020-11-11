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

// // canned filetype extensions
// char *C_HL_extensions[] = {".c", ".h", ".cpp", ".C", ".H", ".CPP", NULL};
// char *Pascal_HL_extensions[] = {".pas", ".pp", ".PAS", ".PP", NULL};
// char *Python_HL_extensions[] = {".py", NULL};
// char *Markdown_HL_extensions[] = {".md", ".MD", NULL};
// char *Text_HL_extensions[] = {".txt", ".TXT", NULL};
// //
// // keywords originally supported two tiers of keywords. For C it was broken
// // down by keywords and common types. types were originally flagged by a suffix
// // pipe symbol, but that causes problems once operators are added to the keyword
// // lists. Now a suffix of \xff is used. Still to come is a third tier for
// // operators.
// //
// // All the highlight chunking needs to work from larger to smaller chunks,
// // so the larger supersedes the smaller. For example, comments wrapping
// // code should suppress highlighting of interior elements.
// //
// // Also, tokens with the same prefix should work from longer to smaller.
// // If not, ! comes before != and this leaves the = not highlighted.
// //
// // At load time, the keyword tables are sorted alphabetically and then
// // a fixup pass is done so that the cases like ! != are reversed.
// //
// // TODO: preprocessor directives start a line, ... test for this.
// // TODO: currently preprocessor directs are entered both with and
// // without the leading # due to a bug in separator handling.
// char *C_HL_Keywords[] = {"switch",     "if",       "while",
//                          "for",        "break",    "continue",
//                          "return",     "else",     "struct",
//                          "union",      "typedef",  "static",
//                          "enum",       "class",    "case",
//                          "include",    "define",   "NULL",
//                          "#include",   "#define",  "ifdef",
//                          "#ifdef",     "#then",    "#",
//                          "namespace",  "!",        "!=",
//                          "=",          "<",        ">",
//                          "->",         "<<",       ">>",
//                          "==",         "&&",       "|",
//                          "||",         "|=",       "&",
//                          "&=",
// 
//                          "int\xff",    "long\xff", "double\xff",
//                          "float\xff",  "char\xff", "unsigned\xff",
//                          "signed\xff", "void\xff", NULL};
// 
// char *Pascal_HL_Keywords[] = {
//     "begin",       "end",        "if",         "then",           "else",
//     "goto",        "while",      "do",         "until",          "program",
//     "type",        "const",      "var",        "procedure",      "function",
//     "repeat",      "for",        "to",         "downto",         "unit",
//     "uses",        "with",       "interface",  "implementation", "in",
//     "constructor", "destructor", "nil",        "exit",
// 
//     "array\xff",   "file\xff",   "object\xff", "packed\xff",     "label\xff",
//     "record\xff",  "set\xff",    "string\xff", "type\xff",       "integer\xff",
//     "float\xff",   "double\xff", "real\xff",   "char\xff",       NULL};
// 
// char *Python_HL_Keywords[] = {
//     "and",     "as",        "assert",      "break",    "class",     "continue",
//     "def",     "del",       "elif",        "else",     "except",    "False",
//     "finally", "for",       "from",        "global",   "if",        "import",
//     "in",      "is",        "lambda",      "None",     "nonlocal",  "not",
//     "or",      "pass",      "raise",       "return",   "True",      "try",
//     "while",   "with",      "yield",
// 
//     "int\xff", "float\xff", "complex\xff", "list\xff", "tuple\xff", "range\xff",
//     "str\xff", NULL};
// 
// char *Markdown_HL_Keywords[] = {NULL};
// 
// char *Text_HL_Keywords[] = {NULL};
// 
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

// // clang-format off
// // syntax highlighting definitions
// struct editorSyntax HLDB[] = {
// 
//     {"C",
//     C_HL_extensions,
//     C_HL_Keywords,
//     0,
//     1,
//     "//",
//     "/*",
//     "*/",
//     HL_HIGHLIGHT_NUMBERS
//     | HL_HIGHLIGHT_STRINGS
//     | HL_HIGHLIGHT_COMMENT
//     | HL_HIGHLIGHT_KEYWORDS},
// 
//     {"Pascal",
//     Pascal_HL_extensions,
//     Pascal_HL_Keywords,
//     0,
//     0,
//     "//",
//     "{",
//     "}",
//     HL_HIGHLIGHT_NUMBERS
//     | HL_HIGHLIGHT_STRINGS
//     | HL_HIGHLIGHT_COMMENT
//     | HL_HIGHLIGHT_KEYWORDS},
// 
//     {"Python",
//     Python_HL_extensions,
//     Python_HL_Keywords,
//     0,
//     1,
//     "#",
//     NULL,
//     NULL,
//     HL_HIGHLIGHT_NUMBERS
//     | HL_HIGHLIGHT_STRINGS
//     | HL_HIGHLIGHT_COMMENT
//     | HL_HIGHLIGHT_KEYWORDS},
// 
//     {"Markdown",
//     Markdown_HL_extensions,
//     Markdown_HL_Keywords,
//     0,
//     0,
//     NULL,
//     NULL,
//     NULL,
//     0},
// 
//     {"Text",
//     Text_HL_extensions,
//     Text_HL_Keywords,
//     0,
//     0,
//     NULL,
//     NULL,
//     NULL,
//     HL_HIGHLIGHT_NUMBERS
//     | HL_HIGHLIGHT_PUNCTUATION}
//   };
// 
// #define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
// // clang-format on

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

struct editorConfig E;

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }


////////////////////////////////
// prototypes for foward references
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void die(const char *s);

#endif // !FILE_TVI_H_SEEN
