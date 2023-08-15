#include <cvirte.h>		
#include <userint.h>
#include "baxiao.h"
#include <rs232.h>
#include <utility.h>
#include <ansi_c.h>      
#include <userint.h>

union HexToFloat{
	unsigned int idata;
	float  fdata;
};  //ʮ������������
#define NUM 6
#define DELAYTIME 0.025  //��ʱʱ��
typedef enum
{
    COM_GET_HEADER1 = 0,
    COM_GET_HEADER2,
    COM_GET_LEN,
    COM_GET_DATA,
    COM_GET_CHECK
}ComDecodeState;  //ת��״̬
ComDecodeState comDecodeState = COM_GET_HEADER1;
uint8_t frameDataRx[512];
int frameDataRxLen = 0;

uint8_t payloadLen = 0;
uint8_t payloadLenCnt = 0;
uint8_t getaddrflag = 0;
static int panelHandle,pfpanelset;
CmtThreadPoolHandle threadPoolHandle;
CmtThreadFunctionID backgroundThreadFunctionID;
int CVICALLBACK BackgroundThreadFunction (void *functionData);
uint16_t CRC16_MudBus(uint8_t *puchMsg, uint8_t usDataLen);
int tenToHex(int data,uint8_t s[4]);
char errorStringBuffer[256];
int sendDataCnt = 0;
int receiveDataCnt = 0;
uint8_t receiveData[513];
int ireceiveData[513];
//uint8_t tmpData[513];
uint16_t checkSumRx = 0;

uint16_t checkSumCal = 0;

volatile int AppRunningFlag = 0;
int portOpenFlag = 0;
int portNum = 1;
int CreateBackgroundThread(void)  //������̨�߳�
{
    int ret = -1;
    ret = CmtNewThreadPool (1, &threadPoolHandle);   //�����̳߳ز��Ѿ�����浽threadpoolHandle������
    
    if(ret < 0)
    {
        CmtGetErrorMessage(ret,errorStringBuffer);
        printf("%s\r\n",errorStringBuffer);
    }
    else
    {
        ret = CmtScheduleThreadPoolFunctionAdv(threadPoolHandle,BackgroundThreadFunction,NULL,THREAD_PRIORITY_HIGHEST,NULL,EVENT_TP_THREAD_FUNCTION_END,NULL,RUN_IN_SCHEDULED_THREAD,&backgroundThreadFunctionID);
        
        if(ret < 0)
        {
            CmtGetErrorMessage(ret,errorStringBuffer);
            printf("%s\r\n",errorStringBuffer); 
        }
    }
    return ret;
}

int QuitBackgroundThread(void)  //������̨�̣߳��ͷ�,����
{    
	CmtReleaseThreadPoolFunctionID (threadPoolHandle, backgroundThreadFunctionID);
				backgroundThreadFunctionID = 0;
				
    return 0;
}

