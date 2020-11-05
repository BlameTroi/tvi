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
// *** TODO: must add more formal attribution to snaptoken and
// ***       antirez.
//
// Ideas in no particular order:
//
// Code cleanup and formatting more to my preferences
//
// Along the way learn why the author made some choices, were they just
// illustrative (eg, sometimes unsigned int is used and I don't see
// why)
//
// DONE: Add more filetypes and highlighting
//       pascal and python
//
// Sort keyword highlight tables at initialization for a speedier
// highlighter, not that it seems to matter.
// I've thought about moving the tier indicator character from the
// back to front of the keywords, but I am not sure that will work
// well.
//
// Add third tier of highlighting to support operators
// so first tier is reserved or base language keywords,
// second tier is common types and constants, and
// third tier is operators and special characters
//
// Display line numbers
//
// Minimal auto indent
//
// Replace in addition to search
// (regex? I think not)
//
// Copy and Paste
//
// Modal as vim
//
// Load/save/insert file
//
// Smart wrapping for text editing
//
// Better status line
//
// Hex edit mode
//
// clang-format on

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

// canned filetype extensions
char *C_HL_extensions[] = {".c", ".h", ".cpp", ".C", ".H", ".CPP", NULL};
char *Pascal_HL_extensions[] = {".pas", ".pp", ".PAS", ".PP", NULL};
char *Python_HL_extensions[] = {".py", NULL};
char *Markdown_HL_extensions[] = {".md", ".MD", NULL};
char *Text_HL_extensions[] = {".txt", ".TXT", NULL};

// keywords originally supported two tiers of keywords. For C it was broken
// down by keywords and common types. types were originally flagged by a suffix
// pipe symbol, but that causes problems once operators are added to the keyword
// lists. Now a suffix of \xff is used. Still to come is a third tier for
// operators.
//
// All the highlight chunking needs to work from larger to smaller chunks,
// so the larger supersedes the smaller. For example, comments wrapping
// code should suppress highlighting of interior elements.
//
// Also, tokens with the same prefix should work from longer to smaller.
// If not, ! comes before != and this leaves the = not highlighted.
//
// At load time, the keyword tables are sorted alphabetically and then
// a fixup pass is done so that the cases like ! != are reversed.
//
// TODO: preprocessor directives start a line, ... test for this.
// TODO: currently preprocessor directs are entered both with and
// without the leading # due to a bug in separator handling.
char *C_HL_Keywords[] = {"switch",     "if",       "while",
                         "for",        "break",    "continue",
                         "return",     "else",     "struct",
                         "union",      "typedef",  "static",
                         "enum",       "class",    "case",
                         "include",    "define",   "NULL",
                         "#include",   "#define",  "ifdef",
                         "#ifdef",     "#then",    "#",
                         "namespace",  "!",        "!=",
                         "=",          "<",        ">",
                         "->",         "<<",       ">>",
                         "==",         "&&",       "|",
                         "||",         "|=",       "&",
                         "&=",

                         "int\xff",    "long\xff", "double\xff",
                         "float\xff",  "char\xff", "unsigned\xff",
                         "signed\xff", "void\xff", NULL};

char *Pascal_HL_Keywords[] = {
    "begin",       "end",        "if",         "then",           "else",
    "goto",        "while",      "do",         "until",          "program",
    "type",        "const",      "var",        "procedure",      "function",
    "repeat",      "for",        "to",         "downto",         "unit",
    "uses",        "with",       "interface",  "implementation", "in",
    "constructor", "destructor", "nil",        "exit",

    "array\xff",   "file\xff",   "object\xff", "packed\xff",     "label\xff",
    "record\xff",  "set\xff",    "string\xff", "type\xff",       "integer\xff",
    "float\xff",   "double\xff", "real\xff",   "char\xff",       NULL};

char *Python_HL_Keywords[] = {
    "and",     "as",        "assert",      "break",    "class",     "continue",
    "def",     "del",       "elif",        "else",     "except",    "False",
    "finally", "for",       "from",        "global",   "if",        "import",
    "in",      "is",        "lambda",      "None",     "nonlocal",  "not",
    "or",      "pass",      "raise",       "return",   "True",      "try",
    "while",   "with",      "yield",

    "int\xff", "float\xff", "complex\xff", "list\xff", "tuple\xff", "range\xff",
    "str\xff", NULL};

char *Markdown_HL_Keywords[] = {NULL};

