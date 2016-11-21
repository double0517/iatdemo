#include "stdlib.h"
#include "stdio.h"
#include <windows.h>
#include <conio.h>
#include <errno.h>
#include <Mmsystem.h>
#pragma comment(lib,"winmm.lib")

#include "../include/qisr.h"
#include "../include/msp_cmn.h"
#include "../include/msp_errors.h"


#ifdef _WIN64
#pragma comment(lib,"../lib/msc_x64.lib")//x64
#else
#pragma comment(lib,"../lib/msc.lib")//x86
#endif

//���¼��ʱ��
#define MAX_REC_TIME 100


#define AMPLITUDE_CRITICAL 4000
#define VOICE_NUM 2

//�Զ��������ṹ��
typedef struct voice_info{
	char * voice ;
	int recTime ;
	bool recOver;
	bool availaible;
};

struct voice_info voices[VOICE_NUM];

typedef int SR_DWORD;
typedef short int SR_WORD ;


void WaveInitFormat(LPWAVEFORMATEX m_WaveFormat, WORD nCh, DWORD nSampleRate, WORD BitsPerSample)
{
	m_WaveFormat->wFormatTag = WAVE_FORMAT_PCM;// ��ṹ���в������и�ֵ
	m_WaveFormat->nChannels = nCh;
	m_WaveFormat->nSamplesPerSec = nSampleRate;
	m_WaveFormat->nAvgBytesPerSec = nSampleRate * nCh * BitsPerSample / 8;
	m_WaveFormat->nBlockAlign = nCh * BitsPerSample / 8;
	m_WaveFormat->wBitsPerSample = BitsPerSample;
	m_WaveFormat->cbSize = 0;
}
/*                      wave������ʼ�������б�

����WAVEFORMATEX�ṹ��ĺ��壺
typedef struct {
WORD wFormatTag;    //���������ĸ�ʽ,������˫����ʹ��WAVE_FORMAT_PCM.��������WAVEFORMATEXTENSIBLE�ṹ��ʱ,ʹ��WAVE_FORMAT_EXTENSIBLE.
WORD nChannels;    //��������
DWORD nSamplesPerSec;  //������.wFormatTagΪWAVE_FORMAT_PCMʱ,��8.0kHz,11.025kHz,22.05kHz,��44.1kHz.
DWORD nAvgBytesPerSec;  //ÿ��Ĳ����ֽ���.ͨ��nSamplesPerSec * nChannels * wBitsPerSample / 8����
WORD nBlockAlign;    //ÿ�β������ֽ���.ͨ��nChannels * wBitsPerSample / 8����
WORD wBitsPerSample;  //����λ��.wFormatTagΪWAVE_FORMAT_PCMʱ,Ϊ8����16
WORD cbSize;      //wFormatTagΪWAVE_FORMAT_PCMʱ,���Դ˲���
} WAVEFORMATEX;

*/

