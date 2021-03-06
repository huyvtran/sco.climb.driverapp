/**************************************************************************************************
  Filename:       simpleBLEPeripheral.c
  Revised:        $Date: 2015-07-13 11:43:11 -0700 (Mon, 13 Jul 2015) $
  Revision:       $Revision: 44336 $

  Description:    This file contains the Simple BLE Peripheral sample application
                  for use with the CC2650 Bluetooth Low Energy Protocol Stack.

  Copyright 2013 - 2015 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <string.h>

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Queue.h>

#include "hci_tl.h"
#include "gatt.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "devinfoservice.h"
#include "CLIMBProfile.h"
#include "bsp_i2c.h"

#if defined(SENSORTAG_HW)
#include "bsp_spi.h"
#endif // SENSORTAG_HW

#include "peripheralObserver.h"
#include "gapbondmgr.h"

#include "osal_snv.h"
#include "ICallBleAPIMSG.h"

#include "util.h"
//#include "board_lcd.h"
//#include "board_key_ST.h"
#include "Board.h"
#include "sensor.h"
#include "CLIMBPerOb.h"

#include <ti/drivers/lcd/LCDDogm1286.h>
#include <xdc/runtime/System.h>

#if defined(SENSORTAG_HW)
#include "sensor_bmp280.h"
#include "sensor_hdc1000.h"
#include "sensor_mpu9250.h"
#include "sensor_opt3001.h"
#include "sensor_tmp007.h"
#ifdef FEATURE_LCD
#include "devpk_lcd.h"
#include <stdio.h>
#endif
#endif
/*********************************************************************
 * CONSTANTS
 */
// Advertising interval when device is discoverable (units of 625us, 160=100ms)
#define DEFAULT_CONNECTABLE_ADVERTISING_INTERVAL          400
#define DEFAULT_NON_CONNECTABLE_ADVERTISING_INTERVAL          800

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_GENERAL

// Minimum connection interval (units of 1.25ms, 80=100ms) if automatic
// parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL   	 202

// Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic
// parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     202

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         0

// Supervision timeout value (units of 10ms, 1000=10s) if automatic parameter
// update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          61

// Whether to enable automatic parameter update request when a connection is
// formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         10

// Scan duration in ms
#define DEFAULT_SCAN_DURATION                 900//1000 //Tempo di durata di una scansione, allo scadere la scansione viene ricominciata

#define EPOCH_PERIOD						  1010
// Scan interval value in 0.625ms ticks
#define SCAN_INTERVAL 						  480

// scan window value in 0.625ms ticks
#define SCAN_WINDOW							  480

// Whether to report all contacts or only the first for each device
#define FILTER_ADV_REPORTS					  FALSE

// Discovery mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE                DEVDISC_MODE_ALL //non � ancora chiaro cosa cambi, con le altre due opzioni non vede

// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN         FALSE

// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST          FALSE

// Maximum number of scan responses to be reported to application
#define DEFAULT_MAX_SCAN_RES                  50

// How often to perform periodic event (in msec)
#define PERIODIC_EVT_PERIOD              	  2000

#define NODE_TIMEOUT_OS_TICKS				  500000

#define LED_TIMEOUT						  	  1

//#define NODE_ID								  0x01 //per ora non � applicabile ai nodi master

#define NODE_ID_LENGTH						  1

#define ADV_PKT_ID_OFFSET					  12
#define ADV_PKT_STATE_OFFSET				  ADV_PKT_ID_OFFSET + NODE_ID_LENGTH


#ifdef FEATURE_OAD
// The size of an OAD packet.
#define OAD_PACKET_SIZE                       ((OAD_BLOCK_SIZE) + 2)
#endif // FEATURE_OAD

// Task configuration
#define SBP_TASK_PRIORITY                     1


#ifndef SBP_TASK_STACK_SIZE
#define SBP_TASK_STACK_SIZE                   644
#endif

// Internal Events for RTOS application
#define P_STATE_CHANGE_EVT                  0x0001
#define P_CHAR_CHANGE_EVT                   0x0002
#define PERIODIC_EVT                        0x0004
#define CONN_EVT_END_EVT                    0x0008
#define O_STATE_CHANGE_EVT                  0x0010
#define ADVERTISE_EVT					    0x0020
#define KEY_CHANGE_EVT					  	0x0040
#define LED_TIMEOUT_EVT						0x0080

#define EPOCH_EVT							0x0400
/*********************************************************************
 * TYPEDEFS
 */

typedef enum ChildClimbNodeStateType_t {
	BY_MYSELF,
	CHECKING,
	ON_BOARD,
	ALERT,
	ERROR,
	INVALID_STATE
} ChildClimbNodeStateType_t;

typedef enum MasterClimbNodeStateType_t {
	NOT_CONNECTED,
	CONNECTED,
	INVALID_MASTER_STATE
} MasterClimbNodeStateType_t;

typedef enum ClimbNodeType_t {
	CLIMB_CHILD_NODE,
	CLIMB_MASTER_NODE,
	NOT_CLIMB_NODE,
	NAME_NOT_PRESENT,
	WRONG_PARKET_TYPE
} ClimbNodeType_t;

// App event passed from profiles.
typedef struct {
  appEvtHdr_t hdr;  // event header.
  uint8_t *pData;  // event data
} sbpEvt_t;

typedef struct {
	gapDevRec_t devRec;
	uint8 advDataLen;
	uint8 advData[31];
	//uint8 scnDataLen;
	//uint8 scnData[31];
	uint32 lastContactTicks;
	uint8 rssi;
	uint8 contactsCounter;
	ChildClimbNodeStateType_t stateToImpose;
} myGapDevRec_t;

typedef struct listNode{
    myGapDevRec_t device;
    struct listNode *next;
}listNode_t;

listNode_t* childListRootPtr = NULL;
listNode_t* masterListRootPtr = NULL;

/*********************************************************************
 * LOCAL VARIABLES
 */


// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID selfEntity;

// Semaphore globally used to post events to the application thread
static ICall_Semaphore sem;

// Clock instances for internal periodic events.
static Clock_Struct periodicClock;
static Clock_Struct ledTimeoutClock;
static Clock_Struct epochClock;

// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// events flag for internal application events.
static uint16_t events;

// Task configuration
Task_Struct sbpTask;
Char sbpTaskStack[SBP_TASK_STACK_SIZE];

MasterClimbNodeStateType_t thisNodeState = NOT_CONNECTED;

static uint8 Climb_childNodeName[] = {'C','L','I','M','B','C'}; //CLIMBB are child node used for BATTERY LIFE tests


static uint8 advUpdateReq = FALSE;

static uint8 beaconActive = 0;

static uint8 myAddr[B_ADDR_LEN];
static uint8 defAdvertData[] = {
		0x07,// length of this data
		GAP_ADTYPE_LOCAL_NAME_COMPLETE,
		'C',
		'L',
		'I',
		'M',
		'B',
		'M',
		0x02,   // length of this data
		GAP_ADTYPE_FLAGS,
		DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

};

// GAP - SCAN RSP data (max size = 31 bytes)
static uint8 defScanRspData[] = {

		0x03,
		GAP_ADTYPE_APPEARANCE,
		(uint8)(GAP_APPEARE_GENERIC_TAG & 0xFF),
		(uint8)((GAP_APPEARE_GENERIC_TAG>>8) & 0xFF)

};

// GAP GATT Attributes
static uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = "CLIMB Node";

// Globals used for ATT Response retransmission
static gattMsgEvent_t *pAttRsp = NULL;
static uint8_t rspTxRetry = 0;

static uint32 lastGATTCheckTicks = 0;

#if defined(SENSORTAG_HW)
// Pins that are actively used by the application
static PIN_Config SensortagAppPinTable[] =
{
    Board_LED1| PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* LED initially off             */
    Board_LED2       | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* LED initially off             */
    Board_KEY_LEFT   | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,        /* Button is active low          */
    Board_KEY_RIGHT  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,        /* Button is active low          */
    Board_RELAY      | PIN_INPUT_EN | PIN_PULLDOWN | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,      /* Relay is active high          */
    Board_BUZZER     | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* Buzzer initially off          */

    PIN_TERMINATE
};
#endif

#if defined(CC2650EM_7ID)
// Pins that are actively used by the application
static PIN_Config SensortagAppPinTable[] =
{
    Board_LED1		 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* LED initially off             */
    Board_LED2       | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* LED initially off             */
    Board_KEY_LEFT   | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,        /* Button is active low          */
    Board_KEY_RIGHT  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,        /* Button is active low          */

    PIN_TERMINATE
};
#endif
// Global pin resources
PIN_State pinGpioState;
PIN_Handle hGpioPin;