char *Text_HL_Keywords[] = {NULL};

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

// clang-format off
// syntax highlighting definitions
struct editorSyntax HLDB[] = {

    {"C",
    C_HL_extensions,
    C_HL_Keywords,
    0,
    1,
    "//",
    "/*",
    "*/",
    HL_HIGHLIGHT_NUMBERS
    | HL_HIGHLIGHT_STRINGS
    | HL_HIGHLIGHT_COMMENT
    | HL_HIGHLIGHT_KEYWORDS},

    {"Pascal",
    Pascal_HL_extensions,
    Pascal_HL_Keywords,
    0,
    0,
    "//",
    "{",
    "}",
    HL_HIGHLIGHT_NUMBERS
    | HL_HIGHLIGHT_STRINGS
    | HL_HIGHLIGHT_COMMENT
    | HL_HIGHLIGHT_KEYWORDS},

    {"Python",
    Python_HL_extensions,
    Python_HL_Keywords,
    0,
    1,
    "#",
    NULL,
    NULL,
    HL_HIGHLIGHT_NUMBERS
    | HL_HIGHLIGHT_STRINGS
    | HL_HIGHLIGHT_COMMENT
    | HL_HIGHLIGHT_KEYWORDS},

    {"Markdown",
    Markdown_HL_extensions,
    Markdown_HL_Keywords,
    0,
    0,
    NULL,
    NULL,
    NULL,
    0},

    {"Text",
    Text_HL_extensions,
    Text_HL_Keywords,
    0,
    0,
    NULL,
    NULL,
    NULL,
    HL_HIGHLIGHT_NUMBERS
    | HL_HIGHLIGHT_PUNCTUATION}
  };

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
// clang-format on

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
};

struct editorConfig E;

////////////////////////////////
// prototypes for foward references
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

//////////////////////////////////
// error reporting

void die(const char *s) {
  /* NOTE: that trying to open a non-existant file when first
     invoking tvi throws an error. The following prints are
     done before raw mode is disabled, but I don't feel that
     I can safely disable here because it my break errno.
     TODO: investigate preserving perror output or errno. */
  write(STDOUT_FILENO, "\x1b[2J", 4); // erase display
  write(STDOUT_FILENO, "\x1b[H", 3);  // home cursor
  perror(s);
  exit(1);
}

////////////////////////////////////////////////////
// terminal state and management

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("disableRawMode-tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("enableRawMode-tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_cflag |= (CS8);
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_oflag &= ~(OPOST);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("enableRawMode-tcsetattr");
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("editorReadKey-read");
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == '0') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

///////////////////////////////////////////////////////////
// syntax highlighting

int is_punctuation(int c) { return strchr(".,():;[]!?", c) != 0; }

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr("\"\',.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->size);

  if (E.syntax == NULL)
    return;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->lineCommentStart;
  char *mcs = E.syntax->blockCommentStart;
  char *mce = E.syntax->blockCommentEnd;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_string = 0;
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (E.syntax->flags & HL_HIGHLIGHT_COMMENT) {
      if (scs_len && !in_string && !in_comment) {
        if (!strncmp(&row->render[i], scs, scs_len)) {
          memset(&row->hl[i], HL_COMMENT, row->rsize - i);
          break;
        }
      }

      if (mcs_len && mce_len && !in_string) {
        if (in_comment) {
          row->hl[i] = HL_MLCOMMENT;
          if (!strncmp(&row->render[i], mce, mce_len)) {
            memset(&row->hl[i], HL_MLCOMMENT, mce_len);
            i += mce_len;
            in_comment = 0;
            prev_sep = 1;
            continue;
          } else {
            i++;
            continue;
          }
        } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
          memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
          i += mcs_len;
          in_comment = 1;
          continue;
        }
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->size) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string)
          in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep && E.syntax->flags & HL_HIGHLIGHT_KEYWORDS) {
      int j;
      for (j = 0; keywords[j]; j++) {

        // TODO: is it worth doing this at init time as well? create a
        // smarter list for the lookup?
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '\xff';
        if (kw2)
          klen--;

        int not_found = (E.syntax->keywordsCaseSensitive
                             ? strncmp(&row->render[i], keywords[j], klen)
                             : strncasecmp(&row->render[i], keywords[j], klen));
        if (!not_found && is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    // first attempt at operators, depending on the
    // coding standards and langauge, there may not
    // be any prior seperator.
    if (E.syntax->flags & HL_HIGHLIGHT_OPERATORS) {
      /* TODO: need to implement, note that by relying on
           prev_sep we're not going to get operators without
           whitespace around them. */
    }

    // first attempt at punctuation, checking for prior
    // separator is not needed to decide to highlight, but
    // I think we do need to consider this to have been a
    // separator.
    if (E.syntax->flags & HL_HIGHLIGHT_PUNCTUATION) {
      if (is_punctuation(c)) {
        // only treat as punctuation if followed by whitespace
        if (i == row->rsize - 1 || isspace(row->render[i + 1])) {
          row->hl[i] = HL_PUNCTUATION;
          i++;
          prev_sep = 1;
          continue;
        }
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
  if (!E.highlighting)
    return 37;
  switch (hl) {
  case HL_COMMENT:
    return 36;
  case HL_MLCOMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_STRING:
    return 35;
  case HL_NUMBER:
    return 31;
  case HL_MATCH:
    return 34;
  case HL_PUNCTUATION:
    return 34;
  default:
    return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL)
    return;

  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->extensions[i]) {
      int is_ext = (s->extensions[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->extensions[i])) ||
          (!is_ext && strstr(E.filename, s->extensions[i]))) {
        E.syntax = s;
        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
  }
}

/////////////////////////////////////////////////////////////
// row of screen and in buffer mapping
//
// TODO: should the actual display be segregated?

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TVI_TAB_STOP - 1) - (rx % TVI_TAB_STOP);
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (TVI_TAB_STOP - 1) - (cur_rx % TVI_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx)
      return cx;
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs * (TVI_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TVI_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows)
    return;

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = at + 1; j <= E.numrows; j++)
    E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows)
    return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++)
    E.row[j].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

