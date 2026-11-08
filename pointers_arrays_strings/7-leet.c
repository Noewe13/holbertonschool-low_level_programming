#include "main.h"

/**
 * leet - encodes a string into 1337
 * @s: the string to encode
 *
 * Return: a pointer to s
 */
char *leet(char *s)
{
	char *letters = "aAeEoOtTlL";
	char *codes = "4433007711";
	int i;
	int j;

	for (i = 0; s[i] != '\0'; i++)
	{
		for (j = 0; letters[j] != '\0'; j++)
		{
			if (s[i] == letters[j])
				s[i] = codes[j];
		}
	}

	return (s);
}
