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
// DONE: Sort keyword highlight tables at initialization for a speedier
//       highlighter, not that it seems to matter.
//
// DONE: Modal as vim
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
// Load/save/insert file
//
// Smart wrapping for text editing
//
// Better status line
//
// Hex edit mode
//
// clang-format on

#include "tvi.h"

#include "highlight.h"
#include "terminal.h"
#include "rowscreen.h"

struct editorConfig E;

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

// to be implemented
void editorFindNext() { return; }

// needs to deal with forward and backward
void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query = editorPrompt("Search: %s (ESC to cancel, Arrows, or Enter)",
                             editorFindCallback);

  if (query) {
    // todo: i'm pretty sure this behavior isn't right wrt just hitting
    // a slash or question followed by enter
    free(E.findString);
    E.findString = query;
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/*** append buffer ***/

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

void editorProcessNormalKeypress(int c) {
  if (c == ' ')
    return;
  // to be provided
}

// insert mode, keys just go through mostly
// until an escape
void editorProcessInsertKeypress(int c) {
  if (c == '\x1b') {
    E.mode = EM_NORMAL;
    editorSetStatusMessage("", "");
    return;
  }
  switch (c) {
  case '\r':
    editorInsertNewLine();
    break;
  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY)
      editorMoveCursor(ARROW_RIGHT);
    editorDelChar();
    break;

  default:
    editorInsertChar(c);
  }
}

// visual keypress, movement and highlight
void editorProcessVisualKeypress(int c) {
  if (c == '\x1b') {
    E.mode = EM_NORMAL;
    editorSetStatusMessage("", "");
    return;
  }
  // cursor movement here ...
  return;
}

// input down on the status line
// todo: write and quit need to take input up to an enter
void editorProcessEXKeypress(int c) {

  if (c == '\x1b') {
    E.mode = EM_NORMAL;
    editorSetStatusMessage("", "");
    return;
  }

  switch (c) {

  case 'q':
    if (E.dirty) {
      editorSetStatusMessage(" Warning!!! Unsaved changes. :q! to override.");
      E.mode = EM_NORMAL;
      break;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4); // erase display
    write(STDOUT_FILENO, "\x1b[H", 3);  // home cursor
    exit(0);

  case 'w':
    editorSave();
    E.mode = EM_NORMAL;
    break;
  }

  return;
}

int translateViKeys(int c) {
  switch (c) {
  case 'h':
    return ARROW_LEFT;
  case 'j':
    return ARROW_DOWN;
  case 'k':
    return ARROW_UP;
  case 'l':
    return ARROW_RIGHT;
  case 'w':
    // word right, does nothing right now
    return 0;
  case 'b':
    // word left, does nothing right now
    return 0;
  case 'a':
    // insert after, does nothing right now
    return 0;
  case CTRL_KEY('f'):
    return PAGE_DOWN;
  case CTRL_KEY('b'):
    return PAGE_UP;
  }
  return c;
}

void editorProcessKeypress() {
  static int quit_times = TVI_QUIT_TIMES;

  // probably need to break this out at a high level by mode,
  // and then process keys applicable to that mode. we'll get
  // there.
  //
  // note: when in visual mode, keys are limited to esc and
  // movement.
  // full vi allows cursor movement while in input mode but
  // i probably won't.

  int c = editorReadKey();

  if (E.mode == EM_INSERT) {
    editorProcessInsertKeypress(c);
    return;
  } else if (E.mode == EM_VISUAL) {
    editorProcessVisualKeypress(c);
    return;
  } else if (E.mode == EM_COMMAND) {
    editorProcessEXKeypress(c);
    return;
  }

  // in normal mode, be sure to remap vi style movement keys
  c = translateViKeys(c);

  // normal mode here
  switch (c) {

  case 'i':
    E.mode = EM_INSERT;
    editorSetStatusMessage("-- %s --", "INSERT");
    break;

  case 'v':
    E.mode = EM_VISUAL;
    editorSetStatusMessage("-- %s --", "VISUAL");
    break;

  case ':':
    E.mode = EM_COMMAND;
    editorSetStatusMessage("-- %s -- :", "COMMAND");
    break;

  case '/':
    E.findForward = 1;
    editorFind(); // forwards
    break;

  case '?':
    E.findForward = 0;
    editorFind(); // backwards
    break;

  case 'n':
    if (E.findString) {
      editorFindNext(); // in appropriate direction
    }
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

  case CTRL_KEY('t'):
    E.highlighting = !E.highlighting;
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
    // this is a clear or repaint screen command usually, i'm not sure why
    // it's blocked out here, must review the tutorial.
    break;

  default:
    // now only insert if in insert mode
    // editorInsertChar(c);
    break;
  }
  quit_times = TVI_QUIT_TIMES;
}

//////////////////////////////////////////////////////////////
// initialization

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
  E.mode = EM_NORMAL;
  E.findForward = 1;
  E.findString = NULL;
}

///////////////////////////////////////////////////////////////////
// main, fire it up

int main(int argc, char *argv[]) {
  initializeKeywordTables();
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage(
      " HELP: <esc>:q! = quit, <esc>:w = save, <esc>/ = find, "
      "Ctrl-T = toggle hilighting");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };

  return 0;
}
