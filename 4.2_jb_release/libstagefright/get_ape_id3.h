#ifndef _GETAPETAGEX_H_
#define _GETAPETAGEX_H_

#define APE_ERROR -1
#define APE_SUCCESS 0

#define APE_LENGTH 256

struct APETAGEX
{
	char Title[APE_LENGTH];	//����
	char Artist[APE_LENGTH];	//������
	char Album[APE_LENGTH];	//ר��
	char Year[APE_LENGTH];	//���
	char Comment[APE_LENGTH];	//��ע
	char Track[APE_LENGTH];	//����
	char Genre[APE_LENGTH];	//����
};

class ApeId3
{
		public:
			int getapetagex(FILE *stream);
			struct APETAGEX apetagex;
};

#endif

