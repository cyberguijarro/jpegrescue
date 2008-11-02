/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * @produces "jpegrescue" gcc $# -o $@
 * @default "jpegrescue"
 */

#include <stdio.h>

// Static definitions
#define READ_BUFFER_SIZE	1024*4

// Type definitions
typedef unsigned char byte;
typedef enum { FALSE = 0, TRUE } bool;
typedef unsigned long ulong;

// Read buffers
byte vReadBuffer[READ_BUFFER_SIZE];
byte vCopyBuffer[READ_BUFFER_SIZE];

// Global options
ulong uRecoveryBlockSize = 0;

// Search state machine, returns TRUE when the feeder has to advance
static bool scan(byte b, ulong* uMatchPosition, ulong* uMatchSize)
{
	static byte state;
	static ulong initialPosition;
	static ulong position;
	byte initialState = state;

	switch (state)
	{
	case 0:	// Initial state, look for first SOI marker byte
		if (b == 0xFF)
		{
			initialPosition = position;
			state = 1;
		}
		break;
	case 1:	// Look for second SOI marker byte
		if (b == 0xD8)
			state = 2;
		else
			state = 0;
		break;
	case 2:	// Look for first JFIF marker byte
		if (b == 0xFF)
			state = 3;
		else
			state = 0;
		break;
	case 3:	// Look for second JFIF marker byte
		if ((b >= 0xE0) && (b <= 0xE9))
		{
			*uMatchPosition = initialPosition;

			if (uRecoveryBlockSize == 0)
				*uMatchSize = (1024*1024*1);
			else
				*uMatchSize = uRecoveryBlockSize;

			initialPosition = 0;
			initialState = 0;
			state = 0;
		}
		else
			state = 0;
		break;
	}

	if ((initialState == 0) || (state != 0))
	{
		position++;
		return TRUE;
	}
	else
		return FALSE;
}

static ulong min(ulong a, ulong b)
{
	if (a > b)
		return b;
	else
		return a;
}

// Block copy utility function
static void extract(FILE* pSourceFile, ulong uMatchPosition, ulong uMatchSize, FILE* pDestinationFile)
{
	long uOldPosition = ftell(pSourceFile);
	size_t uBytesCopied = 0;

	fseek(pSourceFile, uMatchPosition, SEEK_SET);

	while (uBytesCopied < uMatchSize)
	{
		size_t uBytesRead = fread(vCopyBuffer, 1, min(READ_BUFFER_SIZE, (uMatchSize - uBytesCopied)), pSourceFile);

		if (uBytesRead == 0)
			break;

		fwrite((void*)vCopyBuffer, 1, uBytesRead, pDestinationFile);
		uBytesCopied += uBytesRead;
	}

	fseek(pSourceFile, uOldPosition, SEEK_SET);
}

// Main function
int main (int argc, char * const argv[])
{
	FILE* pFile;
	char* szFileExtension = "bin";

	if (argc >= 2)
	{
		pFile = fopen(argv[1], "rb");

		if (argc >= 3)
		{
			uRecoveryBlockSize = atoi(argv[2]);

			if (argc >= 4)
			{
				szFileExtension = argv[3];
			}
		}
	}
	else
	{
		printf("Usage: %s input-file [recovery-block-size [output-file-extension]]\n", argv[0]);
		return 1;
	}

	if (pFile)
	{
		size_t uBufferCount = fread((void*)vReadBuffer, 1, READ_BUFFER_SIZE, pFile);
		unsigned int uMatchCount = 0;

		while (uBufferCount > 0)
		{
			size_t i = 0;

			for (; i < uBufferCount; i++)
			{
				ulong uMatchPosition, uMatchSize = 0;

				while (!scan(vReadBuffer[i], &uMatchPosition, &uMatchSize))
					;

				if (uMatchSize > 0)
				{
					char szFilename[32];

					sprintf(szFilename, "%d.%s", ++uMatchCount, szFileExtension);
					FILE* pNewFile = fopen(szFilename, "wb");

					if (pNewFile)
					{
						printf("Pattern found at 0x%X (%d bytes), saving as %s...\n", uMatchPosition, uMatchSize, szFilename);
						extract(pFile, uMatchPosition, uMatchSize, pNewFile);
						fclose(pNewFile);
					}

					uMatchSize = 0;
				}
			}

			uBufferCount = fread((void*)vReadBuffer, 1, READ_BUFFER_SIZE, pFile);
		}

		fclose(pFile);
	}

	return 0;
}
