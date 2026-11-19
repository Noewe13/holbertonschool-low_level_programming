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
			printf("Not implemented yet\n");
	}
	return (0);
}
