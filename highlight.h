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

#ifndef FILE_HIGHLIGHT_H_SEEN
#define FILE_HIGHLIGHT_H_SEEN
////////////////////////////////
// prototypes for foward references
// TODO: some of these need renames
// TODO: and those not in tvi need to be moved
//       to the appropriate header
extern void initializeKeywordTables();
extern void editorSelectSyntaxHighlight();
extern int editorSyntaxToColor(int);
extern void editorUpdateSyntax(erow *row);

#endif // !FILE_HIGHLIGHT_H_SEEN
