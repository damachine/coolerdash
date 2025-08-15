---
applyTo: "**"
---

**Chat-Sprachregeln**
- Beantworte Chat-Fragen in deutscher Sprache.

**Immer ganz oben am Anfang einer Datei `.c` `.h`**
```c
/**
 * @author damachine (christkue79@gmail.com)
 * @website https://github.com/damachine
 * @copyright (c) 2025 damachine
 * @license MIT
 * @version 1.0
 */
 ```
- Vermeide und entferne den `Immer ganz oben am Anfang einer Datei...`, wenn er nicht am anfang der Datei steht.

**Kommentar- und Dokumentationsstil**
- Dokumentiere in der `README.md` und `AUR-README.md` in englischer Sprache.
- Schreibe Code-Kommentare in englischer Sprache.
- Verwende Doxygen-Stil für Funktionskommentare.
- Nutze Doxygen-Kommentare für Funktionen, Strukturen und wichtige Abschnitte.
- Öffnende geschweifte Klammern stehen bei Funktionen und Kontrollstrukturen in derselben Zeile (K&R-Stil).
- Nutze `//` für einzeilige Kommentare.
- Nutze `/* ... */` für mehrzeilige Kommentare.
- Nutze Inline-Kommentare sparsam, nur wenn nötig.
- Doppelte Header-Kommentare vermeiden.
- Kommentiere alle nicht sofort verständlichen Codeabschnitte.
- Vermeide redundante Kommentare, die den Code wiederholen.
- Dokumentiere komplexe Algorithmen und Datenstrukturen ausführlich.
- Nutze als 1. Kommentar `@brief` für eine kurze Zusammenfassung der Funktion.
- Nutze als 2. Kommentar `@details` für eine ausführliche Beschreibung der Funktion.
- Beispiel für nutze als 1. 2. Kommentar:
```c
/**
 * @brief
 * ...
 * @details
 * ...
 */
 ```
- Entferne Kommentare, die den Code nicht mehr beschreiben oder veraltet sind.

