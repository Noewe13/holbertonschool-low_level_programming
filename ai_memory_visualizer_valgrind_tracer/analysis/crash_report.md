# Crash Report — `crash_example.c`

## 1. Description of the crash

| Item | Value |
|---|---|
| Program | `crash_example` (built with `make crash_example`, flags `-Wall -Wextra -Werror -pedantic -std=gnu89 -g`) |
| Symptom | `Segmentation fault`, shell exit status **139** (= 128 + 11, signal 11 = SIGSEGV) |
| Faulting line | `crash_example.c:32` → `nums[0] = 42;` |
| Faulting address | `0x0` |
| Access type | **write** of 4 bytes (one `int`) |
| Reproducibility | 100 % deterministic: same line, same address on every run |

Output in a terminal:

```
crash_example: deterministic NULL dereference (segmentation fault)
  requesting n=0
Segmentation fault
```

The line `nums[0]=...` (line 34) is never printed: the process is killed at line 32.
(Side note: when stdout is redirected to a file or a pipe, even the two first lines can be missing,
because stdout is then fully buffered and the buffer is never flushed when the process is killed.
This is a property of stdio buffering, not a second bug.)

Evidence collected once (it confirms the reasoning below, it is not needed for it):

```
Valgrind:  Invalid write of size 4
              at main (crash_example.c:32)
           Address 0x0 is not stack'd, malloc'd or (recently) free'd
           Access not within mapped region at address 0x0  (signal 11, SIGSEGV)

gdb:       Program received signal SIGSEGV
           main () at crash_example.c:32   32   nums[0] = 42;
           => movl $0x2a,(%rax)      rax = 0x0      nums = (int *) 0x0
```

`movl $0x2a,(%rax)` = "store the value 42 (0x2a) at the address contained in register rax".
`rax` = 0, so the CPU tries to write to address 0.

---

## 2. Root cause analysis — full causal chain

The segfault is not the bug. It is the last step of the following chain:

| Step | Line | What happens | Memory state |
|---|---|---|---|
| 1 | 24-25 | `nums = NULL`, `n = 0` | STACK `main`: `nums = NULL`, `n = 0`. HEAP: nothing |
| 2 | 30 | `allocate_numbers(0)` is called | new STACK frame: `n = 0`, `arr = NULL`, `i = 0` |
| 3 | 9-10 | `if (n <= 0) return NULL;` → condition is true | the function returns **before** line 12: **`malloc` is never called**, no heap block exists |
| 4 | 30 | the frame of `allocate_numbers` is destroyed; return value `NULL` is stored in `nums` | STACK `main`: `nums = NULL` (0x0). HEAP: still nothing |
| 5 | — | **bug: `main` never checks `nums` before using it** | `nums` is a valid stack variable whose value is the null pointer |
| 6 | 32 | `nums[0] = 42` means `*(nums + 0) = 42` → write 4 bytes at address `0x0 + 0` = `0x0` | — |
| 7 | 32 | the CPU translates virtual address 0: page 0 is **never mapped** in a Linux process → page fault | — |
| 8 | 32 | the kernel finds no valid mapping → sends **SIGSEGV** to the process → default action: terminate | process killed, exit status 139 |

**Root cause:** `main` dereferences the return value of `allocate_numbers` without checking for
`NULL`, while `allocate_numbers` is *documented by its own code* to return `NULL` when `n <= 0`, and
`main` passes `n = 0`.

The function behaves correctly. The bug is in the **caller**: a broken contract
("this function can return NULL" was ignored).

---

## 3. Why the memory access is invalid

### 3.1 Stack, heap, or both?

- **Stack:** the pointer variable `nums` lives in `main`'s stack frame. It is alive and valid
  during the whole crash. Reading `nums` itself (to get the value 0) is perfectly legal.
- **Heap:** **not involved.** No block was ever allocated (`malloc` not called), so there is no
  heap object, no lifetime to violate, nothing to free.
- **The invalid access** is the write to the address **stored** in `nums`, i.e. address `0x0`,
  which belongs to **no object at all** (not the stack, not the heap, not global data).

### 3.2 Category of undefined behaviour

**Null pointer dereference** (C standard: applying `*` / `[]` to a null pointer is undefined
behaviour; CERT C rule EXP34-C, "Do not dereference null pointers").

It is **not**:

