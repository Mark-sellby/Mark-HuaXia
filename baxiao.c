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
};  //十六进制联合体
#define NUM 6
#define DELAYTIME 0.025  //延时时长
typedef enum
{
    COM_GET_HEADER1 = 0,
    COM_GET_HEADER2,
    COM_GET_LEN,
    COM_GET_DATA,
    COM_GET_CHECK
}ComDecodeState;  //转译状态
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
int CreateBackgroundThread(void)  //创建后台线程
{
    int ret = -1;
    ret = CmtNewThreadPool (1, &threadPoolHandle);   //创建线程池并把句柄保存到threadpoolHandle变量中
    
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

int QuitBackgroundThread(void)  //结束后台线程，释放,清零
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
                if(data[i] == 0x03)// du多个寄存器数据
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
	                payloadLen =data[i]; // 两个数据位
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
                    //校验成功
                    frameDataRx[frameDataRxLen - 1] = 0;
					if(payloadLen ==2)
					{
						// 根据地址解析
						switch (getaddrflag)
						{
							case 0: // 运行模式
							{	
								if(frameDataRx[4] == 0)
								{
									SetCtrlVal (panelHandle, PANEL_YXMODE,"位置" );
								}
								else
								{
									SetCtrlVal (panelHandle, PANEL_YXMODE,"压力" );
								}
								break;
							}
							case 130:
							{
								if(frameDataRx[4]==0)
								{
									SetCtrlVal (panelHandle, PANEL_YXMODSET, "位置");
								}
								else
								{
									SetCtrlVal (panelHandle, PANEL_YXMODSET, "压力");
								}
							}	
							case 10:// 运行状态
							{	
								if(frameDataRx[4] == 0)
								{
									SetCtrlVal (panelHandle, PANEL_YXSTATUS,"停机" );
								}
								else
								{
									SetCtrlVal (panelHandle, PANEL_YXSTATUS,"运行" );
								}
								break;
							}
							case 20: // 设备状态
							{
								switch (frameDataRx[4])
								{
									case 0:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"正常" );
										break;
									}
									case 1:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"区间压力下限报警" );
										break;
									}
									case 2:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"区间压力上限报警" );
										break;
									}
									case 3:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置1压力下限报警" );
										break;
									}
									case 4:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置1压力上限报警" );
										break;
									}
									case 5:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置2压力下限报警" );
										break;
									}
									case 6:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置2压力上限报警" );
										break;
									}
									case 7:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置3压力下限报警" );
										break;
									}
									case 8:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置3压力上限报警" );
										break;
									}
									case 9:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置4压力下限报警" );
										break;
									}
									case 10:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置4压力上限报警" );
										break;
									}
									case 11:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置5压力下限报警" );
										break;
									}
									case 12:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"位置5压力上限报警" );
										break;
									}
									case 13:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"退极限位报警" );
										break;
									}
									case 14:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"进极限位报警" );
										break;
									}
									case 15:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"光栅报警" );
										break;
									}
									case 16:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"伺服报警" );
										break;
									}
									case 17:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"机械原点未复位" );
										break;
									}
									case 18:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"上极限位置报警" );
										break;
									}
									case 19:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"下极限位置报警" );
										break;
									}
									case 20:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"急停" );
										break;
									}
									case 21:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"超设备压力量程上限" );
										break;
									}
									case 22:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"未放置料件" );
										break;
									}
									case 23:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"自动下压时操作按钮松开" );
										break;
									}
									case 24:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"压力位置判断下限报警" );
										break;
									}
									case 25:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"压力位置判断上限报警" );
										break;
									}
									case 26:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"快进过程中检测到压力" );
										break;
									}
									case 27:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"NG产品未入仓" );
										break;
									}
									case 28:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"瞬间失压报警" );
										break;
									}
									case 29:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"累计失压报警" );
										break;
									}
									case 30:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标1压力下限报警" );
										break;
									}
									case 31:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标1压力上限报警" );
										break;
									}
									case 32:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标2压力下限报警" );
										break;
									}
									case 33:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标2压力上限报警" );
										break;
									}
									case 34:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标3压力下限报警" );
										break;
									}
									case 35:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标3压力上限报警" );
										break;
									}
									case 36:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标4压力下限报警" );
										break;
									}
									case 37:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标4压力上限报警" );
										break;
									}
									case 38:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标5压力下限报警" );
										break;
									}
									case 39:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"样标5压力上限报警" );
										break;
									}
									case 40:
									{
										SetCtrlVal (panelHandle, PANEL_STRING,"光栅尺异常报警" );
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
						// 根据地址解析
						switch (getaddrflag)
						{
							case 1:// 目标位置
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
							case 3:// 目标压力
							{								
								SetCtrlVal (panelHandle, PANEL_MBYALI, revdata.fdata);
								break;
							}							
							case 5: // 完成位置
							{
								SetCtrlVal (panelHandle, PANEL_WCWZ, revdata.fdata);
								break;
							}	
							case 7: // 完成压力
							{
								SetCtrlVal (panelHandle, PANEL_WCYALI, revdata.fdata);
								break;
							}
							case 11: // 实时位置
							{
								SetCtrlVal (panelHandle, PANEL_SSWZ, revdata.fdata);
								PlotStripChart (panelHandle, PANEL_WEIZICURVE,datapoints , 1, 0, 0, VAL_FLOAT);
								break;
							}
							case 13:// 实时压力
							{
								SetCtrlVal (panelHandle, PANEL_SSYALI, revdata.fdata);
								 PlotStripChart (panelHandle, PANEL_YALICURVE,&revdata.fdata , 1, 0, 0, VAL_FLOAT);
								break;
							}
							case 131:// 压装位置设置
							{
								SetCtrlVal (panelHandle, PANEL_YZWZSET, revdata.fdata);
								break;
							}
							case 133:// 目标位置设置
							{
								SetCtrlVal (panelHandle, PANEL_MBWZSET, revdata.fdata);
								break;
							}
							case 135:// 快进速度设置
							{
								SetCtrlVal (panelHandle, PANEL_KJSDSET, revdata.fdata);
								break;
							}
							case 137:// 慢压速度设置
							{
								SetCtrlVal (panelHandle, PANEL_MYSDSET, revdata.fdata);
								break;
							}
							case 139:// 到位压力设置
							{
								SetCtrlVal (panelHandle, PANEL_DWYLSET, revdata.fdata);
								break;
							}
							case 141:// 保压时间设置
							{
								SetCtrlVal (panelHandle, PANEL_BYTIMESET, revdata.idata);
								break;
							}
							case 143:// 原点位置设置
							{
								SetCtrlVal (panelHandle, PANEL_YDWZSET, revdata.fdata);
								break;
							}
							case 145:// 压力系数设置
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
	if (InitCVIRTE (0, argv, 0) == 0) //初始化CVI运行时环境的函数，确保内存分配成功。
		return -1;	/* out of memory */
	if ((panelHandle = LoadPanel (0, "baxiao.uir", PANEL)) < 0) //加载名为 "baxiao.uir" 的用户界面文件中的主面板 "PANEL"。
		return -1;
	if ((pfpanelset = LoadPanel (0, "baxiao.uir", PFPARASET)) < 0) // 加载用户界面文件中的另一个面板 "PFPARASET"。
		return -1;
	//returnval = OpenComConfig (portNum, "", 9600, 0, 8, 1, 512, 512); 
	returnval = OpenComConfig (portNum, "", 9600, 2, 8, 1, 512, 512); // 数据位设置影响数据范围的读取，8的范围为0~255,7为0~127
	if(returnval ==0 )
	{
		portOpenFlag= 1;
	}
	else 
	{
		MessagePopup("串口打开","失败");
	}
	DisplayPanel (panelHandle);
	RunUserInterface ();
	CloseCom(portNum);
	DiscardPanel (panelHandle);
	return 0;
}

int CVICALLBACK act_callback (int panel, int control, int event,  
							  void *callbackData, int eventData1, int eventData2)  //启动
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t startorder[11]={0x64,0x10,0x00,0x0F,0x00,0x01,0x02,0x00,0x01,0xF0,0x3D}; // 启动
			
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
								void *callbackData, int eventData1, int eventData2)  //机械原点
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t jxstartorder[11]={0x64,0x10,0x00,0x11,0x00,0x01,0x02,0x00,0x01,0xF3,0x83}; // 机械原点启动
			for(iloop=0;iloop<11;iloop++)
			{
				ComWrtByte(portNum,jxstartorder[iloop]);
			}
			
			break;
	}
	return 0;
}

