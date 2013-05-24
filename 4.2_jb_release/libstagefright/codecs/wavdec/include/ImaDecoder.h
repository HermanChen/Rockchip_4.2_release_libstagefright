#ifndef _H_IMA_DECODER_

#define _H_IMA_DECODER_

extern void decode_ima_adpcm(char in[],long int in_buf_size,short out[],short outR[],int Channels,int blocksize);
extern short decode_ima_adpcm_one(char code,int *index,short cur_sample);

#endif  //_H_IMA_DECODER