#ifdef CLIMB_DEBUG
static uint8 adv_counter = 0;
#endif
/*********************************************************************
 * LOCAL FUNCTIONS
 */

////TASK FUNCTIONS
static void SimpleBLEPeripheral_init( void );
static void SimpleBLEPeripheral_taskFxn(UArg a0, UArg a1);

////GENERIC BLE STACK SUPPORT FUNCTIONS - EVENTS,MESSAGES ecc
static uint8_t BLE_processStackMsg(ICall_Hdr *pMsg);
static uint8_t SimpleBLEPeripheral_processGATTMsg(gattMsgEvent_t *pMsg);
static void SimpleBLEPeripheral_processAppMsg(sbpEvt_t *pMsg);
static void BLEPeripheral_processStateChangeEvt(gaprole_States_t newState);
static void SimpleBLEPeripheral_freeAttRsp(uint8_t status);
static void BLE_ConnectionEventHandler(void);
static void BLE_AdvertiseEventHandler(void);

////BLE STACK CBs
static uint8_t BLEObserver_eventCB(gapObserverRoleEvent_t *pEvent);
static void BLEPeripheral_stateChangeCB(gaprole_States_t newState);
static void BLEPeripheral_charValueChangeCB(uint8_t paramID);

////CLIMB IN/OUT FUNCTIONS
static void Climb_contactsCheckSendThroughGATT(void);
static void Climb_advertisedStatesUpdate(void);
static void Climb_processCharValueChangeEvt(uint8_t paramID);
static void Climb_processRoleEvent(gapObserverRoleEvent_t *pEvent);

////CLIMB MANAGEMENT
static ClimbNodeType_t isClimbNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent_a);
static void Climb_addNodeDeviceInfo( gapDeviceInfoEvent_t *gapDeviceInfoEvent , ClimbNodeType_t nodeType);
static listNode_t* Climb_findNodeByDevice(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType);
static listNode_t* Climb_findChildNodeById(uint8 *nodeID);
static listNode_t* Climb_addNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent,  ClimbNodeType_t nodeType);
static void Climb_updateNodeMetadata(gapDeviceInfoEvent_t *gapDeviceInfoEvent,	listNode_t* targetNode);
static void Climb_advertisedStatesCheck(void);
static void Climb_nodeTimeoutCheck();
static void Climb_removeNode(listNode_t* targetNode,listNode_t* previousNode);
static void Climb_periodicTask();
#ifdef PRINTF_ENABLED
static void Climb_printfNodeInfo(gapDeviceInfoEvent_t *gapDeviceInfoEvent );
#endif
static void Climb_epochStartHandler();

////HARDWARE RELATED FUNCTIONS
static void CLIMB_FlashLed(PIN_Id pinId);
static void CLIMB_handleKeys(uint8 shift, uint8 keys);
static void Key_callback(PIN_Handle handle, PIN_Id pinId);
#ifdef FEATURE_LCD
static void displayInit(void);
#endif

////GENERIC SUPPORT FUNCTIONS
static uint8_t SimpleBLEPeripheral_enqueueMsg(uint8_t event, uint8_t state, uint8_t *pData);
static void Climb_clockHandler(UArg arg);
static uint8 memcomp(uint8 * str1, uint8 * str2, uint8 len);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t SimpleBLEPeripheral_gapRoleCBs =
{
  BLEPeripheral_stateChangeCB,     // Profile State Change Callbacks
  BLEObserver_eventCB	// GAP Event callback
};

// GAP Bond Manager Callbacks
static gapBondCBs_t simpleBLEPeripheral_BondMgrCBs =
{
  NULL, // Passcode callback (not used by application)
  NULL  // Pairing / Bonding state Callback (not used by application)
};

// Simple GATT Profile Callbacks
static climbProfileCBs_t SimpleBLEPeripheral_climbProfileCBs =
{
  BLEPeripheral_charValueChangeCB, // Characteristic value change callback
};


/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleBLEPeripheral_createTask
 *
 * @brief   Task creation function for the Simple BLE Peripheral.
 *
 * @param   None.
 *
 * @return  None.
 */
void SimpleBLEPeripheral_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = sbpTaskStack;
  taskParams.stackSize = SBP_TASK_STACK_SIZE;
  taskParams.priority = SBP_TASK_PRIORITY;

  Task_construct(&sbpTask, SimpleBLEPeripheral_taskFxn, &taskParams, NULL);
}

/*********************************************************************
 * @fn      SimpleBLEPeripheral_init
 *
 * @brief   Called during initialization and contains application
 *          specific initialization (ie. hardware initialization/setup,
 *          table initialization, power up notification, etc), and
 *          profile initialization/setup.
 *
 * @param   None.
 *
 * @return  None.
 */
static void SimpleBLEPeripheral_init(void)
{
#if defined(SENSORTAG_HW)
  // Setup I2C for sensors
  bspI2cInit();
#endif

  // Handling of buttons, LED, relay
  hGpioPin = PIN_open(&pinGpioState, SensortagAppPinTable);
  PIN_registerIntCb(hGpioPin, Key_callback);

  // ******************************************************************
  // N0 STACK API CALLS CAN OCCUR BEFORE THIS CALL TO ICall_registerApp
  // ******************************************************************
  // Register the current thread as an ICall dispatcher application
  // so that the application can send and receive messages.
  ICall_registerApp(&selfEntity, &sem);

  // Hard code the BD Address till CC2650 board gets its own IEEE address
  //uint8 bdAddress[B_ADDR_LEN] = { 0xAD, 0xD0, 0x0A, 0xAD, 0xD0, 0x0A };
  //HCI_EXT_SetBDADDRCmd(bdAddress);

  // Set device's Sleep Clock Accuracy
  //HCI_EXT_SetSCACmd(40);

  // Create an RTOS queue for message from profile to be sent to app.
  appMsgQueue = Util_constructQueue(&appMsg);

  // Create one-shot clocks for internal periodic events.
  Util_constructClock(&periodicClock, Climb_clockHandler,
                      PERIODIC_EVT_PERIOD, 0, false, PERIODIC_EVT);

  Util_constructClock(&ledTimeoutClock, Climb_clockHandler,
		  	  	  	  LED_TIMEOUT, 0, false, LED_TIMEOUT_EVT);

  Util_constructClock(&epochClock, Climb_clockHandler,
	  	  	   	   	  EPOCH_PERIOD, 0, false, EPOCH_EVT);
#ifndef SENSORTAG_HW
//  Board_openLCD();
#endif //SENSORTAG_HW
  
#if SENSORTAG_HW
  // Setup SPI bus for serial flash and Devpack interface
  bspSpiOpen();
#endif //SENSORTAG_HW
  
  // Setup the GAP
  GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL);

  // Setup the GAP Peripheral Role Profile
  {
    // For all hardware platforms, device starts advertising upon initialization
    uint8_t initialAdvertEnable = FALSE;

    // By setting this to zero, the device will go into the waiting state after
    // being discoverable for 30.72 second, and will not being advertising again
    // until the enabler is set back to TRUE
    uint16_t advertOffTime = 0;

    uint8_t enableUpdateRequest = DEFAULT_ENABLE_UPDATE_REQUEST;
    uint16_t desiredMinInterval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
    uint16_t desiredMaxInterval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
    uint16_t desiredSlaveLatency = DEFAULT_DESIRED_SLAVE_LATENCY;
    uint16_t desiredConnTimeout = DEFAULT_DESIRED_CONN_TIMEOUT;

    // Set the GAP Role Parameters
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&initialAdvertEnable);
    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16_t),&advertOffTime);

    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(defScanRspData),defScanRspData);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);

    GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8_t),&enableUpdateRequest);
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t),&desiredMinInterval);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t),&desiredMaxInterval);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16_t),&desiredSlaveLatency);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16_t),&desiredConnTimeout);
  }

  // Set the GAP Characteristics
  GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

  // Set advertising interval
  {
    uint16_t advInt = DEFAULT_NON_CONNECTABLE_ADVERTISING_INTERVAL;

    GAP_SetParamValue(TGAP_CONN_ADV_INT_MIN ,    advInt);
    GAP_SetParamValue(TGAP_CONN_ADV_INT_MAX ,    advInt);

    advInt = DEFAULT_CONNECTABLE_ADVERTISING_INTERVAL;

    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, advInt);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, advInt);

    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, advInt);
    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, advInt);

  }

  // Setup the GAP Bond Manager
  {
    uint32_t passkey = 0; // passkey "000000"
    uint8_t pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;//GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
    uint8_t mitm = FALSE;
    uint8_t ioCap = GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
    uint8_t bonding = TRUE;

    GAPBondMgr_SetParameter(GAPBOND_DEFAULT_PASSCODE, sizeof(uint32_t),&passkey);
    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(uint8_t), &pairMode);
    GAPBondMgr_SetParameter(GAPBOND_MITM_PROTECTION, sizeof(uint8_t), &mitm);
    GAPBondMgr_SetParameter(GAPBOND_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
    GAPBondMgr_SetParameter(GAPBOND_BONDING_ENABLED, sizeof(uint8_t), &bonding);
  }

  //sensorTestExecute(ST_TEST_MAP);
