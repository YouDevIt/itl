# ITL Language Manual

**ITL (Incredibly Tiny Language)** is a minimalist interpreted language inspired by VTL‑2 (Very Tiny Language 2, developed by Gary Shannon and Frank McCoy in 1976). Each line of source code is a statement. There are no keywords, because meaning is determined entirely by the line’s leading character and by the punctuation used within expressions. In fact, every program line is an assignment to either a system or a user variable. The `=` assignment operator between the variable and the expression is usually omitted, and whitespace characters are ignored.

---

## Table of Contents

1. [Program structure](#1-program-structure)
2. [Variables](#2-variables)
3. [Numbers and strings](#3-numbers-and-strings)
4. [Expressions](#4-expressions)
5. [Operators](#5-operators)
6. [Assignment](#6-assignment)
7. [Print statement](#7-print-statement)
8. [Input](#8-input)
9. [Control flow – the line-jump](#9-control-flow--the-line-jump)
10. [Arrays](#10-arrays)
11. [Math functions](#11-math-functions)
12. [Screen functions](#12-screen-functions)
13. [Graphics functions](#13-graphics-functions)
14. [Mouse functions](#15-mouse-functions)
15. [Text window mouse functions](#16-text-window-mouse-functions)
16. [Timing functions](#14-timing-functions)
17. [Special tokens](#13-special-tokens)
18. [Statement separator](#14-statement-separator)
19. [Parenthesis blocks](#15-parenthesis-blocks)
20. [Type conversion](#16-type-conversion)
21. [Comments](#17-comments)
22. [Error handling](#18-error-handling)
23. [Examples](#19-examples)

---

## 1. Program structure

A program is a sequence of lines, numbered 1, 2, 3, … in the order they appear in the file (or the order they were typed in the REPL). Line numbers are implicit; there are no explicit `10 PRINT` style numbers in the source file.

Each physical line in a source file may contain multiple statements separated by `;` at the top level (outside parentheses or strings). Each resulting segment is stored as a separate numbered line.

```
A = 1; B = 2; ? = A+B+"\n"
```

becomes three lines internally:

```
1: A = 1
2: B = 2
3: ? = A+B+"\n"
```

---

## 2. Variables

ITL has **27 single-letter variables**: `A` through `Z` (uppercase only) and the underscore `_`.

Variables are dynamically typed: they hold either a **number** (double-precision float) or a **string** at any given moment.

A variable that has never been assigned is *undefined*. If an undefined variable is read for the first time, the interpreter performs a *forward reference scan*: it scans the remaining program lines to see if the variable is assigned there and, if so, executes that assignment first. If no assignment is found, an undefined variable returns `0` numerically or `"0"` as a string.

To explicitly set a variable back to *undefined*, write its name alone on a line.

---

## 3. Numbers and strings

### Numeric literals

Any sequence of digits, optionally with a decimal point. Scientific notation is accepted (via the standard C `strtod` parser):

```
42
3.14
1.5e-3
```

### String literals

Delimited by double quotes. Supported escape sequences:

| Sequence | Meaning |
|----------|---------|
| `\n` | newline |
| `\t` | horizontal tab |
| `\r` | carriage return |
| `\\` | literal backslash |
| `\"` | literal double quote |
| `\nnn` | character with octal code `nnn` (1–3 octal digits) |

```
?"Hello\tWorld\n"
```

---

## 4. Expressions

Expressions are evaluated **left-to-right** with no operator precedence (use parentheses to group). An expression may be:

- A numeric literal: `42`, `3.14`
- A string literal: `"hello"`
- A variable: `A`, `Z`, `_`
- A unary minus: `-A`, `-3`
- A logical NOT: `!A` (returns 1 if A is 0, else 0)
- A binary operation: `A+B`, `X*2`, `N<10`
- A system function call: `sin(A)`, `gotoxy(10,5)`
- A parenthesis block: `(A=1; B=A+1; B)` (see §15)
- Type conversion: `$A` (see §16)
- Random number: `'`
- Array access: `@N`
- Current line number: `#`
- Keyboard input: `?` (reads a line)
- Keyboard buffer check: `:` (non-blocking, returns ASCII or 0)

---

## 5. Operators

All operators work left-to-right without precedence. Use parentheses to control order.

| Operator | Types | Effect |
|----------|-------|--------|
| `+` | num+num | Addition |
| `+` | str or num | String concatenation (either operand can be a string) |
| `-` | num | Subtraction |
| `*` | num | Multiplication |
| `/` | num | Division (division by zero yields 0) |
| `%` | num | Modulo (fmod; modulo by zero yields 0) |
| `^` | num | Exponentiation (pow) |
| `&` | num | Logical AND: 1 if both non-zero, else 0 |
| `\|` | num | Logical OR: 1 if either non-zero, else 0 |
| `!` | num | Logical NOT (unary): 1 if zero, else 0 |
| `<` | num | Less than: 1 if true, else 0 |
| `>` | num | Greater than: 1 if true, else 0 |
| `=` | num | Equality: 1 if equal, else 0 |

There is no `!=`, `<=`, or `>=`; compose them from existing operators:

```
A<B | A=B    (* less than or equal *)
!(A=B)       (* not equal *)
```

---

## 6. Assignment

### Explicit assignment

```
A = expr
```

Whitespace around `=` is optional.

### Implicit assignment

```
A expr
```

If a variable is followed directly by a value (no operator between them), it is assigned that value:

```
A 42         (* A = 42 *)
S "hello"    (* S = "hello" *)
```

### Self-referential shorthand

If a variable is immediately followed by a binary operator (not `=`), the statement is expanded to `VAR = VAR OP expr`:

```
A+1          (* same as A = A + 1 *)
N*2          (* same as N = N * 2 *)
```

### Making a variable undefined

Write the variable name alone:

```
A
```

---

## 7. Print statement

A line beginning with `?` followed by an expression (or by `=` and an expression) prints the expression:

```
?"Hello, World!\n"
?A
?A + " + " + B + " = " + (A+B) + "\n"
```

Numbers are printed with up to 15 significant digits. No newline is added automatically; include `"\n"` explicitly when needed.

---

## 8. Input

### Reading a line of text

Use `?` *inside an expression* (not at the start of a line):

```
A = ?
```

This displays `> ` in REPL mode and waits for the user to type a line. The result is a string.

```
?"Enter a number: "
N = ?
?"You entered: " + N + "\n"
```

### Non-blocking keyboard check

The `:` token reads one character from the keyboard buffer without waiting. Returns the ASCII code of the key, or `0` if no key is available:

```
K = :
```

Useful for real-time loops and games.

---

## 9. Control flow – the line-jump

The special variable `#` holds the **current line number**. Assigning to `#` is a **jump**:

```
#=10          (* unconditional jump to line 10 *)
#=N*0         (* no jump, continue execution *)
```

**Conditional jump** – multiply the target line by the condition (1 or 0):

```
#=(N<10)*25
```

The line above executes a jump to line 25 while N < 10; when N >= 10 continues the execution with no jump.

A jump to a negative line terminates execution. A jump to a line beyond the last line also terminates.

#### Subroutine pattern

Since there are no `GOSUB`/`RETURN` statements, subroutines can be simulated using a return-address variable:

```
R=#+1; #=20    (* call subroutine at line 20, return to the line that follows here *)
...            (* line 5: continuation *)

(* --- subroutine at line 20 --- *)
...work...
#=R            (* return *)
```

---

## 10. Arrays

ITL has a single global array accessed via `@index`. The array grows automatically:

### Reading

```
V = @5          (* read element 5 *)
```

Accessing an index beyond the current array size returns `0`.

### Writing

```
3@ = 99        (* write 99 to element 3 *)
N@ = X         (* write X to element N, using 0 as dummy base *)
```

The general syntax for an array write is:

```
index@ = value
```

Array indices are zero-based integers. Negative indices are clamped to 0. The maximum size is `MAX_ARRAY_SIZE` (1 000 000 elements in the current implementation).

---

## 11. Math functions

System functions are called with a lowercase name followed by parenthesised arguments. Multiple arguments are comma-separated.

### Trigonometric

| Function | Description |
|----------|-------------|
| `sin(x)` | sine |
| `cos(x)` | cosine |
| `tan(x)` | tangent |
| `asin(x)` | arcsine |
| `acos(x)` | arccosine |
| `atan(x)` | arctangent |
| `atan2(y,x)` | two-argument arctangent |
| `sinh(x)` | hyperbolic sine |
| `cosh(x)` | hyperbolic cosine |
| `tanh(x)` | hyperbolic tangent |

All angles are in **radians**.

### Exponential / logarithmic

| Function | Description |
|----------|-------------|
| `exp(x)` | e^x |
| `log(x)` | natural logarithm |
| `log2(x)` | base-2 logarithm |
| `log10(x)` | base-10 logarithm |
| `sqrt(x)` | square root |
| `cbrt(x)` | cube root |
| `pow(x,y)` | x^y |

### Rounding

| Function | Description |
|----------|-------------|
| `ceil(x)` | round toward +∞ |
| `floor(x)` | round toward −∞ |
| `round(x)` | round to nearest integer |
| `trunc(x)` | truncate toward zero |

### Miscellaneous

| Function | Description |
|----------|-------------|
| `abs(x)` | absolute value |
| `fabs(x)` | absolute value (alias) |
| `sign(x)` | −1, 0, or 1 |
| `fmod(x,y)` | floating-point remainder |
| `hypot(x,y)` | Euclidean distance sqrt(x²+y²) |
| `max(x,y)` | larger of x and y |
| `min(x,y)` | smaller of x and y |

### Constants (zero-argument)

| Name | Value |
|------|-------|
| `pi` | π ≈ 3.14159265358979 |
| `e` | e ≈ 2.71828182845905 |

Constants may be written with or without parentheses: `pi` or `pi()`.

---

## 12. Screen functions

These functions are implemented via **PDCurses** and control the terminal display. They are called using the same lowercase function-call syntax as math functions.

### Color codes

Both foreground and background accept a color number 0–7:

| Code | Color |
|------|-------|
| 0 | Black |
| 1 | Blue |
| 2 | Green |
| 3 | Cyan |
| 4 | Red |
| 5 | Magenta |
| 6 | Yellow |
| 7 | White |

### Attribute codes (setattr)

| Code | Attribute |
|------|-----------|
| 0 | Normal |
| 1 | Bold |
| 2 | Reverse video |

### Function reference

#### `gotoxy(x, y)`

Moves the text cursor to column `x` (0-based) and row `y` (0-based).  
Returns `1` on success, `0` if the coordinates are outside the screen bounds.

```
gotoxy(0, 0)          (* move to top-left *)
gotoxy(getw()-1, geth()-1)   (* move to bottom-right *)
```

#### `putch(c)`

Writes a character (or string) at the current cursor position.

- If `c` is a **number**, it is interpreted as an ASCII code and that single character is written.
- If `c` is a **string**, the entire string is written starting at the current position.

Returns the ASCII code of the character that was at the cursor before writing, or `-1` on error. The cursor advances after writing.

```
putch(65)         (* write 'A' *)
putch("Hello")    (* write a string *)
```

#### `getch()`

Reads the character **currently displayed** at the cursor position (a screen read, not a keyboard read) and returns its ASCII code. Returns `-1` on error.

```
C = getch()
```

#### `setfore(color)`

Sets the foreground color for subsequent output. Argument is 0–7 (see color table above).  
Returns `1` on success, `0` if the color number is out of range.

```
setfore(2)    (* green text *)
```

#### `setback(color)`

Sets the background color for subsequent output. Argument is 0–7.  
Returns `1` on success, `0` if the color number is out of range.

```
setback(4)    (* blue background *)
```

#### `setattr(attribute)`

Sets the character attribute for subsequent output. Argument is 0–2 (see attribute table above).  
Returns the attribute value that was set.

```
setattr(1)    (* bold *)
setattr(0)    (* back to normal *)
```

#### `getw()`

Returns the number of columns (width) of the visible screen.

```
W = getw()
```

#### `geth()`

Returns the number of rows (height) of the visible screen.

```
H = geth()
```

#### `clear()`

Clears the entire screen, fills it with the current background color, and moves the cursor to position (0, 0).

```
setback(0)
clear()
```

---

## 13. Graphics functions

These functions open and draw into a separate **WinAPI GDI window** (pixel-based). They are independent of the PDCurses terminal. `gopen()` must be called before any other `g*` function.

### Opening the window

#### `gopen(w, h)`

Opens a graphics window of `w × h` pixels. If the window is already open, does nothing.  
Returns `1`.
```
gopen(800, 600)
```

### Drawing colors

#### `gpen(r, g, b)`

Sets the current **pen color** (used by lines, borders, pixels, and text). Each component is 0–255.
```
gpen(255, 0, 0)    (* red *)
```

#### `gbr(r, g, b)`

Sets the current **brush color** (used for filled shapes and `gclear()`). Each component is 0–255.
```
gbr(0, 0, 128)    (* dark blue fill *)
```

### Clearing and refreshing

#### `gclear()`

Fills the entire graphics window with the current brush color.

#### `grefresh()`

Forces an immediate repaint of the graphics window. Drawing functions call this automatically, but it can be called manually if needed.

### Pixel

#### `gpixel(x, y)`

Draws a single pixel at `(x, y)` with the current pen color.

### Lines and rectangles

#### `gline(x1, y1, x2, y2)`

Draws a line from `(x1, y1)` to `(x2, y2)` with the current pen color.

#### `grect(x1, y1, x2, y2)`

Draws the **border** of a rectangle using the current pen color. The interior is transparent.

#### `gfillrect(x1, y1, x2, y2)`

Draws a **filled** rectangle using the current pen color for the border and the current brush color for the interior.

### Circles (ellipses)

#### `gcircle(x, y, r)`

Draws the **border** of a circle centered at `(x, y)` with radius `r`, using the current pen color. The interior is transparent.

#### `gfillcircle(x, y, r)`

Draws a **filled** circle centered at `(x, y)` with radius `r`, using the current pen color for the border and the current brush color for the interior.

### Text

#### `gtext(x, y, str)`

Draws the string `str` at pixel position `(x, y)` using the current pen color on a transparent background.
```
gtext(10, 10, "Hello from ITL!")
```

### Example
```
gopen(640, 480)
gbr(0, 0, 0)
gclear()
gpen(0, 255, 0)
gcircle(320, 240, 100)
gpen(255, 255, 0)
gtext(280, 230, "ITL!")
```
---

## 14. Mouse functions

These functions read the state of the mouse inside the GDI graphics window. They only work after `gopen()` has been called. All coordinates are in pixels relative to the top-left corner of the graphics window client area.

#### `gmx()`

Returns the current X coordinate of the mouse cursor.

#### `gmy()`

Returns the current Y coordinate of the mouse cursor.

#### `gmb()`

Returns a bitmask of the mouse buttons currently held down:

| Bit | Value | Button |
|-----|-------|--------|
| 0 | 1 | Left button |
| 1 | 2 | Right button |
| 2 | 4 | Middle button |
```
B = gmb()
#=(B&1)*10    (* jump to line 10 if left button is held *)
```

#### `gmclick()`

Returns the button number of the most recent mouse click and **clears** the event so the next call returns 0 until a new click occurs:

| Return value | Meaning |
|---|---|
| 0 | No new click since last call |
| 1 | Left button clicked |
| 2 | Right button clicked |
| 3 | Middle button clicked |
```
C = gmclick()
#=(C=0)*W    (* loop back if no click *)
```

#### `gmdrag(b)`

Returns `1` if button `b` is currently held down **while the mouse is moving** (drag), `0` otherwise. `b` is 1 for left, 2 for right, 3 for middle.
```
#=!gmdrag(1)*W    (* loop back if not dragging with left button *)
gpixel(gmx(), gmy())
```

---

## 16. Text window mouse functions

These functions read the state of the mouse inside the **PDCurses text window**. Coordinates are in **character cells** (column/row), not pixels. Mouse support is always active — no initialization is required.

> **Note:** These functions rely on the `:` token to pump the PDCurses event queue. At least one `:` must appear in any loop that uses text mouse functions, otherwise events are never received.

#### `tmx()`

Returns the current **column** (0-based) of the mouse cursor inside the text window.

#### `tmy()`

Returns the current **row** (0-based) of the mouse cursor inside the text window.

#### `tmclick()`

Returns the button number of the most recent mouse click and **clears** the event so the next call returns 0 until a new click occurs:

| Return value | Meaning |
|---|---|
| 0 | No new click since last call |
| 1 | Left button clicked |
| 2 | Right button clicked |
| 3 | Middle button clicked |

#### `tmdrag(b)`

Returns `1` if button `b` is currently held down while the mouse is moving (drag), `0` otherwise. `b` is 1 for left, 2 for right, 3 for middle.

---

## 16. Timing functions

Timing functions return time values as numbers (seconds or milliseconds). They use the Windows high-resolution performance counter and are therefore not available on non-Windows builds.

#### `time()`

Returns the current Unix timestamp: the number of whole **seconds** elapsed since 1 January 1970 00:00:00 UTC.
```
T = time()
?"Unix time: "+T+"\n"
```

#### `ticks()`

Returns the number of **milliseconds** elapsed since the interpreter started. Useful for measuring total run time or creating time-based animations.
```
?"Uptime ms: "+ticks()+"\n"
```

#### `elapsed()`

Returns the number of **milliseconds** elapsed since the **last call** to `elapsed()`. On the first call, the reference point is interpreter startup. Resets its internal reference on every call, making it suitable for per-frame timing.
```
elapsed()        (* reset reference *)
W=#
"... work ..."
?"Frame time: "+elapsed()+" ms\n"
#=W
```

---

## 17. Special tokens

### `#` – current line number

In an expression, `#` evaluates to the number of the line currently being executed. Assigning to `#` causes a jump (see §9).

### `'` – random number / RNG seed

Alone, `'` returns a random floating-point number uniformly distributed in **[0, 0.999999]**:

```
R = '
```

Followed by an integer expression, `'` sets the random number generator seed:

```
'42       (* seed the RNG with 42 *)
```

The RNG is automatically seeded from `time()` at startup, so results differ each run unless explicitly seeded.

### `:` – non-blocking keyboard read

Returns the ASCII code of the next character in the keyboard buffer, or `0` if the buffer is empty. Does not wait.

```
K = :
#=(K=0)*#    (* spin until a key is pressed *)
```

---

## 18. Statement separator

The semicolon `;` separates multiple statements on a single line (at the top level, i.e., outside parentheses and strings):

```
A=1; B=2; C=3; ?A+B+C+"\n"
```

Each segment becomes a separately numbered line internally.

---

## 19. Parenthesis blocks

A pair of parentheses encloses a **block** of statements separated by `;` or `,`. The block is evaluated as an expression; its value is the value of the last statement inside it.

Assignment rules inside a block:
- `VAR = expr` or `VAR expr` — always assigns to the variable.
- `VAR op expr` followed by `;` — self-referential assignment (e.g. `B+1;` means `B = B+1`).
- `VAR op expr` as the **last item** in the block — the expression is evaluated but the variable is **not** modified; the result is returned as the block's value.

```
X = (A=1; B=2; A+B)     (* X = 3, A = 1, B = 2 *)
```

Parenthesis blocks can be nested:

```
Y = (A=10; (B=A*2; B+1))    (* Y = 21, A = 10, B = 20 *)
```

---

## 20. Type conversion

`$VAR` converts a variable from one type to the other:

- If `VAR` holds a **number**, `$VAR` returns its string representation.
- If `VAR` holds a **string**, `$VAR` parses it as a number and returns that.

```
N = 42
S = $N          (* S = "42" *)

S = "3.14"
X = $S          (* X = 3.14 *)
```

---

## 21. Comments

ITL has no comment syntax. Common workarounds:

- Put a string at the start of a line — it is evaluated and the value discarded: `"-- this is a comment --"`.
- Use a variable assignment that you never use: `_ = 0`.

---

## 22. Error handling

Runtime errors (division by zero, unknown function, out-of-range values) are reported as messages on the screen. In **file mode** an unrecoverable error terminates the interpreter. In **REPL mode** errors are reported and execution continues from the next input.

Array indexing out of bounds: reading beyond the array returns 0; writing auto-expands the array.

---

## 23. Examples

### Hello, World

```
?"Hello, World!\n"
```

### Counting loop (1 to 10)

```
N=1
?N+"\n"
N+1
#=(N<11)*2
```

Line 2 prints N. Line 3 increments N. Line 4 jumps back to line 2 while N < 11; when N = 11 the product is 0, jumping to line 0 (stop).

### Fibonacci numbers

```
A=0
B=1
?A+"\n"
_=A+B
A=B
B=_
#=(A<1000)*3
```

### Simple animation (press a key to move the *)

```
clear()
X=0
setfore(2)
gotoxy(X,10)
putch(42)
gotoxy(X-1,10)
putch(32)
X+1
#:=0*#
#=(X<getw())*4
```

### Colored text banner

```
clear()
setback(4)
setfore(7)
setattr(1)
gotoxy(5,2)
?"  Welcome to ITL!  "
setattr(0)
setfore(7)
setback(0)
gotoxy(0,4)
```

### Reading user input

```
?"What is your name? "
N=?
?"Hello, "+N+"!\n"
```

### Random walk

```
clear
Xgetw/2
Ygeth/2
setfore(3)
gotoxy(X,Y)
putch(32)
Dfloor('*4)
X+(D=0)-(D=1)
Y+(D=2)-(D=3)
Xmax(1,min(X,getw-2))
Ymax(1,min(Y,geth-2))
gotoxy(X,Y)
putch(42)
#:=0*5
```

### GDI graphics: bouncing ball

```
gopen(640,480)
gbr(0,0,0)
gclear()
grefresh()
X=320
Y=240
P=3
Q=2
R=15
E=0
elapsed()
W=#
E=E+elapsed()
#=(E<16)*(W+1)
E=0
gpen(0,0,0)
gbr(0,0,0)
gfillcircle(X,Y,R)
X=X+P
Y=Y+Q
U=(X<R)|(X>(639-R))
V=(Y<R)|(Y>(479-R))
P=P*(1-(2*U))
Q=Q*(1-(2*V))
gpen(0,200,255)
gbr(0,200,255)
gfillcircle(X,Y,R)
grefresh()
K=:
#=(K=0)*(W+1)
```

### GDI graphics: bouncing ball (esoteric version)

```
gopen(640,480)
gbr(0,0,0)
gclear;grefresh
X320;Y240
P3;Q2
R15;E0
elapsed
W#+1
E+elapsed()
#E<16*W
E0
gpen(0,0,0);gbr(0,0,0)
gfillcircle(X,Y,R)
X+P;Y+Q
UX<R|(X>(639-R))
VY<R|(Y>(479-R))
P*(1-(2*U))
Q*(1-(2*V))
gpen(0,200,255);gbr(0,200,255)
gfillcircle(X,Y,R)
grefresh
#:=0*W
```

**Draw a circle on each click:** (in graphics window)
```
gopen(640,480)
gclear()
grefresh()
W=#
C=gmclick()
#=(C=0)*(W+1)
gpen(255,255,0)
gfillcircle(gmx(),gmy(),10)
grefresh()
#=W
```

**Paint with drag:**  (in graphics window)
```
gopen(640,480)
gclear()
grefresh()
gpen(255,0,0)
W=#
#=!gmdrag(1)*(W+1)
gpixel(gmx(),gmy())
grefresh()
#=W
```

**Write an X at each click position:** (in text window)
```
W=#
K:
C=tmclick()
#=(C=0)*(W+1)
gotoxy(tmx(),tmy())
putch(88)
#=W
```
