#include <stdio.h>

/**
 * print_menu - prints the calculator menu
 */
void print_menu(void)
{
	printf("1) Add\n");
	printf("2) Subtract\n");
	printf("3) Multiply\n");
	printf("4) Divide\n");
	printf("0) Quit\n");
	printf("Choice: ");
}

/**
 * read_choice - reads the user's choice
 *
 * Return: the number typed, -1 if it is not a number, 0 on end of input
 */
int read_choice(void)
{
	int choice, ret, c;

	ret = scanf("%d", &choice);
	if (ret == EOF)
		return (0);
	if (ret != 1)
	{
		c = getchar();
		while (c != '\n' && c != EOF)
			c = getchar();
		return (-1);
	}
	return (choice);
}

/**
 * read_number - prompts for a number until a valid one is typed
 * @label: text shown before reading (example: "A: ")
 * @n: where to store the number
 *
 * Return: 1 on success, 0 on end of input
 */
int read_number(char *label, double *n)
{
	int ret, c;

	while (1)
	{
		printf("%s", label);
		ret = scanf("%lf", n);
		if (ret == 1)
			return (1);
		if (ret == EOF)
			return (0);
		c = getchar();
		while (c != '\n' && c != EOF)
			c = getchar();
		printf("Invalid number\n");
	}
}

/**
 * calculate - asks for A and B, then prints the result of the operation
 * @choice: the operation (1 add, 2 subtract, 3 multiply, 4 divide)
 */
void calculate(int choice)
{
	double a, b, result;

	if (!read_number("A: ", &a) || !read_number("B: ", &b))
		return;
	if (choice == 4 && b == 0)
	{
		printf("Error: division by zero\n");
		return;
	}
	if (choice == 1)
		result = a + b;
	else if (choice == 2)
		result = a - b;
	else if (choice == 3)
		result = a * b;
	else
		result = a / b;
	if (result == 0)
		result = 0;
	printf("Result: %.10g\n", result);
}

/**
 * main - entry point of the simple calculator
 *
 * Return: Always 0 (Success)
 */
int main(void)
{
	int choice;

	printf("Simple Calculator\n");
	while (1)
	{
		print_menu();
		choice = read_choice();
		if (choice == 0)
		{
			printf("Bye!\n");
			return (0);
		}
		if (choice < 1 || choice > 4)
			printf("Invalid choice\n");
		else
			calculate(choice);
	}
	return (0);
}