DWORD WINAPI iat(LPVOID lpParam){
	//printf("*******************�����̣߳�***************\n");

	int voiceIndex = (int)lpParam;

	int errCode;
	int epStatus = MSP_EP_LOOKING_FOR_SPEECH;
	int recStatus;
	const char* param = "sub = iat, domain = iat, language = zh_ch, accent = mandarin, sample_rate = 16000, result_type = json, result_encoding = gb2312,sch=1,nlp_version=2.0";//�ɲο������ò����б�
	const char * sessionID = QISRSessionBegin(NULL, param, &errCode);//��ʼһ·�Ự

	//printf("****************��ʼ�Ự����%d\n",errCode);
	char result[1000]={0};
	int first = 0;
	int ret;

	for(int send = 0 ; send< voices[voiceIndex].recTime * 32000 || voices[voiceIndex].recOver != true; ){
		if(send < voices[voiceIndex].recTime * 32000 )
		{

			ret = QISRAudioWrite(sessionID, (const void *)(voices[voiceIndex].voice + send), 16000, MSP_AUDIO_SAMPLE_CONTINUE, &epStatus, &recStatus);//д��Ƶ
			send+=16000;
		//	printf("ret %d ep %d recStatus %d \n",ret , epStatus , recStatus);

		}
		if (recStatus == MSP_REC_STATUS_SUCCESS) {
			const char *rslt = QISRGetResult(sessionID, &recStatus, 0, &errCode);//������Ѿ���ʶ���������Ի�ȡ
			
			if (NULL != rslt)
				strcat(result,rslt);
			
		}
		;
		if (epStatus == MSP_EP_AFTER_SPEECH)
			break;
		_sleep(150);
	}
	voices[voiceIndex].availaible = true;
	QISRAudioWrite(sessionID, (const void *)NULL, 0, MSP_AUDIO_SAMPLE_LAST, &epStatus, &recStatus);
	//printf("errCode %d\n",errCode);
	while (recStatus != MSP_REC_STATUS_COMPLETE && 0 == errCode) {
		const char *rslt = QISRGetResult(sessionID, &recStatus, 0, &errCode);//��ȡ���
		if (NULL != rslt)
		{
			strcat(result,rslt);

		}
		_sleep(150);
	}

	QISRSessionEnd(sessionID, NULL);
	printf("***************************************\n");
	printf("\nʶ����Ϊ��%s\n",result);
	printf("\n***************************************\n");

	unsigned int len = strlen(result);
		const char * yuyi = MSPSearch("tte=gb2312,rst=json,rse=gb2312,nlp_version=2.0", result, &len, &errCode);
		
		//const char* recResult = (char*)QISRGetResult("tte=gb2312,rst=json,rse=gb2312,nlp_version=2.0",&result,5000,&ret);
	if (MSP_SUCCESS != errCode)
	{

		printf("MSPSearch failed, error code is : %d", errCode);

	}
	else
	{
		printf("***************************************\n");
		printf("\n**********������Ϊ%s**********\n",yuyi);
			printf("***************************************\n");
	}
	
//	printf("*********************�˳��̣߳�******************\n");
	
	return 0;
}


