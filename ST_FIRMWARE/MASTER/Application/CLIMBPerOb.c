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
#include "Keys_Task.h"

#include <ti/drivers/lcd/LCDDogm1286.h>
#include <xdc/runtime/System.h>

#include "sensor_bmp280.h"
#include "sensor_hdc1000.h"
#include "sensor_mpu9250.h"
#include "sensor_opt3001.h"
#include "sensor_tmp007.h"

#include <ti/drivers/timer/GPTimerCC26XX.h>
#include <xdc/runtime/Types.h>
#include <ti/sysbios/BIOS.h>

#ifdef HEAPMGR_METRICS
#include "ICall.h"
#endif

#ifdef FEATURE_LCD
#include "devpk_lcd.h"
#include <stdio.h>
#endif

#ifndef CLIMB_DEBUG
#error "It is not safe to run without CLIMB_DEBUG defined, the logic behind the call of Climb_advertisedStatesUpdate(..) should be checked!"
#endif
/*********************************************************************
 * CONSTANTS
 */
// Advertising interval when device is discoverable (units of 625us, 160=100ms)
#define DEFAULT_CONNECTABLE_ADVERTISING_INTERVAL          600
#define DEFAULT_NON_CONNECTABLE_ADVERTISING_INTERVAL          1000//1131//1592
#define EPOCH_PERIOD										1000

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_GENERAL

// Minimum connection interval (units of 1.25ms, 80=100ms) if automatic
// parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL   	 165

// Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic
// parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     165

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY        0

// Supervision timeout value (units of 10ms, 1000=10s) if automatic parameter
// update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          150

// Whether to enable automatic parameter update request when a connection is
// formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         4

// Scan duration in ms
#define DEFAULT_SCAN_DURATION                 2000//1300//250//1000 //Tempo di durata di una scansione, allo scadere la scansione viene ricominciata

#define PRE_ADV_TIMEOUT				  (DEFAULT_NON_CONNECTABLE_ADVERTISING_INTERVAL*625)/1000-3
#define PRE_CE_TIMEOUT				  (DEFAULT_DESIRED_MIN_CONN_INTERVAL*1250)/1000-5


// Scan interval value in 0.625ms ticks
#define SCAN_INTERVAL 						  160

// scan window value in 0.625ms ticks
#define SCAN_WINDOW							  160

// Whether to report all contacts or only the first for each device
#define FILTER_ADV_REPORTS					  FALSE

// Discovery mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE                DEVDISC_MODE_ALL //non � ancora chiaro cosa cambi, con le altre due opzioni non vede

// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN         FALSE

// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST          FALSE

// Maximum number of scan responses to be reported to application
#define DEFAULT_MAX_SCAN_RES                  60

// How often to perform periodic event (in msec)
#define PERIODIC_EVT_PERIOD              	  2000

#define NODE_TIMEOUT_OS_TICKS				  500000

#define LED_TIMEOUT						  	  10
#define RESET_BROADCAST_CMD_TIMEOUT			  6000

#define WAKEUP_DEFAULT_TIMEOUT_SEC				60*60*24
#define GOTOSLEEP_DEFAULT_TIMEOUT_SEC			60*60
#define GOTOSLEEP_POSTPONE_INTERVAL_SEC			10*60

#define MAX_ALLOWED_TIMER_DURATION_SEC	      42000 //actual max timer duration 42949.67sec
//#define NODE_ID								  0x01 //per ora non � applicabile ai nodi master

#define CHILD_NODE_ID_LENGTH				  1
#define MASTER_NODE_ID_LENGTH				  6

#define MAX_SUPPORTED_CHILD_NODES			  87
#define MAX_SUPPORTED_MASTER_NODES			  5

#define ADV_PKT_ID_OFFSET					  12
#define ADV_PKT_STATE_OFFSET				  ADV_PKT_ID_OFFSET + CHILD_NODE_ID_LENGTH

#define BROADCAST_MSG_TYPE_STATE_UPDATE_CMD	  0x01
#define BROADCAST_MSG_LENGTH_STATE_UPDATE_CMD 0x02

#define BROADCAST_MSG_TYPE_WU_SCHEDULE_CMD	  0x02
#define BROADCAST_MSG_LENGTH_WU_SCHEDULE_CMD  0x04

#define BROADCAST_RESET_CMD_FLAG_CMD		  0xFF
#define BROADCAST_LENGTH_RESET_CMD_FLAG_CMD   0x01

#define	DEFAULT_MTU_LENGTH						23

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
#define RESET_BROADCAST_CMD_EVT				0x0100
#define EPOCH_EVT							0x0400
#define PRE_ADV_EVT							0x1000
#define WATCHDOG_EVT						0x2000
#define WAKEUP_TIMEOUT_EVT					0x4000
#define GOTOSLEEP_TIMEOUT_EVT				0x8000
#define PRE_CE_EVT							0x0800
/*********************************************************************
 * TYPEDEFS
 */

typedef enum ChildClimbNodeStateType_t {
	BY_MYSELF,
	CHECKING,
	ON_BOARD,
	ALERT,
	GOING_TO_SLEEP,
	ERROR,
	INVALID_STATE
} ChildClimbNodeStateType_t;

typedef enum MasterClimbNodeStateType_t {
	NOT_USED_FOR_NOW
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
	//uint8 addrType;         //!< Address Type: @ref GAP_ADDR_TYPE_DEFINES
	//uint8 addr[B_ADDR_LEN]; //!< Device's Address
	uint8 id[6];
	ChildClimbNodeStateType_t state;
	uint32 lastContactTicks;
	uint8 rssi;
	uint8 contactSentThoughGATT;
	ChildClimbNodeStateType_t stateToImpose;
} myGapDevRec_t;

typedef struct listNode {
	myGapDevRec_t device;
	struct listNode *next;
} listNode_t;


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
#ifdef WORKAROUND
static Clock_Struct epochClock;
#endif
static Clock_Struct resetBroadcastCmdClock;
static Clock_Struct preAdvClock;
static Clock_Struct preCEClock;
static Clock_Struct wakeUpClock;
static Clock_Struct goToSleepClock;

// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// events flag for internal application events.
static uint16_t events;

// Task configuration
Task_Struct sbpTask;
Char sbpTaskStack[SBP_TASK_STACK_SIZE];

static uint8 BLE_connected = 0;

static uint8 Climb_childNodeName[] = { 'C', 'L', 'I', 'M', 'B', 'C' };

static uint8 advUpdateReq = FALSE;

static uint8 isBroadcastMessageValid = FALSE;
static uint8 broadcastMessage[10];

static uint8 beaconActive = 0;

static uint8 myAddr[B_ADDR_LEN];
static uint8 defAdvertData[] = { 0x07,	// length of this data
		GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'C', 'L', 'I', 'M', 'B', 'M', 0x02,   // length of this data
		GAP_ADTYPE_FLAGS,
		DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

};

// GAP - SCAN RSP data (max size = 31 bytes)
static uint8 defScanRspData[] = {

0x03,
GAP_ADTYPE_APPEARANCE, (uint8) (GAP_APPEARE_GENERIC_TAG & 0xFF), (uint8) ((GAP_APPEARE_GENERIC_TAG >> 8) & 0xFF)

};

// GAP GATT Attributes
static uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = "CLIMB Node";

// Globals used for ATT Response retransmission
static gattMsgEvent_t *pAttRsp = NULL;
static uint8_t rspTxRetry = 0;

//static uint32 lastGATTCheckTicks = 0;
// Pins that are actively used by the application
// Pins that are actively used by the application
static PIN_Config SensortagAppPinTable[] =
{
    Board_LED1       | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* LED initially off             */
    Board_LED2       | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* LED initially off             */
    //Board_KEY_LEFT   | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,        /* Button is active low          */
    //Board_KEY_RIGHT  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,        /* Button is active low          */
    //Board_RELAY      | PIN_INPUT_EN | PIN_PULLDOWN | PIN_IRQ_BOTHEDGES | PIN_HYSTERESIS,      /* Relay is active high          */
    Board_BUZZER     | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,     /* Buzzer initially off          */
	//Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,

    PIN_TERMINATE
};

// Global pin resources
PIN_State pinGpioState;
PIN_Handle hGpioPin;

#ifdef CLIMB_DEBUG
static uint8 adv_counter = 0;
#endif

static uint8 broadcastID[] = { 0xFF };
static uint8 zeroID[] = {0x00};

//static listNode_t* childListRootPtr = NULL;
//static listNode_t* masterListRootPtr = NULL;
static uint8 adv_startNodeIndex = 0;
static uint8 gatt_startNodeIndex = 0;

static myGapDevRec_t* childListArray = NULL;
static myGapDevRec_t* masterListArray = NULL;

static uint8 mtu_size = DEFAULT_MTU_LENGTH;
static uint8 ready = FALSE;

static uint8 scanning = FALSE;

GPTimerCC26XX_Handle hTimer;

static uint8 unclearedWatchdogEvents = 0;

static uint16 connectionHandle = 0;

static uint32 wakeUpTimeout_sec_global = 0;

static uint8 onBoardChildren = 0; //not to be used in critical context, updated on Climb_contactsCheckSendThroughGATT