int CVICALLBACK gzact_callback (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)  //工作原点
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t gzstartorder[11]={0x64,0x10,0x00,0x12,0x00,0x01,0x02,0x00,0x01,0xF3,0xB0}; // 工作原点启动
			for(iloop=0;iloop<11;iloop++)
			{
				ComWrtByte(portNum,gzstartorder[iloop]);
			}
			break;
	}
	return 0;
}

int CVICALLBACK fwwarn_callback (int panel, int control, int event,
								 void *callbackData, int eventData1, int eventData2)  //报警复位
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t warnorder[8]={0x64,0x06,0x00,0x13,0x00,0x01,0xB0,0x3A}; // 报警复位
			for(iloop=0;iloop<8;iloop++)
			{
				ComWrtByte(portNum,warnorder[iloop]);
			}

			break;
	}
	return 0;
}

int CVICALLBACK pfset_callback (int panel, int control, int event,
								void *callbackData, int eventData1, int eventData2)  //配方设置
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
								  void *callbackData, int eventData1, int eventData2)  //退出界面
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
			uint8_t yzlocalorder[13]={0x64,0x10,0x00,0x83,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00};// 压装位置132-1
			uint8_t mblocalorder[13]={0x64,0x10,0x00,0x85,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00};// 目标位置134-1
			uint8_t kjspeedorder[13]={0x64,0x10,0x00,0x87,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // 快进速度136-1
			uint8_t myspeedorder[13]={0x64,0x10,0x00,0x89,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // 慢压速度138-1
			uint8_t dwylorder[13]={0x64,0x10,0x00,0x8B,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // 到位压力140-1
			uint8_t ydlocalorder[13]={0x64,0x10,0x00,0x8F,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // 原点位置144-1
			uint8_t ylxsorder[13]={0x64,0x10,0x00,0x91,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // 压力系数 146-1
			
			uint8_t yxmodorder[11]={0x64,0x10,0x00,0x82,0x00,0x01,0x02,0x00,0x00,0x00,0x00}; // 运行模式131-1
			uint8_t bytimeorder[13]={0x64,0x10,0x00,0x8D,0x00,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00}; // 保压时间142-1
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
			
			/*************************压装位置设置***************************/
			tenToHex(yzlocalset.idata, yzlocalval);
			for(iloop=0;iloop<4;iloop++)
			{
				yzlocalorder[7+iloop] = yzlocalval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(yzlocalorder,11);
			yzlocalorder[11]=checkSumCal>>8;
			yzlocalorder[12]=checkSumCal&0xFF;
					
			
			/*************************目标位置设置***************************/
			tenToHex(mblocalset.idata, mblocalval);
			for(iloop=0;iloop<4;iloop++)
			{
				mblocalorder[7+iloop] = mblocalval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(mblocalorder,11);
			mblocalorder[11]=checkSumCal>>8;
			mblocalorder[12]=checkSumCal&0xFF;
			
			
			/*************************快进速度设置***************************/
			tenToHex(kjspeedset.idata, kjspeedval);// kjspeedset.idata
			for(iloop=0;iloop<4;iloop++)
			{
				kjspeedorder[7+iloop] = kjspeedval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(kjspeedorder,11);
			kjspeedorder[11]=checkSumCal>>8;
			kjspeedorder[12]=checkSumCal&0xFF;
			
			/*************************慢压速度设置***************************/
			tenToHex(myspeedset.idata, myspeedval);
			for(iloop=0;iloop<4;iloop++)
			{
				myspeedorder[7+iloop] = myspeedval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(myspeedorder,11);
			myspeedorder[11]=checkSumCal>>8;
			myspeedorder[12]=checkSumCal&0xFF;
			
			
			/*************************到位压力设置***************************/
			tenToHex(dwylset.idata, dwylval);
			for(iloop=0;iloop<4;iloop++)
			{
				dwylorder[7+iloop] = dwylval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(dwylorder,11);
			dwylorder[11]=checkSumCal>>8;
			dwylorder[12]=checkSumCal&0xFF;
			
			
			/*************************原点位置设置***************************/
			tenToHex(ydloacalset.idata, ydloacalval);
			for(iloop=0;iloop<4;iloop++)
			{
				ydlocalorder[7+iloop] = ydloacalval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(ydlocalorder,11);
			ydlocalorder[11]=checkSumCal>>8;
			ydlocalorder[12]=checkSumCal&0xFF;
			
			/*************************压力系数设置***************************/
			tenToHex(ylxsset.idata, ylxsval);
			for(iloop=0;iloop<4;iloop++)
			{
				ylxsorder[7+iloop] = ylxsval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(ylxsorder,11);
			ylxsorder[11]=checkSumCal>>8;
			ylxsorder[12]=checkSumCal&0xFF;
			
			
			/*************************保压时间设置***************************/
			tenToHex(bytimeset, bytimeval);
			for(iloop=0;iloop<4;iloop++)
			{
				bytimeorder[7+iloop] = bytimeval[iloop];
			}
			checkSumCal=0;
			checkSumCal = CRC16_MudBus(bytimeorder,11);
			bytimeorder[11]=checkSumCal>>8;
			bytimeorder[12]=checkSumCal&0xFF;
			
			/*************************运行模式设置***************************/
			if(yxmod==0) // 位置
			{
				yxmodorder[7] =0x00;
				yxmodorder[8]=0x00;								
			}
			else //压力
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
* 函数名 :CRC16
* 描述 : 计算CRC16
* 输入 : puchMsg---数据地址,usDataLen---数据长度
* 输出 : 校验值
*/
uint16_t CRC16_MudBus(uint8_t *puchMsg, uint8_t usDataLen)
{
	
	uint16_t uCRC = 0xffff;//CRC寄存器
	uint8_t uchCRCHi = 0xFF ;              // 高CRC字节初始化  
	uint8_t uchCRCLo = 0xFF ;              // 低CRC 字节初始化 
	
	
	for(uint8_t num=0;num<usDataLen;num++){
		uCRC = (*puchMsg++)^uCRC;//把数据与16位的CRC寄存器的低8位相异或，结果存放于CRC寄存器。
		for(uint8_t x=0;x<8;x++)
		{	//循环8次
			if(uCRC&0x0001)
			{	//判断最低位为：“1”
				uCRC = uCRC>>1;	//先右移
				uCRC = uCRC^0xA001;	//再与0xA001异或
			}else
			{	//判断最低位为：“0”
				uCRC = uCRC>>1;	//右移
			}
		}
	}
	uchCRCHi = (uint8_t)uCRC;
	uchCRCLo = uCRC>> 8 ;
//	return uCRC;//返回CRC校验值
	return (uchCRCHi << 8 | uCRC>> 8);	// MODBUS 规定高位在前
	
	
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
								  void *callbackData, int eventData1, int eventData2)  //产量清零
{
	switch (event)
	{
		case EVENT_COMMIT:
			int iloop;
			uint8_t warnorder[8]={0x64,0x06,0x00,0x10,0x00,0x01,0x40,0x3A}; // 产量清零
			for(iloop=0;iloop<8;iloop++)
			{
				ComWrtByte(portNum,warnorder[iloop]);
			}

			break;
	}
	return 0;
}

int CVICALLBACK read_callback (int panel, int control, int event,
							   void *callbackData, int eventData1, int eventData2)  //时钟
{
	switch (event)
	{
		case EVENT_TIMER_TICK:
			int iloop,i;
			int len;
			static int startnum =0;
			uint8_t runstateorder[8]={0x64,0x03,0x00,0x0A,0x00,0x01,0xAD,0xFD}; //运行状态
			uint8_t runmodorder[8]  ={0x64,0x03,0x00,0x00,0x00,0x01,0x8D,0xFF}; //运行模式
			uint8_t mbwzorder[8]  ={0x64,0x03,0x00,0x01,0x00,0x02,0x9C,0x3E}; //目标位置
			uint8_t mbylorder[8]  ={0x64,0x03,0x00,0x03,0x00,0x02,0x3D,0xFE}; //目标压力
			uint8_t ssylorder[8]  ={0x64,0x03,0x00,0x0D,0x00,0x02,0x5C,0x3D}; //实时压力
			uint8_t wcwzorder[8]  ={0x64,0x03,0x00,0x05,0x00,0x02,0xDD,0xFF}; //完成位置
			uint8_t wcylorder[8]  ={0x64,0x03,0x00,0x07,0x00,0x02,0x7C,0x3F}; //完成压力
			
			uint8_t sbztorder[8]  ={0x64,0x03,0x00,0x14,0x00,0x01,0xCD,0xFB}; //设备状态
			uint8_t sswzorder[8]  ={0x64,0x03,0x00,0x0B,0x00,0x02,0xBC,0x3C}; //实时位置
			
			uint8_t yzwzset[8]  ={0x64,0x03,0x00,0x83,0x00,0x02,0x3C,0x16}; //压装位置设置
			uint8_t mbwzset[8]  ={0x64,0x03,0x00,0x85,0x00,0x02,0xDC,0x17}; //目标位置设置
			uint8_t kjsdset[8]  ={0x64,0x03,0x00,0x87,0x00,0x02,0x7D,0xD7}; //快进速度设置
			uint8_t mysdset[8]  ={0x64,0x03,0x00,0x89,0x00,0x02,0x1C,0x14}; //慢压速度设置
			
			uint8_t dwylset[8]  ={0x64,0x03,0x00,0x8B,0x00,0x02,0xBD,0xD4}; //到位压力设置
			uint8_t bysjset[8]  ={0x64,0x03,0x00,0x8D,0x00,0x02,0x5D,0xD5}; //保压时间设置
			uint8_t ydwzset[8]  ={0x64,0x03,0x00,0x8F,0x00,0x02,0xFC,0x15}; //原点位置设置
			uint8_t ylxsset[8]  ={0x64,0x03,0x00,0x91,0x00,0x02,0x9C,0x13}; //压力系数设置
			uint8_t yxmsset[8]  ={0x64,0x03,0x00,0x82,0x00,0x01,0x2D,0xD7}; //运行模式设置
			
			/************************运行状态*************************/
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
						getaddrflag = 10;//运行状态
		                ComDecodeProcess(receiveData,len);   
		            }
		        }
			}
			
			
			///************************运行模式*************************/
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
			
			///************************目标位置*************************/
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
			
			/************************目标压力*************************/
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
			
			///************************实时压力*************************/
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
			
			///************************完成位置*************************/
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
			
			///************************完成压力*************************/
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
			
			///************************设备状态*************************/
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
			
			///************************实时位置*************************/
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
			
			/************************压装位置设置*************************/
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
			
			/************************目标位置设置*************************/
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
			
			/************************快进速度设置*************************/
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
			
			/************************慢压速度设置*************************/
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
			
			/************************到位压力设置*************************/
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
			
			/************************保压时间设置*************************/
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
			/************************原点位置设置*************************/
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
			
			/************************压力系数设置*************************/
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
			
			/************************运行模式设置*************************/
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
