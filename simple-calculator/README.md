# Simple Calculator

A simple calculator in C, used from the terminal.

## Menu
1) Add
2) Subtract
3) Multiply
4) Divide
0) Quit

## Numbers: decimals
The calculator uses decimal numbers (double) for A, B and every result.
- You can type integers (10) or decimals (2.5)
- 10 / 4 gives 2.5 (not 2)
- Results are printed without useless zeros: 35, -15, 2.5
- Dividing by 0 prints: Error: division by zero
- A choice not in the menu prints: Invalid choice

## Compile
gcc -Wall -Werror -Wextra -pedantic -std=gnu89 calculator.c -o calculator

## Run
./calculator
