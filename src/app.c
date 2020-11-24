/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/
/*******************************************************************************
Copyright (C) 2020 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include "app.h"
#include "system/mqtt/sys_mqtt.h"
// *****************************************************************************
// *****************************************************************************
// Section: Declarations
// *****************************************************************************
// *****************************************************************************


// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************
// *****************************************************************************
volatile APP_DATA            g_appData;

SYS_MODULE_OBJ      g_sSysMqttHandle = SYS_MODULE_OBJ_INVALID;
SYS_MQTT_Config     g_sTmpSysMqttCfg;
static uint32_t     g_lastPubTimeout = 0;
static uint32_t     PubMsgCnt = 0;
static SYS_WIFI_CONFIG wificonfig;
static bool         UserBtnState;

#define MQTT_PERIOIDC_PUB_TIMEOUT   30 //Sec
#define MQTT_PUB_TIMEOUT_CONST (MQTT_PERIOIDC_PUB_TIMEOUT * SYS_TMR_TickCounterFrequencyGet())

#define EXAMPLE_PRODUCT_KEY     "a1YlVZHpBjx"
#define EXAMPLE_DEVICE_NAME     "XIuLwuKwpIJg5P4hjtMp"
#define EXAMPLE_DEVICE_SECRET   "52e51c1461b97cef59b07e7e53514da8"
extern int aiotMqttSign(const char *productKey, const char *deviceName, const char *deviceSecret,
                     char clientId[150], char username[64], char password[65]);
// *****************************************************************************
// *****************************************************************************
// Section: Local data
// *****************************************************************************
// *****************************************************************************


// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************
int32_t MqttCallback(SYS_MQTT_EVENT_TYPE eEventType, void *data, uint16_t len, void* cookie)
{
    switch(eEventType)
    {
        case SYS_MQTT_EVENT_MSG_RCVD:
        {   
			SYS_MQTT_PublishConfig	*psMsg = (SYS_MQTT_PublishConfig	*)data;
            psMsg->message[psMsg->messageLength] = 0;
            psMsg->topicName[psMsg->topicLength] = 0;
            SYS_CONSOLE_PRINT("\nMqttCallback(): Msg received on Topic: %s ; Msg: %s\r\n", 
				psMsg->topicName, psMsg->message);
            char *toggleToken = "\"YellowLEDStatus\":";
            char *subString;

            if ((subString = strstr((char*)psMsg->message, toggleToken))) {
                if (subString[strlen(toggleToken)] == '1' ) {
                    LED_GREEN_Set();
                } else {
                    LED_GREEN_Clear();
                }
            }
        }
        break;
        
        case SYS_MQTT_EVENT_MSG_DISCONNECTED:
        {
            SYS_CONSOLE_PRINT("\nMqttCallback(): MQTT Disconnected\r\n");    
        }
        break;

        case SYS_MQTT_EVENT_MSG_CONNECTED:
        {
            SYS_CONSOLE_PRINT("\nMqttCallback(): MQTT Connected\r\n");
            g_lastPubTimeout = SYS_TMR_TickCountGet();
        }
        break;

        case SYS_MQTT_EVENT_MSG_SUBSCRIBED:
        {
            SYS_MQTT_SubscribeConfig	*psMqttSubCfg = (SYS_MQTT_SubscribeConfig	*)data;
            SYS_CONSOLE_PRINT("\nMqttCallback(): Subscribed to Topic '%s'\r\n", psMqttSubCfg->topicName);
        }
        break;

        case SYS_MQTT_EVENT_MSG_UNSUBSCRIBED:
        {
            SYS_MQTT_SubscribeConfig	*psMqttSubCfg = (SYS_MQTT_SubscribeConfig	*)data;
            SYS_CONSOLE_PRINT("\nMqttCallback(): UnSubscribed to Topic '%s'\r\n", psMqttSubCfg->topicName);                
        }
        break;

		case SYS_MQTT_EVENT_MSG_PUBLISHED:
		{
            SYS_CONSOLE_PRINT("\nMqttCallback(): Published Msg(%d) to Topic\r\n", PubMsgCnt);                
            g_lastPubTimeout = SYS_TMR_TickCountGet();
		}
		break;
        
        case SYS_MQTT_EVENT_MSG_CONNACK_TO:
		{
            SYS_CONSOLE_PRINT("\nMqttCallback(): Connack Timeout Happened\r\n");                
		}
		break;

        case SYS_MQTT_EVENT_MSG_SUBACK_TO: 
		{
            SYS_CONSOLE_PRINT("\nMqttCallback(): Suback Timeout Happened\r\n");                
		}
		break;

        case SYS_MQTT_EVENT_MSG_PUBACK_TO:
		{
            SYS_CONSOLE_PRINT("\nMqttCallback(): Puback Timeout Happened\r\n");                
            g_lastPubTimeout = SYS_TMR_TickCountGet();
		}
		break;

        case SYS_MQTT_EVENT_MSG_UNSUBACK_TO: 
		{
            SYS_CONSOLE_PRINT("\nMqttCallback(): UnSuback Timeout Happened\r\n");                
		}
		break;

    }
    return SYS_MQTT_SUCCESS;
}

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */
void APP_Initialize ( void )
{
    g_appData.state = APP_STATE_INIT_DONE;
    SYS_CONSOLE_MESSAGE("Application: Paho MQTT Client\r\n");
}