static uint8 devicesHeardDuringLastScan = 0;
/*********************************************************************
 * LOCAL FUNCTIONS
 */

////TASK FUNCTIONS
static void SimpleBLEPeripheral_init(void);
static void nodeListInit(void);
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
static void Keys_EventCB(keys_Notifications_t notificationType);
static void updateConnParam_CB(uint16_t connInterval, uint16_t connSlaveLatency, uint16_t connTimeout);

////CLIMB IN/OUT FUNCTIONS
static void Climb_contactsCheckSendThroughGATT(void);
static void Climb_advertisedStatesUpdate(void);
static void Climb_processCharValueChangeEvt(uint8_t paramID);
static void Climb_processRoleEvent(gapObserverRoleEvent_t *pEvent);

////CLIMB MANAGEMENT
static ClimbNodeType_t isClimbNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent_a);
static void Climb_addNodeDeviceInfo(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType);
//static listNode_t* Climb_findNodeByDevice(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType);
static uint8 Climb_findNodeById(uint8 *nodeID, ClimbNodeType_t nodeType);
static uint8 Climb_addNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType);
static void Climb_updateNodeMetadata(gapDeviceInfoEvent_t *gapDeviceInfoEvent, uint8 index, ClimbNodeType_t nodeType);
static void Climb_advertisedStatesCheck(void);
//static uint8 Climb_isMyChild(uint8 nodeID);
static void Climb_nodeTimeoutCheck();
static void Climb_removeNode(uint8 indexToRemove, ClimbNodeType_t nodeType) ;
static void Climb_periodicTask();
#ifdef PRINTF_ENABLED
#ifndef HEAPMGR_METRICS
static void Climb_printfNodeInfo(gapDeviceInfoEvent_t *gapDeviceInfoEvent );
#endif
#endif
#ifdef WORKAROUND
static void Climb_epochStartHandler();
#endif
static void Climb_preAdvEvtHandler();
static void Climb_preCEEvtHandler();
static void Climb_goToSleepHandler();
static void Climb_wakeUpHandler();
static void Climb_setWakeUpClock(uint32 wakeUpTimeout_sec_local);

////HARDWARE RELATED FUNCTIONS
static void CLIMB_FlashLed(PIN_Id pinId);
static void CLIMB_handleKeys(uint8 keys);
static void startNode();
static void stopNode();
static void destroyChildNodeList();
static void destroyMasterNodeList();
//static void Key_callback(PIN_Handle handle, PIN_Id pinId);
#ifdef FEATURE_LCD
static void displayInit(void);
#endif
static void watchdogTimerInit();
static void timerCallback(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask);


////GENERIC SUPPORT FUNCTIONS
static uint8_t SimpleBLEPeripheral_enqueueMsg(uint8_t event, uint8_t state, uint8_t *pData);
static void Climb_clockHandler(UArg arg);

static uint8 memcomp(uint8 * str1, uint8 * str2, uint8 len);
static uint8 isNonZero(uint8 * str1, uint8 len);
#ifdef HEAPMGR_METRICS
static void plotHeapMetrics();
#endif
/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t SimpleBLEPeripheral_gapRoleCBs = { BLEPeripheral_stateChangeCB,     // Profile State Change Callbacks
		BLEObserver_eventCB	// GAP Event callback
		};

// Keys Callbacks
static keysEventCBs_t Keys_EventCBs = { Keys_EventCB, // Profile State Change Callbacks
		};

// GAP Bond Manager Callbacks
static gapBondCBs_t simpleBLEPeripheral_BondMgrCBs = {
NULL, // Passcode callback (not used by application)
		NULL  // Pairing / Bonding state Callback (not used by application)
		};

// Simple GATT Profile Callbacks
static climbProfileCBs_t SimpleBLEPeripheral_climbProfileCBs = { BLEPeripheral_charValueChangeCB, // Characteristic value change callback
		};

static gapRolesParamUpdateCB_t gapRoleUpdateConnParam_CB = updateConnParam_CB;


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
void SimpleBLEPeripheral_createTask(void) {
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
static void SimpleBLEPeripheral_init(void) {

	// Setup I2C for sensors
	bspI2cInit();

	// Handling of buttons, LED, relay
	hGpioPin = PIN_open(&pinGpioState, SensortagAppPinTable);
	//PIN_registerIntCb(hGpioPin, Key_callback);

	//initialize keys
	Keys_Init(&Keys_EventCBs);
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

	Util_constructClock(&resetBroadcastCmdClock, Climb_clockHandler,
	RESET_BROADCAST_CMD_TIMEOUT, 0, false, RESET_BROADCAST_CMD_EVT);

	Util_constructClock(&preAdvClock, Climb_clockHandler,
	PRE_ADV_TIMEOUT, 0, false, PRE_ADV_EVT);

	Util_constructClock(&wakeUpClock, Climb_clockHandler,
	WAKEUP_DEFAULT_TIMEOUT_SEC*1000, 0, false, WAKEUP_TIMEOUT_EVT);

	Util_constructClock(&goToSleepClock, Climb_clockHandler,
	GOTOSLEEP_DEFAULT_TIMEOUT_SEC*1000, 0, false, GOTOSLEEP_TIMEOUT_EVT);

	Util_constructClock(&preCEClock, Climb_clockHandler,
			PRE_CE_TIMEOUT, 0, false, PRE_CE_EVT);

#ifdef WORKAROUND
	Util_constructClock(&epochClock, Climb_clockHandler,
			EPOCH_PERIOD, 0, false, EPOCH_EVT);
#endif

#ifndef SENSORTAG_HW
	Board_openLCD();
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
		GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initialAdvertEnable);
		GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16_t), &advertOffTime);

		GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(defScanRspData), defScanRspData);
		GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);

		GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8_t), &enableUpdateRequest);
		GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &desiredMinInterval);
		GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &desiredMaxInterval);
		GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16_t), &desiredSlaveLatency);
		GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16_t), &desiredConnTimeout);
	}

	// Set the GAP Characteristics
	GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

	// Set advertising interval
	{
		uint16_t advInt = DEFAULT_NON_CONNECTABLE_ADVERTISING_INTERVAL;

		GAP_SetParamValue(TGAP_CONN_ADV_INT_MIN, advInt);
		GAP_SetParamValue(TGAP_CONN_ADV_INT_MAX, advInt);

		advInt = DEFAULT_CONNECTABLE_ADVERTISING_INTERVAL;

		GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, advInt);
		GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, advInt);

		GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, advInt);
		GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, advInt);

	}

	// Setup the GAP Bond Manager
	{
		uint32_t passkey = 0; // passkey "000000"
		uint8_t pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ; //GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
		uint8_t mitm = FALSE;
		uint8_t ioCap = GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
		uint8_t bonding = TRUE;

		GAPBondMgr_SetParameter(GAPBOND_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
		GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(uint8_t), &pairMode);
		GAPBondMgr_SetParameter(GAPBOND_MITM_PROTECTION, sizeof(uint8_t), &mitm);
		GAPBondMgr_SetParameter(GAPBOND_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
		GAPBondMgr_SetParameter(GAPBOND_BONDING_ENABLED, sizeof(uint8_t), &bonding);
	}

	//sensorTestExecute(ST_TEST_MAP);

	sensorBmp280Init();
	sensorBmp280Enable(FALSE);
	sensorHdc1000Init();
	//PIN_setOutputValue(hGpioPin, Board_MPU_POWER, 0);
	//sensorMpu9250Init(); //ho gia scollegato l'alimentazione all'interno del file Board.c
	//sensorMpu9250Reset();
	sensorOpt3001Init();
	sensorTmp007Init();


#ifdef FEATURE_LCD
	displayInit();
#endif
	// Initialize GATT attributes
	GGS_AddService(GATT_ALL_SERVICES);           // GAP
	GATTServApp_AddService(GATT_ALL_SERVICES);   // GATT attributes
	//DevInfo_AddService();                        // Device Information Service

	ClimbProfile_AddService(GATT_ALL_SERVICES); // Simple GATT Profile

	// Setup Observer Profile

	uint8 scanRes = DEFAULT_MAX_SCAN_RES;
	GAPRole_SetParameter(GAPOBSERVERROLE_MAX_SCAN_RES, sizeof(uint8_t), &scanRes);

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

	// Start Bond Manager
	VOID GAPBondMgr_Register(&simpleBLEPeripheral_BondMgrCBs);

	// Register with GAP for HCI/Host messages
	GAP_RegisterForMsgs(selfEntity);

	// Register for GATT local events and ATT Responses pending for transmission
	GATT_RegisterForMsgs(selfEntity);

	//SETTING POWER
	HCI_EXT_SetTxPowerCmd(HCI_EXT_TX_POWER_0_DBM);


	GAPRole_RegisterAppCBs(&gapRoleUpdateConnParam_CB);

	watchdogTimerInit();

	nodeListInit();

	//automatically start-up the node
	events |= WAKEUP_TIMEOUT_EVT;
	Semaphore_post(sem);

#ifdef PRINTF_ENABLED
	System_printf("I'm working!\n\n");
#endif

}


