#include <stdio.h>
#include <time.h>

#define DATASET_SIZE 50000
#define SEED_VALUE 42u

static int dataset[DATASET_SIZE];

/**
 * next_value - advances a linear congruential generator
 * @state: pointer to the generator state
 *
 * Return: the new state value
 */
static unsigned int next_value(unsigned int *state)
{
	*state = (*state * 1103515245u) + 12345u;
	return (*state);
}

/**
 * build_dataset - fills the dataset with pseudo-random values
 */
static void build_dataset(void)
{
	unsigned int state;
	int i;

	state = SEED_VALUE;

	for (i = 0; i < DATASET_SIZE; i++)
		dataset[i] = (int)(next_value(&state) % 100000);
}

/**
 * process_dataset - applies an arithmetic transform to each element
 */
static void process_dataset(void)
{
	int i;
	int v;

	for (i = 0; i < DATASET_SIZE; i++)
	{
		v = dataset[i];
		v = (v * 3) + (v / 7) - (v % 11);
		if (v < 0)
			v = -v;
		dataset[i] = v;
	}
}

/**
 * reduce_checksum - reduces the dataset to a single checksum
 *
 * Return: the checksum
 */
static unsigned long reduce_checksum(void)
{
	unsigned long sum;
	int i;

	sum = 0;
	for (i = 0; i < DATASET_SIZE; i++)
		sum = (sum * 131ul) + (unsigned long)dataset[i];

	return (sum);
}

/**
 * elapsed - converts a clock interval to seconds
 * @start: clock value at the start of the interval
 * @end: clock value at the end of the interval
 *
 * Return: elapsed CPU time in seconds
 */
static double elapsed(clock_t start, clock_t end)
{
	return ((double)(end - start) / CLOCKS_PER_SEC);
}

/**
 * main - times each phase of the program with clock()
 *
 * Return: Always 0
 */
int main(void)
{
	unsigned long checksum;
	clock_t t_start, t_build, t_process, t_reduce;

	t_start = clock();
	build_dataset();
	t_build = clock();
	process_dataset();
	t_process = clock();
	checksum = reduce_checksum();
	t_reduce = clock();

	if (checksum == 0ul)
		printf("impossible\n");

	printf("TOTAL seconds: %.6f\n", elapsed(t_start, t_reduce));
	printf("BUILD_DATA seconds: %.6f\n", elapsed(t_start, t_build));
	printf("PROCESS seconds: %.6f\n", elapsed(t_build, t_process));
	printf("REDUCE seconds: %.6f\n", elapsed(t_process, t_reduce));

	return (0);
}
