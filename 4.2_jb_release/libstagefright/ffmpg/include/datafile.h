#ifndef DATA_FILE_H_
#define DATA_FILE_H_

class IAvReader
{
    public:
        virtual int Read(long long position, long length, unsigned char* buffer) = 0;
        virtual int Length(long long* total, long long* available) = 0;
        virtual void updatecache(off64_t offset) = 0;
        long long fileoffset;

    protected:
       virtual ~IAvReader(){;}
};

#endif