static void nodeListInit(void){

	uint16 arraySize = MAX_SUPPORTED_CHILD_NODES*sizeof(myGapDevRec_t);
	uint16 i = 0;
	childListArray = (myGapDevRec_t*) ICall_malloc( arraySize );

	//resetta la memoria
	for(i = 0; i < arraySize; i++ ){
		((uint8*)childListArray)[i] = 0;
	}


	arraySize = MAX_SUPPORTED_MASTER_NODES*sizeof(myGapDevRec_t);
	masterListArray = (myGapDevRec_t*) ICall_malloc( arraySize );

	//resetta la memoria
	for(i = 0; i < arraySize; i++ ){
		((uint8*)masterListArray)[i] = 0;
	}

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

			if (ICall_fetchServiceMsg(&src, &dest, (void **) &pMsg) == ICALL_ERRNO_SUCCESS) {
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
			if (beaconActive == 1) {
				Util_startClock(&periodicClock);
			}
			// Perform periodic application task
			if (BLE_connected) {
				Climb_periodicTask();
			}
		}

		if (events & LED_TIMEOUT_EVT) {

			events &= ~LED_TIMEOUT_EVT;
			//only turn off leds
			PIN_setOutputValue(hGpioPin, Board_LED1, Board_LED_OFF);
			PIN_setOutputValue(hGpioPin, Board_LED2, Board_LED_OFF);
		}
		if (events & PRE_ADV_EVT) {
			events &= ~PRE_ADV_EVT;

			Climb_preAdvEvtHandler();

		}
		if (events & PRE_CE_EVT) {
			events &= ~PRE_CE_EVT;

			Climb_preCEEvtHandler();

		}
		if (events & RESET_BROADCAST_CMD_EVT) {

			events &= ~RESET_BROADCAST_CMD_EVT;

			isBroadcastMessageValid = FALSE;
		}

		if (events & WATCHDOG_EVT) {
			events &= ~WATCHDOG_EVT;

			unclearedWatchdogEvents = 0;



		}

		if (events & GOTOSLEEP_TIMEOUT_EVT) {
			events &= ~GOTOSLEEP_TIMEOUT_EVT;

			Climb_goToSleepHandler();
		}

		if (events & WAKEUP_TIMEOUT_EVT) {
			events &= ~WAKEUP_TIMEOUT_EVT;

			Climb_wakeUpHandler();
		}


#ifdef WORKAROUND
		if (events & EPOCH_EVT) {
			events &= ~EPOCH_EVT;

			if (beaconActive == 1) {

				Climb_epochStartHandler();

				float randDelay = 10 * ((float) Util_GetTRNG()) / 4294967296;
				if(thisNodeState == CONNECTED) {
					Util_restartClock(&epochClock, EPOCH_PERIOD + randDelay);
				} else {
					Util_restartClock(&epochClock, EPOCH_PERIOD/2 + randDelay);
				}
			}
		}
#endif
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
static uint8_t BLE_processStackMsg(ICall_Hdr *pMsg) {
	uint8_t safeToDealloc = TRUE;

	switch (pMsg->event) {
	case GATT_MSG_EVENT:
		// Process GATT message
		safeToDealloc = SimpleBLEPeripheral_processGATTMsg((gattMsgEvent_t *) pMsg);
		break;

	case HCI_GAP_EVENT_EVENT: {
		// Process HCI message
		switch (pMsg->status) {
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
static uint8_t SimpleBLEPeripheral_processGATTMsg(gattMsgEvent_t *pMsg) {
	// See if GATT server was unable to transmit an ATT response
	if (pMsg->hdr.status == blePending) {
		// No HCI buffer was available. Let's try to retransmit the response
		// on the next connection event.
		if (HCI_EXT_ConnEventNoticeCmd(pMsg->connHandle, selfEntity,CONN_EVT_END_EVT) == SUCCESS) {
			// First free any pending response
			SimpleBLEPeripheral_freeAttRsp(FAILURE);

			// Hold on to the response message for retransmission
			pAttRsp = pMsg;

			// Don't free the response message yet
			return (FALSE);
		}
	} else if (pMsg->method == ATT_FLOW_CTRL_VIOLATED_EVENT) {
		// ATT request-response or indication-confirmation flow control is
		// violated. All subsequent ATT requests or indications will be dropped.
		// The app is informed in case it wants to drop the connection.

		// Display the opcode of the message that caused the violation.
	} else if (pMsg->method == ATT_MTU_UPDATED_EVENT) {
		mtu_size = pMsg->msg.mtuEvt.MTU;
		PIN_setOutputValue(hGpioPin, Board_LED1, Board_LED_ON);

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
static void SimpleBLEPeripheral_processAppMsg(sbpEvt_t *pMsg) {
	switch (pMsg->hdr.event) {
	case P_STATE_CHANGE_EVT:
		BLEPeripheral_processStateChangeEvt((gaprole_States_t) pMsg->hdr.state);
		break;

	case P_CHAR_CHANGE_EVT:
		Climb_processCharValueChangeEvt(pMsg->hdr.state);
		break;

	case O_STATE_CHANGE_EVT:
		BLE_processStackMsg((ICall_Hdr *) pMsg->pData);
		// Free the stack message
		ICall_freeMsg(pMsg->pData);
		break;

	case KEY_CHANGE_EVT:
		CLIMB_handleKeys(pMsg->hdr.state);
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
static void BLEPeripheral_processStateChangeEvt(gaprole_States_t newState) {

	//static bool firstConnFlag = false;

	switch (newState) {
	case GAPROLE_STARTED: {
		uint8_t ownAddress[B_ADDR_LEN];
		uint8_t systemId[DEVINFO_SYSTEM_ID_LEN];
		BLE_connected = FALSE;
		mtu_size = DEFAULT_MTU_LENGTH;

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
	case GAPROLE_ADVERTISING_NONCONN: {
		uint8_t advertEnabled = FALSE;

		// Disable non-connectable advertising.
		GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t), &advertEnabled);

		if(scanning){
			GAPObserverRole_CancelDiscovery();
			scanning = FALSE;
		}

		GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);

		if (beaconActive) {
			advertEnabled = TRUE;

			// Enabled connectable advertising.
			GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advertEnabled);
		}
		// Reset flag for next connection.
		//firstConnFlag = false;

		SimpleBLEPeripheral_freeAttRsp(bleNotConnected);
	}
		break;

	case GAPROLE_CONNECTED: {
		connectionHandle = 0;

		//if(!scanning){
			GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE, DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);
			scanning = TRUE;
		//}

		HCI_EXT_ConnEventNoticeCmd(connectionHandle, selfEntity, CONN_EVT_END_EVT);

		BLE_connected = TRUE;
//		uint8_t peerAddress[B_ADDR_LEN];
//
//		GAPRole_GetParameter(GAPROLE_CONN_BD_ADDR, peerAddress);

		//if (firstConnFlag == false) {
			uint8_t advertEnabled = FALSE; // Turn on Advertising

			// Disable connectable advertising.
			GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advertEnabled);

			// Set to true for non-connectabel advertising.
			advertEnabled = TRUE;

			// Enable non-connectable advertising.
			GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t), &advertEnabled);

		//	firstConnFlag = true;
		//}

	}
		break;

	case GAPROLE_CONNECTED_ADV:
		HCI_EXT_AdvEventNoticeCmd(selfEntity, ADVERTISE_EVT);
		break;

	case GAPROLE_WAITING:

		BLE_connected = FALSE;
		mtu_size = DEFAULT_MTU_LENGTH;
		if(scanning){
			GAPObserverRole_CancelDiscovery();
			scanning = FALSE;
		}
		GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);

		SimpleBLEPeripheral_freeAttRsp(bleNotConnected);

		// Disable connection event end notice
		HCI_EXT_ConnEventNoticeCmd(pAttRsp->connHandle, selfEntity, 0);
		break;

	case GAPROLE_WAITING_AFTER_TIMEOUT: //TIMEOUT

		BLE_connected = FALSE;
		mtu_size = DEFAULT_MTU_LENGTH;
		if(scanning){
			GAPObserverRole_CancelDiscovery();
			scanning = FALSE;
		}
		GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(defAdvertData), defAdvertData);
		// Disable connection event end notice
		HCI_EXT_ConnEventNoticeCmd(pAttRsp->connHandle, selfEntity, 0);

		SimpleBLEPeripheral_freeAttRsp(bleNotConnected);
		//firstConnFlag = false;
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
static void SimpleBLEPeripheral_freeAttRsp(uint8_t status) {
	// See if there's a pending ATT response message
	if (pAttRsp != NULL) {
		// See if the response was sent out successfully
		if (status == SUCCESS) {
		} else {
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
static void BLE_ConnectionEventHandler(void) {

	if(ready){
		Util_startClock(&preCEClock);
	}

	// See if there's a pending ATT Response to be transmitted
	if (pAttRsp != NULL) {
		uint8_t status;

		// Increment retransmission count
		rspTxRetry++;

		// Try to retransmit ATT response till either we're successful or
		// the ATT Client times out (after 30s) and drops the connection.
		status = GATT_SendRsp(pAttRsp->connHandle, pAttRsp->method, &(pAttRsp->msg));
		if ((status != blePending) && (status != MSG_BUFFER_NOT_AVAIL)) {
			// Disable connection event end notice
			//HCI_EXT_ConnEventNoticeCmd(pAttRsp->connHandle, selfEntity, 0);

			// We're done with the response message
			SimpleBLEPeripheral_freeAttRsp(status);
		} else {
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

	if (!BLE_connected) {

		CLIMB_FlashLed(Board_LED2);

	}

	Util_startClock(&preAdvClock);
#ifdef WORKAROUND
	uint8 adv_active = 0;
	uint8 status = GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t),&adv_active);
	status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&adv_active);
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
	if (SimpleBLEPeripheral_enqueueMsg( O_STATE_CHANGE_EVT, SUCCESS, (uint8_t *) pEvent)) {
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
static void BLEPeripheral_stateChangeCB(gaprole_States_t newState) {
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
static void BLEPeripheral_charValueChangeCB(uint8_t paramID) {
	SimpleBLEPeripheral_enqueueMsg(P_CHAR_CHANGE_EVT, paramID, NULL);
}
/*********************************************************************
 * @fn      Keys_EventCB
 *
 * @brief   Callback from Keys task indicating a role state change.
 *
 * @param   notificationType - type of button press
 *
 * @return  None.
 */
static void Keys_EventCB(keys_Notifications_t notificationType) {

	SimpleBLEPeripheral_enqueueMsg(KEY_CHANGE_EVT, (uint8) notificationType,
	NULL);

}
/*********************************************************************
 * @fn      Keys_EventCB
 *
 * @brief   Callback from peripheralObserver task indicating a connection parameter change.
 *
 * @param   notificationType - type of button press
 *
 * @return  None.
 */
static void updateConnParam_CB(uint16_t connInterval, uint16_t connSlaveLatency, uint16_t connTimeout){

	if( (connInterval >=  DEFAULT_DESIRED_MIN_CONN_INTERVAL &&  connInterval <=  DEFAULT_DESIRED_MAX_CONN_INTERVAL) && connSlaveLatency==DEFAULT_DESIRED_SLAVE_LATENCY && connTimeout==DEFAULT_DESIRED_CONN_TIMEOUT){

	}

	//move outside the if for safety...
	ready = TRUE;
	Util_rescheduleClock(&preCEClock, (connInterval * 1250)/1000-10 );

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
static void Climb_contactsCheckSendThroughGATT(void) {
	//INVIA I CONTATTI RADIO AVVENUTI NELL'ULTIMO PERIODO TRAMITE GAT
	//NB: SE ANDROID LIMITA A 4 PPCE LA LUNGHEZZA MASSIMA PER UNA CARATTERISTICA E' 3*27+20 byte = 101 byte = 33 nodi
	// ATTENZIONE NON USARE L'MTU IMPOSTATO NEI PREPROCESSOR DEFINES MA QUELLO OTTENUTO TRAMITE LE API O GLI EVENTI

	//INVIA I CONTATTI RADIO AVVENUTI NELL'ULTIMO PERIODO TRAMITE GATT
	//uint8 childrenNodesData[mtu_size - 3];
	//uint8* childrenNodesData = (uint8*) ICall_malloc((mtu_size - 3));
	uint16 size = mtu_size - 3;
	uint8* childrenNodesData = (uint8 *) GATT_bm_alloc(0, ATT_HANDLE_VALUE_NOTI, GATT_MAX_MTU, &size );
	uint8 i = 0;
	uint8 roundCompleted = FALSE;
	//	listNode_t* node = childListRootPtr;
	//	listNode_t* previousNode = NULL;
	uint8 gattUpdateReq = FALSE;
	uint8 nodeArrayIndex = gatt_startNodeIndex;

	attHandleValueNoti_t noti;
	//noti.handle = climbProfileAttrTbl[2].handle;
	//noti.len = (uint16) len;
	noti.pValue = childrenNodesData;	//(uint8 *) GATT_bm_alloc(0, ATT_HANDLE_VALUE_NOTI, GATT_MAX_MTU, (uint16*) (&len));


	if (childrenNodesData != NULL) { //if allocated
		while (i < mtu_size - 5 && roundCompleted == FALSE) {

			if (isNonZero(childListArray[nodeArrayIndex].id, CHILD_NODE_ID_LENGTH)) { //discard empty spaces

				if (childListArray[nodeArrayIndex].contactSentThoughGATT == FALSE) {				// //invia solo i nodi visti dopo l'ultimo check, invia tutti i nodi CLIMBC
					memcpy(&childrenNodesData[i], childListArray[nodeArrayIndex].id, CHILD_NODE_ID_LENGTH); //salva l'id del nodo
					i += CHILD_NODE_ID_LENGTH;
					childrenNodesData[i++] = childListArray[nodeArrayIndex].state;
#ifdef INCLUDE_RSSI_IN_GATT_DATA
					childrenNodesData[i++] = childListArray[nodeArrayIndex].rssi;
#endif
					childListArray[nodeArrayIndex].contactSentThoughGATT = TRUE;
					gattUpdateReq = TRUE;
				}
			}

			nodeArrayIndex++; //passa al nodo sucessivo

			if (nodeArrayIndex >= MAX_SUPPORTED_CHILD_NODES) {
				nodeArrayIndex = 0;
			}

			if (nodeArrayIndex == gatt_startNodeIndex) {
				roundCompleted = TRUE;
			}
		}
		gatt_startNodeIndex = nodeArrayIndex;

		noti.len = i;

		if (gattUpdateReq) {
			//azzero i byte successivi
			//			while (i < 20) {
			//				childrenNodesData[i++] = 0;
			//			}

			if (ClimbProfile_SetParameter(CLIMBPROFILE_CHAR1, i, &noti) != SUCCESS) {
				PIN_setOutputValue(hGpioPin, Board_LED1, Board_LED_ON);
				PIN_setOutputValue(hGpioPin, Board_LED2, Board_LED_ON);
				GATT_bm_free((gattMsg_t *) &noti, ATT_HANDLE_VALUE_NOTI);
			}
			gattUpdateReq = FALSE;
		}else{
			GATT_bm_free((gattMsg_t *) &noti, ATT_HANDLE_VALUE_NOTI);
		}
		//ICall_free(childrenNodesData);
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

	newChildrenStatesData[0] = 0x07, // length of this data
	newChildrenStatesData[1] = GAP_ADTYPE_LOCAL_NAME_COMPLETE, newChildrenStatesData[2] = defAdvertData[2];
	newChildrenStatesData[3] = defAdvertData[3];
	newChildrenStatesData[4] = defAdvertData[4];
	newChildrenStatesData[5] = defAdvertData[5];
	newChildrenStatesData[6] = defAdvertData[6];
	newChildrenStatesData[7] = defAdvertData[7];
	//newChildrenStatesData[8] = i-9; //assigned later!!!
	newChildrenStatesData[9] = GAP_ADTYPE_MANUFACTURER_SPECIFIC; // manufacturer specific adv data type
	newChildrenStatesData[10] = 0x0D; // Company ID - Fixed //VERIFICARE SE QUESTA REGOLA VALE ANCHE PER I TAG NON COMPATIBILI CON IBEACON
	newChildrenStatesData[11] = 0x00; // Company ID - Fixed

	if (isBroadcastMessageValid == FALSE) {
		uint8 nodeArrayIndex = adv_startNodeIndex;
		uint8 roundCompleted = FALSE;

		while (i < 29 && roundCompleted == FALSE) {

			if (isNonZero(childListArray[nodeArrayIndex].id, CHILD_NODE_ID_LENGTH)) { //discard empty spaces

				memcpy(&newChildrenStatesData[i], childListArray[nodeArrayIndex].id, CHILD_NODE_ID_LENGTH);
				i += CHILD_NODE_ID_LENGTH;
				newChildrenStatesData[i++] = childListArray[nodeArrayIndex].stateToImpose;

			}

			nodeArrayIndex++;

			if(nodeArrayIndex >= MAX_SUPPORTED_CHILD_NODES){
				nodeArrayIndex = 0;
			}

			if(nodeArrayIndex == adv_startNodeIndex){
				roundCompleted = TRUE;
			}

		}
		adv_startNodeIndex = nodeArrayIndex;

	} else { //send broadcast msg
		memcpy(&newChildrenStatesData[i], broadcastID, CHILD_NODE_ID_LENGTH);
		i += CHILD_NODE_ID_LENGTH;

		uint8 broadcastMsgType = broadcastMessage[0];
		switch (broadcastMsgType) {
		case BROADCAST_MSG_TYPE_STATE_UPDATE_CMD:
			memcpy(&newChildrenStatesData[i], &broadcastMessage[0], BROADCAST_MSG_LENGTH_STATE_UPDATE_CMD);
			i = i + BROADCAST_MSG_LENGTH_STATE_UPDATE_CMD;
			break;

		case BROADCAST_MSG_TYPE_WU_SCHEDULE_CMD:
			memcpy(&newChildrenStatesData[i], &broadcastMessage[0], BROADCAST_MSG_LENGTH_WU_SCHEDULE_CMD);
			i = i + BROADCAST_MSG_LENGTH_WU_SCHEDULE_CMD;
			break;

		default: //should not reach here
			break;
		}

	}
#ifdef CLIMB_DEBUG
//	while (i < 30) {
//		newChildrenStatesData[i++] = 0;
//	}
	newChildrenStatesData[i++] = adv_counter; //the counter is always in the last position
#endif

	newChildrenStatesData[8] = i - 9;
	GAP_UpdateAdvertisingData(selfEntity, true, i, &newChildrenStatesData[0]);   //adv data
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

	uint8_t newValue[CLIMBPROFILE_CHAR2_LEN];

	switch (paramID) {
	case CLIMBPROFILE_CHAR1: {
		ClimbProfile_GetParameter(CLIMBPROFILE_CHAR1, newValue);

		break;
	}
	case CLIMBPROFILE_CHAR2: {
		ClimbProfile_GetParameter(CLIMBPROFILE_CHAR2, newValue);

		uint8 i = 0;
		while (i < CLIMBPROFILE_CHAR2_LEN-4) { //TODO: cambiare CLIMBPROFILE_CHAR2_LEN con la lunghezza effettiva dell'operazione di scrittura, per ora risolto con il break nelle righe successive
			uint8 nodeID[CHILD_NODE_ID_LENGTH];
			uint32 wakeUpTimeout_sec_local;

			memcpy(nodeID, &newValue[i], CHILD_NODE_ID_LENGTH);

			if(memcomp(nodeID, zeroID, CHILD_NODE_ID_LENGTH) == 0){ //if it find a zero ID stop checking!
				break;
			}

			if (memcomp(nodeID, broadcastID, CHILD_NODE_ID_LENGTH) == 0) { 	//broadcastID found! ONLY ONE BROADCAST MSG PER NOTIFICATION (PER GATT PACKET)

				isBroadcastMessageValid = TRUE;
				uint8 broadcastMsgType = newValue[i + CHILD_NODE_ID_LENGTH];

				Util_restartClock(&resetBroadcastCmdClock, RESET_BROADCAST_CMD_TIMEOUT);

				switch (broadcastMsgType) {
				case BROADCAST_MSG_TYPE_STATE_UPDATE_CMD:
					memcpy(broadcastMessage, &newValue[i + CHILD_NODE_ID_LENGTH], BROADCAST_MSG_LENGTH_STATE_UPDATE_CMD);
					//i = i + NODE_ID_LENGTH + BROADCAST_MSG_LENGTH_STATE_UPDATE_CMD;
					break;

				case BROADCAST_MSG_TYPE_WU_SCHEDULE_CMD:
					memcpy(broadcastMessage, &newValue[i + CHILD_NODE_ID_LENGTH], BROADCAST_MSG_LENGTH_WU_SCHEDULE_CMD);
					wakeUpTimeout_sec_local = ((newValue[i + CHILD_NODE_ID_LENGTH + 1])<<16) + ((newValue[i + CHILD_NODE_ID_LENGTH + 2])<<8) + (newValue[i + CHILD_NODE_ID_LENGTH + 3]);
					Climb_setWakeUpClock( wakeUpTimeout_sec_local );
					//i = i + NODE_ID_LENGTH + BROADCAST_MSG_LENGTH_WU_SCHEDULE_CMD;
					break;

				case BROADCAST_RESET_CMD_FLAG_CMD:
					//no message to be copied or to be send
					isBroadcastMessageValid = FALSE;
					//i = i + NODE_ID_LENGTH + BROADCAST_LENGTH_RESET_CMD_FLAG_CMD;
					break;

				default: //should not reach here
					//i = i + NODE_ID_LENGTH; //this is to avoid infinite loops in case of wrong packet formatting
					break;
				}

				return; //ONLY ONE BROADCAST MSG PER NOTIFICATION (PER GATT PACKET)

			} else { //broadcastID not found

				uint8 index = Climb_findNodeById(nodeID, CLIMB_CHILD_NODE);
				if (index != 255) {
					if (newValue[i + CHILD_NODE_ID_LENGTH] != childListArray[index].state) {
						if (newValue[i + CHILD_NODE_ID_LENGTH] != INVALID_STATE) {
							childListArray[index].stateToImpose = (ChildClimbNodeStateType_t) newValue[i + CHILD_NODE_ID_LENGTH]; //the correctness of this will be checked in Climb_advertisedStatesCheck
						}
					}
				}else{
					//TODO:ADD THE NODE FROM HERE, SO IF IT IS NOT IN THE LIST THE REQUEST IS NOT DISCARDED
				}

			}
			i = i + CHILD_NODE_ID_LENGTH + 1;
		}
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
		memcpy(myAddr, pEvent->initDone.devAddr, B_ADDR_LEN); //salva l'indirizzo del nodo

#ifdef FEATURE_LCD
		char buf[10];
		sprintf(buf,"Me: ");
		devpkLcdText(buf, 1, 0);
		sprintf(buf,Util_convertBdAddr2Str(childDevList[0].devRec.addr));
		devpkLcdText(buf, 1, 5);
#endif
	}
		break;

	case GAP_DEVICE_INFO_EVENT: {

		devicesHeardDuringLastScan++;

		if (pEvent->deviceInfo.eventType == GAP_ADRPT_ADV_SCAN_IND | //adv data event (Scannable undirected)
				pEvent->deviceInfo.eventType == GAP_ADRPT_ADV_IND | pEvent->deviceInfo.eventType == GAP_ADRPT_ADV_NONCONN_IND) { //adv data event (Connectable undirected)

			ClimbNodeType_t nodeType = isClimbNode((gapDeviceInfoEvent_t*) &pEvent->deviceInfo);
			if (nodeType == CLIMB_CHILD_NODE || nodeType == CLIMB_MASTER_NODE) {
				Climb_addNodeDeviceInfo(&pEvent->deviceInfo, nodeType);
				if (nodeType == CLIMB_CHILD_NODE) {
					//Climb_contactsCheckSendThroughGATT();
#ifdef PRINTF_ENABLED
#ifndef HEAPMGR_METRICS
					Climb_printfNodeInfo(&pEvent->deviceInfo);
#endif
#endif
				}

			} else {


			}
		} else if (pEvent->deviceInfo.eventType == GAP_ADRPT_SCAN_RSP) {  //scan response data event

		}

		if (devicesHeardDuringLastScan >= DEFAULT_MAX_SCAN_RES) {
			if (BLE_connected) {
				GAPObserverRole_CancelDiscovery();
				scanning = FALSE;
				if (!scanning) {
					uint8 status = GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE, DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);
					scanning = TRUE;
				}
			}
		}

	}
		break;

	case GAP_DEVICE_DISCOVERY_EVENT:

	{
		devicesHeardDuringLastScan = 0;
#ifndef WORKAROUND
		scanning = FALSE;
		if (BLE_connected) {
			CLIMB_FlashLed(Board_LED2);
		}
#endif
	}
		break;
	case GAP_ADV_DATA_UPDATE_DONE_EVENT:
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

	if (gapDeviceInfoEvent_a->eventType == GAP_ADRPT_ADV_SCAN_IND | gapDeviceInfoEvent_a->eventType == GAP_ADRPT_ADV_IND
			| gapDeviceInfoEvent_a->eventType == GAP_ADRPT_ADV_NONCONN_IND) { //il nome � contenuto solo dentro i pacchetti di advertise, � inutile cercarli dentro le scan response

		while (index < gapDeviceInfoEvent_a->dataLen) {
			if (gapDeviceInfoEvent_a->pEvtData[index + 1] == GAP_ADTYPE_LOCAL_NAME_COMPLETE) { //ho trovato il nome del dispositivo

				if (memcomp(&(gapDeviceInfoEvent_a->pEvtData[index + 2]), &Climb_childNodeName[0], (gapDeviceInfoEvent_a->pEvtData[index]) - 1) == 0) { //il nome � compatibile con CLIMB

					return CLIMB_CHILD_NODE;

				} else if (memcomp(&(gapDeviceInfoEvent_a->pEvtData[index + 2]), &(defAdvertData[2]), (gapDeviceInfoEvent_a->pEvtData[index]) - 1) == 0) { //TODO: verificare...
					return CLIMB_MASTER_NODE;
				} else {
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
static void Climb_addNodeDeviceInfo(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType) {

	uint8 node_position = 255;

	if(nodeType == CLIMB_CHILD_NODE){
		node_position = Climb_findNodeById(&gapDeviceInfoEvent->pEvtData[ADV_PKT_ID_OFFSET], nodeType);
	}else if(CLIMB_MASTER_NODE){
		node_position = Climb_findNodeById(gapDeviceInfoEvent->addr, nodeType);
		return; //for now a master don't need to store other master nodes data, this saves some memory....
	}

	if(node_position == 255){	//dispositivo nuovo, aggiungilo!
		Climb_addNode(gapDeviceInfoEvent,nodeType);
	}else{
		Climb_updateNodeMetadata(gapDeviceInfoEvent,node_position,nodeType);
	}

	return;
}

/*********************************************************************
 * @fn      Climb_findNodeByDevice
 *
 * @brief	Find the node inside child or master lists and returns the node pointer. If the node cannot be found this function returns null. If the list is empty it returns null.
 *
 * @return  Pointer to node instance that can be used to modify stored data
 */

//static listNode_t* Climb_findNodeByDevice(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType) {
//
//	listNode_t* node = NULL;
//	if (nodeType == CLIMB_CHILD_NODE) {
//		if (childListRootPtr == NULL) {
//			return NULL;
//		}
//		node = childListRootPtr;
//	} else if (nodeType == CLIMB_MASTER_NODE) {
//		if (masterListRootPtr == NULL) {
//			return NULL;
//		}
//		node = masterListRootPtr;
//	}
//
//	while (node != NULL) {
//		if (memcomp(gapDeviceInfoEvent->addr, node->device.addr, B_ADDR_LEN) == 0) {
//			return node; //se trovi il nodo esci e passane il puntatore
//		}
//		node = node->next; //passa al nodo sucessivo
//	}
//
//	return NULL;
//}

/*********************************************************************
 * @fn      Climb_findChildNodeById
 *
 * @brief	Find the node inside child lists (it uses only the ID information) and returns the node pointer. If the node cannot be found this function returns null. If the list is empty it returns null.
 *
 * @return  none
 */
static uint8 Climb_findNodeById(uint8 *nodeID, ClimbNodeType_t nodeType) {

	uint8 i;
	if (nodeType == CLIMB_CHILD_NODE) {

		for (i = 0; i < MAX_SUPPORTED_CHILD_NODES; i++) {
			if (memcomp(nodeID, childListArray[i].id, CHILD_NODE_ID_LENGTH) == 0) {
				return i;
			}
		}

	} else if (nodeType == CLIMB_MASTER_NODE) {
		for (i = 0; i < MAX_SUPPORTED_MASTER_NODES; i++) {
			if (memcomp(nodeID, masterListArray[i].id, MASTER_NODE_ID_LENGTH) == 0) {
				return i;
			}
		}

	}
	return 255;

}

/*********************************************************************
 * @fn      Climb_addNode
 *
 * @brief	Adds a node at the end of the child or master list with informations contained in the gap info event. It automatically manages the adding of first node which is critical because it is referenced by childListRootPtr or masterListRootPtr
 *
 * @return  The pointer to the node istance inside the list
 */
static uint8 Climb_addNode(gapDeviceInfoEvent_t *gapDeviceInfoEvent, ClimbNodeType_t nodeType) {

	uint8 freePos = 0;

	if (nodeType == CLIMB_CHILD_NODE) {

		while( isNonZero(childListArray[freePos].id, CHILD_NODE_ID_LENGTH) ){
			freePos++;

			if(freePos >= MAX_SUPPORTED_CHILD_NODES){
				return 255;
			}

		}


		childListArray[freePos].rssi = gapDeviceInfoEvent->rssi;
		childListArray[freePos].lastContactTicks = Clock_getTicks();

		memcpy(childListArray[freePos].id, &gapDeviceInfoEvent->pEvtData[ADV_PKT_ID_OFFSET], CHILD_NODE_ID_LENGTH);
		childListArray[freePos].state = (ChildClimbNodeStateType_t) gapDeviceInfoEvent->pEvtData[ADV_PKT_STATE_OFFSET];
		childListArray[freePos].stateToImpose = childListArray[freePos].state;
		childListArray[freePos].contactSentThoughGATT = FALSE;

		return 1;
	} else if (nodeType == CLIMB_MASTER_NODE) {

		while( isNonZero(masterListArray[freePos].id, MASTER_NODE_ID_LENGTH) ){
			freePos++;

			if(freePos >= MAX_SUPPORTED_MASTER_NODES){
				return 255;
			}

		}

		masterListArray[freePos].rssi = gapDeviceInfoEvent->rssi;
		masterListArray[freePos].lastContactTicks = Clock_getTicks();

		memcpy(masterListArray[freePos].id, &gapDeviceInfoEvent->addr, MASTER_NODE_ID_LENGTH);
		masterListArray[freePos].state = INVALID_STATE;
		masterListArray[freePos].stateToImpose = masterListArray[freePos].state;
		masterListArray[freePos].contactSentThoughGATT = FALSE;

		return 1;
	}
	return 0;

}

/*********************************************************************
 * @fn      Climb_updateNodeMetadata
 *
 * @brief	updates targetNode metadata with infomation contained in gapDeviceInfoEvent
 *
 * @return  none
 */
static void Climb_updateNodeMetadata(gapDeviceInfoEvent_t *gapDeviceInfoEvent, uint8 index, ClimbNodeType_t nodeType) {

	if (gapDeviceInfoEvent->eventType == GAP_ADRPT_ADV_SCAN_IND | gapDeviceInfoEvent->eventType == GAP_ADRPT_ADV_IND | gapDeviceInfoEvent->eventType == GAP_ADRPT_ADV_NONCONN_IND) {//adv data

		if (nodeType == CLIMB_CHILD_NODE) {

			childListArray[index].rssi = gapDeviceInfoEvent->rssi;
			childListArray[index].lastContactTicks = Clock_getTicks();

			memcpy(childListArray[index].id, &gapDeviceInfoEvent->pEvtData[ADV_PKT_ID_OFFSET], CHILD_NODE_ID_LENGTH);
			childListArray[index].state = (ChildClimbNodeStateType_t) gapDeviceInfoEvent->pEvtData[ADV_PKT_STATE_OFFSET];
			childListArray[index].contactSentThoughGATT = FALSE;

			return;

		} else if (nodeType == CLIMB_MASTER_NODE) {

			masterListArray[index].rssi = gapDeviceInfoEvent->rssi;
			masterListArray[index].lastContactTicks = Clock_getTicks();

			memcpy(masterListArray[index].id, &gapDeviceInfoEvent->addr, MASTER_NODE_ID_LENGTH);
			masterListArray[index].state = INVALID_STATE;
			masterListArray[index].contactSentThoughGATT = FALSE;

			return;

		}

	} else if (gapDeviceInfoEvent->eventType == GAP_ADRPT_SCAN_RSP) {	//scan response data

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
static void Climb_advertisedStatesCheck(void) {
	//FA IL CONTROLLO DELLO STATO ATTUALE RISPETTO A QUELLO VOLUTO
	//listNode_t* node = childListRootPtr;
	onBoardChildren = 0;
	uint8 i = 0;
	while (i < MAX_SUPPORTED_CHILD_NODES) {

		if ( isNonZero(childListArray[i].id,CHILD_NODE_ID_LENGTH) ) { //discard empty spaces

			if (isBroadcastMessageValid == TRUE && broadcastMessage[0] == BROADCAST_MSG_TYPE_STATE_UPDATE_CMD) { //SE IL MESSAGGIO DI BROADCAST E' VALIDO SOVRASCRIVI IL CAMPO stateToImpose su tutti i nodi

				if ((ChildClimbNodeStateType_t) broadcastMessage[1] != INVALID_STATE) {
					childListArray[i].stateToImpose = (ChildClimbNodeStateType_t) broadcastMessage[1];
				}

			}

//		// se il nodo � BY_MYSELF non ha senso cercare di imporre uno stato inviato come broadcast, quindi si pu� sovrascrivere
//		if (node->device.advData[ADV_PKT_STATE_OFFSET] == BY_MYSELF && Climb_isMyChild(node->device.advData[ADV_PKT_ID_OFFSET])) {
//
//			node->device.stateToImpose = CHECKING;
//
//		}

			{ //CHECK IF THE REQUESTED STATE CHANGE IS VALID OR NOT
				ChildClimbNodeStateType_t actualNodeState = childListArray[i].state; //(ChildClimbNodeStateType_t)node->device.advData[ADV_PKT_STATE_OFFSET];
				switch (childListArray[i].stateToImpose) {
				case BY_MYSELF:
					if (actualNodeState == CHECKING || actualNodeState == ON_BOARD || actualNodeState == ALERT || actualNodeState == GOING_TO_SLEEP) { // && Climb_isMyChild(node->device.advData[ADV_PKT_ID_OFFSET])) {
					//allowed transition
					} else {
						childListArray[i].stateToImpose = actualNodeState; //mantieni lo stato precedente
					}
					break;

				case CHECKING:
					if (actualNodeState == BY_MYSELF) { // && Climb_isMyChild(node->device.advData[ADV_PKT_ID_OFFSET])) {
						//allowed transition
					} else {
						childListArray[i].stateToImpose = actualNodeState; //mantieni lo stato precedente
					}
					break;

				case ON_BOARD:
					if ((actualNodeState == CHECKING || actualNodeState == ALERT)) { // && Climb_isMyChild(node->device.advData[ADV_PKT_ID_OFFSET])) {
						//allowed transition
					} else {
						childListArray[i].stateToImpose = actualNodeState; //mantieni lo stato precedente
					}
					break;

				case ALERT:
					childListArray[i].stateToImpose = ON_BOARD; //don't broadcast ALERT state!
					break;

				case GOING_TO_SLEEP:
					if (actualNodeState == CHECKING || actualNodeState == ON_BOARD) { // && Climb_isMyChild(node->device.advData[ADV_PKT_ID_OFFSET])) {
					//allowed transition
					} else {
						childListArray[i].stateToImpose = actualNodeState; //mantieni lo stato precedente
					}
					break;

				case INVALID_STATE:
					break;

				default:
					break;
				}

				if (actualNodeState != childListArray[i].stateToImpose) {
					advUpdateReq = TRUE;
				}

				if (actualNodeState == ON_BOARD) {
					onBoardChildren++;
				}

			}

		}
		i++; //passa al nodo sucessivo
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

	uint32 nowTicks = Clock_getTicks();

	uint8 i = 0;
	//controlla la lista dei child
	//listNode_t* targetNode = childListRootPtr;
	//listNode_t* previousNode = NULL;
	while (i < MAX_SUPPORTED_CHILD_NODES) { //NB: ENSURE targetNode IS UPDATED ANY CYCLE, OTHERWISE IT RUNS IN AN INFINITE LOOP
		if (isNonZero(childListArray[i].id, CHILD_NODE_ID_LENGTH)) { //discard empty spaces

			if (nowTicks - childListArray[i].lastContactTicks > NODE_TIMEOUT_OS_TICKS) {
				switch ((ChildClimbNodeStateType_t) childListArray[i].stateToImpose) {
				case BY_MYSELF:
				case CHECKING:
					Climb_removeNode(i,CLIMB_CHILD_NODE); //rimuovi il nodo
					break;

				case ON_BOARD:
					//do nothing, app will trigger the alert!!
				case ALERT:
					//do nothing
					childListArray[i].state = ALERT;
					break;
				case GOING_TO_SLEEP:
					//do nothing
					Climb_removeNode(i,CLIMB_CHILD_NODE); //rimuovi il nodo
					break;

				default:
					//should not reach here
					break;
				}
			} else { //il nodo non � andato in timeout,

			}
			//NB: ENSURE targetNode IS UPDATED ANY CYCLE, OTHERWISE IT RUNS IN AN INFINITE LOOP

		}
		i++;
	}


	//controlla la lista dei master
	//targetNode = masterListRootPtr;
	//previousNode = NULL;
	i = 0;
	while (i < MAX_SUPPORTED_MASTER_NODES) { //NB: ENSURE targetNode IS UPDATED ANY CYCLE, OTHERWISE IT RUNS IN AN INFINITE LOOP
		if (isNonZero(masterListArray[i].id, CHILD_NODE_ID_LENGTH)) { //discard empty spaces
			if (nowTicks - masterListArray[i].lastContactTicks > NODE_TIMEOUT_OS_TICKS) { //se il timeout � scaduto
				Climb_removeNode(i,CLIMB_MASTER_NODE); //rimuovi il nodo
				advUpdateReq = TRUE;
			} else { //il nodo non � andato in timeout,

			}
			//NB: ENSURE targetNode IS UPDATED ANY CYCLE, OTHERWISE IT RUNS IN AN INFINITE LOOP
		}

		i++;
	}
}

/*********************************************************************
 * @fn      Climb_removeNode
 *
 * @brief	Removes nodeToRemove node form the list which contains it
 *
 * @return  pointer to the next node
 */
static void Climb_removeNode(uint8 indexToRemove, ClimbNodeType_t nodeType) {

	uint8 i = 0;

	for (i = 0; i < sizeof(myGapDevRec_t); i++) {

		if (nodeType == CLIMB_CHILD_NODE) {

			((uint8*) (&childListArray[indexToRemove]))[i] = 0;

		} else if (nodeType == CLIMB_MASTER_NODE) {

			((uint8*) (&masterListArray[indexToRemove]))[i] = 0;

		}

	}

	return;
}

/*********************************************************************
 * @fn      Climb_periodicTask
 *
 * @brief	Handler associated with Periodic Clock instance.
 *
 * @return  none
 */
static void Climb_periodicTask() {
	Climb_nodeTimeoutCheck();


#ifdef  HEAPMGR_METRICS
	plotHeapMetrics();
#endif

}

#ifdef PRINTF_ENABLED
#ifndef HEAPMGR_METRICS
static void Climb_printfNodeInfo(gapDeviceInfoEvent_t *gapDeviceInfoEvent ){
	static uint8 usbPktsCounter = 0;
	uint32 nowTicks = Clock_getTicks();
	System_printf("%d ", nowTicks);
	//System_printf(Util_convertBdAddr2Str(myAddr));
	System_printf(Util_convertBdAddr2Str(gapDeviceInfoEvent->addr));
	System_printf(" CLIMBC ADV %02x %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",usbPktsCounter++, gapDeviceInfoEvent->pEvtData[12],gapDeviceInfoEvent->pEvtData[13],gapDeviceInfoEvent->pEvtData[14],gapDeviceInfoEvent->pEvtData[15],gapDeviceInfoEvent->pEvtData[16],gapDeviceInfoEvent->pEvtData[17],gapDeviceInfoEvent->pEvtData[18],gapDeviceInfoEvent->pEvtData[19],gapDeviceInfoEvent->pEvtData[20],gapDeviceInfoEvent->pEvtData[21],gapDeviceInfoEvent->pEvtData[22],gapDeviceInfoEvent->pEvtData[23],gapDeviceInfoEvent->pEvtData[24],gapDeviceInfoEvent->pEvtData[25],gapDeviceInfoEvent->pEvtData[26],gapDeviceInfoEvent->pEvtData[27],gapDeviceInfoEvent->pEvtData[28],gapDeviceInfoEvent->pEvtData[29],gapDeviceInfoEvent->pEvtData[30] );
	//System_printf(" CLIMBD ADV %02x %02x%02x%02x\n",usbPktsCounter++, gapDeviceInfoEvent->pEvtData[12] ,gapDeviceInfoEvent->pEvtData[30] );

}
#endif
#endif
#ifdef WORKAROUND
/*********************************************************************
 * @fn      Climb_epochStartHandler
 *
 * @brief	Handler associated with epoch clock instance.
 *
 * @return  none
 */
static void Climb_epochStartHandler() {
	if(beaconActive) {
		if(thisNodeState == CONNECTED) {
			GAPObserverRole_CancelDiscovery();
			uint8 adv_active = 1;
			uint8 status = GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t),&adv_active);
			GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE,DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);

		} else {
			uint8 adv_active = 1;
			uint8 status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t),&adv_active);
		}
	}
}
#endif


static void Climb_preAdvEvtHandler(){
#ifdef CLIMB_DEBUG  //force adv data update to include the new value of adv_counter
	if (BLE_connected) {
		advUpdateReq = true;
		adv_counter++;
	}
#endif

	if (advUpdateReq) {
		Climb_advertisedStatesCheck(); //aggiorna l'adv
		Climb_advertisedStatesUpdate();
		advUpdateReq = false;
	}

	if (BLE_connected) {
		GAPObserverRole_CancelDiscovery();
		scanning = FALSE;
		if (!scanning) {
			uint8 status = GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE, DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);
			scanning = TRUE;
		}
	}
}

static void Climb_preCEEvtHandler(){
	Climb_contactsCheckSendThroughGATT();
}
/*********************************************************************
 * @fn      Climb_goToSleepHandler
 *
 * @brief	Handler associated with goToSleep Clock instance.
 *
 * @return  none
 */
static void Climb_goToSleepHandler(){

	if(onBoardChildren == 0){
		stopNode();
	}else{
		Util_restartClock(&goToSleepClock, GOTOSLEEP_POSTPONE_INTERVAL_SEC*1000);
	}

}

/*********************************************************************
 * @fn      Climb_wakeUpHandler
 *
 * @brief	Handler associated with wakeUp Clock instance.
 *
 * @return  none
 */
static void Climb_wakeUpHandler(){

	if(wakeUpTimeout_sec_global == 0){
		startNode();
		Util_restartClock(&goToSleepClock, GOTOSLEEP_DEFAULT_TIMEOUT_SEC*1000);
		Climb_setWakeUpClock(WAKEUP_DEFAULT_TIMEOUT_SEC);

		return;
	}

	if(wakeUpTimeout_sec_global > MAX_ALLOWED_TIMER_DURATION_SEC){
		Util_restartClock(&wakeUpClock, MAX_ALLOWED_TIMER_DURATION_SEC*1000);
		wakeUpTimeout_sec_global = wakeUpTimeout_sec_global - MAX_ALLOWED_TIMER_DURATION_SEC;
	}else{
		//randomizza nell'intorno +/-1 secondo rispetto al valore prestabilito
		//float randDelay_msec = 2000 * ((float) Util_GetTRNG()) / 4294967296;
		Util_restartClock(&wakeUpClock, wakeUpTimeout_sec_global*1000);
		wakeUpTimeout_sec_global = 0; //reset this so that when the device wakes up, it knows that there is no need to restart timer but it is the actual time to wake up the device
	}

}
/*********************************************************************
 * @fn      Climb_setWakeUpClock
 *
 * @brief	Helper function that sets the wake up clock, it manages the clock overflow for intervals larger than MAX_ALLOWED_TIMER_DURATION_SEC
 *
 * @return  none
 */
static void Climb_setWakeUpClock(uint32 wakeUpTimeout_sec_local){

	if(wakeUpTimeout_sec_local > MAX_ALLOWED_TIMER_DURATION_SEC){ //max timer duration 42949.67sec
		Util_restartClock(&wakeUpClock, MAX_ALLOWED_TIMER_DURATION_SEC*1000);
		wakeUpTimeout_sec_global = wakeUpTimeout_sec_local - MAX_ALLOWED_TIMER_DURATION_SEC;
	}else{
		//randomizza nell'intorno +/-1 secondo rispetto al valore prestabilito
		float randDelay_msec = 2000 * ((float) Util_GetTRNG()) / 4294967296;
		Util_restartClock(&wakeUpClock, wakeUpTimeout_sec_local*1000 - 1000 + randDelay_msec);
		wakeUpTimeout_sec_global = 0; //reset this so that when the device wakes up, it knows that there is no need to restart timer but it is the actual time to wake up the device
	}

}




/*********************************************************************
 * @fn      CLIMB_FlashLed
 *
 * @brief   Turn on a led and start the timer that switch it off;
 *
 * @param   pinId - the led id to turn on

 * @return  none
 */
static void CLIMB_FlashLed(PIN_Id pinId) {

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
static void CLIMB_handleKeys(uint8 keys) {

	switch ((keys_Notifications_t) keys) {
	case LEFT_SHORT:

		break;

	case RIGHT_SHORT:
		if (beaconActive != 1) {
//			startNode();
//
//			Climb_setWakeUpClock(WAKEUP_DEFAULT_TIMEOUT_SEC);
//			Util_restartClock(&goToSleepClock, GOTOSLEEP_DEFAULT_TIMEOUT_SEC*1000);

			HAL_SYSTEM_RESET();
		}
		break;

	case LEFT_LONG:

		break;

	case RIGHT_LONG:
//		if (beaconActive != 1) {
//			startNode();
//
//			Climb_setWakeUpClock(WAKEUP_DEFAULT_TIMEOUT_SEC);
//			Util_restartClock(&goToSleepClock, GOTOSLEEP_DEFAULT_TIMEOUT_SEC*1000);
//
//
//		} else {
			stopNode();

			//Util_stopClock(&wakeUpClock);
			//Util_stopClock(&goToSleepClock);
		//}
		break;

	case BOTH:
		break;

	default:
		break;
	}
}
/*********************************************************************
 * @fn      startNode
 *
 * @brief   Function to call to start the node.
 *

 * @return  none
 */
static void startNode() {
	if (beaconActive != 1) {

		GPTimerCC26XX_start(hTimer);

		uint8 adv_active = 1;

		uint8 status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_active);

//		lastGATTCheckTicks = Clock_getTicks();

//		GAPRole_StartDiscovery(DEFAULT_DISCOVERY_MODE, DEFAULT_DISCOVERY_ACTIVE_SCAN, DEFAULT_DISCOVERY_WHITE_LIST);
//
//		HCI_EXT_ConnEventNoticeCmd(0, selfEntity, CONN_EVT_END_EVT);
//
		Util_startClock(&periodicClock);
//
//		BLE_connected = TRUE;

		status = HCI_EXT_AdvEventNoticeCmd(selfEntity, ADVERTISE_EVT);
#ifdef WORKAROUND
		Util_startClock(&epochClock);
#endif
		CLIMB_FlashLed(Board_LED2);
		beaconActive = 1;
	}
}

/*********************************************************************
 * @fn      startNode
 *
 * @brief   Function to call to stop the node.
 *

 * @return  none
 */
static void stopNode() {

	uint8 status = HCI_EXT_AdvEventNoticeCmd(selfEntity, 0);

	GPTimerCC26XX_stop(hTimer);

	beaconActive = 0;
	GAPObserverRole_CancelDiscovery();
	scanning = FALSE;
	uint8 adv_active = 0;
	status = GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &adv_active);
	status = GAPRole_SetParameter(GAPROLE_ADV_NONCONN_ENABLED, sizeof(uint8_t), &adv_active);
	GAPRole_TerminateConnection();

	//PIN_setOutputValue(hGpioPin, Board_LED2, Board_LED_OFF);

//	lastGATTCheckTicks = 1;
	uint8 zeroArray[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	ClimbProfile_SetParameter(CLIMBPROFILE_CHAR1, 20, zeroArray);
	Util_stopClock(&periodicClock);

	destroyChildNodeList();
	destroyMasterNodeList();

	Climb_advertisedStatesUpdate();

	CLIMB_FlashLed(Board_LED1);
#ifdef WORKAROUND
	Util_stopClock(&epochClock);
#endif
}
/*********************************************************************
 * @fn      destroyChildNodeList
 *
 * @brief   Destroy child list
 *

 * @return  none
 */
static void destroyChildNodeList() {

	uint8 i;
	for (i = 0; i < MAX_SUPPORTED_CHILD_NODES; i++) {
		if (isNonZero(childListArray[i].id, CHILD_NODE_ID_LENGTH)) { //discard empty spaces

			switch ((ChildClimbNodeStateType_t) childListArray[i].stateToImpose) {
			case BY_MYSELF:
			case CHECKING:
				Climb_removeNode(i, CLIMB_CHILD_NODE); //rimuovi il nodo
				break;

			case ON_BOARD:
				//do nothing
			case ALERT:
				//do nothing
				break;
			case GOING_TO_SLEEP:
				//do nothing
				Climb_removeNode(i, CLIMB_CHILD_NODE); //rimuovi il nodo
				break;

			default:
				//should not reach here
				break;
			}
		}
	}

}

/*********************************************************************
 * @fn      destroyMasterNodeList
 *
 * @brief   Destroy master list
 *

 * @return  none
 */
static void destroyMasterNodeList() {

	uint8 i;
	for(i = 0; i < MAX_SUPPORTED_MASTER_NODES; i++ ){
		Climb_removeNode(i, CLIMB_MASTER_NODE);
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
//static void Key_callback(PIN_Handle handle, PIN_Id pinId)
//{
//
//  SimpleBLEPeripheral_enqueueMsg(KEY_CHANGE_EVT, pinId, NULL);
//
//}
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
static void displayInit(void) {

	// Initialize LCD
	devpkLcdOpen();
}
#endif

static void watchdogTimerInit(){
	GPTimerCC26XX_Params params;
	GPTimerCC26XX_Params_init(&params);
	params.width          = GPT_CONFIG_32BIT;
	params.mode           = GPT_MODE_PERIODIC_UP;
	params.debugStallMode = GPTimerCC26XX_DEBUG_STALL_OFF;
	hTimer = GPTimerCC26XX_open(CC2650_GPTIMER0A, &params);
	if(hTimer == NULL) {
	    //Log_error0("Failed to open GPTimer");
	    //Task_exit();
	}

	Types_FreqHz  freq;
	BIOS_getCpuFreq(&freq);
	GPTimerCC26XX_Value loadVal = freq.lo / 1 - 1; //47999999
	GPTimerCC26XX_setLoadValue(hTimer, loadVal);
	GPTimerCC26XX_registerInterrupt(hTimer, timerCallback, GPT_INT_TIMEOUT);

}

static void timerCallback(GPTimerCC26XX_Handle handle, GPTimerCC26XX_IntMask interruptMask) {

	switch (unclearedWatchdogEvents) {
	case 0:
	case 1:
		events |= WATCHDOG_EVT;
		Semaphore_post(sem);
		break;
	case 2:
		//resetApplication?
		break;
	default:
		//HAL_SYSTEM_RESET();
		//or
		//HCI_EXT_ResetSystemCmd(HCI_EXT_RESET_SYSTEM_HARD);


		break;
	}

	unclearedWatchdogEvents++;
}




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
static uint8_t SimpleBLEPeripheral_enqueueMsg(uint8_t event, uint8_t state, uint8_t *pData) {
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
static void Climb_clockHandler(UArg arg) {
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

#ifdef  HEAPMGR_METRICS
static void plotHeapMetrics(){

#ifdef PRINTF_ENABLED
	System_printf("Metrics:\n");
#endif
	uint16_t pBlkMax[1];
	uint16_t pBlkCnt[1];
	uint16_t pBlkFree[1];
	uint16_t pMemAlo[1];
	uint16_t pMemMax[1];
	uint16_t pMemUb[1];
	uint16_t pFailAm[1];
	ICall_heapGetMetrics(pBlkMax, pBlkCnt,pBlkFree,pMemAlo,pMemMax,pMemUb,pFailAm);

#ifdef PRINTF_ENABLED
	System_printf("max cnt of all blocks ever seen at once: %d\ncurrent cnt of all blocks: %d\ncurrent cnt of free blocks: %d\ncurrent total memory allocated: %d\nmax total memory ever allocated at once: %d\nupper bound of memory usage: %d\namount of failed malloc calls: %d\n\n\n", pBlkMax[0], pBlkCnt[0], pBlkFree[0],pMemAlo[0], pMemMax[0], pMemUb[0],pFailAm[0] );
#endif

}
#endif
static uint8 isNonZero(uint8 * str1, uint8 len) { //ritorna 0 se il vettore � zero, 1 altrimenti
	uint8 index;
	for (index = 0; index < len; index++) {
		if (str1[index] != 0) {
			return 1;
		}
	}
	return 0;
}
/*********************************************************************
 *********************************************************************/