#if defined(SENSORTAG_HW)
  sensorBmp280Init();
  sensorBmp280Enable(FALSE);
  sensorHdc1000Init();
  //sensorMpu9250Init(); //ho gia scollegato l'alimentazione all'interno del file Board.c
  //sensorMpuSleep();
  sensorOpt3001Init();
  sensorTmp007Init();
#ifdef FEATURE_LCD
  displayInit();
#endif
#endif
   // Initialize GATT attributes
  GGS_AddService(GATT_ALL_SERVICES);           // GAP
  GATTServApp_AddService(GATT_ALL_SERVICES);   // GATT attributes
  //DevInfo_AddService();                        // Device Information Service

  ClimbProfile_AddService(GATT_ALL_SERVICES); // Simple GATT Profile

  // Setup Observer Profile

	uint8 scanRes = DEFAULT_MAX_SCAN_RES;
	GAPRole_SetParameter(GAPOBSERVERROLE_MAX_SCAN_RES, sizeof(uint8_t),	&scanRes);

	GAP_SetParamValue(TGAP_GEN_DISC_SCAN, DEFAULT_SCAN_DURATION);
	GAP_SetParamValue(TGAP_LIM_DISC_SCAN, DEFAULT_SCAN_DURATION);

	GAP_SetParamValue(TGAP_GEN_DISC_SCAN_INT, SCAN_INTERVAL);
	GAP_SetParamValue(TGAP_LIM_DISC_SCAN_INT, SCAN_INTERVAL);
	GAP_SetParamValue(TGAP_CONN_SCAN_INT, SCAN_INTERVAL);
	GAP_SetParamValue(TGAP_GEN_DISC_SCAN_WIND, SCAN_WINDOW);
	GAP_SetParamValue(TGAP_LIM_DISC_SCAN_WIND, SCAN_WINDOW);
	GAP_SetParamValue(TGAP_CONN_SCAN_WIND, SCAN_WINDOW);

	GAP_SetParamValue(TGAP_FILTER_ADV_REPORTS, FILTER_ADV_REPORTS);

  // Register callback with SimpleGATTprofile
  ClimbProfile_RegisterAppCBs(&SimpleBLEPeripheral_climbProfileCBs);

  // Start the Device
  bStatus_t status = GAPRole_StartDevice(&SimpleBLEPeripheral_gapRoleCBs);
#ifdef PRINTF_ENABLED
	//System_printf("\nBLE Peripheral+Broadcaster started (advertise started). Status: %d\n\n", status);
#endif
  // Start Bond Manager
  VOID GAPBondMgr_Register(&simpleBLEPeripheral_BondMgrCBs);

  // Register with GAP for HCI/Host messages
  GAP_RegisterForMsgs(selfEntity);
  
  // Register for GATT local events and ATT Responses pending for transmission
  GATT_RegisterForMsgs(selfEntity);


  //SETTING POWER
  HCI_EXT_SetTxPowerCmd(HCI_EXT_TX_POWER_0_DBM);

}

/*********************************************************************
 * @fn      SimpleBLEPeripheral_taskFxn
 *
 * @brief   Application task entry point for the Simple BLE Peripheral.
 *
 * @param   a0, a1 - not used.
 *
 * @return  None.
 */
static void SimpleBLEPeripheral_taskFxn(UArg a0, UArg a1) {
	// Initialize application
	SimpleBLEPeripheral_init();

	// Application main loop
	for (;;) {
		// Waits for a signal to the semaphore associated with the calling thread.
		// Note that the semaphore associated with a thread is signaled when a
		// message is queued to the message receive queue of the thread or when
		// ICall_signal() function is called onto the semaphore.
		ICall_Errno errno = ICall_wait(ICALL_TIMEOUT_FOREVER);

		if (errno == ICALL_ERRNO_SUCCESS) {
			ICall_EntityID dest;
			ICall_ServiceEnum src;
			ICall_HciExtEvt *pMsg = NULL;

			if (ICall_fetchServiceMsg(&src, &dest,
					(void **) &pMsg) == ICALL_ERRNO_SUCCESS) {
				uint8 safeToDealloc = TRUE;

				if ((src == ICALL_SERVICE_CLASS_BLE) && (dest == selfEntity)) {
					ICall_Event *pEvt = (ICall_Event *) pMsg;

					// Check for BLE stack events first
					if (pEvt->signature == 0xffff) {
						if (pEvt->event_flag & CONN_EVT_END_EVT) {
							// Try to retransmit pending ATT Response (if any)
							BLE_ConnectionEventHandler();
						}
						if (pEvt->event_flag & ADVERTISE_EVT) {
							BLE_AdvertiseEventHandler();
						}
					} else {
						// Process inter-task message
						safeToDealloc = BLE_processStackMsg((ICall_Hdr *) pMsg);
					}
				}

				if (pMsg) {
					if (safeToDealloc) {
						ICall_freeMsg(pMsg);
					}
				}
			}

			// If RTOS queue is not empty, process app message.
			while (!Queue_empty(appMsgQueue)) {
				sbpEvt_t *pMsg = (sbpEvt_t *) Util_dequeueMsg(appMsgQueue);
				if (pMsg) {
					// Process message.
					SimpleBLEPeripheral_processAppMsg(pMsg);

					// Free the space from the message.
					ICall_free(pMsg);
				}
			}
		}

		if (events & PERIODIC_EVT) {
			events &= ~PERIODIC_EVT;

			// Perform periodic application task
			if (thisNodeState == CONNECTED) {
				Util_startClock(&periodicClock);
				Climb_periodicTask();
			}
		}

		if (events & LED_TIMEOUT_EVT) {

			events &= ~LED_TIMEOUT_EVT;
			//only turn off leds
			PIN_setOutputValue(hGpioPin, Board_LED1, Board_LED_OFF);
			PIN_setOutputValue(hGpioPin, Board_LED2, Board_LED_OFF);
		}

		if (events & EPOCH_EVT) {
			events &= ~EPOCH_EVT;

			if (beaconActive == 1) {

				Climb_epochStartHandler();
#ifdef ENABLE_ADV
				float randDelay = 10*((float)Util_GetTRNG())/4294967296;
				Util_restartClock(&epochClock, EPOCH_PERIOD + randDelay);
#else
				Util_restartClock(&epochClock, EPOCH_PERIOD);
#endif
			}

		}

	}
}









/*********************************************************************
 * @fn      BLE_processStackMsg
 *
 * @brief   Process an incoming stack message.
 *
 * @param   pMsg - message to process
 *
 * @return  TRUE if safe to deallocate incoming message, FALSE otherwise.
 */
static uint8_t BLE_processStackMsg(ICall_Hdr *pMsg)
{
  uint8_t safeToDealloc = TRUE;
    
  switch (pMsg->event)
  {
    case GATT_MSG_EVENT:
      // Process GATT message
      safeToDealloc = SimpleBLEPeripheral_processGATTMsg((gattMsgEvent_t *)pMsg);
      break;

    case HCI_GAP_EVENT_EVENT:
      {
        // Process HCI message
        switch(pMsg->status)
        {
          case HCI_COMMAND_COMPLETE_EVENT_CODE:
            // Process HCI Command Complete Event
            break;
            
          default:
            break;
        }
      }
      break;

    case GAP_MSG_EVENT:
      Climb_processRoleEvent((gapObserverRoleEvent_t *) pMsg);
      break;

    default:
      // do nothing
      break;
  }
  
  return (safeToDealloc);
}

/*********************************************************************
 * @fn      SimpleBLEPeripheral_processGATTMsg
 *
 * @brief   Process GATT messages and events.
 *
 * @return  TRUE if safe to deallocate incoming message, FALSE otherwise.
 */
