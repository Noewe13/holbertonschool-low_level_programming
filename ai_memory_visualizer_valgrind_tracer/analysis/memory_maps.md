# Memory Maps — AI Memory Visualizer

Programs analysed:

| Program | Status | Main topic |
|---|---|---|
| `stack_example.c` | mandatory | recursion, stack frames, lifetimes of locals |
| `aliasing_example.c` | mandatory | heap allocation, aliasing, dangling pointer, use-after-free |
| `heap_example.c` | optional | nested heap allocations, ownership, leak |

Build: `make` in `programs/` (`-Wall -Wextra -Werror -pedantic -std=gnu89 -g`, no `-O` flag, so no optimisation).

> **Note on addresses:** all addresses below come from one real run (x86-64 Linux, gcc 13).
> They change on every run because of ASLR (Address Space Layout Randomization).
> The **relative** positions (differences between addresses) are what matter, and they stay the same
> for the same binary.

Conventions:

- **STACK**: automatic storage. Created when a function is entered, destroyed when it returns.
  On x86-64 the stack grows **downward** (each new frame has lower addresses).
- **HEAP**: dynamic storage. Exists from `malloc` until `free`, regardless of which function is running.
- `X -> Y` means "pointer X contains the address Y".
- An **alias** exists when two or more pointers contain the same address.

---

## 1. `stack_example.c`

### 1.1 Code summary

```c
main()                      -> walk_stack(0, 3)
walk_stack(depth, max)      -> local: int marker = depth * 10
                               calls dump_frame("enter", depth)
                               if depth < max: walk_stack(depth + 1, max)
                               calls dump_frame("exit", depth)
dump_frame(label, depth)    -> locals: int local_int, char local_buf[16], int *p_local = &local_int
```

This program uses **no heap memory at all**. Every variable lives on the stack.
(The string literals `"enter"`, `"exit"` are in the read-only data segment, not on the stack or heap;
only the pointer `label` that points to them is on the stack.)

### 1.2 Real output (one run)

```
[enter] depth=0  &local_int=0x7ffd6528f964  local_buf=0x7ffd6528f970  &marker=0x7ffd6528f9b4  marker=0
[enter] depth=1  &local_int=0x7ffd6528f934  local_buf=0x7ffd6528f940  &marker=0x7ffd6528f984  marker=10
[enter] depth=2  &local_int=0x7ffd6528f904  local_buf=0x7ffd6528f910  &marker=0x7ffd6528f954  marker=20
[enter] depth=3  &local_int=0x7ffd6528f8d4  local_buf=0x7ffd6528f8e0  &marker=0x7ffd6528f924  marker=30
[exit]  depth=3  (same addresses and values as enter depth=3)
[exit]  depth=2  (same addresses and values as enter depth=2)
[exit]  depth=1  (same addresses and values as enter depth=1)
[exit]  depth=0  (same addresses and values as enter depth=0)
```

Observations taken from these numbers:

- `&marker` decreases by `0x30` (48 bytes) at each recursion level: each `walk_stack` frame is 48 bytes
  and each new one is placed **below** the previous one.
- `p_local == &local_int` every time: `p_local` is an alias of `local_int`, inside the same frame.
- The "exit" print of a given depth shows **the same addresses** as the "enter" print of that depth.

### 1.3 Step-by-step memory map

**Step 1: `main` calls `walk_stack(0, 3)`**

```
STACK (high addresses at the top)
+---------------------------------------------+
| main                                        |  no local variables
+---------------------------------------------+
| walk_stack #0   depth=0  max_depth=3        |
|                 marker = 0    @ ...f9b4     |
+---------------------------------------------+
HEAP: empty
```

**Step 2: `walk_stack #0` calls `dump_frame("enter", 0)`**

