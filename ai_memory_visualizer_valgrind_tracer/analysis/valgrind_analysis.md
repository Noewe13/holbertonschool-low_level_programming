# Valgrind Analysis — AI Memory Tracer

## 0. Method

Command used for every program:

```
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./<program>
```

| Flag | Why |
|---|---|
| `--leak-check=full` | list every leaked block with the stack trace of its `malloc` |
| `--show-leak-kinds=all` | also show "still reachable" / "indirectly lost" blocks, not only "definitely lost" |
| `--track-origins=yes` | if an uninitialised value is used, show where it was created |

Binaries are compiled with `-g`, so Valgrind can print file names and line numbers.

Build note: with gcc 12+, `make` stops on `aliasing_example.c` (`-Werror=use-after-free`, see
`memory_maps.md` section 2.1), so `crash_example` is not built by a plain `make`.
I built `crash_example` with `make crash_example`, and `aliasing_example` once without `-Werror`.

### Summary of all findings

| Program | Valgrind finding | Classification | Memory object | Region |
|---|---|---|---|---|
| `stack_example` | none (0 errors) | — | — | stack only |
| `heap_example` | 6 bytes definitely lost | memory leak due to lost ownership | `alice->name` ("Alice\0") | heap |
| `aliasing_example` | Invalid read of size 4 (line 42) | use-after-free (read through dangling alias) | block H, `b[2]` | heap |
| `aliasing_example` | Invalid write of size 4 (line 44) | use-after-free (write through dangling alias) | block H, `b[3]` | heap |
| `aliasing_example` | Invalid read of size 4 (line 45) | use-after-free (read through dangling alias) | block H, `b[3]` | heap |
| `crash_example` | Invalid write of size 4 at address 0x0 + SIGSEGV (line 32) | NULL pointer dereference | `nums` (stack) holding `NULL` | stack pointer, no heap block |
| all | no "Conditional jump depends on uninitialised value" / "uninitialised value" message | no use of uninitialised memory | — | — |

---

## 1. `stack_example` — no errors

```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: 1 allocs, 1 frees, 4,096 bytes allocated
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

**Interpretation**

- 0 errors is expected: every object is a local variable used only while its frame is alive.
  `p_local` points to `local_int` in the **same** frame and is never used after `dump_frame` returns.
- "1 allocs, 4,096 bytes": the program itself never calls `malloc`. This block is the **stdout buffer**
  allocated internally by `printf` (glibc) on first use, and freed by the C library at exit.
  It is not our memory and not a bug.
- `local_buf[2..15]` are uninitialised (see `memory_maps.md`), but Valgrind reports nothing,
  because Memcheck only complains when an uninitialised value is **used** (in a condition, a
  system call, an address). Those bytes are never read, so there is nothing to report.
  An uninitialised byte that is never used is not an error.

---

## 2. `heap_example` — memory leak due to lost ownership

### 2.1 Valgrind output

```
HEAP SUMMARY:
    in use at exit: 6 bytes in 1 blocks
  total heap usage: 5 allocs, 4 frees, 4,138 bytes allocated

6 bytes in 1 blocks are definitely lost in loss record 1 of 1
   at 0x4846828: malloc (vg_replace_malloc.c)
   by 0x109211: person_new (heap_example.c:21)
   by 0x1092FA: main (heap_example.c:51)

LEAK SUMMARY:
   definitely lost: 6 bytes in 1 blocks