void ComDecodeProcess(uint8_t *data,int dataLen)
{
    int i;
    for(i=0;i<dataLen;i++)
    {
        switch(comDecodeState)
        {
            case COM_GET_HEADER1:
                if(data[i] == 0x64)
                {
                    comDecodeState = COM_GET_HEADER2;
                    frameDataRx[frameDataRxLen++] = data[i];
                }
                break;
            case COM_GET_HEADER2:
                if(data[i] == 0x03)// du����Ĵ�������
                {
                    comDecodeState = COM_GET_LEN;
                    frameDataRx[frameDataRxLen++] = data[i]; 
                }
                else
                {
                    comDecodeState = COM_GET_HEADER1;
                    frameDataRxLen = 0;
                }
                break;
            case COM_GET_LEN:
					comDecodeState = COM_GET_DATA;
	                payloadLen =data[i]; // ��������λ
	                payloadLenCnt = 0;
	                frameDataRx[frameDataRxLen++] = data[i]; 
                break;
            case COM_GET_DATA:
                if(payloadLenCnt < payloadLen)
                {
                    frameDataRx[frameDataRxLen++] = data[i];
                    payloadLenCnt++;
                    if(payloadLenCnt == payloadLen)
                    {
                        comDecodeState = COM_GET_CHECK;    
                    }
                }
                else
                {
                    comDecodeState = COM_GET_HEADER1;
                    frameDataRxLen = 0;
                }
                break;
            case COM_GET_CHECK:
                frameDataRx[frameDataRxLen++] = data[i];  
                checkSumRx = data[i]<<8|data[i+1];
                checkSumCal = CRC16_MudBus(frameDataRx,frameDataRxLen - 1);
                if(checkSumRx != checkSumCal)
                {
                    printf("checkSum error\r\n");        
                }
                else
                {
                    //У��ɹ�
                    frameDataRx[frameDataRxLen - 1] = 0;
					if(payloadLen ==2)
					{
						// ���ݵ�ַ����
						switch (getaddrflag)
						{
							case 0: // ����ģʽ
							{	
								if(frameDataRx[4] == 0)
								{
									SetCtrlVal (panelHandle, PANEL_YXMODE,"λ��" );
								}
								else
								{
									SetCtrlVal (panelHandle, PANEL_YXMODE,"ѹ��" );
								}
								break;
							}
							case 130:
							{
								if(frameDataRx[4]==0)
								{
									SetCtrlVal (panelHandle, PANEL_YXMODSET, "λ��");
								}
								else
								{
									SetCtrlVal (panelHandle, PANEL_YXMODSET, "ѹ��");
								}
							}	
							case 10:// ����״̬
							{	
								if(frameDataRx[4] == 0)
								{
									SetCtrlVal (panelHandle, PANEL_YXSTATUS,"ͣ��" );
								}
								else
								{
									SetCtrlVal (panelHandle, PANEL_YXSTATUS,"����" );
								}
								break;
							}
							case 20: // �豸״̬
							{
								switch (frameDataRx[4])
								{
									case 0:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����" );
										break;
									}
									case 1:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����ѹ�����ޱ���" );
										break;
									}
									case 2:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����ѹ�����ޱ���" );
										break;
									}
									case 3:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��1ѹ�����ޱ���" );
										break;
									}
									case 4:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��1ѹ�����ޱ���" );
										break;
									}
									case 5:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��2ѹ�����ޱ���" );
										break;
									}
									case 6:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��2ѹ�����ޱ���" );
										break;
									}
									case 7:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��3ѹ�����ޱ���" );
										break;
									}
									case 8:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��3ѹ�����ޱ���" );
										break;
									}
									case 9:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��4ѹ�����ޱ���" );
										break;
									}
									case 10:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��4ѹ�����ޱ���" );
										break;
									}
									case 11:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��5ѹ�����ޱ���" );
										break;
									}
									case 12:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"λ��5ѹ�����ޱ���" );
										break;
									}
									case 13:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"�˼���λ����" );
										break;
									}
									case 14:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"������λ����" );
										break;
									}
									case 15:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"��դ����" );
										break;
									}
									case 16:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"�ŷ�����" );
										break;
									}
									case 17:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"��еԭ��δ��λ" );
										break;
									}
									case 18:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"�ϼ���λ�ñ���" );
										break;
									}
									case 19:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"�¼���λ�ñ���" );
										break;
									}
									case 20:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"��ͣ" );
										break;
									}
									case 21:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"���豸ѹ����������" );
										break;
									}
									case 22:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"δ�����ϼ�" );
										break;
									}
									case 23:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"�Զ���ѹʱ������ť�ɿ�" );
										break;
									}
									case 24:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"ѹ��λ���ж����ޱ���" );
										break;
									}
									case 25:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"ѹ��λ���ж����ޱ���" );
										break;
									}
									case 26:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"��������м�⵽ѹ��" );
										break;
									}
									case 27:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"NG��Ʒδ���" );
										break;
									}
									case 28:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"˲��ʧѹ����" );
										break;
									}
									case 29:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"�ۼ�ʧѹ����" );
										break;
									}
									case 30:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����1ѹ�����ޱ���" );
										break;
									}
									case 31:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����1ѹ�����ޱ���" );
										break;
									}
									case 32:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����2ѹ�����ޱ���" );
										break;
									}
									case 33:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����2ѹ�����ޱ���" );
										break;
									}
									case 34:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����3ѹ�����ޱ���" );
										break;
									}
									case 35:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����3ѹ�����ޱ���" );
										break;
									}
									case 36:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����4ѹ�����ޱ���" );
										break;
									}
									case 37:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����4ѹ�����ޱ���" );
										break;
									}
									case 38:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����5ѹ�����ޱ���" );
										break;
									}
									case 39:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"����5ѹ�����ޱ���" );
										break;
									}
									case 40:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"��դ���쳣����" );
										break;
									}
									default:
										break;
								}
								break;
							}				
							default:
								break;
						}
					}
					else if(payloadLen ==4)
					{
						float datapoints[4];
						union HexToFloat revdata;
						revdata.idata = frameDataRx[3]<<24|frameDataRx[4]<<16|frameDataRx[5]<<8|frameDataRx[6];
						datapoints[0] = revdata.fdata;		
						// ���ݵ�ַ����
						switch (getaddrflag)
						{
							case 1:// Ŀ��λ��
							{
								SetCtrlVal (panelHandle, PANEL_MLLOCAL, revdata.fdata);	
				             /* while(num)
							  {
								  num--;
							  	PlotStripChart (panelHandle, PANEL_YALICURVE,datapoints , 2, 0, 0, VAL_FLOAT);
							  }
							  */
								break;
							}								
							case 3:// Ŀ��ѹ��
							{								
								SetCtrlVal (panelHandle, PANEL_MBYALI, revdata.fdata);
								break;
							}							
							case 5: // ���λ��
							{
								SetCtrlVal (panelHandle, PANEL_WCWZ, revdata.fdata);
								break;
							}	
							case 7: // ���ѹ��
							{
								SetCtrlVal (panelHandle, PANEL_WCYALI, revdata.fdata);
								break;
							}
							case 11: // ʵʱλ��
							{
								SetCtrlVal (panelHandle, PANEL_SSWZ, revdata.fdata);
								PlotStripChart (panelHandle, PANEL_WEIZICURVE,datapoints , 1, 0, 0, VAL_FLOAT);
								break;
							}
							case 13:// ʵʱѹ��
							{
								SetCtrlVal (panelHandle, PANEL_SSYALI, revdata.fdata);
								 PlotStripChart (panelHandle, PANEL_YALICURVE,&revdata.fdata , 1, 0, 0, VAL_FLOAT);
								break;
							}
							case 131:// ѹװλ������
							{
								SetCtrlVal (panelHandle, PANEL_YZWZSET, revdata.fdata);
								break;
							}
							case 133:// Ŀ��λ������
							{
								SetCtrlVal (panelHandle, PANEL_MBWZSET, revdata.fdata);
								break;
							}
							case 135:// ����ٶ�����
							{
								SetCtrlVal (panelHandle, PANEL_KJSDSET, revdata.fdata);
								break;
							}
							case 137:// ��ѹ�ٶ�����
							{
								SetCtrlVal (panelHandle, PANEL_MYSDSET, revdata.fdata);
								break;
							}
							case 139:// ��λѹ������
							{
								SetCtrlVal (panelHandle, PANEL_DWYLSET, revdata.fdata);
								break;
							}
							case 141:// ��ѹʱ������
							{
								SetCtrlVal (panelHandle, PANEL_BYTIMESET, revdata.idata);
								break;
							}
							case 143:// ԭ��λ������
							{
								SetCtrlVal (panelHandle, PANEL_YDWZSET, revdata.fdata);
								break;
							}
							case 145:// ѹ��ϵ������
							{
								SetCtrlVal (panelHandle, PANEL_YLXSSET, revdata.fdata);
								break;
							}
							default:
								break;
						}
					}   
                }
                comDecodeState = COM_GET_HEADER1;
                frameDataRxLen = 0;
                break;
        }
    }
}