```
+---------------------------------------------+
| main                                        |
+---------------------------------------------+
| walk_stack #0   marker = 0    @ ...f9b4     |
+---------------------------------------------+
| dump_frame      label -> "enter" (rodata)   |
|                 depth = 0                   |
|                 local_int = 100  @ ...f964  |
|                 local_buf = "A\0" + 14 uninitialised bytes  @ ...f970 |
|                 p_local -> ...f964 (alias of local_int)        |
+---------------------------------------------+
```

`local_buf[2]` to `local_buf[15]` are never written: their values are **indeterminate**.
The program never reads them, so this is not a bug, but they must not be described as "zero".

**Step 3: `dump_frame` returns**

The `dump_frame` frame is **destroyed**: `local_int`, `local_buf`, `p_local` no longer exist.
The bytes at `...f964` are still physically in RAM, but they no longer belong to any variable
and will be overwritten by the next call.

```
+---------------------------------------------+
| main                                        |
+---------------------------------------------+
| walk_stack #0   marker = 0    @ ...f9b4     |   <- top of stack again
+---------------------------------------------+
```

**Step 4: recursion `walk_stack(1, 3)`**

The new frame is placed right below `walk_stack #0`, **in the same memory region that
`dump_frame("enter", 0)` used a moment ago** (`marker` of depth 1 is at `...f984`, between
`...f964` and `...f9b4`). Stack memory is reused, not reserved per call.

**Step 5: deepest point (depth = 3, inside `dump_frame("enter", 3)`)**

```
STACK
+---------------------------------------------+
| main                                        |
+---------------------------------------------+
| walk_stack #0   marker = 0    @ ...f9b4     |  alive (waiting)
+---------------------------------------------+
| walk_stack #1   marker = 10   @ ...f984     |  alive (waiting)
+---------------------------------------------+
| walk_stack #2   marker = 20   @ ...f954     |  alive (waiting)
+---------------------------------------------+
| walk_stack #3   marker = 30   @ ...f924     |  alive (running)
+---------------------------------------------+
| dump_frame      local_int = 103 @ ...f8d4   |
|                 local_buf = "D\0..." @ ...f8e0 |
|                 p_local -> ...f8d4           |
+---------------------------------------------+
HEAP: empty
```

5 frames of our code at the same time (main + 4 `walk_stack`) + 1 `dump_frame`.
There are **4 different `marker` variables**, one per frame, each with its own address and value.

**Step 6: unwinding**

`walk_stack #3` does not recurse (3 < 3 is false), calls `dump_frame("exit", 3)`, then returns.
Its frame is destroyed. Control goes back to `walk_stack #2` at line 28, where its own `marker`
(still 20, still at `...f954`) is intact, because the frames below it never touched it.

Order of destruction is the reverse of creation (LIFO): #3, #2, #1, #0, then `main` returns.

### 1.4 Lifetimes

| Variable | Frame | Created | Destroyed |
|---|---|---|---|
| `marker` (one per depth) | `walk_stack` | entry of that `walk_stack` call | return of that `walk_stack` call |
| `local_int`, `local_buf`, `p_local` | `dump_frame` | entry of each `dump_frame` call (8 calls in total) | return of that `dump_frame` call |
| `label`, `depth` (params) | `dump_frame` | entry | return |
| `depth`, `max_depth` (params) | `walk_stack` | entry | return |
| `"enter"`, `"exit"` | read-only data | program start | program end |

### 1.5 Why "exit" shows the same addresses as "enter"

`dump_frame("enter", d)` and `dump_frame("exit", d)` are **two different calls**, with **two different
`local_int` variables**. The first one was destroyed long before the second was created.
They have the same address only because both calls are made from the same `walk_stack #d` frame,
so the stack pointer has the same value both times and the new frame is placed at the same spot.
Same address does **not** mean same variable.

### 1.6 Aliasing in this program

- `p_local -> local_int`: valid alias, because both live in the same frame and die at the same time.
- It would become a **dangling pointer** if `dump_frame` returned `p_local` (the caller would get the
  address of a destroyed variable). The program does not do this, so there is no bug here.

