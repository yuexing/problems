#include "container.hpp"
// Gray Code: the consecutive ones only diff 1 bit.
// n=1:
// 0, 1
// n=2:
// 00, 01, 11, 10
// n=3:
// 000, 001, 011, 010, 110, 111, 101, 100
void printGrayCode(int n)
{
    vector<int> codes;
    codes.push_back(0);

    for(int i = 0; i < n; ++i) {
        int size = codes.size();
        for(int j = 0; j < size; ++j) {
            codes.push_back(codes[j] | (1 << i));
        }
    }
}

// detect BOM(byte-order-mark)
// UTF-8: 0xEF, 0xBB, 0xBF
// UTF-16: U+FEFF
// UTF-32: the same as UTF-16 00 00 FE FF, FF FE 00 00

// detect LRM(left-right-mark)
// U+200E

// to/from base64
// testcase: 
// 3 -> 4 Man -> TWFu
// Input: any carnal pleasure.  Output: YW55IGNhcm5hbCBwbGVhc3VyZS4=  
// Input has 20 bytes, output has 28 bytes (1 padding). 
// Input: any carnal pleasure   Output: YW55IGNhcm5hbCBwbGVhc3VyZQ==  
// Input has 19 bytes, output has 28 bytes (2 padding). 
// Input: any carnal pleasur    Output: YW55IGNhcm5hbCBwbGVhc3Vy      
// Input has 18 bytes, output has 24 bytes (no padding). 
// Input: any carnal pleasu     Output: YW55IGNhcm5hbCBwbGVhc3U=      
// Input has 17 bytes, output has 24 bytes (1 padding). 
// Input: any carnal pleas      Output: YW55IGNhcm5hbCBwbGVhcw==      
// Input has 16 bytes, output has 24 bytes (2 padding). 