int CVICALLBACK BackgroundThreadFunction (void *functionData)
{
    AppRunningFlag = 1;
    while(AppRunningFlag)
    {
    }
    return 0;
}

int main (int argc, char *argv[])
{
	int returnval;
	if (InitCVIRTE (0, argv, 0) == 0) //��ʼ��CVI����ʱ�����ĺ�����ȷ���ڴ����ɹ���
		return -1;	/* out of memory */
	if ((panelHandle = LoadPanel (0, "baxiao.uir", PANEL)) < 0) //������Ϊ "baxiao.uir" ���û������ļ��е������ "PANEL"��
		return -1;
	if ((pfpanelset = LoadPanel (0, "baxiao.uir", PFPARASET)) < 0) // �����û������ļ��е���һ����� "PFPARASET"��
		return -1;
	//returnval = OpenComConfig (portNum, "", 9600, 0, 8, 1, 512, 512); 
	returnval = OpenComConfig (portNum, "", 9600, 2, 8, 1, 512, 512); // ����λ����Ӱ�����ݷ�Χ�Ķ�ȡ��8�ķ�ΧΪ0~255,7Ϊ0~127
	if(returnval ==0 )
	{
		portOpenFlag= 1;
	}
	else 
	{
		MessagePopup("���ڴ�","ʧ��");
	}
	DisplayPanel (panelHandle);
	RunUserInterface ();
	CloseCom(portNum);
	DiscardPanel (panelHandle);
	return 0;
}