---

## 2. `aliasing_example.c`

### 2.1 Important compilation note

With gcc 13, the provided Makefile **refuses to build this program**:

```
aliasing_example.c:42: error: pointer 'b' used after 'free' [-Werror=use-after-free]
aliasing_example.c:44: error: pointer 'b' used after 'free' [-Werror=use-after-free]
aliasing_example.c:45: error: pointer 'b' used after 'free' [-Werror=use-after-free]
```

Recent gcc versions (12+) detect this use-after-free at compile time, and `-Werror` turns the warning
into an error. To observe the runtime behaviour I compiled it once without `-Werror`
(`gcc -Wall -Wextra -pedantic -std=gnu89 -g`). With an older gcc the Makefile builds it normally.

### 2.2 Real output (one run)

```
aliasing_example: aliasing and use-after-free (Valgrind should report it)
  a=0x55892fe022b0 b=0x55892fe022b0 a[2]=22 b[2]=22
  after free(a): b=0x55892fe022b0 (dangling)
  reading b[2]=1411303502
  wrote b[3]=1234
```

### 2.3 Step-by-step memory map

**Step 1: start of `main` (lines 24-26)**

```
STACK
+-----------------------------+
| main  a = NULL              |
|       b = NULL              |
|       n = 5                 |
+-----------------------------+
HEAP: empty
```

**Step 2: inside `make_numbers(5)`, after `malloc` and the loop (lines 12-17)**

`malloc(5 * sizeof(int))` = 20 bytes on the heap.

```
STACK                                  HEAP
+-----------------------------+
| main  a = NULL              |
|       b = NULL              |        block H (20 bytes) @ 0x...22b0
|       n = 5                 |        +----+----+----+----+----+
+-----------------------------+        | 0  | 11 | 22 | 33 | 44 |
| make_numbers  n = 5         |        +----+----+----+----+----+
|               i = 5         |         [0]  [1]  [2]  [3]  [4]
|               arr --------------->   (offsets 0, 4, 8, 12, 16)
+-----------------------------+
```

**Step 3: `make_numbers` returns, `a = make_numbers(n)` (line 30)**

The `make_numbers` frame is destroyed (`arr`, `i`, its `n` disappear).
**The heap block survives**: heap lifetime does not depend on the function that called `malloc`.
The address is copied into `a`, so `a` is now the owner of the block.

```
STACK                                  HEAP
+-----------------------------+        block H (20 bytes) @ 0x...22b0
| main  a ---------------------------> [ 0 | 11 | 22 | 33 | 44 ]
|       b = NULL              |
|       n = 5                 |
+-----------------------------+
```

**Step 4: `b = a;` (line 34): aliasing**

Only the **address** is copied. No new memory is allocated, the array is not duplicated.

```
STACK                                  HEAP
+-----------------------------+        block H (20 bytes) @ 0x...22b0
| main  a ---------------------------> [ 0 | 11 | 22 | 33 | 44 ]
|       b ---------------------------^
|       n = 5                 |
+-----------------------------+
```

`a` and `b` are aliases: `a[2]` and `b[2]` are the **same 4 bytes** (offset 8), so both print 22.
Writing through one would be visible through the other.
There is only **one** block, so it must be freed **exactly once**, through either pointer.

**Step 5: `free(a);` (line 38)**

```
STACK                                  HEAP
+-----------------------------+        block H @ 0x...22b0: FREED
| main  a = 0x...22b0 (dangling) ----> [ returned to the allocator ]
|       b = 0x...22b0 (dangling) ---^
|       n = 5                 |
+-----------------------------+
```

What `free` does and does **not** do:

- It gives the 20 bytes back to the allocator. The program no longer owns them.
- It does **not** change `a`: `free` receives a **copy** of the address, so it cannot modify the
  variable `a` in `main`. `a` still contains `0x...22b0`.