```

### 2.2 Classification

**Memory leak — "definitely lost" — leak due to lost ownership.**

### 2.3 Which object

- Allocated at `heap_example.c:21` (`p->name = malloc(len + 1)`), inside `person_new`,
  called from `main` line 51 → this is the call `person_new("Alice", 30)`.
- Size 6 = `len + 1` = 5 letters of "Alice" + 1 byte for `'\0'`.
- So the leaked object is **the name buffer of Alice**, `alice->name`.

Counting the allocations confirms it:

| # | Allocation | Size | Freed? |
|---|---|---|---|
| 1 | stdout buffer (glibc, by `printf`) | 4096 | yes, by libc at exit |
| 2 | `Person` Alice (line 14) | 16 | yes, line 69 via `person_free_partial` |
| 3 | `"Alice"` (line 21) | 6 | **no** |
| 4 | `Person` Bob (line 14) | 16 | yes, line 67 |
| 5 | `"Bob"` (line 21) | 4 | yes, line 66 |

Total = 4096 + 16 + 6 + 16 + 4 = **4138 bytes, 5 allocs, 4 frees**: exactly Valgrind's summary.

### 2.4 Lifetime / ownership violation

Ownership chain of the "Alice" buffer:

1. Line 21: owned by `p->name` inside the new `Person` struct (heap).
2. Line 51: `person_new` returns, its local `p` dies, but `main`'s `alice` now points to the struct.
   The buffer is reachable through `alice->name`. Owner: the `Person` struct, which is owned by `alice`.
3. Line 69: `person_free_partial(alice)` calls only `free(p)`. The struct is released.
   **The only pointer to the "Alice" buffer was stored inside that struct**, so this pointer
   disappears with it.
4. From this moment, no pointer anywhere (stack, heap, globals) contains the address of the buffer.
   It still exists (it was never freed) but it can never be freed again.

This is why Valgrind says **"definitely lost"** and not "still reachable": at exit, Valgrind scans
all memory for a pointer to the block and finds none.

Why "indirectly lost: 0": a block is "indirectly lost" when it is only reachable through another
lost block. Here the parent struct was **freed** (not lost), so the name buffer is directly lost.

Note: Valgrind points to the line of the **allocation** (21), not to the line where the ownership was
lost (69). A leak has no "bad line" that executes; it is the absence of a `free`. Locating line 69
requires the reasoning above.

Correct ownership rule: whoever frees a `Person` must free `name` first, then the struct
(as done for Bob, lines 66-67).

---

## 3. `aliasing_example` — use-after-free through a dangling alias

### 3.1 Valgrind output (3 errors)

```
Invalid read of size 4
   at 0x1092F5: main (aliasing_example.c:42)
 Address 0x4a7f088 is 8 bytes inside a block of size 20 free'd
   at 0x484988F: free
   by 0x1092D1: main (aliasing_example.c:38)
 Block was alloc'd at
   at 0x4846828: malloc
   by 0x1091E4: make_numbers (aliasing_example.c:12)
   by 0x109272: main (aliasing_example.c:30)

Invalid write of size 4
   at 0x109315: main (aliasing_example.c:44)
 Address 0x4a7f08c is 12 bytes inside a block of size 20 free'd
   (same free at line 38, same malloc at line 12)

Invalid read of size 4
   at 0x109323: main (aliasing_example.c:45)
 Address 0x4a7f08c is 12 bytes inside a block of size 20 free'd
   (same free at line 38, same malloc at line 12)

All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 3 errors from 3 contexts
```

### 3.2 How to read one message

| Part of the message | Meaning |
|---|---|
| `Invalid read` / `Invalid write` | the program accessed memory it does not own |
| `of size 4` | 4 bytes accessed = one `int` |
| `at main (aliasing_example.c:42)` | the line performing the bad access |
| `8 bytes inside a block of size 20` | offset 8 in a 20-byte block → `20 / 4 = 5` ints, offset `8 / 4` = index **2** |
| `free'd ... main (aliasing_example.c:38)` | the block was already released by `free(a)` |
| `Block was alloc'd at ... make_numbers (:12)` | the block is the array created in `make_numbers` |

Valgrind gives the **full lifetime** of the object: born line 12, died line 38, touched line 42.

### 3.3 The memory object

Block H: 20 bytes on the heap, `malloc` at line 12, array `{0, 11, 22, 33, 44}`.

- Line 30: `a` receives its address → `a` is the owner.
- Line 34: `b = a` → `b` is an **alias** (same address, no copy of the data, no new allocation).
- Line 38: `free(a)` ends the lifetime of block H. `free` gets a copy of the address, so neither
  `a` nor `b` is modified: both still contain `0x4a7f080`, both are now **dangling pointers**.

### 3.4 Each error

**Error 1 — line 42, `b[2]` read → use-after-free (invalid read)**
Reads offset 8 (index 2) of block H after its lifetime ended at line 38.
The `free` was done through `a`, the access is done through `b`: the alias is what hides the bug
in the source code, because the name `b` never appears in the `free` call.

**Error 2 — line 44, `b[3] = 1234` → use-after-free (invalid write)**
Writes offset 12 (index 3) of the freed block. A write is more dangerous than a read:
in a normal run (without Valgrind), the allocator reuses freed memory for its own bookkeeping
and for later `malloc` calls, so this write can corrupt allocator data or another object.

**Error 3 — line 45, `b[3]` read → use-after-free (invalid read)**
Same block, same offset as error 2. It "works" (prints 1234) only because nothing reused the
block between line 44 and line 45. Still undefined behaviour.

### 3.5 What the output does NOT contain, and why

- **No leak**: "All heap blocks were freed". Block H **was** freed, exactly once. The bug is not a
  missing `free`, it is using the memory **after** the `free`.
- **No crash**: the freed block is still inside memory mapped by the process, so the CPU allows the
  access. The error is a violation of the C object lifetime rules, not of hardware memory protection.
  Only Memcheck (which tracks the state of every heap byte) can see it.