int CVICALLBACK act_callback (int panel, int control, int event,  
							  void *callbackData, int eventData1, int eventData2)  //����
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t startorder[11]={0x64,0x10,0x00,0x0F,0x00,0x01,0x02,0x00,0x01,0xF0,0x3D}; // ����
			
			/*for(iloop=0;iloop<11;iloop++)
			{
				ComWrtByte(portNum,startorder[iloop]);
			}*/
			SetCtrlAttribute (panelHandle, PANEL_TIMER, ATTR_ENABLED, 1);
			break;
	}
	return 0;
}

int CVICALLBACK jxact_callback (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)  //��еԭ��
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t jxstartorder[11]={0x64,0x10,0x00,0x11,0x00,0x01,0x02,0x00,0x01,0xF3,0x83}; // ��еԭ������
			for(iloop=0;iloop<11;iloop++)
			{
				ComWrtByte(portNum,jxstartorder[iloop]);
			}
			
			break;
	}
	return 0;
}

int CVICALLBACK gzact_callback (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)  //����ԭ��
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t gzstartorder[11]={0x64,0x10,0x00,0x12,0x00,0x01,0x02,0x00,0x01,0xF3,0xB0}; // ����ԭ������
			for(iloop=0;iloop<11;iloop++)
			{
				ComWrtByte(portNum,gzstartorder[iloop]);
			}
			break;
	}
	return 0;
}

int CVICALLBACK fwwarn_callback (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)  //������λ
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t warnorder[8]={0x64,0x06,0x00,0x13,0x00,0x01,0xB0,0x3A}; // ������λ
			for(iloop=0;iloop<8;iloop++)
			{
				ComWrtByte(portNum,warnorder[iloop]);
			}

			break;
	}
	return 0;
}

int CVICALLBACK pfset_callback (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)  //�䷽����
{
	switch (event)
	{
		case EVENT_COMMIT:

				HidePanel (panelHandle);
				DisplayPanel (pfpanelset);
			break;
	}
	return 0;
}

int CVICALLBACK exitcmd_callback (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)  //�˳�����
{
	switch (event)
	{
		case EVENT_COMMIT:
			portOpenFlag=0;
			AppRunningFlag = 0;
			SetCtrlAttribute (panelHandle, PANEL_TIMER, ATTR_ENABLED, 0);
			QuitUserInterface (0);
			break;
	}
	return 0;
}