////////////////////////////////////////////////////
// editor actions/operations

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewLine() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy]; // note: prior call could have done a realloc
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.numrows)
    return;
  if (E.cx == 0 && E.cy == 0)
    return;
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

///////////////////////////////////////////////////////////
// file access

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;

  char *buf = malloc(totlen); // really? I'd buffer in case memory is an issue
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  /* TODO: if file does not exist, we shouldn't crash.
     Instead, offer to create a new file or exit gracefully. */

  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("editorOpen-fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t maxlinelen = -1;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    if (linelen > maxlinelen)
      maxlinelen = linelen;
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage(" %d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage(" Can't save! I/O error: %s", strerror(errno));
}

////////////////////////////////////////////////////
// search and maybe someday replace

void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;
  int current = last_match;
  int i;
  for (i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1)
      current = E.numrows - 1;
    else if (current == E.numrows)
      current = 0;

    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query = editorPrompt("Search: %s (ESC to cancel, Arrows, or Enter)",
                             editorFindCallback);

  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/////////////////////////////////////////////////
// screen display

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "TVI editor -- version %s", TVI_VERSION);
        if (welcomelen > E.screencols)
          welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        // oh god this hurts but will fix it after i'm through
        // the tutorial
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0)
        len = 0;
      if (len > E.screencols)
        len = E.screencols;
      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int current_color = -1;
      int j;
      for (j = 0; j < len; j++) {
        if (hl[j] == HL_NORMAL) {
          if (iscntrl(c[j])) {
            char sym = (c[j] <= 26) ? '@' + c[j] : '?';
            abAppend(ab, "\x1b[7m", 4);
            abAppend(ab, &sym, 1);
            abAppend(ab, "\x1b[m", 3);
            if (current_color != -1) {
              char buf[16];
              int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
              abAppend(ab, buf, clen);
            }
          } else if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80];
  char rstatus[80];
  int len = snprintf(status, sizeof(status), " %.20s - %d lines %s",
                     E.filename ? E.filename : " [No Name]", E.numrows,
                     E.dirty ? "(modified)" : "");
  int rlen =
      snprintf(rstatus, sizeof(rstatus), "%s %d/%d ",
               E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);
  if (len > E.screencols)
    len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6); // disable cursor, not all older vts support
  // abAppend(&ab, "\x1b[2J", 4); // erase display
  abAppend(&ab, "\x1b[H", 3); // home cursor

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // enable cursor
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/////////////////////////////////////////////////
// line oriented input

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback)
        callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback)
          callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback)
      callback(buf, c);
  }
}

