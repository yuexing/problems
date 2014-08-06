#ifndef FLATTEN_HPP
#define FLATTEN_HPP

// cpp lacks the support for (de-)serialization, which could be implemented
// recursively at highlevel with the support of flatten which supports
// xfer-basic-types.
class Flatten
{
public:
    // write the dest or read the dest for size
    int xferPartialBlock(char *dest, int size)
    {
       if(m_reading) {
           return readBlock(dest, size);
       } else {
           return writeBlock(dest, size);
       }
    }
    // throw an exception if can't read size
    int xferFullBlock(char *dest, int size)
    {
        cond_assert_return(xferPartialBlock(dest, size) == size, -1);
        return size;
    }

    void xferByte(char *dest)
    {
        xferFullBlock(dest, 1);
    }

    void xferUint16(char *dest)
    {
        xferFullBlock(dest, 2);
    }
    // etc.
protected:
    virtual int innerReadBlock(char *dest, int size) == 0

private:
    int readBlock(char *dest, int size)
    {
        if(end - next > size) {
            memcpy(dest, next, size);
            next += size;
        } else {
            innerReadBlock(dest, size);
        }
    } 

    int innerWriteBlock(char *dest, int size);
    // read or write
    bool m_reading;
    //
    char *next, end;
};

#define BUFFER_SIZE 0x100;
// Has a buffer to cache the write/read
class BufferedFlatten : public Flatten
{
protected:
    int innerReadBlock(char *dest, int size)
    {
        int cpysize = size;
        while(true) {
            int bufLen = next - end;
            if(bufLen > size) {
                //
            }

            memcpy(dest, next, bufLen);
            dest += bufLen;
            size -= bufLen;

            int got = fillBuf(buffer, BUFFER_SIZE);

            if(got == 0) {
                return cpysize -= size; 
            }
            
            next = buffer;
            end = buffer + got;
        }
    }

    virtual fillBuf(char *buffer, int size) = 0;
    virtual emptyBuf(char *buffer, int size) = 0;
private:
    char buffer[BUFFER_SIZE];
};

// use a string to implement fill/empty buffer
class StringFlatten : public BufferedFlatten
{
};

// Use iostream to implement fill/empty buffer
class IOSFlatten : public BufferedFlatten
{
};

// write/read a file
class FileFlatten : public IOSFlatten
{

};
#endif /* FLATTEN_HPP */