int CVICALLBACK savepara_callback (int panel, int control, int event,
								   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:

			union HexToFloat yzlocalset,mblocalset,kjspeedset,myspeedset,dwylset,ydloacalset,ylxsset;
			int yxmod,bytimeset;
			uint8_t yzlocalorder[13]={0x64,0x10,0x00,0x83,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00};// ѹװλ��132-1
			uint8_t mblocalorder[13]={0x64,0x10,0x00,0x85,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00};// Ŀ��λ��134-1
			uint8_t kjspeedorder[13]={0x64,0x10,0x00,0x87,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // ����ٶ�136-1
			uint8_t myspeedorder[13]={0x64,0x10,0x00,0x89,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // ��ѹ�ٶ�138-1
			uint8_t dwylorder[13]={0x64,0x10,0x00,0x8B,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // ��λѹ��140-1
			uint8_t ydlocalorder[13]={0x64,0x10,0x00,0x8F,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // ԭ��λ��144-1
			uint8_t ylxsorder[13]={0x64,0x10,0x00,0x91,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // ѹ��ϵ�� 146-1
			
			uint8_t yxmodorder[11]={0x64,0x10,0x00,0x82,0x00,0x01,0x02,0x00,0x00,0x00,0x00}; // ����ģʽ131-1
			uint8_t bytimeorder[13]={0x64,0x10,0x00,0x8D,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // ��ѹʱ��142-1
			uint8_t yzlocalval[4],mblocalval[4],kjspeedval[4],myspeedval[4],dwylval[4],ydloacalval[4],ylxsval[4],bytimeval[4];
			int iloop,jloop;
			
			GetCtrlVal (pfpanelset, PFPARASET_YZWZSET, &yzlocalset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_MBWZSET, &mblocalset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_KJSDSET, &kjspeedset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_MYSDSET, &myspeedset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_DWYLSET, &dwylset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_YDWZSET, &ydloacalset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_YLXSSET, &ylxsset.fdata);
			GetCtrlVal (pfpanelset, PFPARASET_YXMOD, &yxmod);
			GetCtrlVal (pfpanelset, PFPARASET_BYTIMESET, &bytimeset);
			
			/*************************ѹװλ������***************************/
			tenToHex(yzlocalset.idata, yzlocalval);
			for(iloop=0;iloop<4;iloop++)
			{
				yzlocalorder[7+iloop] = yzlocalval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(yzlocalorder,11);
			yzlocalorder[11]=checkSumCal>>8;
			yzlocalorder[12]=checkSumCal&0xFF;
					
			
			/*************************Ŀ��λ������***************************/
			tenToHex(mblocalset.idata, mblocalval);
			for(iloop=0;iloop<4;iloop++)
			{
				mblocalorder[7+iloop] = mblocalval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(mblocalorder,11);
			mblocalorder[11]=checkSumCal>>8;
			mblocalorder[12]=checkSumCal&0xFF;
			
			
			/*************************����ٶ�����***************************/
			tenToHex(kjspeedset.idata, kjspeedval);// kjspeedset.idata
			for(iloop=0;iloop<4;iloop++)
			{
				kjspeedorder[7+iloop] = kjspeedval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(kjspeedorder,11);
			kjspeedorder[11]=checkSumCal>>8;
			kjspeedorder[12]=checkSumCal&0xFF;
			
			/*************************��ѹ�ٶ�����***************************/
			tenToHex(myspeedset.idata, myspeedval);
			for(iloop=0;iloop<4;iloop++)
			{
				myspeedorder[7+iloop] = myspeedval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(myspeedorder,11);
			myspeedorder[11]=checkSumCal>>8;
			myspeedorder[12]=checkSumCal&0xFF;
			
			
			/*************************��λѹ������***************************/
			tenToHex(dwylset.idata, dwylval);
			for(iloop=0;iloop<4;iloop++)
			{
				dwylorder[7+iloop] = dwylval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(dwylorder,11);
			dwylorder[11]=checkSumCal>>8;
			dwylorder[12]=checkSumCal&0xFF;
			
			
			/*************************ԭ��λ������***************************/
			tenToHex(ydloacalset.idata, ydloacalval);
			for(iloop=0;iloop<4;iloop++)
			{
				ydlocalorder[7+iloop] = ydloacalval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(ydlocalorder,11);
			ydlocalorder[11]=checkSumCal>>8;
			ydlocalorder[12]=checkSumCal&0xFF;
			
			/*************************ѹ��ϵ������***************************/
			tenToHex(ylxsset.idata, ylxsval);
			for(iloop=0;iloop<4;iloop++)
			{
				ylxsorder[7+iloop] = ylxsval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(ylxsorder,11);
			ylxsorder[11]=checkSumCal>>8;
			ylxsorder[12]=checkSumCal&0xFF;
			
			
			/*************************��ѹʱ������***************************/
			tenToHex(bytimeset, bytimeval);
			for(iloop=0;iloop<4;iloop++)
			{
				bytimeorder[7+iloop] = bytimeval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(bytimeorder,11);
			bytimeorder[11]=checkSumCal>>8;
			bytimeorder[12]=checkSumCal&0xFF;
			
			/*************************����ģʽ����***************************/
			if(yxmod==0) // λ��
			{
				yxmodorder[7] =0x00;
				yxmodorder[8]=0x00;								
			}
			else //ѹ��
			{
				yxmodorder[7] =0x00;
				yxmodorder[8]=0x01;
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(yxmodorder,9);
			yxmodorder[9]=checkSumCal>>8;
			yxmodorder[10]=checkSumCal&0xFF;
			for(jloop=0;jloop<NUM;jloop++)
			{
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,yzlocalorder[iloop]);
				}
			
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,mblocalorder[iloop]);
				}
				
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,kjspeedorder[iloop]);
				}
				
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,myspeedorder[iloop]);
				}
				
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,dwylorder[iloop]);
				}
				
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,ydlocalorder[iloop]);
				}
				
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,ylxsorder[iloop]);
				}
				
				for(iloop=0;iloop<13;iloop++)
				{
					ComWrtByte(portNum,bytimeorder[iloop]);
				}
				
				for(iloop=0;iloop<11;iloop++)
				{
					ComWrtByte(portNum,yxmodorder[iloop]);
				}	
			}
			DisplayPanel (panelHandle);
				HidePanel (pfpanelset);		
			break;
	}
	return 0;
}