static uint8_t SimpleBLEPeripheral_processGATTMsg(gattMsgEvent_t *pMsg)
{
  // See if GATT server was unable to transmit an ATT response
  if (pMsg->hdr.status == blePending)
  {
    // No HCI buffer was available. Let's try to retransmit the response
    // on the next connection event.
    if (HCI_EXT_ConnEventNoticeCmd(pMsg->connHandle, selfEntity,
                                   CONN_EVT_END_EVT) == SUCCESS)
    {
      // First free any pending response
      SimpleBLEPeripheral_freeAttRsp(FAILURE);
      
      // Hold on to the response message for retransmission
      pAttRsp = pMsg;
      
      // Don't free the response message yet
      return (FALSE);
    }
  }
  else if (pMsg->method == ATT_FLOW_CTRL_VIOLATED_EVENT)
  {
    // ATT request-response or indication-confirmation flow control is
    // violated. All subsequent ATT requests or indications will be dropped.
    // The app is informed in case it wants to drop the connection.
    
    // Display the opcode of the message that caused the violation.
  }    
  else if (pMsg->method == ATT_MTU_UPDATED_EVENT)
  {
    // MTU size updated
  }
  
  // Free message payload. Needed only for ATT Protocol messages
  GATT_bm_free(&pMsg->msg, pMsg->method);
  
  // It's safe to free the incoming message
  return (TRUE);
}

/*********************************************************************
 * @fn      SimpleBLEPeripheral_processAppMsg
 *
 * @brief   Process an incoming callback from a profile.
 *
 * @param   pMsg - message to process
 *
 * @return  None.
 */
static void SimpleBLEPeripheral_processAppMsg(sbpEvt_t *pMsg)
{
  switch (pMsg->hdr.event)
  {
    case P_STATE_CHANGE_EVT:
      BLEPeripheral_processStateChangeEvt((gaprole_States_t)pMsg->hdr.state);
      break;

    case P_CHAR_CHANGE_EVT:
      Climb_processCharValueChangeEvt(pMsg->hdr.state);
      break;

	case O_STATE_CHANGE_EVT:
	  BLE_processStackMsg((ICall_Hdr *)pMsg->pData);
	  // Free the stack message
	  ICall_freeMsg(pMsg->pData);
	  break;

	case KEY_CHANGE_EVT:
	  CLIMB_handleKeys(0, pMsg->hdr.state);
	  break;

	default:
      // Do nothing.
      break;
  }
}

/*********************************************************************
 * @fn      BLEPeripheral_processStateChangeEvt
 *
 * @brief   Process a pending GAP Role state change event.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
static void BLEPeripheral_processStateChangeEvt(gaprole_States_t newState)
{

  static bool firstConnFlag = false;

#ifdef PRINTF_ENABLED
		//System_printf("\nGAP role state change: NewState = 0x%x\n",newState);
#endif
  switch ( newState )
  {
    case GAPROLE_STARTED:
      {
        uint8_t ownAddress[B_ADDR_LEN];
        uint8_t systemId[DEVINFO_SYSTEM_ID_LEN];
        thisNodeState = NOT_CONNECTED;

        GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddress);

        // use 6 bytes of device address for 8 bytes of system ID value
        systemId[0] = ownAddress[0];
        systemId[1] = ownAddress[1];
        systemId[2] = ownAddress[2];

        // set middle bytes to zero
        systemId[4] = 0x00;
        systemId[3] = 0x00;

        // shift three bytes up
        systemId[7] = ownAddress[5];
        systemId[6] = ownAddress[4];
        systemId[5] = ownAddress[3];

        DevInfo_SetParameter(DEVINFO_SYSTEM_ID, DEVINFO_SYSTEM_ID_LEN, systemId);

      }
      break;

    case GAPROLE_ADVERTISING:
      break;


    /* After a connection is dropped a device in PLUS_BROADCASTER will continue
     * sending non-connectable advertisements and shall sending this change of 
     * state to the application.  These are then disabled here so that sending 
     * connectable advertisements can resume.
     */
    case GAPROLE_ADVERTISING_NONCONN:
      {
        uint8_t advertEnabled = FALSE;
      
        // Disable non-connectable advertising.
        GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t),
                           &advertEnabled);
      
        GAPObserverRole_CancelDiscovery();

        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);

        advertEnabled = TRUE;

        // Enabled connectable advertising.
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),
                             &advertEnabled);
        
        // Reset flag for next connection.
        firstConnFlag = false;
        
        SimpleBLEPeripheral_freeAttRsp(bleNotConnected);
      }
      break;


    case GAPROLE_CONNECTED:
      {
//pMsg->connHandle //per ora fisso connection handle a 0
    	//GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE,DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);

    	HCI_EXT_ConnEventNoticeCmd(0, selfEntity,CONN_EVT_END_EVT);
    	thisNodeState = CONNECTED;

    	Util_startClock(&periodicClock);

        uint8_t peerAddress[B_ADDR_LEN];

        GAPRole_GetParameter(GAPROLE_CONN_BD_ADDR, peerAddress);



        if (firstConnFlag == false)
        {
          uint8_t advertEnabled = FALSE; // Turn on Advertising

          // Disable connectable advertising.
          GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),
                               &advertEnabled);

          // Set to true for non-connectabel advertising.
          advertEnabled = TRUE;

          // Enable non-connectable advertising.
          GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t),
                               &advertEnabled);

          uint8 status = HCI_EXT_AdvEventNoticeCmd(selfEntity, ADVERTISE_EVT);

#ifdef PRINTF_ENABLED
		  //System_printf("\nAdvertise (non connectable) events notification enabled, status : %d\n", status);
#endif
          firstConnFlag = true;
        }

      }
      break;

    case GAPROLE_CONNECTED_ADV:
      break;

    case GAPROLE_WAITING:
      thisNodeState = NOT_CONNECTED;
      GAPObserverRole_CancelDiscovery();
      GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);

      SimpleBLEPeripheral_freeAttRsp(bleNotConnected);

      // Disable connection event end notice
      HCI_EXT_ConnEventNoticeCmd(pAttRsp->connHandle, selfEntity, 0);
      break;

    case GAPROLE_WAITING_AFTER_TIMEOUT: //TIMEOUT

	  thisNodeState = NOT_CONNECTED;
	  GAPObserverRole_CancelDiscovery();
	  GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);
      SimpleBLEPeripheral_freeAttRsp(bleNotConnected);
      
      firstConnFlag = false;

      break;

    case GAPROLE_ERROR:
      break;

    default:
      break;
  }
}

/*********************************************************************
 * @fn      SimpleBLEPeripheral_freeAttRsp
 *
 * @brief   Free ATT response message.
 *
 * @param   status - response transmit status
 *
 * @return  none
 */
static void SimpleBLEPeripheral_freeAttRsp(uint8_t status)
{
  // See if there's a pending ATT response message
  if (pAttRsp != NULL)
  {
    // See if the response was sent out successfully
    if (status == SUCCESS)
    {
    }
    else
    {
      // Free response payload
      GATT_bm_free(&pAttRsp->msg, pAttRsp->method);

    }

    // Free response message
    ICall_freeMsg(pAttRsp);

    // Reset our globals
    pAttRsp = NULL;
    rspTxRetry = 0;
  }
}

/*********************************************************************
 * @fn      BLE_ConnectionEventHandler
 *
 * @brief   Called after every connection event
 *
 * @param   none
 *
 * @return  none
 */
static void BLE_ConnectionEventHandler(void)
{

  Climb_contactsCheckSendThroughGATT();


  // See if there's a pending ATT Response to be transmitted
  if (pAttRsp != NULL)
  {
    uint8_t status;

    // Increment retransmission count
    rspTxRetry++;

    // Try to retransmit ATT response till either we're successful or
    // the ATT Client times out (after 30s) and drops the connection.
    status = GATT_SendRsp(pAttRsp->connHandle, pAttRsp->method, &(pAttRsp->msg));
    if ((status != blePending) && (status != MSG_BUFFER_NOT_AVAIL))
    {
      // Disable connection event end notice
      //HCI_EXT_ConnEventNoticeCmd(pAttRsp->connHandle, selfEntity, 0);

      // We're done with the response message
      SimpleBLEPeripheral_freeAttRsp(status);
    }
    else
    {
      // Continue retrying
    }
  }
}

/*********************************************************************
 * @fn      BLE_AdvertiseEventHandler
 *
 * @brief   Called after every advertise event
 *
 * @param
 *
 * @return  none
 */
