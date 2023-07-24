
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define off_t off64_t
#define fopen fopen64
#define fseeko fseeko64
#define ftello ftello64
#endif

#define BLOCK_SIZE 32
#define BLOCK_COUNT 512
#define BLOCK_AREA (BLOCK_SIZE*BLOCK_COUNT)

#define ALLOC_MAP_SIZE (BLOCK_SIZE/32)

typedef struct _block {
	unsigned int alloc_map[ALLOC_MAP_SIZE];
	unsigned char byte[BLOCK_SIZE];
} block;

typedef struct _patch {
	off_t offset;
	block *blocks[BLOCK_COUNT];
	struct _patch *next;
} patch;

int usage(char *name)
{
	printf("Usage: %s [-i] file address=byte[,byte,...] [...]\n", name);
	return 1;
}

patch *new_patch(off_t address)
{
	int i;
	patch *p = malloc(sizeof(patch));

	if (p == NULL)
		return NULL;

	for (i = 0; i < BLOCK_COUNT; i++)
		p->blocks[i] = NULL;

	p->offset = address / BLOCK_AREA;
	p->next = NULL;

	return p;
}

inline int already_allocated(unsigned int *c, int index)
{
	return c[index>>5]&(1<<(index&31));
}

inline void allocate(unsigned int *c, int index)
{
	c[index>>5] |= (1<<(index&31));
}

int update_patch(patch *p, off_t address, int byte)
{
	off_t patch_start, index;
	int relative;
	block *b;

	patch_start = p->offset * BLOCK_AREA;
	index = (address - patch_start)/BLOCK_SIZE;

	if (p->blocks[index] == NULL) {
		p->blocks[index] = calloc(1, sizeof(block));
		if (p->blocks[index] == NULL)
			return 3;
	}

	b = p->blocks[index];

	relative = address - (patch_start + index * BLOCK_SIZE);

	if (already_allocated(b->alloc_map, relative))
		return 4;

	allocate(b->alloc_map, relative);
	b->byte[relative] = (unsigned char) byte;

	return 0;
}

int add_patch(patch **p, off_t address, int byte)
{
	off_t offset;
	block *b;
	patch *t;

	if (*p == NULL) {
		*p = new_patch(address);
		if (*p == NULL)
			return 1;
		return update_patch(*p, address, byte);
	}

	offset = address/BLOCK_AREA;

	while (*p != NULL)
	{
		if ((*p)->offset == offset)
			return update_patch(*p, address, byte);
		if ((*p)->offset > offset) {
			t = new_patch(address);
			if (t == NULL)
				return 1;
			t->next = *p;
			*p = t;
			return update_patch(*p, address, byte);
		}
		p = &((*p)->next);
	}

	*p = new_patch(address);
	if (*p == NULL)
		return 1;
	return update_patch(*p, address, byte);
}
		
off_t getplainint(char *str, int start, int end)
{
	int i;
	off_t accum = 0;

	for (i = start; i < end; i++)
	{
		if (str[i] < '0' || str[i] > '9')
			return -1;
		accum = accum*10 + (str[i] - '0');
	}

	return accum;
}

int validHex(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return -1;
}

off_t gethexint(char *str, int start, int end)
{
	int i, value;
	off_t accum = 0;

	for (i = start + 2; i < end; i++)
	{
		if ((value = validHex(str[i])) < 0)
			return -1;
		accum = accum*16 + value;
	}

	return accum;
}

off_t getint(char *str, int start, int end)
{
	int length;

	length = strlen(str);

	if (start > end || start > length || end > length || start == end)
		return -1;

	if (end - start < 3 || str[start] != '0' || (str[start+1] != 'x' && str[start+1] != 'X'))
		return getplainint(str, start, end);

	return gethexint(str, start, end);
}

int process(patch **p, off_t size, char *patch)
{
	int i, seperate;
	off_t address;
	int byte;
	int start, stop;
	int length;

	length = strlen(patch);

	if (length < 3)
		return 1;

	seperate = -1;

	for (i = 0; i < length; i++)
	{
		if (patch[i] == '=') {
			if (i == 0 || patch[i+1] == '\0')
				return 1;
			seperate = i;
			break;
		}
	}

	if (seperate == -1)
		return 1;	

	if ((address = getint(patch, 0, seperate)) < 0 || address >= size)
		return 1;

	start = seperate + 1;
	while (start < length) {
		if (address >= size)
			return 1;
		for (stop = start; stop < length; stop++)
		{
			if (patch[stop] == '=')
				return 1;
			if (patch[stop] == ',')
				break;
		}
		if ((byte = getint(patch, start, stop)) < 0 || byte > 255)
			return 1;
		if (add_patch(p, address, byte))
			return 1;
		address++;
		start = stop + 1;
	}

	return 0;
}

