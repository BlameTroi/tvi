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

#include "tvi.h"

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
//
//////////////////////////////////////////////////////////////
// initialization of keyword tabls

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