static void BLE_AdvertiseEventHandler(void) {

#ifdef CLIMB_DEBUG  //force adv data update to include the new value of adv_counter
	//if(thisNodeState == CONNECTED){
		advUpdateReq = true;
		adv_counter++;
	//}
#endif
#ifdef ENABLE_ADV
	if(advUpdateReq){
		Climb_advertisedStatesUpdate();
		advUpdateReq = false;
	}


	uint8 adv_active = 0;
	uint8 status = GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t),&adv_active);
	status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&adv_active);
#endif

	CLIMB_FlashLed(Board_LED2);

#ifdef PRINTF_ENABLED
		//System_printf("\nAdvertise evt!");
#endif

}











/*********************************************************************
 * @fn      BLEObserver_eventCB
 *
 * @brief   Observer event callback function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  TRUE if safe to deallocate event message, FALSE otherwise.
 */
static uint8_t BLEObserver_eventCB(gapObserverRoleEvent_t *pEvent) {
	// Forward the role event to the application
	if ( SimpleBLEPeripheral_enqueueMsg( O_STATE_CHANGE_EVT , SUCCESS , (uint8_t *) pEvent) ) {
		// App will process and free the event
		return FALSE;
	}

	// Caller should free the event
	return TRUE;
}

/*********************************************************************
 * @fn      BLEPeripheral_stateChangeCB
 *
 * @brief   Callback from GAP Role indicating a role state change.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
static void BLEPeripheral_stateChangeCB(gaprole_States_t newState)
{
  SimpleBLEPeripheral_enqueueMsg(P_STATE_CHANGE_EVT, newState, NULL);
}

/*********************************************************************
 * @fn      BLEPeripheral_charValueChangeCB
 *
 * @brief   Callback from CLIMB Profile indicating a characteristic
 *          value change.
 *
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  None.
 */
static void BLEPeripheral_charValueChangeCB(uint8_t paramID)
{
  SimpleBLEPeripheral_enqueueMsg(P_CHAR_CHANGE_EVT, paramID, NULL);
}










/*********************************************************************
 * @fn      Climb_contactsCheckSendThroughGATT
 *
 * @brief	Checks all nodes in child list and send, through gatt, contacts recorded since last call of this function
 *
 * @param   None.
 *
 * @return  None.
 */
static void Climb_contactsCheckSendThroughGATT(void)
{
	//INVIA I CONTATTI RADIO AVVENUTI NELL'ULTIMO PERIODO TRAMITE GATT
	uint8 childrenNodesData[20];
	uint8 i = 0;
	listNode_t* node = childListRootPtr;
	uint8 gattUpdateReq = FALSE;

	while (node != NULL && i < 17) {
		if (node->device.lastContactTicks >= lastGATTCheckTicks) { //invia solo i nodi visti dopo l'ultimo check
			memcpy(&childrenNodesData[i], &node->device.advData[ADV_PKT_ID_OFFSET], NODE_ID_LENGTH); //salva l'indirizzo del nodo
			i += NODE_ID_LENGTH;
			childrenNodesData[i++] = node->device.advData[ADV_PKT_STATE_OFFSET];
#ifdef INCLUDE_RSSI_IN_GATT_DATA
			childrenNodesData[i++] = node->device.contactsCounter;
#endif
			gattUpdateReq = TRUE;
		}

		node = node->next; //passa al nodo sucessivo
	}

	//salva il valore di ticks
	lastGATTCheckTicks = Clock_getTicks();

	if (gattUpdateReq) {
		//azzero i byte successivi
		while (i < 20) {
			childrenNodesData[i++] = 0;
		}

		ClimbProfile_SetParameter(CLIMBPROFILE_CHAR1, 20, childrenNodesData);
		gattUpdateReq = FALSE;
	}

}
/*********************************************************************
 * @fn      Climb_advertisedStatesUpdate
 *
 * @brief	Changes advertised data accordingly with states stored in stateToImpose
 *
 * @param   None.
 *
 * @return  None.
 */
static void Climb_advertisedStatesUpdate(void) {

	uint8 i = 12;
	uint8 newChildrenStatesData[31];

	newChildrenStatesData[0]	= 	0x07,// length of this data
 	newChildrenStatesData[1]	=	GAP_ADTYPE_LOCAL_NAME_COMPLETE,
	newChildrenStatesData[2]	=	'C';
	newChildrenStatesData[3]	=	'L';
	newChildrenStatesData[4]	=	'I';
	newChildrenStatesData[5]	=	'M';
	newChildrenStatesData[6]	=	'B';
	newChildrenStatesData[7]	=	'M';

	newChildrenStatesData[9] = GAP_ADTYPE_MANUFACTURER_SPECIFIC; // manufacturer specific adv data type
	newChildrenStatesData[10] = 0x0D; // Company ID - Fixed //VERIFICARE SE QUESTA REGOLA VALE ANCHE PER I TAG NON COMPATIBILI CON IBEACON
	newChildrenStatesData[11] = 0x00; // Company ID - Fixed

	listNode_t* node = childListRootPtr;
	while (node != NULL && i < 28) {
		//TODO: ogni tanto broadcasta l'ID 00, cercare di capire perch� succede e come sistemare, per ora sistemo da qua
		if(node->device.advData[ADV_PKT_ID_OFFSET] != 0){
			memcpy(&newChildrenStatesData[i], & node->device.advData[ADV_PKT_ID_OFFSET], NODE_ID_LENGTH);
			i += NODE_ID_LENGTH;
			newChildrenStatesData[i++] = node->device.stateToImpose;
		}
#ifdef PRINTF_ENABLED
		//System_printf("\nRequesting state: 0x%02x, to addr: 0x%02x!",newChildrenStatesData[i-1],newChildrenStatesData[i-2]);
#endif
		node = node->next; //passa al nodo sucessivo
	}

#ifdef CLIMB_DEBUG
	while(i < 30){
		newChildrenStatesData[i++] = 0;
	}
	newChildrenStatesData[i++] = adv_counter; //the counter is always in the last position
#endif

	newChildrenStatesData[8] = i-9;
	GAP_UpdateAdvertisingData(selfEntity, true, i,	&newChildrenStatesData[0]);   //adv data
	//GAP_UpdateAdvertisingData(selfEntity, false, i,	&newChildrenStatesData[0]); //scn data

}

/*********************************************************************
 * @fn      Climb_processCharValueChangeEvt
 *
 * @brief   Process a pending Simple Profile characteristic value change
 *          event.
 *
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  None.
 */
static void Climb_processCharValueChangeEvt(uint8_t paramID) {

	uint8_t newValue[20];

	switch (paramID) {
	case CLIMBPROFILE_CHAR1:
	{
		ClimbProfile_GetParameter(CLIMBPROFILE_CHAR1, newValue);

		break;
	}
	case CLIMBPROFILE_CHAR2:
	{
		ClimbProfile_GetParameter(CLIMBPROFILE_CHAR2, newValue);

		uint8 i = 0;
		while (i < 16) { //TODO: verificare il 16
			uint8 nodeID[] = { newValue[i] };//{ newValue[i+1], newValue[i] }; //TODO: usare memcpy

			listNode_t *node = Climb_findChildNodeById(nodeID);
			if (node != NULL) {
				//controlla che lo stato richiesto tramite gatt non sia lo stesso che attualmente ha il nodo e che lo stato richiesto non sia ALERT
				if( newValue[i + NODE_ID_LENGTH] != node->device.advData[ADV_PKT_STATE_OFFSET] && (ChildClimbNodeStateType_t)newValue[i + NODE_ID_LENGTH] != ALERT ){	//PER ORA NON BROADCASTARE LO STATO ALERT ALTRIMENTI SE C'E' UN FALSO ALERT IL CHILD VA IN ALERT ANCHE SE NON DOVREBBE

					node->device.stateToImpose = (ChildClimbNodeStateType_t) newValue[i + NODE_ID_LENGTH]; //quando uno di questi viene cambiato aggiorna l'adv
					advUpdateReq = TRUE;

				}else{ //se non devo cambiare lo stato � meglio essere sicuri che rimanga nello stesso di prima
					if(node->device.stateToImpose != ON_BOARD && node->device.stateToImpose != ALERT ){ //in questo caso non cambiare stateToImpose, altrimenti se un device si resetta non torna ON_BOARD
						//node->device.stateToImpose = (ChildClimbNodeStateType_t)node->device.advData[ADV_PKT_STATE_OFFSET];
					}
				}

			}
			i = i + NODE_ID_LENGTH + 1;

		}
		CLIMB_FlashLed(Board_LED1);
		break;
	}
	default:
		// should not reach here!
		break;
	}
}


