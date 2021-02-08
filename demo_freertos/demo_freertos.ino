//#define configCHECK_FOR_STACK_OVERFLOW 1 

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <assert.h>

#include "read_write_lock.hpp"
#include "DMLayer.h"

DMLayer* pDMLayer = NULL;

const char pszProducer[] = "THREAD/PRODUCE/VALUE";

const char pszBinConsumer[] = "THREAD/CONSUME/BIN/VALUE";

static int nSID = 0;

#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t) (((TickType_t) (xTimeInMs) * (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000))

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName);

static void Thread_Producer (void* pValue)
{
    noInterrupts();
    size_t nID = nSID++;
    int nRand = 0;
    bool nResponse = false;
    interrupts();

    TickType_t nValue = (TickType_t) pValue;

    for (;;)
    {
        nResponse = false;

        nRand++;

        nResponse = DMLayer_SetNumber (pDMLayer, pszProducer, strlen (pszProducer), nID, (dmlnumber)nRand);

#if 0     
        Serial.print (__FUNCTION__);
        Serial.print (F(", ID: "));
        Serial.print (nID);
        Serial.print (F(", Response: "));
        Serial.print (nResponse);
        Serial.print (F(", Value: "));
        Serial.print (nRand);
        Serial.print (F(", Unused Stack: "));
        Serial.println (uxTaskGetStackHighWaterMark(0));
        Serial.flush ();
#endif

        // Sleep for one second.
        vTaskDelay(pdMS_TO_TICKS (nValue));
    }
}

int nValues[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};



void Consumer_Callback_Notify (DMLayer* pDMLayer, const char* pszVariable, size_t nVarSize, size_t nUserType, uint8_t nNotifyType)
{
    bool nSuccess = false;

    nValues[nUserType] = (int)DMLayer_GetNumber (pDMLayer, pszVariable, nVarSize, &nSuccess);

    //Serial.printf ("->[%s]: Variable: [%*s]\n", __FUNCTION__, (int)nVarSize, pszVariable);

    VERIFY (nSuccess == true, "Error, variable is invalid", );

    //Serial.printf ("->[%s]: from: [%zu], Type: [%u] -> Value: [%u]\n", __FUNCTION__, nUserType, nNotifyType, nValues[nUserType]);

    DMLayer_SetBinary (pDMLayer, pszBinConsumer, strlen (pszBinConsumer), (size_t)nUserType, (void*)nValues, sizeof (nValues));
}

static void Thread_Consumer (void* pValue)
{
    size_t nID = nSID++;

    int nRemoteValues[10];
    int nCount = 0;
    size_t nUserType = 0;

    while (DMLayer_ObserveVariable (pDMLayer, pszBinConsumer, strlen (pszBinConsumer), &nUserType) || true)
    {
        NOTRACE ("[%s] From: [%zu] -> type: [%u - bin: %u], size: [%zu]\n",
                 __FUNCTION__,
                 nUserType,
                 DMLayer_GetVariableType (pDMLayer, pszBinConsumer, strlen (pszBinConsumer)),
                 (uint8_t)VAR_TYPE_BINARY,
                 DMLayer_GetVariableBinarySize (pDMLayer, pszBinConsumer, strlen (pszBinConsumer)));

        if (DMLayer_GetVariableType (pDMLayer, pszBinConsumer, strlen (pszBinConsumer)) == VAR_TYPE_BINARY)
        {
            DMLayer_GetBinary (pDMLayer, pszBinConsumer, strlen (pszBinConsumer), nRemoteValues, sizeof (nRemoteValues));

            noInterrupts(); 

            Serial.printf ("[%s] Values: ", __FUNCTION__);

            for (nCount = 0; nCount < sizeof (nRemoteValues) / sizeof (nRemoteValues[0]); nCount++)
            {
                Serial.printf ("[%u] ", nRemoteValues[nCount]);
            }

            Serial.print (F(", Variables: "));
            Serial.print (DMLayer_PrintVariables (pDMLayer));

            Serial.print (F(", Unused Stack: "));
            Serial.println (uxTaskGetStackHighWaterMark(0));

            interrupts();
        }

        vTaskDelay (pdMS_TO_TICKS (1000));
    }
}

void DMLayer_YieldContext ()
{
    yield ();
    vTaskDelay (10);
}

bool DMLayer_LockInit (DMLayer* pDMLayer)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);

    ReadWriteLock_t* wrlock = CreateReadWriteLockPreferWriter ();

    DMLayer_SetUserData (pDMLayer, (void*)wrlock);

    return true;
}

bool DMLayer_Lock (DMLayer* pDMLayer)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer);

    ::WriterLock (wrlock);

    return true;
}

bool DMLayer_SharedLock (DMLayer* pDMLayer)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer);

    ReaderLock ((ReadWriteLock_t*) wrlock);

    return true;
}

bool DMLayer_Unlock (DMLayer* pDMLayer)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer);

    ::WriterUnlock (wrlock);

    return true;
}

bool DMLayer_SharedUnlock (DMLayer* pDMLayer)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer);

    ReaderUnlock ((ReadWriteLock_t*) wrlock);

    return true;
}

bool DMLayer_LockEnd (DMLayer* pDMLayer)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer);

    ::FreeReadWriteLock (wrlock);

    return true;
}

/**
 * \brief Called if stack overflow during execution
 */
void vApplicationStackOverflowHook(xTaskHandle *pxTask,
		signed char *pcTaskName)
{
	Serial.printf ("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	/* If the parameters have been corrupted then inspect pxCurrentTCB to
	 * identify which task has overflowed its stack.
	 */
	for (;;) {  vTaskDelay (1000); }
}

void setup ()
{
    Serial.begin (115200);

    while (!Serial)
    {
        ;  // wait for serial port to connect. Needed for native USB port only
    }

    delay (2000);
    // prints title with ending line break
    Serial.println ("Starting, wait...");
    Serial.flush ();

    assert ((pDMLayer = DMLayer_CreateInstance ()) != NULL);

    assert (DMLayer_AddObserverCallback (pDMLayer, pszProducer, strlen (pszProducer), Consumer_Callback_Notify));

    xTaskCreate(Thread_Producer,"Producer1", configMINIMAL_STACK_SIZE * 2, (void*) 1, tskIDLE_PRIORITY + 2, NULL);

    delay (100);

    xTaskCreate(Thread_Producer,"Producer2", configMINIMAL_STACK_SIZE * 2, (void*) 250, tskIDLE_PRIORITY + 3, NULL);

    delay (100);

    xTaskCreate (Thread_Consumer, "Consumer1", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 10, NULL);

    vTaskStartScheduler ();

    Serial.println ("Finished....");
}

void loop ()
{
}