bool checkTimeOut(uint32_t timeOutValue, uint32_t lastTimeOut)
{
    if(lastTimeOut == 0)
        return 0;
    
    return (SYS_TMR_TickCountGet() - lastTimeOut > timeOutValue);
}

void Publish_PeriodicMsg(void)
{
    if (checkTimeOut(MQTT_PUB_TIMEOUT_CONST, g_lastPubTimeout) || (UserBtnState != SW1_Get()))
    {
        char        message[200] = {0};
        SYS_MQTT_PublishTopicCfg	sMqttTopicCfg;
        int32_t retVal = SYS_MQTT_FAILURE;
        UserBtnState = SW1_Get();

        //reset the timer
        g_lastPubTimeout = 0;
        
        /* All Params other than the message are initialized by the config provided in MHC*/
        sprintf(sMqttTopicCfg.topicName, "/sys/%s/%s/thing/event/property/post", EXAMPLE_PRODUCT_KEY, EXAMPLE_DEVICE_NAME);
        sMqttTopicCfg.topicLength = strlen(sMqttTopicCfg.topicName);
        sMqttTopicCfg.retain = SYS_MQTT_DEF_PUB_RETAIN;
        sMqttTopicCfg.qos = SYS_MQTT_DEF_PUB_QOS;

        int rawTemperature = 50;
        uint8_t ledYellowStatus = 0;
    	sprintf(message,
	                  "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{\"YellowLEDStatus\":%u,\"UserBtn\":%u,\"Temp\":\"%d."
	                  "%02d\"},\"method\":\"thing.event.property.post\"}",
	                  ledYellowStatus,
	                  !UserBtnState, /* Idle = VCC, Pressed = 0V */
	                  rawTemperature / 100,
	                  abs(rawTemperature) % 100);

        retVal = SYS_MQTT_Publish(g_sSysMqttHandle, 
                            &sMqttTopicCfg,
                            message,
                            sizeof(message));
        if(retVal != SYS_MQTT_SUCCESS)
        {
            SYS_CONSOLE_PRINT("\nPublish_PeriodicMsg(): Failed (%d)\r\n", retVal);                            
        }
        PubMsgCnt++;
    }
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */
void APP_Tasks ( void )
{
    switch (g_appData.state) 
	{
        case APP_STATE_INIT_DONE:
        {
            if(SYS_WIFI_CtrlMsg(sysObj.syswifi, SYS_WIFI_GETCONFIG, &wificonfig, sizeof(wificonfig)) == SYS_WIFI_SUCCESS)
            {
                if (SYS_WIFI_STA == wificonfig.mode)
                {
                    SYS_CONSOLE_PRINT("%s(): Device mode = STA \r\n", __func__);
                    g_appData.state = APP_STATE_MODE_STA;
                }
                else
                {
                    SYS_CONSOLE_PRINT("%s(): Device mode = AP \r\n", __func__);
                    g_appData.state = APP_STATE_MODE_AP;                
                }
            }                
            break;
        }

        case APP_STATE_MODE_AP:
        {
            break;
        }
        
        case APP_STATE_MODE_STA:
        {
            SYS_MQTT_Config   *psMqttCfg;

            memset(&g_sTmpSysMqttCfg, 0, sizeof(g_sTmpSysMqttCfg));
            psMqttCfg = &g_sTmpSysMqttCfg;
            psMqttCfg->sBrokerConfig.autoConnect = false;
            psMqttCfg->sBrokerConfig.tlsEnabled = false;
            psMqttCfg->sBrokerConfig.keepAliveInterval = 60; /* Need change to 60, otherwise the Ali Cloud will response MQTT connect ACK with error code 2: identifier rejected. */
            sprintf(psMqttCfg->sBrokerConfig.brokerName, "%s.iot-as-mqtt.cn-shanghai.aliyuncs.com", EXAMPLE_PRODUCT_KEY);
         
            aiotMqttSign(EXAMPLE_PRODUCT_KEY, EXAMPLE_DEVICE_NAME, EXAMPLE_DEVICE_SECRET,
                    psMqttCfg->sBrokerConfig.clientId,
                    psMqttCfg->sBrokerConfig.username,
                    psMqttCfg->sBrokerConfig.password);
            psMqttCfg->sBrokerConfig.serverPort = 1883;
            psMqttCfg->subscribeCount = 1;
            psMqttCfg->sSubscribeConfig[0].qos = 1;
            sprintf(psMqttCfg->sSubscribeConfig[0].topicName, "/sys/%s/%s/thing/service/property/set", EXAMPLE_PRODUCT_KEY, EXAMPLE_DEVICE_NAME);
            g_sSysMqttHandle = SYS_MQTT_Connect(&g_sTmpSysMqttCfg, MqttCallback, NULL);    
            g_appData.state = APP_STATE_SERVICE_TASK;
            break;
        }
        
        case APP_STATE_SERVICE_TASK:
        {
            Publish_PeriodicMsg();
            SYS_MQTT_Task(g_sSysMqttHandle);
            break;
        }
        default:
        {
            break;
        }
    }
    
    /* Check the application's current state. */
    SYS_CMD_READY_TO_READ();
}


/*******************************************************************************
 End of File
 */