/*********************************************************************
 * @fn      Climb_processRoleEvent
 *
 * @brief   Observer role event processing function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  none
 */
static void Climb_processRoleEvent(gapObserverRoleEvent_t *pEvent) {

	switch (pEvent->gap.opcode) {
	case GAP_DEVICE_INIT_DONE_EVENT: {
		memcpy(myAddr, pEvent->initDone.devAddr,B_ADDR_LEN); //salva l'indirizzo del nodo

#ifdef PRINTF_ENABLED

		//System_printf("\nInitialized, this is my MAC: ");
		//System_printf(Util_convertBdAddr2Str(pEvent->initDone.devAddr));
		//System_printf("\n");
#endif
#ifdef FEATURE_LCD
		char buf[10];
		sprintf(buf,"Me: ");
		devpkLcdText(buf, 1, 0);
		sprintf(buf,Util_convertBdAddr2Str(childDevList[0].devRec.addr));
		devpkLcdText(buf, 1, 5);
#endif
		// enable advertise event notification
		uint8 status = HCI_EXT_AdvEventNoticeCmd(selfEntity, ADVERTISE_EVT);
#ifdef PRINTF_ENABLED
		//System_printf("\nAdvertise events notification enabled, status : %d\n", status);
#endif
	}
		break;

	case GAP_DEVICE_INFO_EVENT: {
		if (pEvent->deviceInfo.eventType == GAP_ADRPT_ADV_SCAN_IND | //adv data event (Scannable undirected)
			pEvent->deviceInfo.eventType == GAP_ADRPT_ADV_IND	   |
			pEvent->deviceInfo.eventType == GAP_ADRPT_ADV_NONCONN_IND) { //adv data event (Connectable undirected)

#ifdef PRINTF_ENABLED
			//System_printf("\nGAP_DEVICE_INFO_EVENT-GAP_ADRPT_ADV_SCAN_IND, ADV_DATA. address: ");
			//System_printf(Util_convertBdAddr2Str(pEvent->deviceInfo.addr));
			//System_printf("\nChecking if it is a CLIMB node.\n");
#endif
			ClimbNodeType_t nodeType = isClimbNode((gapDeviceInfoEvent_t*) &pEvent->deviceInfo);
			if (nodeType == CLIMB_CHILD_NODE || nodeType == CLIMB_MASTER_NODE) {
				Climb_addNodeDeviceInfo(&pEvent->deviceInfo, nodeType);

				if (nodeType == CLIMB_CHILD_NODE){
					//Climb_contactsCheckSendThroughGATT();
#ifdef PRINTF_ENABLED
					Climb_printfNodeInfo(&pEvent->deviceInfo);
#endif
				}

			}else {

#ifdef PRINTF_ENABLED
				//System_printf("\nIt isn't a CLIMB node, device discarded!\n");
#endif
			}
		} else if(pEvent->deviceInfo.eventType == GAP_ADRPT_SCAN_RSP) {  //scan response data event
#ifdef PRINTF_ENABLED
			//System_printf("\nScan response received!\n");
#endif
		}

	}
		break;

	case GAP_DEVICE_DISCOVERY_EVENT:

	{
		uint8 status = GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE,DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);


#ifdef PRINTF_ENABLED
		//System_printf("\nDevices found in this epoch: %d\n", pEvent->discCmpl.numDevs);
		//System_printf("\nTrigger a new discovery, status: %d\n", status);
#endif


	}
		break;
	case GAP_ADV_DATA_UPDATE_DONE_EVENT:
#ifdef PRINTF_ENABLED
		//System_printf("\nAdvertise data updated!!\n");
#endif
		break;
	case GAP_MAKE_DISCOVERABLE_DONE_EVENT:
		break;
	case GAP_END_DISCOVERABLE_DONE_EVENT:
		break;
		//per abilitare la ricezione dei due eventi sopra cambiare gapEvtNotify = TRUE; per gli stessi eventi in peripheralObserver.c
	default:
		break;
	}
}









/*********************************************************************
 * @fn      isClimbNode
 *
 * @brief	checks if the gap event is related to a Climb node or not
 *
 * @param	pointer to gap info event
 *
 * @return  Type of node, see ClimbNodeType_t
 */

static ClimbNodeType_t isClimbNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent_a) {
	uint8 index = 0;

	if (gapDeviceInfoEvent_a->eventType == GAP_ADRPT_ADV_SCAN_IND |
		gapDeviceInfoEvent_a->eventType == GAP_ADRPT_ADV_IND      |
		gapDeviceInfoEvent_a->eventType == GAP_ADRPT_ADV_NONCONN_IND) { //il nome � contenuto solo dentro i pacchetti di advertise, � inutile cercarli dentro le scan response

		while (index < gapDeviceInfoEvent_a->dataLen) {
			if (gapDeviceInfoEvent_a->pEvtData[index + 1]
					== GAP_ADTYPE_LOCAL_NAME_COMPLETE) { //ho trovato il nome del dispositivo

				if (memcomp(&(gapDeviceInfoEvent_a->pEvtData[index + 2]),&Climb_childNodeName[0],(gapDeviceInfoEvent_a->pEvtData[index]) - 1) == 0) { //il nome � compatibile con CLIMB

					return CLIMB_CHILD_NODE;

				}else if(memcomp(&(gapDeviceInfoEvent_a->pEvtData[index + 2]),&(defAdvertData[2]),(gapDeviceInfoEvent_a->pEvtData[index]) - 1) == 0 ){ //TODO: verificare...
					return CLIMB_MASTER_NODE;
				}
				else {
					return NOT_CLIMB_NODE; //con questo return blocco la ricerca appena trovo un nome, quindi se dentro il pachetto sono definiti due nomi la funzione riconoscer� solo il primo
				}

			} else { // ricerca il nome nella parte successiva del pacchetto
				index = index + gapDeviceInfoEvent_a->pEvtData[index] + 1;
			}

		}
		return NAME_NOT_PRESENT; //

	} else { //sto cercando il nome dentro un scan response
		return WRONG_PARKET_TYPE; //
	}
}

/*********************************************************************
 * @fn      Climb_addNodeDeviceInfo
 *
 * @brief	adds information related to a gap info event, if the node isn't in the list is added, otherwise if the node is already inside the list its data is updated
 *
 * @return  none
 */
static void Climb_addNodeDeviceInfo( gapDeviceInfoEvent_t *gapDeviceInfoEvent , ClimbNodeType_t nodeType) {

	listNode_t* node_position = Climb_findNodeByDevice(gapDeviceInfoEvent, nodeType);

	if(node_position == NULL){	//dispositivo nuovo, aggiungilo!
		if(Climb_addNode(gapDeviceInfoEvent,nodeType) == NULL){
			while(1){
				node_position = NULL;
			}
		}
	}else{
		Climb_updateNodeMetadata(gapDeviceInfoEvent,node_position);
	}

	Climb_advertisedStatesCheck(); //aggiorna l'adv

	return;
}

/*********************************************************************
 * @fn      Climb_findNodeByDevice
 *
 * @brief	Find the node inside child or master lists and returns the node pointer. If the node cannot be found this function returns null. If the list is empty it returns null.
 *
 * @return  Pointer to node instance that can be used to modify stored data
 */

static listNode_t* Climb_findNodeByDevice(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType){

	listNode_t* node = NULL;
	if (nodeType == CLIMB_CHILD_NODE ) {
		if (childListRootPtr == NULL) {
			return NULL;
		}
		node = childListRootPtr;
	} else if (nodeType == CLIMB_MASTER_NODE ) {
		if (masterListRootPtr == NULL) {
			return NULL;
		}
		node = masterListRootPtr;
	}

	while (node != NULL) {
		if (memcomp(gapDeviceInfoEvent->addr, node->device.devRec.addr,	B_ADDR_LEN) == 0) {
			return node; //se trovi il nodo esci e passane il puntatore
		}
		node = node->next; //passa al nodo sucessivo
	}

	return NULL;
}

/*********************************************************************
 * @fn      Climb_findChildNodeById
 *
 * @brief	Find the node inside child lists (it uses only the ID information) and returns the node pointer. If the node cannot be found this function returns null. If the list is empty it returns null.
 *
 * @return  none
 */
