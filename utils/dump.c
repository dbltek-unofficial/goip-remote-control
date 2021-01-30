
#include <ctype.h>
#include <stdio.h>

#include "debug.h"

#if 0
void dump(int level, unsigned char *buf, int len) {
		int i, j;
		for(i=0; i<len; i+=16){
				dprint(level, "%04x:", i);
				for(j=0; j<16; j++)
						dprint(level, "%c%02x", j==8?'-':' ',buf[i+j]);
				dprint(level, "  ");
				for(j=0; j<16; j++)
						dprint(level, "%c", buf[i+j]>=' '&&buf[i+j]<0x7f?buf[i+j]:'.');
				dprint(level, "\n");
		}
}
#endif

static const char* indent[] = {
    "  ",
    "    ",
    "      ",
    "	",
    "	  ",
    "	    ",
    "	      ",
    "		",
    "		  ",
    "		    ",
    "		      ",
    "			",
    "			  ",
    "			    ",
    "			      ",
    "				",
    "				  ",
    "				    ",
    "				      ",
    "					",
    "					  ",
    "					    ",
    "					      ",
    "						",
    "						  ",
    "						    ",
    "						      ",
    "							",
};

void dump(int level, const unsigned char* buf, int len)
{
    int count = 0;

    while (count < len) {
        int i;

        dprint(0, "%s", indent[level + 1]);
        for (i = 0; i < 16; i++) {
            if (i && !(i % 4))
                dprint(0, " ");
            if (i + count >= len)
                dprint(0, "   ");
            else
                dprint(0, "%.02x ", buf[count + i]);
        }
        dprint(0, "    ");
        for (i = 0; i < 16; i++, count++) {
            if (count >= len)
                break;

            if (isprint(buf[count])) {
                dprint(0, "%c", buf[count]);
            } else {
                dprint(0, ".");
            }
        }
        dprint(0, "\n");
    }
}
