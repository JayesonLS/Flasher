// Glue to allow compiling in a modern compiler for faster iteration.
// Not used for DOS compiles.
//
// Copyright (C) 2021 Titanium Studios Pty Ltd 
// 

#define __FAKEDOS__
#define far 

static unsigned char *fakeMem;

static void *MK_FP(unsigned long seg, unsigned long off)
{
	if (fakeMem == NULL)
	{
		const int size = 1024 * 1024;
		fakeMem = (unsigned char *)malloc(size);
		memset(fakeMem, 0xAA, size);
	}

	return fakeMem + (seg << 4) + off;
}