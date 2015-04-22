#include <iostream>
using namespace std;
#include <stdlib.h>

typedef unsigned char uint8_t;
typedef unsigned int uint_t;

#define uint_size sizeof(uint_t)

uint8_t buf[uint_size] = {0, 0, 0, 0};

void reset_buf()
{
  for(int i = 0; i < uint_size; ++i) {
    buf[i] = 0;
  }
}

uint8_t* encode(uint_t a)
{
  reset_buf();

  printf("%x\n", a);

  {
    int i = 0;
    uint8_t prev_ms_bits = 0;
    while(a && i < uint_size) {
      // ls - byte
      uint8_t b = (a & 0xff);
      a >>= 8;

      // last: has more
      if(i >= 1) { 
        buf[i-1] |= 0x80;
      }

      // gen mask for msbits
      uint8_t msbits_num = (i + 1),
              msbits_shift = 8 - msbits_num;
      uint8_t msbits_mask = (0xff >> msbits_shift << msbits_shift);

      // cur
      uint8_t cur_ms_bits = ((b & msbits_mask) >> msbits_shift);
      // eliminate the msbits to track whether has next on the highest bit
      // track the prev_ms_bits on the lower bits
      b = (b << msbits_num >> 1); 
      b |= prev_ms_bits;
      buf[i] = b;

      prev_ms_bits = cur_ms_bits;

      ++i;
    }

    if(prev_ms_bits) {
      if(i < uint_size) {
        buf[i-1] |= 0x80;
        buf[i] = prev_ms_bits;
      } else {
        reset_buf();
        printf("unable to encode, abort!\n");
        return buf; 
      }
    } 
  }

  printf("encode: ");
  for(int i = 0; i < uint_size; ++i) {
    printf("%x,", buf[i]);
  }
  printf("\n");

  return buf;
}


int decode(uint8_t buf[uint_size])
{
  int res = 0;

  for(int i = 0; i < uint_size; ++i) {
    uint8_t b = buf[i];
    bool has_more = b & 0x80;
    
    b &= (0x7f);
    res = res + b * (1 << (i * 7)); 

    if(!has_more) {
      break;
    }
  }

  printf("decode: %x %d\n", res, res);
  return res;
}


int main(int argc, const char** argv) 
{
  cout << "uint_size: " << uint_size << endl;
  if(argc < 2) {
    cout << "unit test: " << endl;
    decode(encode(0x12c)); // 0000 0001 0010 1100
    decode(encode(0x412c));    // 0100 0001 0010 1100
    decode(encode(0x800000));
    decode(encode(0x80000000));
  } else {
    int a = atoi(argv[1]);
    cout << "read: " << a << endl;
    encode(a);
  }
}
