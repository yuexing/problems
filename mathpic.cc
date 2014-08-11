#include <stdio.h>
#include <iostream>
#include <cmath>
#define DIM 1024
#define DM1 (DIM-1)
#define _sq(x) ((x)*(x))                           // square
#define _cb(x) abs((x)*(x)*(x))                    // absolute value of cube
#define _cr(x) (unsigned short)(pow((x),1.0/3.0))  // cube root

unsigned short rd(int i,int j){
    return (unsigned short)sqrt((double)(_sq(i-DIM/2)*_sq(j-DIM/2))*2.0);
}
unsigned short gr(int i,int j){
    return (unsigned short)sqrt((double)(
                    (_sq(i-DIM/2)|_sq(j-DIM/2))*
                    (_sq(i-DIM/2)&_sq(j-DIM/2))
                    )); 
}
unsigned short bl(int i,int j){
    return (unsigned short)sqrt((double)(_sq(i-DIM/2)&_sq(j-DIM/2))*2.0);
}

unsigned short rd1(int i,int j){
    return i&&j?(i%j)&(j%i):0;
}
unsigned short gr1(int i,int j){
    return i&&j?(i%j)&(j%i):0;
}
unsigned short bl1(int i,int j){
    return i&&j?(i%j)&(j%i):0;
}

typedef unsigned short (*pixel_fn_t)(int,int);
pixel_fn_t RD = rd1;
pixel_fn_t GR = gr1;
pixel_fn_t BL = bl1;

// PPM: stands for portable pixel map
// header:
// magic        2 P6
// whitespace   1
// width        decimal
// whitespace   1
// height   
// whitespace
// the maximum color value
// whitespace
// raster of rows of columns
void pixel_write(int,int);
FILE *fp;
int main(){
    fp = fopen("MathPic","wb");
    fprintf(fp, "P6\r%d\r%d\r1023\n", DIM, DIM);
    for(int j=0;j<DIM;j++)
        for(int i=0;i<DIM;i++)
            pixel_write(i,j);
    fclose(fp);
    return 0;
}
void pixel_write(int i, int j){
    static unsigned short color[3];
    color[0] = RD(i,j)&DM1;
    color[1] = GR(i,j)&DM1;
    color[2] = BL(i,j)&DM1;
    fwrite(color, 2, 3, fp);
}