int CVICALLBACK exitpfset_callback (int panel, int control, int event,
									void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			DisplayPanel (panelHandle);
				HidePanel (pfpanelset);
			break;
	}
	return 0;
}

int CVICALLBACK test_callback (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
				float datapoints[3];
				float gain = 1.0;
				int i=100;
							gain = gain * 5.8 + 1;
			while(i)
			{
			 datapoints[0] = 32 + gain * ((rand() / (double)RAND_MAX) - 0.5);
            datapoints[1] = 96 + gain * ((rand() / (float)RAND_MAX) - 0.5);
            datapoints[2] = 160 + gain * ((rand() / (float)RAND_MAX) - 0.5);
			
							PlotStripChart (panelHandle, PANEL_YALICURVE,datapoints , 2, 0, 0, VAL_FLOAT);
							i--;
			}	
			break;
	}
	return 0;
}

/*
* ������ :CRC16
* ���� : ����CRC16
* ���� : puchMsg---���ݵ�ַ,usDataLen---���ݳ���
* ��� : У��ֵ
*/
uint16_t CRC16_MudBus(uint8_t *puchMsg, uint8_t usDataLen)
{
	
	uint16_t uCRC = 0xffff;//CRC�Ĵ���
	uint8_t uchCRCHi = 0xFF ;              // ��CRC�ֽڳ�ʼ��  
	uint8_t uchCRCLo = 0xFF ;              // ��CRC �ֽڳ�ʼ�� 
	
	
	for(uint8_t num=0;num<usDataLen;num++){
		uCRC = (*puchMsg++)^uCRC;//��������16λ��CRC�Ĵ����ĵ�8λ����򣬽�������CRC�Ĵ�����
		for(uint8_t x=0;x<8;x++)
		{	//ѭ��8��
			if(uCRC&0x0001)
			{	//�ж����λΪ����1��
				uCRC = uCRC>>1;	//������
				uCRC = uCRC^0xA001;	//����0xA001���
			}else
			{	//�ж����λΪ����0��
				uCRC = uCRC>>1;	//����
			}
		}
	}
	uchCRCHi = (uint8_t)uCRC;
	uchCRCLo = uCRC>> 8 ;
//	return uCRC;//����CRCУ��ֵ
	return (uchCRCHi << 8 | uCRC>> 8);	// MODBUS �涨��λ��ǰ
	
	
}

