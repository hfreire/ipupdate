#include "base64.h"

int base64_decode(char* in, char* out)
{
	int i;

	//keep looping until in[0] is null (end of input)
	//every loop, increment the Input pointer by 4 and Output pointer by 3
	for (; in[0]; in += 4, out += 3)
	{
		//decode the next 4 bytes
		for (i = 0; i < 4; i++)
		{
			if      ((in[i] > 64) && (in[i] <  91))	{ in[i] -= 65; } // A-Z =  0-25
			else if ((in[i] > 96) && (in[i] < 123))	{ in[i] -= 71; } // a-z = 26-51
			else if ((in[i] > 47) && (in[i] <  58))	{ in[i] +=  4; } // 0-9 = 52-61
			else if (in[i] == '+')                  { in[i]  = 62; } //  +  = 62
			else if (in[i] == '/')                  { in[i]  = 63; } //  /  = 63
			else if (in[i] == '=')                  { in[i]  =  0; } //  =  = filler byte, set to null
			else {
				//there was an invalid byte in the input
				return 0;
			}
		}

		//bit-shifting to convert 6-bit to 8-bit values and store them in the Output buffer
		out[0] = (in[0] << 2) + (in[1] >> 4);
		out[1] = (in[1] << 4) + (in[2] >> 2);
		out[2] = (in[2] << 6) + (in[3]);
	}

	return 1;
}
