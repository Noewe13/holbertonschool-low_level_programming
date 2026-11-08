#include "main.h"

/**
 * _strspn - gets the length of a prefix substring
 * @s: the string to scan
 * @accept: the bytes allowed in the prefix
 *
 * Return: the number of bytes at the start of s made only of accept bytes
 */
unsigned int _strspn(char *s, char *accept)
{
	unsigned int count;
	int j;

	count = 0;
	while (s[count] != '\0')
	{
		for (j = 0; accept[j] != '\0'; j++)
		{
			if (s[count] == accept[j])
				break;
		}
		if (accept[j] == '\0')
			return (count);
		count++;
	}

	return (count);
}
