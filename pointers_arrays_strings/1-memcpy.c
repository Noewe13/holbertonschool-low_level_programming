#include "main.h"

/**
 * _memcpy - copies a memory area
 * @dest: the memory area to copy to
 * @src: the memory area to copy from
 * @n: the number of bytes to copy
 *
 * Return: a pointer to dest
 */
char *_memcpy(char *dest, char *src, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		dest[i] = src[i];

	return (dest);
}
