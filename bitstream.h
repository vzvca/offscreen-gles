#ifndef __BITSTREAM_H__
#define __BITSTREAM_H__

struct __bitstream {
    unsigned int *buffer;
    int bit_offset;
    int max_size_in_dword;
};
typedef struct __bitstream bitstream;

void bitstream_start(bitstream *bs);
void bitstream_end(bitstream *bs);
void bitstream_put_ui(bitstream *bs, unsigned int val, int size_in_bits);
void bitstream_put_ue(bitstream *bs, unsigned int val);
void bitstream_put_se(bitstream *bs, int val);
void bitstream_byte_aligning(bitstream *bs, int bit);

#endif
