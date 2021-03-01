/**
 * @file    main.c
 * @author  Gustavo Campos (www.github.com/solariun)
 * @brief
 * @version 0.1
 * @date    2020-12-22
 *
 * @copyright Copyright (c) 2020
 *
 */

#define __DEBUG__ 1

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef bool
#include <stdbool.h>
#endif

#include "CorePartition.h"

#include "DMLayer.h"

DMLayer* pDMLayer = NULL;

const char pszProducer[] = "THREAD/PRODUCE/VALUE";

const char pszBinConsumer[] = "THREAD/CONSUME/BIN/VALUE";

void Thread_Producer (void* pValue)
{
    int nRand = 0;

    while (Cpx_Yield ())
    {
        bool nResponse = false;
        
        nRand ++; 
        
        nResponse =  DMLayer_SetNumber (pDMLayer, pszProducer, strlen (pszProducer), Cpx_GetID (), (dmlnumber) nRand);
        
        NOTRACE ("[%s (%zu)]: func: (%u), nRand: [%u]\n", __FUNCTION__, Cpx_GetID(), nResponse, nRand);
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

    assert (DMLayer_AddObserverCallback (pDMLayer, pszProducer, strlen (pszProducer), Consumer_Callback_Notify));

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

            printf ("[%s] Values: ", __FUNCTION__);

            for (nCount = 0; nCount < sizeof (nRemoteValues) / sizeof (nRemoteValues[0]); nCount++)
            {
                printf ("[%u] ", nRemoteValues[nCount]);
            }

            printf ("\n");
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
 
    NOTRACE ("Starting up Lock.. \n");
    
    /*
     * Initiate Variable lock
     */
    CpxSmartLock* pLock = malloc (sizeof (pLock));
    
    VERIFY (Cpx_LockInit(pLock), "Error initiating lock.", false);
    
    DMLayer_SetUserData(pDMLayer, (void*) pLock, false);

    /*
     * Initiate Notify lock
     */
    pLock = malloc (sizeof (pLock));
    
    VERIFY (Cpx_LockInit(pLock), "Error initiating lock.", false);
    
    DMLayer_SetUserData(pDMLayer, (void*) pLock, true);

    return true;
}

bool DMLayer_Lock(DMLayer* pDMLayer, bool bIsNotify)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer, bIsNotify)) != NULL, "No Lock defined.", false);

    NOTRACE ("%s: (Notify: %pX-%u): Pre-Lock: [%u:%zu]\n", __FUNCTION__, pLock, bIsNotify, pLock->bExclusiveLock, pLock->nSharedLockCount);
    
    VERIFY (Cpx_Lock(pLock), "Failed acquire exclusive lock", false);
    
    
    NOTRACE ("%s: Lock: [%u:%zu]\n", __FUNCTION__, pLock->bExclusiveLock, pLock->nSharedLockCount);
    
    return true;
}

bool DMLayer_SharedLock(DMLayer* pDMLayer, bool bIsNotify)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer, bIsNotify)) != NULL, "No Lock defined.", false);

    NOTRACE ("%s: (Notify: %pX-%u): Pre-Lock: [%u:%zu]\n", __FUNCTION__, pLock, bIsNotify, pLock->bExclusiveLock, pLock->nSharedLockCount);
    
    VERIFY (Cpx_SharedLock(pLock), "Failed acquire shared lock", false);

    NOTRACE (" Lock: [%zu]\n", pLock->nSharedLockCount);

    return true;
}

bool DMLayer_Unlock(DMLayer* pDMLayer, bool bIsNotify)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer, bIsNotify)) != NULL, "No Lock defined.", false);
    
    NOTRACE ("%s: (Notify: %u)\n", __FUNCTION__, bIsNotify);

    VERIFY (Cpx_Unlock(pLock), "Failed unlock", false);

    NOTRACE (" Lock: [%u]\n", pLock->bExclusiveLock);

    return true;
}

bool DMLayer_SharedUnlock(DMLayer* pDMLayer, bool bIsNotify)
{
    CpxSmartLock* pLock = NULL;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer, bIsNotify)) != NULL, "No Lock defined.", false);
    
    NOTRACE ("%s: (Notify: %u)\n", __FUNCTION__, bIsNotify);
    
    VERIFY (Cpx_SharedUnlock(pLock), "Failed to unlock", false);

    NOTRACE (" Lock: [%zu]\n", pLock->nSharedLockCount);

    return true;
}

bool DMLayer_LockEnd(DMLayer* pDMLayer)
{
    CpxSmartLock* pLock = NULL;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);

    /*
     * Releaseing Varaible lock
     */
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer, false)) != NULL, "No variable Lock defined.", false);
    Cpx_Unlock(pLock);
    free (pLock);
    DMLayer_SetUserData(pDMLayer, NULL, false);
    
    /*
     * Releaseing Notify lock
     */
    VERIFY ((pLock = (CpxSmartLock*) DMLayer_GetUserData(pDMLayer, true)) != NULL, "No notification Lock defined.", false);
    Cpx_Unlock(pLock);
    free (pLock);
    DMLayer_SetUserData(pDMLayer, NULL, true);
    
    return true;
}


void Cpx_SleepTicks (uint32_t nSleepTime)
{
    usleep ((useconds_t)nSleepTime * 1000);
}

uint32_t Cpx_GetCurrentTick (void)
{
    struct timeval tp;
    gettimeofday (&tp, NULL);

    return (uint32_t)tp.tv_sec * 1000 + tp.tv_usec / 1000;  // get current timestamp in milliseconds
}

void Cpx_StackOverflowHandler ()
{
    printf ("Error, Thread#%zu Stack %zu / %zu max\n", Cpx_GetID (), Cpx_GetStackSize (), Cpx_GetMaxStackSize ());
    exit (1);
}

int main ()
{
    // start random
    srand ((unsigned int)(time_t)time (NULL));

    VERIFY ((pDMLayer = DMLayer_CreateInstance ()) != NULL, "Error creating DMLayer instance", 1);

    assert (Cpx_Start (20));



    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 1));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 300));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 300));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 500));
    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 500));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 50));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 800));
    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 800));

    assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 1000));

    //assert (Cpx_CreateThread (Thread_Producer, NULL, 1024, 60000));

    assert (Cpx_CreateThread (Thread_Consumer, NULL, 1024, 1000));
    
    
    Cpx_Join ();
    
    DMLayer_ReleaseInstance(pDMLayer);
    
    return 0;
}

