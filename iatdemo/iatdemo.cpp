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

//最大录音时长
#define MAX_REC_TIME 100


#define AMPLITUDE_CRITICAL 4000
#define VOICE_NUM 2

//自定义声音结构体
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
	m_WaveFormat->wFormatTag = WAVE_FORMAT_PCM;// 向结构体中参数进行赋值
	m_WaveFormat->nChannels = nCh;
	m_WaveFormat->nSamplesPerSec = nSampleRate;
	m_WaveFormat->nAvgBytesPerSec = nSampleRate * nCh * BitsPerSample / 8;
	m_WaveFormat->nBlockAlign = nCh * BitsPerSample / 8;
	m_WaveFormat->wBitsPerSample = BitsPerSample;
	m_WaveFormat->cbSize = 0;
}
/*                      wave函数初始化参数列表；

介绍WAVEFORMATEX结构体的含义：
typedef struct {
WORD wFormatTag;    //波形声音的格式,单声道双声道使用WAVE_FORMAT_PCM.当包含在WAVEFORMATEXTENSIBLE结构中时,使用WAVE_FORMAT_EXTENSIBLE.
WORD nChannels;    //声道数量
DWORD nSamplesPerSec;  //采样率.wFormatTag为WAVE_FORMAT_PCM时,有8.0kHz,11.025kHz,22.05kHz,和44.1kHz.
DWORD nAvgBytesPerSec;  //每秒的采样字节数.通过nSamplesPerSec * nChannels * wBitsPerSample / 8计算
WORD nBlockAlign;    //每次采样的字节数.通过nChannels * wBitsPerSample / 8计算
WORD wBitsPerSample;  //采样位数.wFormatTag为WAVE_FORMAT_PCM时,为8或者16
WORD cbSize;      //wFormatTag为WAVE_FORMAT_PCM时,忽略此参数
} WAVEFORMATEX;

*/

DWORD WINAPI iat(LPVOID lpParam){
	//printf("*******************进入线程！***************\n");

	int voiceIndex = (int)lpParam;

	int errCode;
	int epStatus = MSP_EP_LOOKING_FOR_SPEECH;
	int recStatus;
	const char* param = "sub = iat, domain = iat, language = zh_ch, accent = mandarin, sample_rate = 16000, result_type = json, result_encoding = gb2312,sch=1,nlp_version=2.0";//可参考可设置参数列表
	const char * sessionID = QISRSessionBegin(NULL, param, &errCode);//开始一路会话

	//printf("****************开始会话错误%d\n",errCode);
	char result[1000]={0};
	int first = 0;
	int ret;

	for(int send = 0 ; send< voices[voiceIndex].recTime * 32000 || voices[voiceIndex].recOver != true; ){
		if(send < voices[voiceIndex].recTime * 32000 )
		{

			ret = QISRAudioWrite(sessionID, (const void *)(voices[voiceIndex].voice + send), 16000, MSP_AUDIO_SAMPLE_CONTINUE, &epStatus, &recStatus);//写音频
			send+=16000;
		//	printf("ret %d ep %d recStatus %d \n",ret , epStatus , recStatus);

		}
		if (recStatus == MSP_REC_STATUS_SUCCESS) {
			const char *rslt = QISRGetResult(sessionID, &recStatus, 0, &errCode);//服务端已经有识别结果，可以获取
			
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
		const char *rslt = QISRGetResult(sessionID, &recStatus, 0, &errCode);//获取结果
		if (NULL != rslt)
		{
			strcat(result,rslt);

		}
		_sleep(150);
	}

	QISRSessionEnd(sessionID, NULL);
	printf("***************************************\n");
	printf("\n识别结果为：%s\n",result);
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
		printf("\n**********语义结果为%s**********\n",yuyi);
			printf("***************************************\n");
	}
	
//	printf("*********************退出线程！******************\n");
	
	return 0;
}