static listNode_t* Climb_findChildNodeById(uint8 *nodeID){

	listNode_t* node = NULL;

	if (childListRootPtr == NULL) {
		return NULL;
	}
	node = childListRootPtr;

	while (node != NULL) {
		//if (*nodeID == node->device.devRec.addr[0]){//
		if (memcomp(nodeID, &node->device.advData[ADV_PKT_ID_OFFSET], NODE_ID_LENGTH) == 0) {
			return node; //se trovi il nodo esci e passane il puntatore
		}
		node = node->next; //passa al nodo sucessivo
	}

	return NULL;

}


/*********************************************************************
 * @fn      Climb_addNode
 *
 * @brief	Adds a node at the end of the child or master list with informations contained in the gap info event. It automatically manages the adding of first node which is critical because it is referenced by childListRootPtr or masterListRootPtr
 *
 * @return  The pointer to the node istance inside the list
 */
static listNode_t* Climb_addNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType){

	listNode_t* new_Node_Ptr = (listNode_t*) ICall_malloc(sizeof(listNode_t));
	if (new_Node_Ptr == NULL) {
		//malloc fail!
		return NULL;
	}

	//inserisci metadata nel nuovo elemtno della lista
	new_Node_Ptr->device.advDataLen = gapDeviceInfoEvent->dataLen;
	new_Node_Ptr->device.rssi = gapDeviceInfoEvent->rssi;
	new_Node_Ptr->device.contactsCounter = 0;
	new_Node_Ptr->next = NULL; //il nuovo nodo finir� in coda
	new_Node_Ptr->device.lastContactTicks = Clock_getTicks();
	memcpy(new_Node_Ptr->device.advData, gapDeviceInfoEvent->pEvtData,gapDeviceInfoEvent->dataLen);
	memcpy(new_Node_Ptr->device.devRec.addr, gapDeviceInfoEvent->addr,B_ADDR_LEN);
	new_Node_Ptr->device.stateToImpose = (ChildClimbNodeStateType_t)new_Node_Ptr->device.advData[ADV_PKT_STATE_OFFSET];

	//connetti il nuovo elemento della lista in coda
	if (nodeType == CLIMB_CHILD_NODE) {
		if (childListRootPtr != NULL) { //il nodo che sto inserendo NON � il primo
			listNode_t* node = childListRootPtr;
			while (node->next != NULL) { //cerca l'ultimo nodo
				node = node->next; //passa al nodo sucessivo
			}
			node->next = new_Node_Ptr; //aggiorna riferimento del nodo precedente
		} else {								//sto inserendo il primo nodo
			childListRootPtr = new_Node_Ptr;//salva il puntatore al nodo root
		}
		//childScanRes++;
	} else if (nodeType == CLIMB_MASTER_NODE) {
		if (masterListRootPtr != NULL) { //il nodo che sto inserendo NON � il primo
			listNode_t* node = masterListRootPtr;
			while (node->next != NULL) { //cerca l'ultimo nodo
				node = node->next; //passa al nodo sucessivo
			}
			node->next = new_Node_Ptr; //aggiorna riferimento del nodo precedente
		} else {								//sto inserendo il primo nodo
			masterListRootPtr = new_Node_Ptr;//salva il puntatore al nodo root
		}
		//masterScanRes++;
	}
	return new_Node_Ptr;

}

/*********************************************************************
 * @fn      Climb_updateNodeMetadata
 *
 * @brief	updates targetNode metadata with infomation contained in gapDeviceInfoEvent
 *
 * @return  none
 */
static void Climb_updateNodeMetadata(gapDeviceInfoEvent_t *gapDeviceInfoEvent,
		listNode_t* targetNode) {

	if (gapDeviceInfoEvent->eventType == GAP_ADRPT_ADV_SCAN_IND	|
		gapDeviceInfoEvent->eventType == GAP_ADRPT_ADV_IND      |
		gapDeviceInfoEvent->eventType == GAP_ADRPT_ADV_NONCONN_IND) {	//adv data

		//inserisci metadata
		targetNode->device.advDataLen = gapDeviceInfoEvent->dataLen;
		targetNode->device.rssi = gapDeviceInfoEvent->rssi;
		targetNode->device.contactsCounter++;
		targetNode->device.lastContactTicks = Clock_getTicks();
		memcpy(targetNode->device.advData, gapDeviceInfoEvent->pEvtData,gapDeviceInfoEvent->dataLen);

	} else if (gapDeviceInfoEvent->eventType == GAP_ADRPT_SCAN_RSP) {//scan response data

	}

	return;

}

/*********************************************************************
 * @fn      Climb_advertisedStatesCheck
 *
 * @brief	Checks if the states advertised by nodes are the same as stateToImpose, if not it requests the update of advertised data to inform nodes that they have to change their states
 *
 * @return  none
 */
static void Climb_advertisedStatesCheck(void)
{
	//FA IL CONTROLLO DELLO STATO ATTUALE RISPETTO A QUELLO VOLUTO, SE LO STATO E' BY_MYSELF PASSA AUTOMATICAMENTE IN CHECKING
	listNode_t* node = childListRootPtr;

	while (node != NULL) {

		//ACCETTA AUTOMATICAMENTE TUTTI I NODI CLIMBC E FALLI ANDARE IN CHECKIN
		if (node->device.advData[ADV_PKT_STATE_OFFSET] == BY_MYSELF && node->device.stateToImpose == BY_MYSELF) { //accetta automaticamente tutti i nodi climb!
			node->device.stateToImpose = CHECKING;
		}


		if (node->device.advData[ADV_PKT_STATE_OFFSET] != node->device.stateToImpose) {
			advUpdateReq = TRUE;
		}

		node = node->next; //passa al nodo sucessivo
	}
}


/*********************************************************************
 * @fn      Climb_nodeTimeoutCheck
 *
 * @brief	Check the timeout for every node contained in child and master lists. If timeout is expired the node is removed.
 *
 *
 * @return  none
 */
static void Climb_nodeTimeoutCheck() {
#ifdef PRINTF_ENABLED
	//System_printf("\nRunning timeout check!\n");
#endif
	uint32 nowTicks = Clock_getTicks();

	//controlla la lista dei child
	listNode_t* targetNode = childListRootPtr;
	listNode_t* previousNode = NULL;
	while (targetNode != NULL) {

		if (nowTicks - targetNode->device.lastContactTicks> NODE_TIMEOUT_OS_TICKS) {
			//TODO: ATTENZIONE, SE UN NODO E' ON BOARD, ALLO SCADERE DEL TIMEOUT NON DEVE ESSERE ELIMINATO MA ANDARE IN ALLERTA!!!
			//INOLTRE SE UN NODO E' IN ALLERTA NON DEVE ESSERE ELIMINATO DALLA LISTA
			if(targetNode->device.advData[ADV_PKT_STATE_OFFSET] == ON_BOARD ||
			   targetNode->device.advData[ADV_PKT_STATE_OFFSET] == ALERT ||
			   targetNode->device.stateToImpose == ON_BOARD ||
			   targetNode->device.stateToImpose == ALERT){
				//TRIGGER ALERT SIGNAL
			}else{
				Climb_removeNode(targetNode, previousNode);
				advUpdateReq = TRUE;
			}
		}
		previousNode = targetNode;
		targetNode = targetNode->next; //passa al nodo sucessivo
	}

	//controlla la lista dei master
	targetNode = masterListRootPtr;
	previousNode = NULL;
	while (targetNode != NULL) {

		if (nowTicks - targetNode->device.lastContactTicks> NODE_TIMEOUT_OS_TICKS) {
			Climb_removeNode(targetNode, previousNode);
			advUpdateReq = TRUE;
		}
		previousNode = targetNode;
		targetNode = targetNode->next; //passa al nodo sucessivo
	}
}

/*********************************************************************
 * @fn      Climb_removeNode
 *
 * @brief	Removes nodeToRemove node form the list which contains it
 *
 * @return  none
 */
