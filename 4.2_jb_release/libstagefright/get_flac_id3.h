#ifndef _GETFLACTAG_H_
#define _GETFLACTAG_H_

#define FLAC_ERROR -1
#define FLAC_END -1
#define FLAC_SUCCESS 0

#define FLAC_LENGTH 128


struct FLACTAG
{
	char Title[FLAC_LENGTH];	//����
	char Artist[FLAC_LENGTH];	//������
	char Album[FLAC_LENGTH];	//ר��
	char Data[FLAC_LENGTH];	//���
	char Comment[FLAC_LENGTH];	//��ע
	char Tracknumber[FLAC_LENGTH];	//����
	char Genre[FLAC_LENGTH];	//����
};

class FlacId3
{
	public:
			struct FLACTAG flactag;
			int handleFlacTag(char *buffer);
			int getFlacTag(FILE *stream);
};
#endif