| Wrong category | Why it does not apply |
|---|---|
| use-after-free | nothing was freed; Valgrind: "not ... (recently) free'd" |
| heap buffer overflow | there is no heap block to overflow; Valgrind: "not ... malloc'd" |
| stack overflow | recursion depth is 1, the stack is almost empty; address 0 is not near the stack |
| uninitialised pointer | `nums` is initialised: first to `NULL` at line 24, then to the return value at line 30 |
| memory leak | nothing was allocated by the program |

### 3.3 Why it is deterministic

Undefined behaviour in general is not predictable, but this specific case always ends the same way
on Linux x86-64 because:

1. the value of `nums` is always exactly `0` (`n` is a constant 0, the `return NULL` path is always taken);
2. the operating system **never** maps the first page(s) of virtual memory
   (`vm.mmap_min_addr`, typically 65536), precisely so that NULL dereferences always fault;
3. so the write always targets an unmapped address and always produces SIGSEGV at line 32.

Compare with the use-after-free in `aliasing_example.c`: there the address points into memory
that **is** mapped, so the CPU does not fault and the bug is silent. Here the address is unmapped,
so the hardware itself detects the error.

(With optimisation, a compiler is allowed to assume a dereferenced pointer is not NULL and transform
the code. This build uses no `-O` flag, so the generated code is a direct store to `(%rax)` as shown
by gdb.)

---

## 4. Critique of AI-proposed causes and fixes

An AI assistant was asked: "Why does this program crash and how do I fix it?"

### 4.1 Proposed causes

| AI explanation | Verdict | Reason |
|---|---|---|
| "`nums` is NULL because `allocate_numbers(0)` returns NULL, and line 32 dereferences it" | **Correct** | matches the code (lines 9-10, 30, 32), Valgrind (address 0x0) and gdb (`nums = 0x0`) |
| "`malloc` failed because the system is out of memory" | **Incorrect** | `malloc` is never reached: `return NULL` at line 10 runs before line 12. Even `malloc(0)` would not mean "out of memory" |
| "`malloc(0)` returns NULL, which causes the crash" | **Incorrect / speculative** | `malloc(0)` is not called at all. (And if it were, glibc returns a unique non-NULL pointer for `malloc(0)`; writing to it would be a heap overflow, a different bug) |
| "The memory was already freed (use-after-free)" | **Incorrect** | the only `free` is at line 36, after the crash. Valgrind explicitly says "not (recently) free'd" |
| "Stack overflow" | **Incorrect** | no recursion, tiny frames; address 0 is nowhere near the stack (`0x1ffeff....` / `0x7ffd....` range) |
| "The crash is random / depends on memory contents" | **Incorrect** | the value of `nums` is fully determined by the code (always 0); see 3.3 |

### 4.2 Proposed fixes

| AI fix | Verdict | Reason |
|---|---|---|
| Check the return value: `if (nums == NULL) return (1);` before line 32 | **Correct** | fixes the root cause in the caller; the function's NULL contract is respected |
| Change `n = 0` to `n = 5` | **Partially correct** | the crash disappears for this input, but the missing check remains: any future `n <= 0` crashes again. It hides the bug instead of fixing it |
| Make `allocate_numbers` return `malloc(0)` or a static array when `n <= 0` | **Incorrect** | the program would write `nums[0]` into a 0-byte block (heap overflow) or into shared static data; it replaces a detected crash with silent corruption |
| Wrap line 32 in a signal handler for SIGSEGV | **Incorrect** | catches the symptom, not the cause; continuing after an invalid write is still undefined behaviour |
| "Remove `free(nums)`, it causes the crash" | **Incorrect** | line 36 is never reached; and `free(NULL)` is defined by the C standard to do nothing |

**Main limitation observed:** the AI listed several plausible causes with the same confidence, without
using the code to eliminate them. Reading lines 9-12 alone is enough to discard every "malloc"
explanation, and the Valgrind sentence "not stack'd, malloc'd or (recently) free'd" discards
use-after-free and overflow.

---

## 5. (Optional) Suggested fix — clearly labelled

> **SUGGESTED FIX (not applied, the task requires analysis only)**

```c
nums = allocate_numbers(n);
if (nums == NULL)
{
    fprintf(stderr, "allocate_numbers(%d) failed\n", n);
    return (1);
}
nums[0] = 42;
```

Ownership after the fix: if `allocate_numbers` returns non-NULL, `main` owns the block and must
`free(nums)` (line 36 already does). If it returns NULL, nothing was allocated and nothing must be freed.
