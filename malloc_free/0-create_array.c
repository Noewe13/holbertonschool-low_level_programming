#include "main.h"
#include <stdlib.h>

/**
 * create_array - creates an array of chars and fills it with a char
 * @size: size of the array
 * @c: char to fill the array with
 *
 * Return: pointer to the array, or NULL if size is 0 or if it fails
 */
char *create_array(unsigned int size, char c)
{
	char *array;
	unsigned int i;

	if (size == 0)
		return (NULL);

	array = malloc(sizeof(char) * size);
	if (array == NULL)
		return (NULL);

	for (i = 0; i < size; i++)
		array[i] = c;

	return (array);
}