- It does **not** change `b` either (the program prints `b=0x55892fe022b0` after the free, the same value).
- So **both** `a` and `b` are now dangling pointers. The program only uses `b` afterwards.

**Step 6: `printf(... b[2])` (line 42): invalid read (use-after-free)**

`b[2]` reads the 4 bytes at offset 8 of a freed block. Result: **1411303502**, not 22.
Why: glibc stores its own bookkeeping data inside a freed block (in the tcache free list:
a "next" pointer in the first 8 bytes, and a "key" value in the next 8 bytes). Offset 8 is exactly
where the key is written, so `b[2]` now shows part of that allocator data.
This is undefined behaviour: the printed value is not guaranteed to be anything.

**Step 7: `b[3] = 1234;` (line 44): invalid write (use-after-free)**

Writes 4 bytes at offset 12 of the freed block, which overwrites the other half of the allocator's
key field. The program does not crash here, but it is corrupting memory that the allocator owns.
In a larger program, this block could already have been given to another `malloc`, and this write
would silently corrupt someone else's data.

**Step 8: `printf(... b[3])` (line 45): invalid read**

Prints 1234 only because nothing reused the block between line 44 and 45. Still undefined behaviour.

### 2.4 Valgrind confirmation

```
Invalid read of size 4   at main (aliasing_example.c:42)
  Address ... is 8 bytes inside a block of size 20 free'd
    by main (aliasing_example.c:38)
  Block was alloc'd at make_numbers (aliasing_example.c:12)
Invalid write of size 4  at main (aliasing_example.c:44)
  Address ... is 12 bytes inside a block of size 20 free'd
Invalid read of size 4   at main (aliasing_example.c:45)
  Address ... is 12 bytes inside a block of size 20 free'd
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 3 errors from 3 contexts
```

This matches the map exactly:

- "size 4" = one `int`.
- "8 bytes inside" = `b[2]` (2 x 4), "12 bytes inside" = `b[3]` (3 x 4).
- "block of size 20" = `5 * sizeof(int)`.
- Freed at line 38, allocated at line 12: the full history of block H.
- No leak, because the block **was** freed (once). The bug is the use **after** the free, not a missing free.

### 2.5 Lifetimes and ownership

| Object | Storage | Lifetime |
|---|---|---|
| `a`, `b`, `n` (main) | stack | whole execution of `main` |
| `arr`, `i`, `n` (make_numbers) | stack | only during the `make_numbers` call |
| block H (20 bytes) | heap | from `malloc` (line 12) to `free(a)` (line 38) |

Ownership: `arr` (inside `make_numbers`) -> transferred to `a` by the `return` -> `b` becomes a
**non-owning alias**. After line 38, nobody owns the block, and any access through `a` or `b` is invalid.

Correct fix: do not use `b` after the free, or set both aliases to `NULL` right after it
(`free(a); a = NULL; b = NULL;`), because setting only `a = NULL` would leave `b` dangling.

---

## 3. `heap_example.c` (optional)

### 3.1 Map after both `person_new` calls (line 52)

Real addresses from one run: `alice=0x...42b0 name=0x...42d0`, `bob=0x...42f0 name=0x...4310`.

```
STACK                          HEAP
+-------------------+          Person A (16 bytes) @ ...42b0      "Alice\0" (6 bytes) @ ...42d0
| main              |          +-----------------+               +---------------+
|   alice ------------------>  | name ---------------------->    | A l i c e \0  |
|   bob   ------------+        | age = 30        |               +---------------+
+-------------------+ |        +-----------------+
                      |        Person B (16 bytes) @ ...42f0      "Bob\0" (4 bytes) @ ...4310
                      +----->  +-----------------+               +---------+
                               | name ---------------------->    | B o b \0 |
                               | age = 41        |               +---------+
                               +-----------------+
```

4 heap blocks. `sizeof(Person)` = 16 (8-byte pointer + 4-byte int + 4 bytes of padding).
The local variables of `person_new` (`p`, `i`, `len`) were destroyed when it returned,
but the blocks they pointed to survive.

