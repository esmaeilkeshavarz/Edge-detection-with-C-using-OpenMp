#include <png.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

png_uint_32 width;
png_uint_32 height;
double flop=0;
short count=0;
clock_t starttimeread;
clock_t endtimeread;
float **inputfloatpng, **outputfloatpng;

///*** This function returns Error messages when there is a problem with png file***///
static void
fatal_error (const char * message, ...)
{
    va_list args;
    va_start (args, message);
    vfprintf (stderr, message, args);
    va_end (args);
    exit (EXIT_FAILURE);
}

///*** This Function reads the png File into a Byte Matrix and detects the pixels dimention of that***/// 
png_byte **readingstep(const char *pngroot)
{

    png_structp png_ptr;
    png_infop info_ptr;
    FILE * fp;
    int bit_depth;
    int color_type;
    int interlace_method;
    int compression_method;
    int filter_method;
    png_bytepp rows;
    starttimeread=clock();
    fp = fopen (pngroot, "rb");
    if (! fp) 
    {
    fatal_error ("Cannot open '%s': %s\n", pngroot, strerror (errno));
    }
    png_ptr= png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (! png_ptr) 
    {
    fatal_error ("Cannot create PNG read structure");
    }
    info_ptr= png_create_info_struct (png_ptr);
    if (! png_ptr) 
    {
    fatal_error ("Cannot create PNG info structure");
    }
    png_init_io (png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR (png_ptr, info_ptr, & width, & height, & bit_depth,
                  & color_type, & interlace_method, & compression_method,
                  & filter_method);

    png_byte ** row_pointers  =(png_byte **)malloc(sizeof(png_bytep **) * height);
    for (int y = 0; y < height; y++)
    {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));
    }
    png_read_image(png_ptr, row_pointers);
    fclose(fp);
    endtimeread=clock();
    float usedtimeread = endtimeread-starttimeread;
    printf("\nImage size : %d * %d\n",height,width);
    printf("\n\nStep         Time,ms        GFLOP/s\n");
    printf("************************************\n");
    return row_pointers;
    
}

//***This Function converts the png Bytes to the Float***///
float **bytetofloat(png_byte **bytepng)
{
    float **floatpng= (float **)malloc(sizeof(float **) * height);
    for(int i= 0; i< height; i++)
    {
    floatpng[i]= (float *)malloc(sizeof(float *) * width);
    for(int j= 0; j< width; j++)
    floatpng[i][j]= bytepng[i][j]; 
    }
    return floatpng;
}

///***This Function converts the png Float to the Bytes***///
png_byte **floattobyte(float **pngoffloat)
{
    png_byte **pngofbyte = (png_byte **)malloc(sizeof(png_byte **) * height);
    for (int i = 0; i < height; i++)
    {
        pngofbyte[i]= (png_byte *)malloc(sizeof(png_byte *) * width);
        for (int j= 0; j< width; j++)
        pngofbyte[i][j]= pngoffloat[i][j];
    }

    return pngofbyte;
}

///***This Function creates a new Float matrix to save the previous one after executing Stencil code on it***///
float **stencilledhome()
{
    float **row_pointers = malloc(height * sizeof(float **));
    for (int x = 0; x < height; x++)
    {
        row_pointers[x] = malloc(sizeof(float *) * width);
        for (int y = 0; y < width; y++)
        {
        row_pointers[x][y] = 255;
        }
    }
    return row_pointers;
}

///***This Function releases the memory which was allocated by the png Bytes***///
void releaseByteAllocation(png_byte **xfactor)
{
    for (int i = 0; i < height; i++)
    {
    free(xfactor[i]);
    }
    free(xfactor);
}

///***This Function releases the memory which was allocated by the png Floates***///
void releaseFloatAllocation(float **xfactor)
{
    for (int i = 0; i < height; i++)
    {
    free(xfactor[i]);
    }
    free(xfactor);
}

///***This Function Executes the Stencil Filter on PNG Float Matrix***///
double stencilexecution()
{
    short sumpixel=0;
    short x,y;
    clock_t starttime = clock();
    for (x = 1; x < height - 1; x++)
    {
        for (y = 1; y < width - 1; y++)
        {
                sumpixel =((-1 * inputfloatpng[x - 1][y - 1]) + 
                (-1 * inputfloatpng[x - 1][y]) + 
                (-1 * inputfloatpng[x - 1][y + 1]) +
                (-1 * inputfloatpng[x][y - 1]) + 
                ( 8*inputfloatpng[x][y]) + 
                (-1 * inputfloatpng[x][y + 1]) +
                (-1 * inputfloatpng[x + 1][y - 1]) + 
                (-1 * inputfloatpng[x + 1][y]) + 
                (-1 * inputfloatpng[x + 1][y + 1]));

                sumpixel=((sumpixel<0)? (0):(sumpixel));
                sumpixel=((sumpixel>255)? (255):(sumpixel));

                outputfloatpng[x][y]=sumpixel;
        }
    }
    clock_t endtime= clock();
    double usedtime= ((double)(endtime- starttime)/ CLOCKS_PER_SEC);
    double stepby= (18*0.036/usedtime);//-->18*6000*6000/10^9/usedtime(per second)
    count++;
    if(count>3)
    flop+=stepby;
    printf("           %.3f        %f\n", (usedtime*1000),stepby);
    return usedtime;
}

void writingstep(const char *pngroot, float **write_rows)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    int depth = 8;
    fp = fopen(pngroot, "wb");
    if (!fp)
        fatal_error ("Cannot open '%s': %s\n", pngroot, strerror (errno));
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        fatal_error ("Cannot create PNG read structure");
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        fatal_error ("Cannot create PNG info structure");
    if (setjmp(png_jmpbuf(png_ptr)))
        fatal_error ("jmpBuffer ERROR");
    png_set_IHDR(png_ptr,info_ptr,width,height,depth,PNG_COLOR_TYPE_GRAY,PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
    png_byte **f = floattobyte(write_rows);
    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, f);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    for (int y = 0; y < height; y++)
        png_free(png_ptr, f[y]);
    png_free(png_ptr, f);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

int main ()
{   
    double timesum=0;
    printf("**********************************************\n");
    printf("                                             *\n");
    printf("Phase 1 of Edge detection with a 3*3 Stencil *\n");
    printf("                                             *\n");
    printf("                                             *");
    printf("\n**********************************************\n");
    printf("                                             *\n");
    printf("   #*By Esmaeil Keshavarz*#  *\n");
    printf("                                             *");
    printf("\n**********************************************\n");
    png_byte **read_to_matrix= readingstep("test.png");
    float **byteconvertion= bytetofloat(read_to_matrix);    //--> converting PNG Byte to Float 
    float **newpngfloat= stencilledhome();                  //--> creating raw Matrix to saving PNG after Filter
    inputfloatpng=byteconvertion;
    outputfloatpng=newpngfloat;
    for (int i = 0; i < 10; i++)
    {
    
    printf("%2d",(i+1));
    

    
    
                       
    if (i==3)
                    {
                        timesum=0;
                    }
    timesum+= stencilexecution(); //--> Executing Filter
   // releaseFloatAllocation(byteconvertion);                 //--> make free the malloc Float
    if (i==9)    
                     {
                         writingstep("outputpng.png", newpngfloat); //--> saving result PNG file 
                     }               

    } 
    printf("************************************\n");
    printf("Average Performance: \n             %f     %f\n",(timesum/7)*1000,flop/7);
    printf("************************************\n");
    releaseByteAllocation(read_to_matrix);                  //--> make free the malloc Byte
    return 0;
}