**Code-Richtlinien und Codestil**
- Halte dich an ISO/IEC 9899:1999 (C99).
- Binde nur notwendige Header ein; trenne System- und lokale Header
- Verwende Include Guards: `#ifndef HEADER_H` / `#define HEADER_H` / `#endif`.
- Nutze `const` für unveränderliche Variablen und Funktionsparameter.
- Nutze `static` für Funktionen und Variablen, die nur in der Datei sichtbar sein sollen.
- Nutze `inline` für kleine, häufig genutzte Funktionen.
- Nutze `malloc()` für dynamische Speicherallokation.
- Nutze `calloc()` für dynamische Speicherallokation mit Nullinitialisierung.
- Nutze `realloc()` für dynamische Speicheranpassung.
- Nutze `enum` für Status- und Fehlercodes, z.B. `enum Status { SUCCESS, ERROR }`.
- Nutze `typedef` für komplexe Datentypen, z.B. `typedef struct { int x; int y; } Point;`.
- Nutze `struct` für Datenstrukturen, z.B. `struct MyStruct { int a; float b; };`.
- Nutze `union` für gemeinsame Datenstrukturen, z.B. `union Data { int i; float f; };`.
- Nutze `typedef` für Zeiger auf Funktionen, z.B. `typedef void (*Callback)(int);`.
- Nutze `static_assert` für Compile-Zeit-Prüfungen, z.B. `static_assert(sizeof(int) == 4, "int must be 4 bytes");`.
- Nutze `restrict` für Zeiger, die nicht auf dieselben Daten zeigen, z.B. `void func(int * restrict a, int * restrict b);`.
- Nutze `volatile` für Variablen, die sich außerhalb des Programms ändern können, z.B. `volatile int *ptr;`.
- Nutze `inline` für kleine, häufig genutzte Funktionen, z.B. `inline int square(int x) { return x * x; }`.
- Vermeide `free()` auf NULL-Zeiger, aber setze Zeiger nach `free()` auf NULL.
- Vermeide `gets()`, nutze stattdessen `fgets()` oder `getline()`.
- Vermeide `strcpy()`, nutze stattdessen `strncpy()` oder `strlcpy()`.
- Vermeide `sprintf()`, nutze stattdessen `snprintf()` oder `asprintf()`.
- Vermeide `strcat()`, nutze stattdessen `strncat()` oder `strlcat()`.
- Vermeide ` strtok()`, nutze stattdessen `strsep()` oder `strtok_r()`.
- Vermeide `atoi()`, `atol()`, `atoll()`, nutze stattdessen `strtol()`, `strtoll()`, `strtof()`, `strtod()`, `strtold()`.
- Vermeide `printf()` für Fehler und Debugging, nutze stattdessen `fprintf(stderr, ...)`.
- Vermeide `exit()` in Bibliotheksfunktionen, nutze stattdessen `return` oder `longjmp()`.
- Vermeide `goto`, nutze statdessen Schleifen und Kontrollstrukturen.
- Vermeide globale Variablen, nutze stattdessen lokale Variablen oder Strukturen.
- Vermeide rekursive Funktionen, wenn möglich, nutze stattdessen Iteration.
- Vermeide unnötige Typumwandlungen, nutze stattdessen den richtigen Datentyp.
- Vermeide unnötige Zeigerarithmetik, nutze stattdessen Array-Indizes.
- Vermeide unnötige Funktionsaufrufe, nutze stattdessen Inline-Funktionen.
- Vermeide unnötige Schleifen, nutze stattdessen bedingte Anweisungen.
- Vermeide unnötige Speicherallokation, nutze stattdessen statische Arrays oder Strukturen.
- Vermeide unnötige Typdefinitionen, nutze stattdessen Standardtypen.
- Vermeide unnötige Makros, nutze stattdessen Inline-Funktionen oder Konstanten.
- Überprüfe Rückgabewerte von `malloc()`, `calloc()`, `realloc()`.
- Funktionsnamen sind Verben im snake_case, z.B. `calculate_sum()`, `parse_input()`.
- Variablen im snake_case, z.B. `user_count`.
- Konstanten und Makros in UPPER_CASE, z.B. `MAX_SIZE`, `PI`.
- Typdefinitionen in PascalCase, z.B. `MyType`.
- Enum-Namen in UPPER_CASE, z.B. `STATUS_OK`, `
- Gib dynamisch reservierten Speicher frei und setze Zeiger danach auf NULL.
- Halte den Code strukturiert: Trenne Deklarationen, Definitionen und Implementierungen.
- Halte den Code sauber und lesbar: Einrückung mit 4 Leerzeichen, keine Tabulatoren.
- Vermeide unnötigen Code und redundante Kommentare.
- Halte den Code modular: Teile große Funktionen in kleinere auf.
- Halte den Code effizient: Vermeide unnötige Berechnungen und Schleifen.
- Halte den Code portabel: Vermeide plattformspezifische Funktionen und Bibliotheken.
- Halte den Code sicher: Vermeide Pufferüberläufe, nutze sichere Funktionen.
- Halte den Code wartbar: Schreibe klaren, verständlichen Code mit sinnvollen Kommentaren.
- Halte den Code testbar: Schreibe Unit-Tests für wichtige Funktionen.
- Halte den Code dokumentiert: Nutze Doxygen-Kommentare für Funktionen und Strukturen.
- Halte den Code performant: Optimiere nur, wenn es notwendig ist, und vermeide premature optimization.
- Halte den Code konsistent: Nutze einheitliche Stilrichtlinien und Namenskonventionen.
- Halte den Code lesbar: Nutze sprechende Namen und vermeide kryptische Abkürzungen.
- Halte den Code flexibel: Nutze Parameter und Rückgabewerte, um Funktionen anpassbar zu machen.
- Halte den Code erweiterbar: Schreibe Funktionen so, dass sie leicht erweitert werden können.
- Halte den Code robust: Behandle Fehlerfälle und unerwartete Eingaben angemessen.