/////////////////////////////////////////////////////////////
// cursoring

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row && E.cx == row->size) {
      // TODO: this may fail at end of file, i think it will
      // after testing, it doesn't crash but it does move the cursor
      // off the file to a ~ line
      E.cy++;
      E.cx = 0;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0)
      E.cy--;
    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows)
      E.cy++;
    break;
  }
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen)
    E.cx = rowlen;
}

//////////////////////////////////////////////////////
// handle a keypress
void editorProcessKeypress() {
  static int quit_times = TVI_QUIT_TIMES;

  int c = editorReadKey();

  switch (c) {

  case '\r':
    editorInsertNewLine();
    break;

  case CTRL_KEY('q'):
    if (E.dirty && quit_times > 0) {
      editorSetStatusMessage(" Warning!!! Unsaved changes. "
                             "Press ctrl-Q %d more times to quit.",
                             quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4); // erase display
    write(STDOUT_FILENO, "\x1b[H", 3);  // home cursor
    exit(0);
    break;

  case CTRL_KEY('s'):
    editorSave();
    break;

  case HOME_KEY:
    E.cx = 0;
    break;

  case END_KEY:
    if (E.cy < E.numrows)
      E.cx = E.row[E.cy].size;
    break;

  case CTRL_KEY('f'):
    editorFind();
    break;

  case CTRL_KEY('t'):
    E.highlighting = !E.highlighting;
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY)
      editorMoveCursor(ARROW_RIGHT);
    editorDelChar();
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP)
      E.cy = E.rowoff;
    if (c == PAGE_DOWN) {
      E.cy = E.rowoff + E.screenrows - 1;
      if (E.cy > E.numrows)
        E.cy = E.numrows;
    }
    int times = E.screenrows;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    break;
  }

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  default:
    editorInsertChar(c);
    break;
  }
  quit_times = TVI_QUIT_TIMES;
}

//////////////////////////////////////////////////////////////
// initialization

// sort comparison helper
static int cmpstringp(const void *p1, const void *p2) {
  return strcmp(*(char *const *)p1, *(char *const *)p2);
}

// Sort and massage keyword tables for syntax highlighting. Also
// check for duplicate entries in a table and report an error if
// found.
void initializeKeywordTables() {
  unsigned int i;
  for (i = 0; i < HLDB_ENTRIES; i++) {
    if (HLDB[i].keywords) {
      int j = 0;
      while (HLDB[i].keywords[j])
        j++;
      HLDB[i].keywordCount = j;

      // allocate room for the whole list -and- the trailing
      // null entry.
      size_t k = (j + 1) * sizeof(HLDB[i].keywords[0]);
      char **kw_copy = malloc(k);
      if (!kw_copy)
        die("initEditor-malloc");
      memcpy(kw_copy, HLDB[i].keywords, k);

      // do not include trailing null entry in sort
      qsort(kw_copy, j, sizeof(char *), cmpstringp);

      // Sort single character operators behind the double character variants
      // so that they highlight correctly. So, << in front of <, and so on.
      int fixed = 1;
      while (fixed) {
        fixed = 0;
        for (j = 0; j < HLDB[i].keywordCount - 1; j++) {
          if (strlen(kw_copy[j]) == (strlen(kw_copy[j + 1]) - 1)) {
            if (strncmp(kw_copy[j], kw_copy[j + 1], strlen(kw_copy[j])) == 0) {
              fixed = 1;
              void *temp;
              temp = kw_copy[j];
              kw_copy[j] = kw_copy[j + 1];
              kw_copy[j + 1] = temp;
            }
          }
        }
      }

      // Check for duplicate keywords and error out if any are found.
      for (j = 0; j < HLDB[i].keywordCount - 1; j++) {
        if (strcmp(kw_copy[j], kw_copy[j + 1]) == 0) {
          char errmsg[80];
          snprintf(errmsg, 79, "duplicate keyword in syntax table for %s '%s'",
                   HLDB[i].filetype, kw_copy[j]);
          errno = EINVAL;
          die(errmsg);
        }
      }

      HLDB[i].keywords = kw_copy;
    }
  }
}

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;
  E.highlighting = 1;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("initEditor-getWindowSize");
  E.screenrows -= 2;
  initializeKeywordTables();
}

///////////////////////////////////////////////////////////////////
// main, fire it up

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage(" HELP: ctrl-Q = quit, ctrl-S = save, Ctrl-F = find, "
                         "Ctrl-T = toggle hilighting");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };

  return 0;
}