### 3.2 Frees

- Line 66-67: `free(bob->name)` then `free(bob)`: correct order (the inner block first,
  because after `free(bob)` reading `bob->name` would itself be a use-after-free).
- Line 69: `person_free_partial(alice)` only does `free(p)`. The `Person` block is freed, but
  `alice->name` (6 bytes, "Alice") is never freed. The only pointer to it was stored **inside**
  the freed struct, so after this line no pointer anywhere refers to it: **6 bytes definitely lost**.

---

## 4. AI errors found and corrected

AI explanations were treated as hypotheses and checked against the source code, the real program
output, gcc and Valgrind. These are the statements that were wrong, incomplete or misleading.

### Error 1: "After `free(a)`, `a` becomes NULL" / "b still points to valid data"

- **AI claim:** after `free(a)`, `a` is set to `NULL`, and since `b` is "a separate pointer",
  `b[2]` still safely reads 22.
- **Why it is wrong:**
  1. `free` takes the address **by value**. It physically cannot change the variable `a` in `main`.
     The program prints `b=0x55892fe022b0` after the free, the same address as before.
  2. `b` is not independent: `b = a` copied an address, not the array. Both pointers refer to the
     same single block, so freeing through `a` invalidates `b` too.
  3. `b[2]` did **not** print 22, it printed `1411303502`, because glibc wrote its free-list data
     into the freed block (offset 8 = the tcache key field).
- **Correction:** after line 38, `a` and `b` both still contain `0x...22b0` and are both dangling.
  Every access through them is a use-after-free (Valgrind: 2 invalid reads + 1 invalid write).

### Error 2: "The use-after-free always crashes the program"

- **AI claim:** writing `b[3] = 1234` after the free causes a segmentation fault.
- **Why it is wrong:** freed heap memory is usually still mapped in the process (the allocator
  keeps it for future `malloc` calls), so the CPU does not fault. The program runs to the end and
  prints `wrote b[3]=1234`. That is exactly why use-after-free is dangerous: it is silent.
- **Correction:** a segfault only happens when the address is not mapped at all (like `NULL` in
  `crash_example.c`). Here the access is *invalid* for the C language (undefined behaviour) but
  *valid* for the hardware. Only a tool like Valgrind (or gcc 13 at compile time) reveals it.

### Error 3: "Each recursive call keeps its own permanent space for `dump_frame`"

- **AI claim:** `dump_frame("enter", d)` and `dump_frame("exit", d)` print the same addresses
  because "it is the same `local_int` that was kept alive".
- **Why it is wrong:** `local_int` is destroyed at every return of `dump_frame`. Between "enter 0"
  and "exit 0" there were 6 other calls, and `walk_stack #1` itself was placed in the region
  `dump_frame("enter", 0)` had used (`marker` depth 1 at `...f984`, between `...f964` and `...f9b4`).
- **Correction:** same address because the caller frame is the same, so the stack pointer is the same.
  Two separate variables with separate lifetimes, at the same location at different times.

### Error 4 (omission): "`local_buf` contains `"A"` followed by zeros"

- **AI claim:** the 16-byte buffer is "A" then 15 zero bytes.
- **Why it is wrong:** local arrays are **not** initialised automatically in C. Only `[0]` and `[1]`
  are written. Bytes `[2]` to `[15]` contain whatever was previously at those stack addresses.
- **Correction:** described as indeterminate in the map. No bug, because they are never read.

---

## 5. Key takeaways

- Stack variables die when their function returns; the same address can later belong to a
  completely different variable.
- Heap blocks survive the function that allocated them and die only at `free`.
- Copying a pointer creates an alias, not a copy of the data. One block = one `free`, and after it
  **every** alias is dangling.
- `free(p)` never modifies `p`.
- A use-after-free usually does **not** crash; it silently reads allocator data or corrupts memory.