DWORD CALLBACK MicCallback(HWAVEIN hwavein, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	static int voiceUsing = -1;
	switch (uMsg)
	{
	case WIM_OPEN:
		printf("\n设备已经打开...\n");
		break;

	case WIM_DATA:
		{
			//	printf("\n缓冲区%d存满...\n", ((LPWAVEHDR)dwParam1)->dwUser);
			bool blank = true; 
			for (int i = 0; i < ((LPWAVEHDR)dwParam1)->dwBytesRecorded / 2; i++) {
				if (abs(*((short *)((LPWAVEHDR)dwParam1)->lpData + i)) >AMPLITUDE_CRITICAL) {
					blank = false;
					break;
				}
			}
			if (true == blank) {

				// ******  一段语音识别结束 ******
				if (-1 != voiceUsing) {
					voices[voiceUsing].recOver = true;
					voiceUsing = -1;
					printf("\n*************可以进行录音!**************\n");
				}
				else{
				//	printf("*************无声音录入。");
				}
			}
			else {
				if(-1 == voiceUsing){

					// ******  寻找缓冲区进行识别 ******
					int j = 0;
					for(int i = 0 ; i< VOICE_NUM ; i ++){
						if( true == voices[i].availaible){
							// ******  开始进行识别 ******
							printf("\n*************开始进行识别***************\n");
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
						printf("\n************无法进行语音识别，暂无可用缓冲！******************\n");
					}
				}
				else{
				//	printf("*************识别中*************");
					memcpy(voices[voiceUsing].voice + voices[voiceUsing].recTime * 32000,((LPWAVEHDR)dwParam1)->lpData,32000);
					voices[voiceUsing].recTime++;
				}
			}
			waveInAddBuffer(hwavein, (LPWAVEHDR)dwParam1, sizeof(WAVEHDR));//将缓冲区发送给设备，若缓冲区填满，则不起作用。
		}
		break;
		/*
		MMRESULT waveInAddBuffer
		(
		HWAVEIN hwi,  //音频输入设备句柄
		LPWAVEHDR pwh,//指向WAVEHDR结构的指针,标识准备的缓冲区
		UINT cbwh    //WAVEHDR结构的大小,使用sizeof即可
		)

		介绍WAVEHDR结构：
		typedef struct wavehdr_tag
		{
		LPSTR   lpData;     //指向波形格式的缓冲区
		DWORD   dwBufferLength; //缓冲区的大小
		DWORD   dwBytesRecorded; //当前存储了多少数据
		DWORD_PTR dwUser;     //用户数据
		DWORD   dwFlags;      //为缓冲区提供的信息,在waveInPrepareHeader函数中使用WHDR_PREPARED
		DWORD   dwLoops;     //输出时使用,标识播放次数
		struct wavehdr_tag * lpNext;//reserved
		DWORD_PTR reserved;     //reserved
		} WAVEHDR, *LPWAVEHDR;
		*/
	case WIM_CLOSE:
		printf("\n设备已经关闭...\n");
		break;
	default:
		break;
	}
	return 0;
}
/*
每个缓冲块存满时，会产生一个回调信号，并调用回调函数（或回调窗口等，具体定义在waveinopen函数内，这里只讲回调函数的情况）；回调信号自动被回调函数接收，回调函数根据回调信号来执行各种相应的操作。回调函数执行完后，程序跳回到原来执行位置继续执行。回调函数的具体如下：
回调信号一般有三个，对应着三种回调函数被调用的情况：
1、  WIM_OPEN
当执行waveinopen（）函数时，会调用回调函数，并产生这个回调信号。代表录音设备已经打开。在这次回调函数的调用中，可以自己设定一些操作，也可以没有操作。
2、  WIM_DATA
当每块缓存块填满时，产生这个回调信号，并调用回调函数。在这次调用中，回调函数应当完成这样的工作，以便录音连续进行：
将存满的缓存块处理，例如存入文件，或送入其他设备；
向录音设备送入新的缓存块；录音设备任何时刻应当拥有不少于2个的缓存块，以保证录音不间断性。
3、  WIM_CLOSE
当调用waveinclose函数时，会产生这个回调信号，代表录音设备关闭成功。这次回调函数调用中，可以执行相应的一些关闭文件保存信息等等的操作，自定义。
介绍MicCallback回调函数格式：
void CALLBACK MicCallback
(
HWAVEIN hwi,     //回调此函数的设备句柄
UINT uMsg,      //波形声音输入信息,标识关闭(WIM_CLOSE)、缓冲区满(WIM_DATA)、打开(WIM_OPEN).
DWORD_PTR dwInstance, //用户在waveInOpen指定的数据
DWORD_PTR dwParam1,  //(LPWAVEHDR)dwParam1,用户指定的缓冲区
DWORD_PTR dwParam2
);
*/
void RecordWave()
{
	int count = 0;
	count = waveInGetNumDevs();//1st 返回系统中就绪的波形声音输入设备的数量
	if( 0 == count ){
		printf("***********没有输入设置，程序即将结束**************\n", count);
		exit(0);
	}
	// printf("\n音频输入数量：%d\n", count);

	WAVEINCAPS waveIncaps;
	/*
	介绍WAVEINCAPS结构体的含义：
	typedef struct
	{
	WORD   wMid;        //音频设备制造商定义的驱动程序标识
	WORD   wPid;        //音频输入设备的产品标识
	MMVERSION vDriverVersion;    //驱动程序版本号
	TCHAR   szPname[MAXPNAMELEN];//制造商名称
	DWORD   dwFormats;      //支持的格式,参见MSDN
	WORD   wChannels;      //支持的声道数
	WORD   wReserved1;      //保留参数
	} WAVEINCAPS;

	*/
	MMRESULT mmResult = waveInGetDevCaps(0, &waveIncaps, sizeof(WAVEINCAPS));//2nd 检查指定波形输入设备的特性
	/*
	MMRESULT waveInGetDevCaps(UINT_PTR uDeviceID,LPWAVEINCAPS pwic,UINT cbwic);
	//uDeviceID 音频输入设备标识,也可以为一个打开的音频输入设备的句柄.
	//  个人认为如果上一步获得了多个设备，可以用索引标识每一个设备.
	//pwic 对WAVEINCAPS结构体的一个指针,包含设备的音频特性.
	//cbwic WAVEINCAPS结构体的大小,使用sizeof即可.
	//  MMRESULT 函数执行的结果!!!!!!!!!!!
	//  MMSYSERR_NOERROR 表示执行成功
	//  MMSYSERR_BADDEVICEID 索引越界
	//  MMSYSERR_NODRIVER 没有就绪的设备
	//  MMSYSERR_NOMEM 不能分配或者锁定内存
	*/

	//printf("\n音频输入设备：%s\n", waveIncaps.szPname);

	if (MMSYSERR_NOERROR != mmResult){
		switch(mmResult){
		case MMSYSERR_BADDEVICEID:
			printf("**********索引越界错误*********\n");
			break;
		case MMSYSERR_NODRIVER:
			printf("**********没有就绪的设备*********\n");
			break;
		case MMSYSERR_NOMEM:
			printf("**********不能分配或者锁定内存*********\n");
			break;
		default:
			printf("**********发生未知错误*********\n");
			break;
		}
		printf("**********程序即将结束*********\n");
		exit(0);
	}

	HWAVEIN phwi;           //接收打开的音频输入设备标识的HWAVEIN结构的指针
	WAVEFORMATEX pwfx;      //一个所需的格式进行录音的WAVEFORMATEX结构的指针
	WaveInitFormat(&pwfx, 1, 16000, 16);

	//printf("\n请求打开音频输入设备");
	//printf("\n采样参数：单声道 16kHz 16bit\n");

	mmResult = waveInOpen(&phwi, WAVE_MAPPER, &pwfx, (DWORD)(MicCallback), NULL, CALLBACK_FUNCTION);//3rd 打开指定的音频输入设备，进行录音
	/*	
	MMRESULT waveInOpen(
	LPHWAVEIN    phwi,        //接收打开的音频输入设备标识的HWAVEIN结构的指针
	UINT_PTR    uDeviceID,      //指定一个需要打开的设备标识.可以使用WAVE_MAPPER选择一个按指定录音格式录音的设备
	LPWAVEFORMATEX pwfx,        //一个所需的格式进行录音的WAVEFORMATEX结构的指针
	DWORD_PTR   dwCallback,    //指向一个回调函数、事件句柄、窗口句柄、线程标识,对录音事件进行处理.
	DWORD_PTR   dwCallbackInstance, //传给回调机制的参数
	DWORD     fdwOpen      //打开设备的方法标识,指定回调的类型.参见CSDN
	);
	*/

	if(MMSYSERR_NOERROR != mmResult){
		switch(mmResult){
		case MMSYSERR_BADDEVICEID:
			printf("**********设备ID超界*********\n");
			break;
		case MMSYSERR_ALLOCATED:
			printf("**********指定的资源已被分配*********\n");
			break;
		case MMSYSERR_NODRIVER:
			printf("**********没有安装驱动程序*********\n");
			break;
		case MMSYSERR_NOMEM:
			printf("**********不能分配或锁定内存*********\n");
			break;
		case WAVERR_BADFORMAT:
			printf("**********设备不支持请求的波形格式*********\n");
			break;
		default:
			printf("**********发生未知错误*********\n");
			break;
		}
		printf("**********程序即将结束*********\n");
		exit(0);
	}


	/*
	介绍WAVEHDR结构：
	typedef struct wavehdr_tag {
	LPSTR   lpData;     //指向波形格式的缓冲区
	DWORD   dwBufferLength; //缓冲区的大小
	DWORD   dwBytesRecorded; //当前存储了多少数据
	DWORD_PTR dwUser;     //用户数据
	DWORD   dwFlags;      //为缓冲区提供的信息,在waveInPrepareHeader函数中使用WHDR_PREPARED
	DWORD   dwLoops;     //输出时使用,标识播放次数
	struct wavehdr_tag * lpNext;//reserved
	DWORD_PTR reserved;     //reserved
	} WAVEHDR, *LPWAVEHDR;
	*/


	/*
	为音频输入设备准备一个缓冲区
	MMRESULT waveInPrepareHeader(
	HWAVEIN hwi,  //音频输入设备句柄
	LPWAVEHDR pwh,//指向WAVEHDR结构的指针,标识准备的缓冲区
	UINT cbwh    //WAVEHDR结构的大小,使用sizeof即可
	);
	*/
	WAVEHDR pwh[4];
	for (int i = 000; i < 4; i++) {
		//WAVEHDR pwh;
		char * buffer1 = (char *)malloc(pwfx.nAvgBytesPerSec);//一秒 32K （16K 16位 情况下）
		if (NULL == buffer1) {
			printf("**********缓冲区%d申请失败!**********\n",i);
			continue;
		}
		pwh[i].lpData = buffer1;
		pwh[i].dwBufferLength = pwfx.nAvgBytesPerSec;
		pwh[i].dwUser = i;
		pwh[i].dwFlags = 0;
		mmResult = waveInPrepareHeader(phwi, &pwh[i], sizeof(WAVEHDR));//4th 为音频输入设备准备一个缓冲区

		if (MMSYSERR_NOERROR != mmResult){
			switch(mmResult){
			case MMSYSERR_INVALHANDLE:
				printf("**********设备句柄无效*********\n");
				break;
			case MMSYSERR_HANDLEBUSY:
				printf("**********其他进程正在使用本设备*********\n");
				break;
			case MMSYSERR_NOMEM:
				printf("**********不能分配或者锁定内存*********\n");
				break;
			default:
				printf("**********发生未知错误*********\n");
				break;
			}
			printf("**********程序即将结束*********\n");
			exit(0);
		}

		mmResult = waveInAddBuffer(phwi, &pwh[i], sizeof(WAVEHDR));//5th 将缓冲区发送给设备，若缓冲区填满，则不起作用。
		if (MMSYSERR_NOERROR != mmResult){
			switch(mmResult){
			case MMSYSERR_INVALHANDLE:
				printf("**********设备句柄无效*********\n");
				break;
			case MMSYSERR_HANDLEBUSY:
				printf("**********其他进程正在使用本设备*********\n");
				break;
			case WAVERR_UNPREPARED:
				printf("**********没有准备好缓冲区*********\n");
				break;
			default:
				printf("**********发生未知错误*********\n");
				break;
			}
			printf("**********程序即将结束*********\n");
			exit(0);
		}
	}


	mmResult = waveInStart(phwi);//6th 开始进行录制

	/*
	MMRESULT waveInStart(
	HWAVEIN hwi //设备句柄
	);
	*/

	if (MMSYSERR_NOERROR != mmResult){
		switch(mmResult){
		case MMSYSERR_INVALHANDLE:
			printf("**********设备句柄无效*********\n");
			break;
		case MMSYSERR_HANDLEBUSY:
			printf("**********其他进程正在使用本设备*********\n");
			break;
		default:
			printf("**********发生未知错误*********\n");
			break;
		}
		printf("**********程序即将结束*********\n");
		exit(0);
	}
	printf("\n**********可以进行语音识别**********\n");
}

void main()
{

	for(int i  = 0; i < VOICE_NUM ; i++)
	{
		voices[i].voice = (char *)malloc(32000 * MAX_REC_TIME);
		if(NULL == voices[i].voice){
			printf("*************申请第%d块内存失败！！*************\n",i+1);
			if(0 == i){
				printf("即将退出程序！\n");
				return;
			}else{
				voices[i].availaible = false; 
				break;
			}
		}
		voices[i].availaible = true;
	}

	const char* login_configs = "appid = 57188b50, work_dir =   .  ";//登录参数
	int ret = MSPLogin(NULL, NULL, login_configs);//第一个参数为用户名，第二个参数为密码，第三个参数是登录参数，用户名和密码需要在http://open.voicecloud.cn注册并获取appid
	if (ret != MSP_SUCCESS)
	{
		printf("MSPLogin failed , Error code %d.\n", ret);
		return;
	}
	RecordWave();
	while (1);

	MSPLogout();//退出登录
	_getch();
}