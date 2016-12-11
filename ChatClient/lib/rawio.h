// rawio.h
// simple routines for terminal rawio with cursor positioning and sync

// liest 0-terminierten String maximaler Laenge an Spalte/Zeile Position
char *gets_raw(char *s, int maxlen, int col, int row);

// schreibt 0-terminierten String an Spalte/Zeile Position
void writestr_raw(char *s, int col, int row);

// Scrollt Bildschirmteil um 1 Zeile nach oben
void scroll_up(int from_line, int to_line);

// liefert Anzahl Zeilen des Terminals [ beim Initialisieren!! ]
int get_lines();

// Loescht Bildschirm
void clearscr();