static void Climb_removeNode(listNode_t* nodeToRemove, listNode_t* previousNode){

	if ( nodeToRemove == NULL ) {
		return;
	}

	if (previousNode == NULL && (nodeToRemove != childListRootPtr || nodeToRemove != childListRootPtr) ) { // se il nodo precedente non viene dato (e non sto rimuovendo un primo nodo della lista)
		return;
	}
/*
	if (previousNode == NULL) { // se il nodo precedente non viene dato esplicitamente cercalo!
		previousNode = childListRootPtr;
		while (previousNode->next != nodeToRemove) {
			previousNode = previousNode->next;
			if (previousNode == NULL) { //ho raggiunto la fine della lista child senza trovare il nodo
				previousNode = masterListRootPtr; //cercalo dentro la lista dei master
				while (previousNode->next != nodeToRemove) {
					nodeToRemove = previousNode->next;
					if (nodeToRemove == NULL) { //ho scansionato entrambe le liste senza trovare nulla
						return;
					}
				}
			}
		}
	}
	*/
	if (nodeToRemove != childListRootPtr && nodeToRemove != masterListRootPtr) { // se il nodo che voglio rimuovere non � il primo vai liscio
		previousNode->next = nodeToRemove->next;
	} else if (nodeToRemove == childListRootPtr) { //voglio rimuovere il primo nodo della lista dei bambini
		childListRootPtr = nodeToRemove->next; //quindi sostituisco il puntatore root
	} else if (nodeToRemove == masterListRootPtr) { //voglio rimuovere il primo nodo della lista dei master
		masterListRootPtr = nodeToRemove->next; //quindi sostituisco il puntatore root
	}
#ifdef PRINTF_ENABLED
	//System_printf("\nNode ");
	//System_printf(Util_convertBdAddr2Str(nodeToRemove->device.devRec.addr));
	//System_printf(" removed!\n");
#endif
	ICall_free(nodeToRemove); //rilascia la memoria
}

/*********************************************************************
 * @fn      Climb_periodicTask
 *
 * @brief	Handler associated with Periodic Clock instance.
 *
 * @return  none
 */
static void Climb_periodicTask(){
	Climb_nodeTimeoutCheck();
}

#ifdef PRINTF_ENABLED
static void Climb_printfNodeInfo(gapDeviceInfoEvent_t *gapDeviceInfoEvent ){
	static uint8 usbPktsCounter = 0;
	uint32 nowTicks = Clock_getTicks();
	uint8 len = gapDeviceInfoEvent->dataLen;
	uint8 i;
	System_printf("\n%d ", nowTicks);

	//System_printf(Util_convertBdAddr2Str(myAddr));
	System_printf(Util_convertBdAddr2Str(gapDeviceInfoEvent->addr));

	if(len == 31){
		System_printf(" CLIMBC ADV %02x %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",usbPktsCounter++, gapDeviceInfoEvent->pEvtData[12],gapDeviceInfoEvent->pEvtData[13],gapDeviceInfoEvent->pEvtData[14],gapDeviceInfoEvent->pEvtData[15],gapDeviceInfoEvent->pEvtData[16],gapDeviceInfoEvent->pEvtData[17],gapDeviceInfoEvent->pEvtData[18],gapDeviceInfoEvent->pEvtData[19],gapDeviceInfoEvent->pEvtData[20],gapDeviceInfoEvent->pEvtData[21],gapDeviceInfoEvent->pEvtData[22],gapDeviceInfoEvent->pEvtData[23],gapDeviceInfoEvent->pEvtData[24],gapDeviceInfoEvent->pEvtData[25],gapDeviceInfoEvent->pEvtData[26],gapDeviceInfoEvent->pEvtData[27],gapDeviceInfoEvent->pEvtData[28],gapDeviceInfoEvent->pEvtData[29],gapDeviceInfoEvent->pEvtData[30] );
	}else{
		System_printf(" CLIMBC ADV %02x ",usbPktsCounter++);
		for(i = 12; i < len; i++){
			System_printf("%02x",gapDeviceInfoEvent->pEvtData[i]);
		}
	}
	//System_printf(" CLIMBD ADV %02x %02x%02x%02x\n",usbPktsCounter++, gapDeviceInfoEvent->pEvtData[12] ,gapDeviceInfoEvent->pEvtData[30] );

}
#endif
/*********************************************************************
 * @fn      Climb_epochStartHandler
 *
 * @brief	Handler associated with epoch clock instance.
 *
 * @return  none
 */
static void Climb_epochStartHandler(){
	GAPObserverRole_CancelDiscovery();

#ifdef ENABLE_ADV
	if(beaconActive){
		if(thisNodeState == CONNECTED){
			uint8 adv_active = 1;
			uint8 status = GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t),&adv_active);
		}else{
			uint8 adv_active = 1;
			uint8 status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&adv_active);
		}
	}
#endif

	GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE,DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);
}




/*********************************************************************
 * @fn      CLIMB_FlashLed
 *
 * @brief   Turn on a led and start the timer that switch it off;
 *
 * @param   pinId - the led id to turn on

 * @return  none
 */
static void CLIMB_FlashLed(PIN_Id pinId){

	PIN_setOutputValue(hGpioPin, pinId, Board_LED_ON);
	Util_startClock(&ledTimeoutClock); //start the clock that switch the led off

}
/*********************************************************************
 * @fn      CLIMB_handleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                HAL_KEY_SW_1
 *
 * @return  none
 */
static void CLIMB_handleKeys(uint8 shift, uint8 keys) {
	if (keys == Board_KEY_RIGHT) {
		beaconActive = 0;
		GAPObserverRole_CancelDiscovery();
		uint8 adv_active = 0;
		uint8 status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&adv_active);
		GAPRole_TerminateConnection();

		//PIN_setOutputValue(hGpioPin, Board_LED2, Board_LED_OFF);

		listNode_t* node = childListRootPtr;
		while (node != NULL) { //cicla fino all'ultimo nodo
			node->device.stateToImpose = BY_MYSELF;
			node->device.lastContactTicks = 0;
			node = node->next; //passa al nodo sucessivo
		}
		lastGATTCheckTicks = 1;
		uint8 zeroArray[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		ClimbProfile_SetParameter(CLIMBPROFILE_CHAR1, 20, zeroArray);
		PIN_setOutputValue(hGpioPin, Board_LED1, Board_LED_OFF);
		Util_stopClock(&epochClock);
	}
	if (keys == Board_KEY_LEFT) {
		if(beaconActive != 1){

			//uint8 adv_active = 1;
			//uint8 status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&adv_active);
			GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE,DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);
			lastGATTCheckTicks = Clock_getTicks();
			PIN_setOutputValue(hGpioPin, Board_LED1, Board_LED_ON);
			//Util_startClock(&epochClock);
		}
		beaconActive = 1;
	}
}

/*!*****************************************************************************
 *  @fn         Key_callback
 *
 *  Interrupt service routine for buttons, relay and MPU
 *
 *  @param      handle PIN_Handle connected to the callback
 *
 *  @param      pinId  PIN_Id of the DIO triggering the callback
 *
 *  @return     none
 ******************************************************************************/
static void Key_callback(PIN_Handle handle, PIN_Id pinId)
{

  SimpleBLEPeripheral_enqueueMsg(KEY_CHANGE_EVT, pinId, NULL);

}

#ifdef FEATURE_LCD
/*********************************************************************
 * @fn      displayInit
 *
 * @brief
 *
 * @param
 *
 * @return  none
 */
static void displayInit(void){

  // Initialize LCD
  devpkLcdOpen();
}
#endif





/*********************************************************************
 * @fn      SimpleBLEPeripheral_enqueueMsg
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   state - message state.
 *
 * @return  None.
 */
static uint8_t SimpleBLEPeripheral_enqueueMsg(uint8_t event, uint8_t state,uint8_t *pData) {
	sbpEvt_t *pMsg;

	// Create dynamic pointer to message.
	if (pMsg = ICall_malloc(sizeof(sbpEvt_t))) {
		pMsg->hdr.event = event;
		pMsg->hdr.state = state;
		pMsg->pData = pData;

		// Enqueue the message.
		return Util_enqueueMsg(appMsgQueue, sem, (uint8_t *) pMsg);
	}

	return FALSE;
}

/*********************************************************************
 * @fn      Climb_clockHandler
 *
 * @brief   Handler function for clock timeouts.
 *
 * @param   arg - event type
 *
 * @return  None.
 */
static void Climb_clockHandler(UArg arg)
{
  // Store the event.
  events |= arg;

  // Wake up the application.
  Semaphore_post(sem);
}

/*********************************************************************
 * @fn      memcomp
 *
 * @brief   Compares memory locations, same as memcmp, but it returns 0 if two memory sections are identical, 1 otherwise.
 *
 *
 * @return  none
 */

static uint8 memcomp(uint8 * str1, uint8 * str2, uint8 len) { //come memcmp (ma ritorna 0 se � uguale e 1 se � diversa, non dice se minore o maggiore)
	uint8 index;
	for (index = 0; index < len; index++) {
		if (str1[index] != str2[index]) {
			return 1;
		}
	}
	return 0;
}
/*********************************************************************
*********************************************************************/
