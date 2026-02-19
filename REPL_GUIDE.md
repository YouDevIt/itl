# ITL REPL Guide

The ITL **REPL** (Read-Eval-Print Loop) is an interactive mode that lets you type and execute ITL statements one at a time, inspect variables, and incrementally build programs that you can run again from any line.

---

## Starting the REPL

Run the interpreter with no arguments:

```
itl.exe
```

You will see:

```
ITL (Incredibly Tiny Language) Advanced REPL v4.0
Type ':help' for the list of commands or start programming!
Type ':exit' to quit.

1>
```

The number before `>` is the line number that the next statement you type will occupy.

---

## Typing statements

Type any valid ITL statement and press Enter. The interpreter executes it immediately.

```
1> A42
< A = 42
2> ?"The answer is "+A+"\n"
The answer is 42
3>
```

The `< A = 42` line is the REPL's assignment echo – it shows you what was assigned to which variable. This feedback is shown for every assignment.

### Multiple statements on one line

Use `;` to write several statements at once:

```
3> A1; B2; CA+B; ?C+"\n"
< A = 1
< B = 2
< C = 3
3
4>
```

Each `;`-separated segment is stored as a separate numbered line, so the prompt skips several numbers in one go.

---

## Building a program

Everything you type accumulates as a numbered program. You can see it at any time with `:lines`:

```
1> A=1
< A = 1
2> A+1
< A = 2
3> #=(A<10)*2
4> :lines
Program (3 lines):
    1: A=1
    2: A+1
    3: #=(A<10)*2
```

Because lines 2 and 3 are already in memory, the loop they form can be re-run: just type `:reset` to clear everything and re-enter them, or use `#=1` to jump back to the start:

```
4> A=1; #=2
```

This sets A back to 1 and re-enters the loop.

---

## REPL commands

REPL commands start with `:`. They can also be embedded inside a program (as a line starting with `:`).

### `:help`

Displays the quick-reference card for both REPL commands and ITL syntax.

### `:vars`

Lists all defined variables and their current values:

```
:vars
A = 10
B = "hello"
_ = 3.14159265358979
```

### `:array`

Shows the contents of the array (up to the first 20 elements):

```
:array
Array (size: 5):
  @0 = 1
  @1 = 4
  @2 = 9
  @3 = 16
  @4 = 25
```

### `:lines`

Shows the stored program (up to the first 50 lines):

```
:lines
Program (3 lines):
    1: N=1
    2: ?N*N+"\n"
    3: N+1; #=(N<6)*2
```

### `:debug VAR`

Shows the raw byte content of a variable. Useful for diagnosing unexpected string values:

```
:debug A
Variable A (string):
  Content: "hello"
  Bytes (hex): 68 65 6C 6C 6F
  Bytes (dec): 104 101 108 108 111
```

Works with any single-letter variable including `_`:

```
:debug _
```

### `:clear`

Clears all variable values and the array. The stored program lines remain.

### `:reset`

Clears everything: variables, array, and all stored program lines. The REPL resets to its initial empty state. The line counter returns to 1.

### `:exit` / `:quit`

Exits the REPL.

---

## Assignment echo

In REPL mode every assignment prints a line of the form:

```
< VARNAME = value
```

This is intentional feedback to help you track state interactively. It does not appear when running a file.

---

## Jumping and re-running parts of the program

Because `#` is the line counter, you can jump to any stored line by assigning to it:

```
10> #=1         (* restart from line 1 *)
```

This lets you iterate on a loop without retyping. The entire program up to the current last line is available.

---

## Practical workflow

1. **Sketch** – type short expressions to experiment:

   ```
   1> sin(pi/4)
   < undefined   (* sin returns a number but there's no assignment *)
   2> A=sin(pi/4); ?A+"\n"
   < A = 0.707106781186548
   0.707106781186548
   ```

2. **Build** – add lines one at a time, checking output as you go.

3. **Inspect** – use `:vars` and `:array` to check state.

4. **Re-run** – use `#=1` or `#=N` to jump to a line and continue from there.

5. **Save** – once happy with the REPL session, copy the output of `:lines` into a `.itl` file and run it with `itl.exe myprogram.itl`.

---

## Tips and tricks

- **Newlines in output**: the `?` print statement does not add a newline automatically. Always end string output with `"\n"` if you want to move to the next line.

  ```
  ?"Done!\n"
  ```

- **Quick math**: type any expression to see its value via the assignment echo. Assign to `_` as a scratch variable:

  ```
  _ = sqrt(2)*pi
  < _ = 4.44288293815837
  ```

- **Conditionals via multiplication**: since there are no `IF` statements, use the fact that `<`, `>`, `=` return 1 or 0:

  ```
  ?"positive\n" * (A>0)    (* prints only if A > 0, otherwise multiplies by 0 *)
  ```

  Actually, for conditional output it is cleaner to use a conditional jump:

  ```
  #=(A>0)*NEXT_LINE+SKIP_LINE*(!(A>0))
  ```

- **Screen sizing**: always use `getw()` and `geth()` rather than hard-coded dimensions so that your program adapts to any terminal size.

- **Color reset**: after using colors, restore defaults to avoid affecting REPL prompt display:

  ```
  setfore(7); setback(0); setattr(0)
  ```

---

## Running a file

Pass a source file path as the first argument:

```
itl.exe myprogram.itl
```

- There is no REPL prompt.
- Assignment echo is disabled.
- Errors print a message and terminate the interpreter.
- The full PDCurses terminal is still active (screen functions work).
- The terminal is restored when the program ends or exits.

Source files use the same syntax as the REPL. You can use `;` on a single physical line to write compact programs:

```
N=1; ?N+"\n"; N+1; #=(N<11)*2
```

Or spread them over multiple lines for readability:

```
N = 1
? N + "\n"
N + 1
#= (N < 11) * 2
```
