#include "libmpeg3.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
	mpeg3_t *file;
	int x,y,ii,i,j,result,out;
	int howmany;
	unsigned char *output, **output_rows;
	char filename[100];
	char header[100];
	char temp;

	howmany = atoi (argv[2]);

	if ((file = mpeg3_open(argv[1])) == NULL)
	  {
	    printf ("Open failed\n");
	    return 1;
	  }
	mpeg3_set_cpus(file,1);
	mpeg3_set_mmx(file,0);
	if (mpeg3_has_video == 0)
	  {
	    printf ("Stream has no Video\n");
	    return 1;
	  }
	x = mpeg3_video_width(file,0);
	y = mpeg3_video_height(file, 0);
	output = malloc (x*y*3 + 4);
	output_rows = malloc (sizeof(unsigned char*) * y);
	for(i = 0; i < y; i++)
	  output_rows[i] = &output[i * x * 3];
 	
	for (ii = 0; ii < howmany; ii++)
	  {
	    result = mpeg3_read_frame(file,output_rows,0,0,x,y,x,y,0,0);
	
	    sprintf (filename,"/x2/temp/output%03i.ppm",ii);
	    sprintf (header,"P6\n%i %i\n255\n\r",x,y);

/*	    printf ("Opening %s\n",filename); */
	    
	    if ((out = open (filename,O_CREAT|O_WRONLY|O_TRUNC,0755)) == -1)
	      {
		printf ("Can't open %s\n",filename);
		return 1;
	      }

	    write (out,header,strlen(header));
	    
	    for (i = 0; i < y; i++)
	      for (j = 0; j < x; j++)
		{
		  temp = output [(i*x+j)*3 + 1];
		  output[(i*x+j)*3 + 1] = output [(i*x+j)*3 + 0];
		  output[(i*x+j)*3 + 0] = temp;
		}
	    write (out, output, x*y*3);
	    close (out);
	  }
}
