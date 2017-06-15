
/***** Includes *****/
#include <assert.h>
#include <Ash500Parser.h>
#include <stdlib.h>
#include <Board.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Task.h>

/* Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/PIN.h>
#include <ti/display/Display.h>
#include <ti/devices/cc13x0/driverlib/rf_data_entry.h>
#include <ti/devices/cc13x0/driverlib/rf_prop_cmd.h>
#include <ti/devices/cc13x0/driverlib/rf_prop_mailbox.h>
#include <ti/devices/cc13x0/driverlib/rf_mailbox.h>

#include "smartrf_settings/smartrf_settings.h"
#include "arraylist.h"

#define RAW_PAYLOAD_LENGTH             24 /* Max length byte the radio will accept */

Display_Handle lcd;
Display_Handle uart;

PIN_Handle pinHandle;
PIN_State pinState;
RF_Object rfObject;
RF_Handle rfHandle;

PIN_Config pinTable[] =
{
    Board_PIN_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    Board_BUTTON1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

extern const Event_Handle applicationEvent;
extern const Task_Handle mainTask;

struct RxQueueItem : public rfc_dataEntry_t
{
    uint8_t data[RAW_PAYLOAD_LENGTH + 4];

    RxQueueItem();
};

struct SensorListEntry
{
    uint8_t serial;
    ratmr_t timestamp;

    bool operator==(const SensorListEntry& other) const;
};

ArrayList<SensorListEntry, 4> sensors;

RxQueueItem rxItem;
dataQueue_t dataQueue =
{
     reinterpret_cast<uint8_t*>(&rxItem),
     NULL
};

rfc_propRxOutput_t rxMetaData;

Ash500Parser sensor;

rfc_CMD_PROP_RX_ADV_t propRxAdvCommand = {
   .commandNo = CMD_PROP_RX_ADV,        //!<        The command ID number 0x3804
   .pNextOp = NULL,                     //!<        Pointer to the next operation to run after this operation is done
   .startTrigger.triggerType = TRIG_NOW,//!<        The type of trigger
   .condition.rule = COND_NEVER,
   .pktConf.bFsOff = 0x0,
   .pktConf.bRepeatOk = 0x0,
   .pktConf.bRepeatNok = 0x0,
   .pktConf.bUseCrc = 0x0,
   .pktConf.bCrcIncSw = 0x0,
   .pktConf.bCrcIncHdr = 0x0,
   .pktConf.endType = 0x0,
   .pktConf.filterOp = 0x0,
   .rxConf.bAutoFlushIgnored = 0x1,
   .rxConf.bAutoFlushCrcErr = 0x0,
   .rxConf.bIncludeHdr = 0x0,
   .rxConf.bIncludeCrc = 0x0,
   .rxConf.bAppendRssi = 0x0,
   .rxConf.bAppendTimestamp = 0x0,
   .rxConf.bAppendStatus = 0x1,
   .syncWord0 = 0x159,
   .syncWord1 = 0x15a,
   .maxPktLen = RAW_PAYLOAD_LENGTH,
   .hdrConf.numHdrBits = 0,
   .hdrConf.lenPos = 0,
   .hdrConf.numLenBits = 0,
   .addrConf.addrType = 0,
   .addrConf.addrSize = 0,
   .addrConf.addrPos = 0,
   .addrConf.numAddr = 0,
   .lenOffset = 0,
   .endTrigger.triggerType = TRIG_NEVER,
   .pAddr = NULL,
   .pQueue = &dataQueue,
   .pOutput = reinterpret_cast<uint8_t*>(&rxMetaData)
};

void dumpMemory(const void* data, uint32_t length);


extern "C" void mainTaskFunction(UArg arg0, UArg arg1)
{
    Display_Params lcdParams;
    Display_Params_init(&lcdParams);
    lcdParams.lineClearMode = DISPLAY_CLEAR_NONE;
    lcd = Display_open(Display_Type_LCD, &lcdParams);
    uart = Display_open(Display_Type_UART, NULL);

    Display_clear(lcd);
    Display_printf(lcd, 0, 0, " %u.%2uMhz scan..",
                   RF_cmdFs.frequency,
                   (((uint32_t)RF_cmdFs.fractFreq) * 100) / 65536);

    Display_printf(uart, 0, 0, "Serial, Temperature, Humidity, RSSI, Timestamp\n");

    RF_Params rfParams;
    RF_Params_init(&rfParams);

    /* Request access to the radio */
    rfHandle = RF_open(&rfObject, &RF_prop, (RF_RadioSetup*)&RF_cmdPropRadioDivSetup, &rfParams);
    assert(rfHandle != NULL);

    /* Set the frequency */
    RF_runCmd(rfHandle, (RF_Op*)&RF_cmdFs, RF_PriorityNormal, NULL, 0);
    assert(RF_cmdFs.status == DONE_OK);

    for (;;)
    {
        rxItem.status = DATA_ENTRY_PENDING;
        RF_EventMask events = RF_runCmd(rfHandle, (RF_Op*)&propRxAdvCommand, RF_PriorityNormal, NULL, 0);

        switch (reinterpret_cast<volatile RF_Op*>(&propRxAdvCommand)->status)
        {
        case PROP_DONE_OK:
            // Wanted case
            break;
        case ERROR_SYNTH_PROG:
            continue;
        default:
            assert(false);
        }

        rfc_propRxStatus_t status;
        memcpy(&status, rxItem.data + RAW_PAYLOAD_LENGTH, sizeof(status));

        System_printf("Sync: %x, RSSI: %i, RAT: %u ", status.status.syncWordId, rxMetaData.lastRssi, rxMetaData.timeStamp);

        uint8_t decodedBytes = Ash500Parser::decodeManchester(rxItem.data, rxItem.data, 20);
        if (decodedBytes != 20)
        {
            System_printf("Manchester violation after %u bytes.\n", decodedBytes);
            System_flush();
            continue;
        }

        /* 1 bit has been used for the sync word. Restore
         * the message before decoding.
         */
        for (int32_t i = 11; i > 0; i--)
        {
            rxItem.data[i] = rxItem.data[i] >> 1;
            rxItem.data[i] |= (rxItem.data[i-1] & 0x01) << 7;
        }
        rxItem.data[0] = rxItem.data[0] >> 1;

        if (status.status.syncWordId == 0)
        {
            rxItem.data[0] |= ((uint8_t)0x01 << 7);
        }

        sensor.parseData(rxItem.data);

        if (sensor.hasErrors() && (sensor.error() == Ash500Parser::ParityError))
        {
            System_printf("Parity error\n");
            System_flush();
            continue;
        }

        System_printf("Id %u, Temp: %u.%u C, Hum: %u\n",
                      sensor.serial(),
                      sensor.temperature() / 10,
                      sensor.temperature() % 10,
                      sensor.humidity());
        System_flush();

        SensorListEntry entry;
        entry.serial = sensor.serial();
        entry.timestamp = rxMetaData.timeStamp;
        int32_t sensorIndex = sensors.indexOf(entry);
        if (sensorIndex == sensors.InvalidIndex)
        {
            sensorIndex = sensors.length();
            sensors.append(entry);
        }
        else
        {
            sensors[sensorIndex] = entry;
        }

        Display_printf(lcd, (sensorIndex * 3) + 3, 0, "%02x: %u.%u C %u%%",
                       sensor.serial(),
                       sensor.temperature() / 10,
                       sensor.temperature() % 10,
                       sensor.humidity());

        Display_printf(lcd, (sensorIndex * 3) + 4, 4, "%idBm",
                       rxMetaData.lastRssi);

        Display_printf(uart, 0, 0, "%u, %u.%u, %u, %i, %u\n",
                       sensor.serial(),
                       sensor.temperature() / 10,
                       sensor.temperature() % 10,
                       sensor.humidity(),
                       rxMetaData.lastRssi,
                       rxMetaData.timeStamp);
    }
}

void handleButtonEvent(PIN_Handle handle, PIN_Id pin)
{
    switch (pin)
    {
    case Board_BUTTON0:
        break;
    case Board_BUTTON1:
        break;
    }
}


void dumpMemory(const void* data, uint32_t length)
{
    System_printf("Raw: ");
    for (uint32_t i = 0; i < length; i++)
    {
        System_printf(BYTE_TO_BINARY_PATTERN" ", BYTE_TO_BINARY(reinterpret_cast<const uint8_t*>(data)[i]));
    }

    System_printf("\n");
}

RxQueueItem::RxQueueItem()
{
    config.lenSz = 0;
    config.type = DATA_ENTRY_TYPE_GEN;
    status = DATA_ENTRY_PENDING;
    pNextEntry = reinterpret_cast<uint8_t*>(this);
    length = sizeof(data);
}

bool SensorListEntry::operator==(const SensorListEntry& other) const
{
    return this->serial == other.serial;
}


int main(void)
{
    /* Call driver init functions. */
    Board_initGeneral();
    Display_init();

    /* Open LED pins */
    pinHandle = PIN_open(&pinState, pinTable);
    assert(pinHandle != NULL);

    PIN_Status status = PIN_registerIntCb(pinHandle, &handleButtonEvent);
    assert(status == PIN_SUCCESS);

    /* Start BIOS */
    BIOS_start();

    return (0);
}