int apply(FILE *f, patch *p)
{
	int i, j;
	block *b;

	off_t offset, address;
	unsigned char byte;

	while (p != NULL)
	{
		offset = p->offset * BLOCK_AREA;
		for (i = 0; i < BLOCK_COUNT; i++)
		{
			if (p->blocks[i] != NULL) {
				b = p->blocks[i];
				for (j = 0; j < BLOCK_SIZE; j++)
				{
					if (already_allocated(b->alloc_map, j))
					{
						address = offset + j;
						byte = b->byte[j];

						/*printf("Patching 0x%08llx to %d\n", address, byte);*/
						if (fseeko(f, address, SEEK_SET))
							return 1;

						if (fwrite(&byte, 1, 1, f) != 1)
							return 1;
					}
				}
			}
			offset += BLOCK_SIZE;
		}
		p = p->next;
	}

	return 0;
}

int ips(FILE *f, patch *p)
{
	char *header = "PATCH", *eof = "EOF";
	unsigned char buffer[5+65536];
	int idx = 5, size = 0;
	
	off_t offset, address, old_address;

	unsigned char byte;
	int i, j;
	block *b;

	if (fwrite(header, 5, 1, f) != 1)
		return 1;

	while (p != NULL)
	{
		//printf("%08x\n", p);
		offset = p->offset * BLOCK_AREA;
		for (i = 0; i < BLOCK_COUNT; i++)
		{
			if (p->blocks[i] != NULL) {
				b = p->blocks[i];
				for (j = 0; j < BLOCK_SIZE; j++)
				{
					if (already_allocated(b->alloc_map, j))
					{
						address = offset + j;
						byte = b->byte[j];

						//printf("%d, %d : %d %d\n", i, j, address, byte);

						if (size == 65535 || (idx > 5 && old_address != address)) {
							buffer[3] = (size>>8) & 0xff;
							buffer[4] = (size>>0) & 0xff;
							if (fwrite(buffer, idx, 1, f) != 1)
								return 1;
							size = 0;
							idx = 5;
						}

						if (idx == 5) {
							old_address = address;
							buffer[0] = (old_address>>16) & 0xff;
							buffer[1] = (old_address>>8)  & 0xff;
							buffer[2] = (old_address>>0)  & 0xff;
						}
						buffer[idx++] = byte;
						size++;

						old_address++;

						if (old_address > 0xffffff)
							return 1;
					}
				}
			}
			offset += BLOCK_SIZE;
		}
		p = p->next;
	}

	if (size) {
		buffer[3] = (size>>8) & 0xff;
		buffer[4] = (size>>0) & 0xff;
		if (fwrite(buffer, idx, 1, f) != 1)
			return 1;
	}

	if (fwrite(eof, 3, 1, f) != 1)
		return 1;

	return 0;
}

int main(int argc, char *argv[])
{
	FILE *f;
	int i, idx, create_ips;
	off_t size;
	patch *changes = NULL;

	if (argc < 2)
		return usage(argv[0]);

	idx = 1;
	create_ips = 0;

	if (strcmp(argv[1], "-i") == 0) {
		if (argc < 3)
			return usage(argv[0]);
		idx = 2;
		create_ips = 1;
	}

	if (!create_ips) {
		f = fopen(argv[idx+0], "rb+");
	} else {
		f = fopen(argv[idx+0], "wb+");
	}

	if (f == NULL) {
		printf("Error opening %s; patching failed\n", argv[idx+0]);
		return 2;
	}

	if (!create_ips) {
		if (fseeko(f, 0, SEEK_END)) {
			fclose(f);
			printf("Failed seeking %s; patching failed\n", argv[idx+0]);
			return 5;
		}

		size = ftello(f);

		if (size < 0) {
			fclose(f);
			printf("Failed reading size of %s; patching failed\n", argv[idx+0]);
			return 5;
		}
	} else {
		size = 0xffffff;
	}

	for (i = idx+1; i < argc; i++)
	{
		if (process(&changes, size, argv[i])) {
			fclose(f);
			printf("Error processing %s; patching failed\n", argv[i]);
			return 3;
		}
	}

/*
	char buffer[50];

	size = 2;
	for (i = 0; i < 5; i++)
		size *= size;

	for (i = 0; i < 10000; i++)
	{
		sprintf(buffer, "%i=%i", i*3*BLOCK_AREA/2, i&255);
		if (process(&changes, size, buffer)) {
			fclose(f);
			printf("Error processing %s; patching failed\n", buffer);
			return 3;
		}
		if ((i%1000) == 0) {
			printf("%s\n", buffer);
		}
	}
*/

	if (!create_ips) {
		if (apply(f, changes)) {
			fclose(f);
			printf("Error applying patches; some patches may have been applied anyways\n");
			return 4;
		}
		fclose(f);
		printf("Patching successful\n");
	} else {
		if (ips(f, changes)) {
			fclose(f);
			printf("Error creating patch; ips file incomplete\n");
			return 4;
		}
		fclose(f);
		printf("IPS patch creation successful\n");
	}

	return 0;
}
