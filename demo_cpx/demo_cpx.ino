//#define configCHECK_FOR_STACK_OVERFLOW 1 

#include <Arduino.h>
#include <CorePartition.h>
#include <assert.h>

#include "CorePartition.h"

#include "DMLayer.h"

DMLayer* pDMLayer = NULL;

const char pszProducer[] = "THREAD/PRODUCE/VALUE";

const char pszBinConsumer[] = "THREAD/CONSUME/BIN/VALUE";

void Thread_Producer (void* pValue)
{
    int nRand = 0;

    while (true)
    {
        bool nResponse = false;
        
        nRand ++; 
        
        nResponse =  DMLayer_SetNumber (pDMLayer, pszProducer, strlen (pszProducer), Cpx_GetID (), (dmlnumber) nRand);
        
        NOTRACE ("[%s (%zu)]: func: (%u), nRand: [%u]\n", __FUNCTION__, Cpx_GetID(), nResponse, nRand);

        Cpx_Yield ();
    }
}

int nValues[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void Consumer_Callback_Notify (DMLayer* pDMLayer, const char* pszVariable, size_t nVarSize, size_t nUserType, uint8_t nNotifyType)
{
    bool nSuccess = false;
    
    nValues[nUserType] = (int)DMLayer_GetNumber (pDMLayer, pszVariable, nVarSize, &nSuccess);

    NOTRACE ("->[%s]: Variable: [%*s]\n", __FUNCTION__, (int) nVarSize, pszVariable);
    
    VERIFY (nSuccess == true, "Error, variable is invalid", );
    
    NOTRACE ("->[%s]: from: [%zu], Type: [%u] -> Value: [%u]\n", __FUNCTION__, nUserType, nNotifyType, nValues[nUserType]);

    DMLayer_SetBinary (pDMLayer, pszBinConsumer, strlen (pszBinConsumer), (size_t)Cpx_GetID (), (void*)nValues, sizeof (nValues));
}

void Thread_Consumer (void* pValue)
{
    int nRemoteValues[10];
    int nCount = 0;
    size_t nUserType = 0;

    while (DMLayer_ObserveVariable (pDMLayer, pszBinConsumer, strlen (pszBinConsumer), &nUserType) || Cpx_Yield ())
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

            Serial.printf ("[%s] Thread #%u Values: ", __FUNCTION__, Cpx_GetID ());

            for (nCount = 0; nCount < sizeof (nRemoteValues) / sizeof (nRemoteValues[0]); nCount++)
            {
                Serial.printf ("[%u] ", nRemoteValues[nCount]);
            }

            Serial.println (' ');
        }
    }
}

void DMLayer_YieldContext ()
{
    Cpx_Yield ();
}

bool DMLayer_LockInit(DMLayer* pDMLayer)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
 
    YYTRACE ("Starting up Lock.. \n");
    
    CpxSmartLock* pLock = (CpxSmartLock*) malloc (sizeof (CpxSmartLock));
    
    Cpx_LockInit(pLock);
    
    DMLayer_SetUserData(pDMLayer, (void*) pLock);
    
    return true;
}

bool DMLayer_Lock(DMLayer* pDMLayer)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer)) != NULL, "No Lock defined.", false);

    YYTRACE ("%s: ", __FUNCTION__);
    
    VERIFY (Cpx_Lock(pLock), "Failed acquire exclusive lock", false);
    
    YYTRACE (" Lock: [%u]\n", pLock->bExclusiveLock);
    
    return true;
}

bool DMLayer_SharedLock(DMLayer* pDMLayer)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer)) != NULL, "No Lock defined.", false);

    YYTRACE ("%s: ", __FUNCTION__);
    
    VERIFY (Cpx_SharedLock(pLock), "Failed acquire shared lock", false);

    YYTRACE (" Lock: [%zu]\n", pLock->nSharedLockCount);

    return true;
}

bool DMLayer_Unlock(DMLayer* pDMLayer)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer)) != NULL, "No Lock defined.", false);
    
    YYTRACE ("%s: ", __FUNCTION__);
    YYTRACE (" Lock: [%u]\n", pLock->bExclusiveLock);

    VERIFY (Cpx_Unlock(pLock), "Failed unlock", false);


    return true;
}

bool DMLayer_SharedUnlock(DMLayer* pDMLayer)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer)) != NULL, "No Lock defined.", false);
    
    YYTRACE ("%s: ", __FUNCTION__);
    YYTRACE (" Lock: [%zu]\n", pLock->nSharedLockCount);
    
    VERIFY (Cpx_SharedUnlock(pLock), "Failed to unlock", false);

    return true;
}

bool DMLayer_LockEnd(DMLayer* pDMLayer)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer)) != NULL, "No Lock defined.", false);

    VERIFY (Cpx_Lock(pLock), "Failed acquire exclusive lock", false);
    
    free (pLock);
    
    DMLayer_SetUserData(pDMLayer, NULL);
    
    return true;
}

uint32_t Cpx_GetCurrentTick ()
{
    return (uint32_t)millis ();
}

void Cpx_SleepTicks (uint32_t nSleepTime)
{
   delay (nSleepTime);
}

void Cpx_StackOverflowHandler ()
{
    while (!Serial)
        ;

    Serial.println (F ("----------------------------------------"));

    Serial.print (F ("[ERROR] - Stack Overflow - Thread #"));
    Serial.print (Cpx_GetID ());
    Serial.print (F (" Used: "));
    Serial.print (Cpx_GetStackSize ());
    Serial.print (F (", from Max: "));
    Serial.print (Cpx_GetMaxStackSize ());
    Serial.println ("");

    Serial.println (F ("----------------------------------------"));

    while (true) { delay (1000); yield (); }
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


   VERIFY ((pDMLayer = DMLayer_CreateInstance ()) != NULL, "Error creating DMLayer instance", );

    assert (DMLayer_AddObserverCallback (pDMLayer, pszProducer, strlen (pszProducer), Consumer_Callback_Notify));

    assert (Cpx_Start (20));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 1));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 300));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 300));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 500));
    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 500));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 50));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 800));
    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 800));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 1000));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 600, 60000));

    assert (Cpx_CreateThread (Thread_Consumer, NULL, 600, 1000));
    
    
    Cpx_Join ();
    
    DMLayer_ReleaseInstance(pDMLayer);
}

void loop ()
{
}