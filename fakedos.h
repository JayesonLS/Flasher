// Glue to allow compiling in a modern compiler for faster iteration.
// Not used for DOS compiles.
//
// Copyright (C) 2021 Titanium Studios Pty Ltd 
// 
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.If not, see < https://www.gnu.org/licenses/>.

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