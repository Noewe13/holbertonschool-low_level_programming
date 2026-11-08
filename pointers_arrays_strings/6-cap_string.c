#include "main.h"

/**
 * is_separator - checks if a character separates words
 * @c: the character to check
 *
 * Return: 1 if c is a separator, 0 otherwise
 */
int is_separator(char c)
{
	char *seps = " \t\n,;.!?\"(){}";
	int i;

	for (i = 0; seps[i] != '\0'; i++)
	{
		if (c == seps[i])
			return (1);
	}

	return (0);
}

/**
 * cap_string - capitalizes all words of a string
 * @s: the string to modify
 *
 * Return: a pointer to s
 */
char *cap_string(char *s)
{
	int i;

	i = 0;
	while (s[i] != '\0')
	{
		if (s[i] >= 'a' && s[i] <= 'z' &&
		    (i == 0 || is_separator(s[i - 1])))
			s[i] = s[i] - 32;
		i++;
	}

	return (s);
}
