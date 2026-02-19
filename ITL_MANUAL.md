# ITL Language Manual

**ITL (Incredibly Tiny Language)** is a minimalist interpreted language inspired by VTL-2 (Very Tiny Language 2, developed by Gary Shannon and Frank McCoy in 1976). Every line of source code is a statement. There are no keywords; meaning is entirely determined by the leading character of each line and by punctuation inside expressions.

**Philosophy of the language:** every program line is an assignment to a system or user variable. The `=` assignment operator between the variable and the expression is usually omitted.

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
13. [Special tokens](#13-special-tokens)
14. [Statement separator](#14-statement-separator)
15. [Parenthesis blocks](#15-parenthesis-blocks)
16. [Type conversion](#16-type-conversion)
17. [Comments](#17-comments)
18. [Error handling](#18-error-handling)
19. [Examples](#19-examples)

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

Blank lines are stored but silently skipped during execution.

---

## 2. Variables

ITL has **27 single-letter variables**: `A` through `Z` (uppercase only) and the underscore `_`.

Variables are dynamically typed: they hold either a **number** (double-precision float) or a **string** at any given moment.

A variable that has never been assigned is *undefined*. Using an undefined variable returns `0` numerically or `"0"` as a string. If an undefined variable is read for the first time, the interpreter performs a *forward reference scan*: it scans the remaining program lines to see if the variable is assigned there and, if so, executes that assignment first.

To explicitly set a variable back to *undefined*, write its name alone on a line:

```
A
```

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
- A function call: `sin(A)`, `gotoxy(10,5)`
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

A line beginning with `?` followed by an expression prints the expression:

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
#=N*0         (* jump to line 0 = stop execution *)
```

**Conditional jump** – multiply the target line by the condition (1 or 0):

```
#=(N<10)*#    (* stay on current line while N < 10; when N >= 10 jump to 0 *)
```

A jump to line `0` (or any value ≤ 0) terminates execution. A jump to a line beyond the last line also terminates.

#### Subroutine pattern

Since there are no `GOSUB`/`RETURN` statements, subroutines can be simulated using a return-address variable:

```
R=5; #=20      (* call subroutine at line 20, return to line 5 *)
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
3@5 = 99        (* write 99 to element 5 *)
0@N = X         (* write X to element N, using 0 as dummy base *)
```

The general syntax for an array write is:

```
base@index = value
```

The `base` value is parsed but ignored in the current implementation; use `0` or any number as a placeholder. The key part is `@index`.

Array indices are zero-based integers. Negative indices are clamped to 0. The maximum size is `MAX_ARRAY_SIZE` (1 000 000 elements).

---

## 11. Math functions

Functions are called with a lowercase name followed by parenthesised arguments. Multiple arguments are comma-separated.

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
| 1 | Red |
| 2 | Green |
| 3 | Yellow |
| 4 | Blue |
| 5 | Magenta |
| 6 | Cyan |
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

## 13. Special tokens

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

## 14. Statement separator

The semicolon `;` separates multiple statements on a single line (at the top level, i.e., outside parentheses and strings):

```
A=1; B=2; C=3; ?A+B+C+"\n"
```

Each segment becomes a separately numbered line internally.

---

## 15. Parenthesis blocks

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

## 16. Type conversion

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

## 17. Comments

ITL has no comment syntax. Common workarounds:

- Put a string at the start of a line — it is evaluated and the value discarded: `"-- this is a comment --"`.
- Use a variable assignment that you never use: `_ = 0`.

---

## 18. Error handling

Runtime errors (division by zero, unknown function, out-of-range values) are reported as messages on the screen. In **file mode** an unrecoverable error terminates the interpreter. In **REPL mode** errors are reported and execution continues from the next input.

Array indexing out of bounds: reading beyond the array returns 0; writing auto-expands the array.

---

## 19. Examples

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

### Simple animation (requires PDCurses)

```
clear()
X=0
setfore(2)
gotoxy(X,10)
putch(42)
gotoxy(X-1,10)
putch(32)
X+1
#=(X<getw())*2
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
clear()
X=getw()/2
Y=geth()/2
setfore(3)
gotoxy(X,Y)
putch(42)
D='*4
X=X+(D=0)-(D=1)
Y=Y+(D=2)-(D=3)
X=X*(X>0)*(X<getw()-1)+1*(X<1)+(getw()-2)*(X>getw()-2)
Y=Y*(Y>0)*(Y<geth()-1)+1*(Y<1)+(geth()-2)*(Y>geth()-2)
#=(K=0)*5
K=:
```