- **No "double free"**: `free` is called once. If the program had also called `free(b)`, Valgrind
  would report `Invalid free() / delete / delete[]`, because `b` aliases the same block.

### 3.6 Run with and without Valgrind give different values

| Line 42 output | Native run | Under Valgrind |
|---|---|---|
| `reading b[2]=` | `1411303502` | `22` |

- Native: glibc puts the freed chunk in its tcache list and writes a "key" in bytes 8-15 of the
  chunk, which overwrites `b[2]`.
- Valgrind: Memcheck replaces `malloc`/`free` with its own versions and keeps freed blocks in a
  "quarantine" queue without reusing them, so the old value 22 is still there.

This proves the value read after a `free` is **not reliable**: it depends on the allocator.
The correct statement is "the read is invalid", never "the read returns X".

---

## 4. `crash_example` — invalid write at address 0x0

(Full root-cause analysis in `crash_report.md`.)

```
Invalid write of size 4
   at 0x10928A: main (crash_example.c:32)
 Address 0x0 is not stack'd, malloc'd or (recently) free'd

Process terminating with default action of signal 11 (SIGSEGV)
 Access not within mapped region at address 0x0
   at 0x10928A: main (crash_example.c:32)

LEAK SUMMARY:
   still reachable: 4,096 bytes in 1 blocks
```

**Classification:** NULL pointer dereference (invalid write) → SIGSEGV.

- `Address 0x0 is not stack'd, malloc'd or (recently) free'd`: the address is not in any stack
  frame, not in any heap block, and not in a recently freed block. So this is **not** a
  use-after-free and not a heap overflow. Address 0 is the value of `NULL`.
- `nums` (a stack variable of `main`) contains `NULL` because `allocate_numbers(0)` returns `NULL`
  at line 10 when `n <= 0`, **without calling `malloc`**. No heap object is involved at all.
- `size 4`: `nums[0] = 42` writes one `int`.
- `Access not within mapped region`: page 0 is never mapped, so the CPU raises a fault and the
  kernel sends SIGSEGV. Unlike the use-after-free above, this one is detected by the hardware.
- "still reachable: 4,096 bytes": the stdout buffer of `printf`. The program was killed before
  libc could free it. Not a leak of the program's data.

---

## 5. AI usage and AI errors found

AI was used to get a first interpretation of each Valgrind report. Every claim was checked
against the source code, the allocation counts and the memory maps of Task 0.

### AI error 1 — "heap_example leaks at line 21, the malloc is wrong"

- **AI claim:** "The leak is caused by line 21: the `malloc` for the name is never matched.
  Fix line 21."
- **Why it is incorrect:** line 21 is correct. Valgrind prints the stack trace of the **allocation**
  because that is the only event it can attach to a block. The ownership is actually lost at
  **line 69**, where `person_free_partial` frees the struct that holds the only pointer to the name.
  Bob's name, allocated by the same line 21, is **not** leaked, which proves line 21 itself is fine.
- **Correction:** a leak trace answers "where was it born?", not "where was it lost?".

### AI error 2 — "Bob is leaked too / indirectly lost"

- **AI claim:** "Both persons leak their name; Alice's name is indirectly lost because it was inside
  a struct."
- **Why it is incorrect:** Valgrind reports **1** block of **6** bytes. "Bob\0" would be 4 bytes and is
  freed at line 66. And "indirectly lost" means reachable only through another *lost* block;
  Alice's struct was *freed*, not lost, so the name is "definitely lost".
- **Correction:** checked by the allocation table in section 2.3 (5 allocs, 4 frees, 4138 bytes).

### AI error 3 — "b[2] after free still returns 22, so the read is harmless"

- **AI claim:** "The data is still in memory after `free`, so reading `b[2]` returns 22; only the write
  at line 44 is really a problem."
- **Why it is incorrect:** this was true **under Valgrind only**. The native run printed `1411303502`.
  Any access after `free` is undefined behaviour; a read can return allocator metadata or another
  object's data (information leak). Valgrind counts it as an error exactly like the write.
- **Correction:** section 3.6.

### AI error 4 — "crash_example: use-after-free / malloc failed"

- **AI claim:** "The crash happens because `malloc` failed and returned NULL" (or "because the memory
  was already freed").
- **Why it is incorrect:** with `n = 0`, `malloc` is **never called** (`return NULL` at line 10
  happens before line 12). And Valgrind says the address is "not ... (recently) free'd", which rules
  out a use-after-free. The NULL is an intentional return value that `main` does not check.
- **Correction:** see `crash_report.md`.
