//#define configCHECK_FOR_STACK_OVERFLOW 1 

#define __DEBUG__ 1

#include <Arduino.h>
#include <FreeRTOS.h>
#include <assert.h>

#include "read_write_lock.hpp"
#include "DMLayer.h"

#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t) (((TickType_t) (xTimeInMs) * (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000))

DMLayer* pDMGlobal;
DMLayer* pDMHealth;

typedef struct
{
	size_t nDataCounter [10];
	size_t nCounter;
}dmCounterData;

const char* szHealthVar="HEALTH/STRUCT_DATA";
const char* szGlobalCount="GLOBAL/COUNT";

void dmLayer_Health_Callback_Notify (DMLayer* pDMLayer, const char* pszVariable, size_t nVarSize, size_t nUserType, uint8_t nNotifyType)
{
	if (DMLayer_GetVariableType(pDMLayer, pszVariable, nVarSize) == VAR_TYPE_NUMBER)
	{
		dmCounterData dmCounters;
		bool bOnSuccess = false;
		size_t nValue = DMLayer_GetNumber(pDMLayer, pszVariable, nVarSize, &bOnSuccess);

		if (bOnSuccess)
		{
			size_t nSlot = nUserType;
            Serial.printf ("Slot: [%zu]\n", nUserType);

			if (DMLayer_GetVariableType (pDMHealth, szHealthVar, strlen (szHealthVar)) == VAR_TYPE_BINARY)
			{
				VERIFY (DMLayer_GetBinary(pDMHealth, szHealthVar, strlen (szHealthVar), (void*) &dmCounters, sizeof (dmCounters)), "Error getting global variable.", );
			}
			else
			{
				memset ((void*) &dmCounters, 0, sizeof (dmCounters));
			}

			dmCounters.nDataCounter [nSlot] ++;
			dmCounters.nCounter = nValue;

            Serial.printf ("%s: updating heath data...\n", __FUNCTION__);
			VERIFY (DMLayer_SetBinary(pDMHealth, szHealthVar, strlen (szHealthVar), 0, (void*) &dmCounters, sizeof (dmCounters)), "Error storing global variable.", );
		}
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

    // Create variable lock
    ReadWriteLock_t* wrlock = CreateReadWriteLockPreferWriter ();
    VERIFY (wrlock != NULL, "Error creating variable lock", false);
    DMLayer_SetUserData (pDMLayer, (void*)wrlock, false);

    // Creating notify lock
    wrlock = CreateReadWriteLockPreferWriter ();
    VERIFY (wrlock != NULL, "Error creating notify lock", false);
    DMLayer_SetUserData (pDMLayer, (void*)wrlock, true);

    return true;
}

bool DMLayer_Lock (DMLayer* pDMLayer, bool bIsNotify)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer, bIsNotify);
    
    VERIFY (wrlock != NULL, "Error source lock data", false);

    WriterLock (wrlock);

    return true;
}

bool DMLayer_SharedLock (DMLayer* pDMLayer, bool bIsNotify)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer, bIsNotify);

    VERIFY (wrlock != NULL, "Error source lock data", false);
    
    ReaderLock ((ReadWriteLock_t*) wrlock);

    return true;
}

bool DMLayer_Unlock (DMLayer* pDMLayer, bool bIsNotify)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer, bIsNotify);

    VERIFY (wrlock != NULL, "Error source lock data", false);

    WriterUnlock (wrlock);

    return true;
}

bool DMLayer_SharedUnlock (DMLayer* pDMLayer, bool bIsNotify)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer, bIsNotify);

    VERIFY (wrlock != NULL, "Error source lock data", false);

    ReaderUnlock ((ReadWriteLock_t*) wrlock);

    return true;
}

bool DMLayer_LockEnd (DMLayer* pDMLayer)
{
    ReadWriteLock_t* wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer, false);
    VERIFY (wrlock != NULL, "Error sourcing variable lock data", false);
    FreeReadWriteLock (wrlock);

    wrlock = (ReadWriteLock_t*)DMLayer_GetUserData (pDMLayer, true);
    VERIFY (wrlock != NULL, "Error sourcing variable lock data", false);
    FreeReadWriteLock (wrlock);
    
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

void dmLayerTestTask(void *argument)
{
  /* USER CODE BEGIN dmLayerTestTask */
   size_t nCounter = 0;
   dmCounterData dmCounters;

  /* Infinite loop */
  for(;;)
  {
	VERIFY (DMLayer_SetNumber(pDMGlobal, szGlobalCount, strlen(szGlobalCount), (size_t) (9-(nCounter%10)), (dmlnumber) nCounter), "Error saving global variable", );

    nCounter++;

    if (DMLayer_GetVariableType (pDMHealth, szHealthVar, strlen (szHealthVar)) == VAR_TYPE_BINARY)
    {
        VERIFY (DMLayer_GetBinary(pDMHealth, szHealthVar, strlen (szHealthVar), (void*) &dmCounters, sizeof (dmCounters)), "Error getting global variable.", );

        Serial.printf ("Values: ");

        for (int nCount=0; nCount < 10; nCount++)
        {
            Serial.printf ("[%-8zu], ", dmCounters.nDataCounter [nCount]);
        }

        Serial.printf (": T: %zu\n", dmCounters.nCounter);
    }

    vTaskDelay(pdMS_TO_TICKS (100));
  }

  Serial.printf ("Exiting....\n\n");
  Serial.flush ();

  /* USER CODE END dmLayerTestTask */
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

    /* Create Global DMLayer */
    pDMGlobal = DMLayer_CreateInstance ();
    /* Create Health DMLayer */
    pDMHealth = DMLayer_CreateInstance ();

    /* Create Health variable event handler */
    assert (DMLayer_AddObserverCallback (pDMGlobal, szGlobalCount, strlen (szGlobalCount), dmLayer_Health_Callback_Notify));

    xTaskCreate (dmLayerTestTask, "dml_test", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);
}

void loop ()
{
    // delay (100);
    // vTaskStartScheduler ();

    vTaskDelay (pdMS_TO_TICKS (1000));   
}