DWORD CALLBACK MicCallback(HWAVEIN hwavein, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	static int voiceUsing = -1;
	switch (uMsg)
	{
	case WIM_OPEN:
		printf("\n�豸�Ѿ���...\n");
		break;

	case WIM_DATA:
		{
			//	printf("\n������%d����...\n", ((LPWAVEHDR)dwParam1)->dwUser);
			bool blank = true; 
			for (int i = 0; i < ((LPWAVEHDR)dwParam1)->dwBytesRecorded / 2; i++) {
				if (abs(*((short *)((LPWAVEHDR)dwParam1)->lpData + i)) >AMPLITUDE_CRITICAL) {
					blank = false;
					break;
				}
			}
			if (true == blank) {

				// ******  һ������ʶ����� ******
				if (-1 != voiceUsing) {
					voices[voiceUsing].recOver = true;
					voiceUsing = -1;
					printf("\n*************���Խ���¼��!**************\n");
				}
				else{
				//	printf("*************������¼�롣");
				}
			}
			else {
				if(-1 == voiceUsing){

					// ******  Ѱ�һ���������ʶ�� ******
					int j = 0;
					for(int i = 0 ; i< VOICE_NUM ; i ++){
						if( true == voices[i].availaible){
							// ******  ��ʼ����ʶ�� ******
							printf("\n*************��ʼ����ʶ��***************\n");
							voiceUsing = i;
							voices[i].availaible = false;
							voices[i].recTime = 0;
							voices[i].recOver = false;
							memcpy(voices[i].voice + voices[i].recTime * 32000, ((LPWAVEHDR)dwParam1)->lpData, 32000);
							voices[voiceUsing].recTime++;
							CreateThread(NULL,0,iat,LPVOID(i),NULL,NULL);
							break;
						}else{
							j++;
						}
					}
					if( VOICE_NUM == j){
						printf("\n************�޷���������ʶ�����޿��û��壡******************\n");
					}
				}
				else{
				//	printf("*************ʶ����*************");
					memcpy(voices[voiceUsing].voice + voices[voiceUsing].recTime * 32000,((LPWAVEHDR)dwParam1)->lpData,32000);
					voices[voiceUsing].recTime++;
				}
			}
			waveInAddBuffer(hwavein, (LPWAVEHDR)dwParam1, sizeof(WAVEHDR));//�����������͸��豸�����������������������á�
		}
		break;
		/*
		MMRESULT waveInAddBuffer
		(
		HWAVEIN hwi,  //��Ƶ�����豸���
		LPWAVEHDR pwh,//ָ��WAVEHDR�ṹ��ָ��,��ʶ׼���Ļ�����
		UINT cbwh    //WAVEHDR�ṹ�Ĵ�С,ʹ��sizeof����
		)

		����WAVEHDR�ṹ��
		typedef struct wavehdr_tag
		{
		LPSTR   lpData;     //ָ���θ�ʽ�Ļ�����
		DWORD   dwBufferLength; //�������Ĵ�С
		DWORD   dwBytesRecorded; //��ǰ�洢�˶�������
		DWORD_PTR dwUser;     //�û�����
		DWORD   dwFlags;      //Ϊ�������ṩ����Ϣ,��waveInPrepareHeader������ʹ��WHDR_PREPARED
		DWORD   dwLoops;     //���ʱʹ��,��ʶ���Ŵ���
		struct wavehdr_tag * lpNext;//reserved
		DWORD_PTR reserved;     //reserved
		} WAVEHDR, *LPWAVEHDR;
		*/
	case WIM_CLOSE:
		printf("\n�豸�Ѿ��ر�...\n");
		break;
	default:
		break;
	}
	return 0;
}
/*
ÿ����������ʱ�������һ���ص��źţ������ûص���������ص����ڵȣ����嶨����waveinopen�����ڣ�����ֻ���ص���������������ص��ź��Զ����ص��������գ��ص��������ݻص��ź���ִ�и�����Ӧ�Ĳ������ص�����ִ����󣬳������ص�ԭ��ִ��λ�ü���ִ�С��ص������ľ������£�
�ص��ź�һ������������Ӧ�����ֻص����������õ������
1��  WIM_OPEN
��ִ��waveinopen��������ʱ������ûص�����������������ص��źš�����¼���豸�Ѿ��򿪡�����λص������ĵ����У������Լ��趨һЩ������Ҳ����û�в�����
2��  WIM_DATA
��ÿ�黺�������ʱ����������ص��źţ������ûص�����������ε����У��ص�����Ӧ����������Ĺ������Ա�¼���������У�
�������Ļ���鴦����������ļ��������������豸��
��¼���豸�����µĻ���飻¼���豸�κ�ʱ��Ӧ��ӵ�в�����2���Ļ���飬�Ա�֤¼��������ԡ�
3��  WIM_CLOSE
������waveinclose����ʱ�����������ص��źţ�����¼���豸�رճɹ�����λص����������У�����ִ����Ӧ��һЩ�ر��ļ�������Ϣ�ȵȵĲ������Զ��塣
����MicCallback�ص�������ʽ��
void CALLBACK MicCallback
(
HWAVEIN hwi,     //�ص��˺������豸���
UINT uMsg,      //��������������Ϣ,��ʶ�ر�(WIM_CLOSE)����������(WIM_DATA)����(WIM_OPEN).
DWORD_PTR dwInstance, //�û���waveInOpenָ��������
DWORD_PTR dwParam1,  //(LPWAVEHDR)dwParam1,�û�ָ���Ļ�����
DWORD_PTR dwParam2
);
*/
void RecordWave()
{
	int count = 0;
	count = waveInGetNumDevs();//1st ����ϵͳ�о����Ĳ������������豸������
	if( 0 == count ){
		printf("***********û���������ã����򼴽�����**************\n", count);
		exit(0);
	}
	// printf("\n��Ƶ����������%d\n", count);

	WAVEINCAPS waveIncaps;
	/*
	����WAVEINCAPS�ṹ��ĺ��壺
	typedef struct
	{
	WORD   wMid;        //��Ƶ�豸�����̶�������������ʶ
	WORD   wPid;        //��Ƶ�����豸�Ĳ�Ʒ��ʶ
	MMVERSION vDriverVersion;    //��������汾��
	TCHAR   szPname[MAXPNAMELEN];//����������
	DWORD   dwFormats;      //֧�ֵĸ�ʽ,�μ�MSDN
	WORD   wChannels;      //֧�ֵ�������
	WORD   wReserved1;      //��������
	} WAVEINCAPS;

	*/
	MMRESULT mmResult = waveInGetDevCaps(0, &waveIncaps, sizeof(WAVEINCAPS));//2nd ���ָ�����������豸������
	/*
	MMRESULT waveInGetDevCaps(UINT_PTR uDeviceID,LPWAVEINCAPS pwic,UINT cbwic);
	//uDeviceID ��Ƶ�����豸��ʶ,Ҳ����Ϊһ���򿪵���Ƶ�����豸�ľ��.
	//  ������Ϊ�����һ������˶���豸��������������ʶÿһ���豸.
	//pwic ��WAVEINCAPS�ṹ���һ��ָ��,�����豸����Ƶ����.
	//cbwic WAVEINCAPS�ṹ��Ĵ�С,ʹ��sizeof����.
	//  MMRESULT ����ִ�еĽ��!!!!!!!!!!!
	//  MMSYSERR_NOERROR ��ʾִ�гɹ�
	//  MMSYSERR_BADDEVICEID ����Խ��
	//  MMSYSERR_NODRIVER û�о������豸
	//  MMSYSERR_NOMEM ���ܷ�����������ڴ�
	*/

	//printf("\n��Ƶ�����豸��%s\n", waveIncaps.szPname);

	if (MMSYSERR_NOERROR != mmResult){
		switch(mmResult){
		case MMSYSERR_BADDEVICEID:
			printf("**********����Խ�����*********\n");
			break;
		case MMSYSERR_NODRIVER:
			printf("**********û�о������豸*********\n");
			break;
		case MMSYSERR_NOMEM:
			printf("**********���ܷ�����������ڴ�*********\n");
			break;
		default:
			printf("**********����δ֪����*********\n");
			break;
		}
		printf("**********���򼴽�����*********\n");
		exit(0);
	}

	HWAVEIN phwi;           //���մ򿪵���Ƶ�����豸��ʶ��HWAVEIN�ṹ��ָ��
	WAVEFORMATEX pwfx;      //һ������ĸ�ʽ����¼����WAVEFORMATEX�ṹ��ָ��
	WaveInitFormat(&pwfx, 1, 16000, 16);

	//printf("\n�������Ƶ�����豸");
	//printf("\n���������������� 16kHz 16bit\n");

	mmResult = waveInOpen(&phwi, WAVE_MAPPER, &pwfx, (DWORD)(MicCallback), NULL, CALLBACK_FUNCTION);//3rd ��ָ������Ƶ�����豸������¼��
	/*	
	MMRESULT waveInOpen(
	LPHWAVEIN    phwi,        //���մ򿪵���Ƶ�����豸��ʶ��HWAVEIN�ṹ��ָ��
	UINT_PTR    uDeviceID,      //ָ��һ����Ҫ�򿪵��豸��ʶ.����ʹ��WAVE_MAPPERѡ��һ����ָ��¼����ʽ¼�����豸
	LPWAVEFORMATEX pwfx,        //һ������ĸ�ʽ����¼����WAVEFORMATEX�ṹ��ָ��
	DWORD_PTR   dwCallback,    //ָ��һ���ص��������¼���������ھ�����̱߳�ʶ,��¼���¼����д���.
	DWORD_PTR   dwCallbackInstance, //�����ص����ƵĲ���
	DWORD     fdwOpen      //���豸�ķ�����ʶ,ָ���ص�������.�μ�CSDN
	);
	*/

	if(MMSYSERR_NOERROR != mmResult){
		switch(mmResult){
		case MMSYSERR_BADDEVICEID:
			printf("**********�豸ID����*********\n");
			break;
		case MMSYSERR_ALLOCATED:
			printf("**********ָ������Դ�ѱ�����*********\n");
			break;
		case MMSYSERR_NODRIVER:
			printf("**********û�а�װ��������*********\n");
			break;
		case MMSYSERR_NOMEM:
			printf("**********���ܷ���������ڴ�*********\n");
			break;
		case WAVERR_BADFORMAT:
			printf("**********�豸��֧������Ĳ��θ�ʽ*********\n");
			break;
		default:
			printf("**********����δ֪����*********\n");
			break;
		}
		printf("**********���򼴽�����*********\n");
		exit(0);
	}


	/*
	����WAVEHDR�ṹ��
	typedef struct wavehdr_tag {
	LPSTR   lpData;     //ָ���θ�ʽ�Ļ�����
	DWORD   dwBufferLength; //�������Ĵ�С
	DWORD   dwBytesRecorded; //��ǰ�洢�˶�������
	DWORD_PTR dwUser;     //�û�����
	DWORD   dwFlags;      //Ϊ�������ṩ����Ϣ,��waveInPrepareHeader������ʹ��WHDR_PREPARED
	DWORD   dwLoops;     //���ʱʹ��,��ʶ���Ŵ���
	struct wavehdr_tag * lpNext;//reserved
	DWORD_PTR reserved;     //reserved
	} WAVEHDR, *LPWAVEHDR;
	*/


	/*
	Ϊ��Ƶ�����豸׼��һ��������
	MMRESULT waveInPrepareHeader(
	HWAVEIN hwi,  //��Ƶ�����豸���
	LPWAVEHDR pwh,//ָ��WAVEHDR�ṹ��ָ��,��ʶ׼���Ļ�����
	UINT cbwh    //WAVEHDR�ṹ�Ĵ�С,ʹ��sizeof����
	);
	*/
	WAVEHDR pwh[4];
	for (int i = 000; i < 4; i++) {
		//WAVEHDR pwh;
		char * buffer1 = (char *)malloc(pwfx.nAvgBytesPerSec);//һ�� 32K ��16K 16λ ����£�
		if (NULL == buffer1) {
			printf("**********������%d����ʧ��!**********\n",i);
			continue;
		}
		pwh[i].lpData = buffer1;
		pwh[i].dwBufferLength = pwfx.nAvgBytesPerSec;
		pwh[i].dwUser = i;
		pwh[i].dwFlags = 0;
		mmResult = waveInPrepareHeader(phwi, &pwh[i], sizeof(WAVEHDR));//4th Ϊ��Ƶ�����豸׼��һ��������

		if (MMSYSERR_NOERROR != mmResult){
			switch(mmResult){
			case MMSYSERR_INVALHANDLE:
				printf("**********�豸�����Ч*********\n");
				break;
			case MMSYSERR_HANDLEBUSY:
				printf("**********������������ʹ�ñ��豸*********\n");
				break;
			case MMSYSERR_NOMEM:
				printf("**********���ܷ�����������ڴ�*********\n");
				break;
			default:
				printf("**********����δ֪����*********\n");
				break;
			}
			printf("**********���򼴽�����*********\n");
			exit(0);
		}

		mmResult = waveInAddBuffer(phwi, &pwh[i], sizeof(WAVEHDR));//5th �����������͸��豸�����������������������á�
		if (MMSYSERR_NOERROR != mmResult){
			switch(mmResult){
			case MMSYSERR_INVALHANDLE:
				printf("**********�豸�����Ч*********\n");
				break;
			case MMSYSERR_HANDLEBUSY:
				printf("**********������������ʹ�ñ��豸*********\n");
				break;
			case WAVERR_UNPREPARED:
				printf("**********û��׼���û�����*********\n");
				break;
			default:
				printf("**********����δ֪����*********\n");
				break;
			}
			printf("**********���򼴽�����*********\n");
			exit(0);
		}
	}


	mmResult = waveInStart(phwi);//6th ��ʼ����¼��

	/*
	MMRESULT waveInStart(
	HWAVEIN hwi //�豸���
	);
	*/

	if (MMSYSERR_NOERROR != mmResult){
		switch(mmResult){
		case MMSYSERR_INVALHANDLE:
			printf("**********�豸�����Ч*********\n");
			break;
		case MMSYSERR_HANDLEBUSY:
			printf("**********������������ʹ�ñ��豸*********\n");
			break;
		default:
			printf("**********����δ֪����*********\n");
			break;
		}
		printf("**********���򼴽�����*********\n");
		exit(0);
	}
	printf("\n**********���Խ�������ʶ��**********\n");
}

void main()
{

	for(int i  = 0; i < VOICE_NUM ; i++)
	{
		voices[i].voice = (char *)malloc(32000 * MAX_REC_TIME);
		if(NULL == voices[i].voice){
			printf("*************�����%d���ڴ�ʧ�ܣ���*************\n",i+1);
			if(0 == i){
				printf("�����˳�����\n");
				return;
			}else{
				voices[i].availaible = false; 
				break;
			}
		}
		voices[i].availaible = true;
	}

	const char* login_configs = "appid = 57188b50, work_dir =   .  ";//��¼����
	int ret = MSPLogin(NULL, NULL, login_configs);//��һ������Ϊ�û������ڶ�������Ϊ���룬�����������ǵ�¼�������û�����������Ҫ��http://open.voicecloud.cnע�Ტ��ȡappid
	if (ret != MSP_SUCCESS)
	{
		printf("MSPLogin failed , Error code %d.\n", ret);
		return;
	}
	RecordWave();
	while (1);

	MSPLogout();//�˳���¼
	_getch();
}