int tenToHex(int data,uint8_t s[4])
{
	int  i = 0;
   
    char tmp[8]={0};
    while (data)
    {
		 tmp[i] = data % 16;
        i++;
        data = data / 16;
    }
  
				s[0] = (tmp[7]<<8|tmp[6]<<4)>>4;
				s[1] = (tmp[5]<<8|tmp[4]<<4)>>4;
				s[2] = (tmp[3]<<8|tmp[2]<<4)>>4;
				s[3] = (tmp[1]<<8|tmp[0]<<4)>>4;
				for(i=0;i<4;i++)
    return 0;
}

int CVICALLBACK clclean_callback (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)  //��������
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t warnorder[8]={0x64,0x06,0x00,0x10,0x00,0x01,0x40,0x3A}; // ��������
			for(iloop=0;iloop<8;iloop++)
			{
				ComWrtByte(portNum,warnorder[iloop]);
			}

			break;
	}
	return 0;
}

int CVICALLBACK read_callback (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)  //ʱ��
{
	switch (event)
	{
		case EVENT_TIMER_TICK:
			int iloop,i;
			int len;
			static int startnum =0;
			uint8_t runstateorder[8]={0x64,0x03,0x00,0x0A,0x00,0x01,0xAD,0xFD}; //����״̬
			uint8_t runmodorder[8]  ={0x64,0x03,0x00,0x00,0x00,0x01,0x8D,0xFF}; //����ģʽ
			uint8_t mbwzorder[8]  ={0x64,0x03,0x00,0x01,0x00,0x02,0x9C,0x3E}; //Ŀ��λ��
			uint8_t mbylorder[8]  ={0x64,0x03,0x00,0x03,0x00,0x02,0x3D,0xFE}; //Ŀ��ѹ��
			uint8_t ssylorder[8]  ={0x64,0x03,0x00,0x0D,0x00,0x02,0x5C,0x3D}; //ʵʱѹ��
			uint8_t wcwzorder[8]  ={0x64,0x03,0x00,0x05,0x00,0x02,0xDD,0xFF}; //���λ��
			uint8_t wcylorder[8]  ={0x64,0x03,0x00,0x07,0x00,0x02,0x7C,0x3F}; //���ѹ��
			
			uint8_t sbztorder[8]  ={0x64,0x03,0x00,0x14,0x00,0x01,0xCD,0xFB}; //�豸״̬
			uint8_t sswzorder[8]  ={0x64,0x03,0x00,0x0B,0x00,0x02,0xBC,0x3C}; //ʵʱλ��
			
			uint8_t yzwzset[8]  ={0x64,0x03,0x00,0x83,0x00,0x02,0x3C,0x16}; //ѹװλ������
			uint8_t mbwzset[8]  ={0x64,0x03,0x00,0x85,0x00,0x02,0xDC,0x17}; //Ŀ��λ������
			uint8_t kjsdset[8]  ={0x64,0x03,0x00,0x87,0x00,0x02,0x7D,0xD7}; //����ٶ�����
			uint8_t mysdset[8]  ={0x64,0x03,0x00,0x89,0x00,0x02,0x1C,0x14}; //��ѹ�ٶ�����
			
			uint8_t dwylset[8]  ={0x64,0x03,0x00,0x8B,0x00,0x02,0xBD,0xD4}; //��λѹ������
			uint8_t bysjset[8]  ={0x64,0x03,0x00,0x8D,0x00,0x02,0x5D,0xD5}; //��ѹʱ������
			uint8_t ydwzset[8]  ={0x64,0x03,0x00,0x8F,0x00,0x02,0xFC,0x15}; //ԭ��λ������
			uint8_t ylxsset[8]  ={0x64,0x03,0x00,0x91,0x00,0x02,0x9C,0x13}; //ѹ��ϵ������
			uint8_t yxmsset[8]  ={0x64,0x03,0x00,0x82,0x00,0x01,0x2D,0xD7}; //����ģʽ����
			
			/************************����״̬*************************/
			if(startnum%9==0)
			{
				if(portOpenFlag > 0)
		        {
					for(iloop=0;iloop<8;iloop++)
					{
						ComWrtByte(portNum,runstateorder[iloop]);
					}
					Delay(DELAYTIME);
		            len = GetInQLen(portNum);
		            if(len > 6)
		            {
		                memset(ireceiveData,0,513);
						memset(receiveData,0,513);
						
						for(i=0;i<len;i++)
						{
							ireceiveData[i]=ComRdByte(portNum);
							receiveData[i] = ireceiveData[i]&0xFF;
						}	
						getaddrflag = 10;//����״̬
		                ComDecodeProcess(receiveData,len);   
		            }
		        }
			}
			
			
			///************************����ģʽ*************************/
			if(startnum%9==1)
			{
				if(portOpenFlag > 0)
		        {
					for(iloop=0;iloop<8;iloop++)
					{
						ComWrtByte(portNum,runmodorder[iloop]);
					}
					Delay(DELAYTIME);
		            len = GetInQLen(portNum);
		            if(len >= 7)
		            {
		                memset(ireceiveData,0,513);
						memset(receiveData,0,513);
						
						for(i=0;i<len;i++)
						{
							ireceiveData[i]=ComRdByte(portNum);
							receiveData[i] = ireceiveData[i]&0xFF;
						}	
						getaddrflag = 0;//
		                ComDecodeProcess(receiveData,len);   
		            }
		        }
			}
			
			///************************Ŀ��λ��*************************/
			if(startnum%9==2)
			{
				if(portOpenFlag > 0)
		        {
					for(iloop=0;iloop<8;iloop++)
					{
						ComWrtByte(portNum,mbwzorder[iloop]);
					}
					Delay(DELAYTIME);
		            len = GetInQLen(portNum);
		            if(len > 6)
		            {
		                memset(ireceiveData,0,513);
						memset(receiveData,0,513);
						
						for(i=0;i<len;i++)
						{
							ireceiveData[i]=ComRdByte(portNum);
							receiveData[i] = ireceiveData[i]&0xFF;
						}	
						getaddrflag = 1;//
		                ComDecodeProcess(receiveData,len);   
		            }
		        }
			}
			
			/************************Ŀ��ѹ��*************************/
			if(startnum%9==3)
			{
				if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,mbylorder[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 3;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			}
			
			///************************ʵʱѹ��*************************/
			if(startnum%9==4)
			{
				if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,ssylorder[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 13;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			}
			
			///************************���λ��*************************/
			if(startnum%9==5)
			{
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,wcwzorder[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 5;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			}
			
			///************************���ѹ��*************************/
			if(startnum%9==6)
			{
				if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,wcylorder[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 7;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			}
			
			///************************�豸״̬*************************/
			if(startnum%9==7)
			{
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,sbztorder[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 20;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			}
			
			///************************ʵʱλ��*************************/
			if(startnum%9==8)
			{
				if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,sswzorder[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 11;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************ѹװλ������*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,yzwzset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 131;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************Ŀ��λ������*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,mbwzset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 133;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************����ٶ�����*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,kjsdset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 135;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************��ѹ�ٶ�����*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,mysdset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 137;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************��λѹ������*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,dwylset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 139;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************��ѹʱ������*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,bysjset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 141;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			/************************ԭ��λ������*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,ydwzset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 143;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************ѹ��ϵ������*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,ylxsset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 145;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			
			/************************����ģʽ����*************************/
			if(portOpenFlag > 0)
	        {
				for(iloop=0;iloop<8;iloop++)
				{
					ComWrtByte(portNum,yxmsset[iloop]);
				}
				Delay(DELAYTIME);
	            len = GetInQLen(portNum);
	            if(len > 6)
	            {
	                memset(ireceiveData,0,513);
					memset(receiveData,0,513);
					
					for(i=0;i<len;i++)
					{
						ireceiveData[i]=ComRdByte(portNum);
						receiveData[i] = ireceiveData[i]&0xFF;
					}	
					getaddrflag = 130;//
	                ComDecodeProcess(receiveData,len);   
	            }
	        }
			}
			startnum++;
			break;
	}
	return 0;
}
