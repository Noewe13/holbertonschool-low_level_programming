#include "main.h"

/**
 * _strncat - concatenates at most n bytes of src to dest
 * @dest: the string to append to
 * @src: the string to append
 * @n: the maximum number of bytes to use from src
 *
 * Return: a pointer to the resulting string dest
 */
char *_strncat(char *dest, char *src, int n)
{
	int i;
	int j;

	i = 0;
	while (dest[i] != '\0')
		i++;

	j = 0;
	while (j < n && src[j] != '\0')
	{
		dest[i] = src[j];
		i++;
		j++;
	}
	dest[i] = '\0';

	return (dest);
}
