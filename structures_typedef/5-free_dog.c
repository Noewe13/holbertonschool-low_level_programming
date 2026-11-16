#include "dog.h"
#include <stdlib.h>

/**
 * free_dog - frees a dog created by new_dog
 * @d: pointer to the dog to free
 *
 * Return: Nothing.
 */
void free_dog(dog_t *d)
{
	if (d == NULL)
		return;

	free(d->name);
	free(d->owner);
	free(d);
}
