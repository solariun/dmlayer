/**
 * @file    obsvariant.c
 * @author  Gustavo Campos (www.github.com/solariun)
 * @brief
 * @version 0.1
 * @date    2020-12-22
 *
 * @copyright Copyright (c) 2020
 *
 */

#ifndef bool
#include <stdbool.h>
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DMLayer.h"

#ifdef __DEBUG__
#define UNLOCK_VERIFY(term,isnotify,message,ret) if (!(term)){fprintf (stderr, "OBSVAR(ULK):%s[%u](%s):ERROR:[%s]\n", __FUNCTION__, __LINE__, #term, (message [0] == '\0' ? strerror (errno) : message)); DMLayer_Unlock (pDMLayer,isnotify); return ret;}
#define SUNLOCK_VERIFY(term,isnotify,message,ret) if (!(term)){fprintf (stderr, "OBSVAR(SULK):%s[%u](%s):ERROR:[%s]\n", __FUNCTION__, __LINE__, #term, (message [0] == '\0' ? strerror (errno) : message)); DMLayer_SharedUnlock (pDMLayer,isnotify); return ret;}

#else

#define UNLOCK_VERIFY(term,isnotify,message,ret) if (!(term)){DMLayer_Unlock (pDMLayer,isnotify); return ret;}
#define SUNLOCK_VERIFY(term,isnotify,message,ret) if (!(term)){DMLayer_SharedUnlock (pDMLayer,isnotify); return ret;}

#endif

struct Observable
{
    bool bEnable;
    obs_callback_func callback;
    Observable* pPrev;
};

struct ObsVariable
{
    uint32_t nVariableID;
    uint16_t nLastEventPoint;
    char* pBinData;
    size_t nBinDataSize;
    size_t nBinAllocSize;
    dmlnumber nValue;

    size_t nUserType;

    uint8_t nType;

    Observable* pLastObserver;
    ObsVariable* pPrev;
};

/*
//                                      16   12   5
// this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
// This works out to be 0x1021, but the way the algorithm works
// lets us use 0x8408 (the reverse of the bit pattern).  The high
// bit is always assumed to be set, thus we only use 16 bits to
// represent the 17 bit value.
*/

#define POLY 0x8408

uint16_t DMLayer_CRC16 (const uint8_t* pData, size_t nSize, uint16_t nCRC)
{
    unsigned char nCount;
    unsigned int data;

    nCRC = ~nCRC;

    if (nSize == 0) return (~nCRC);

    do
    {
        for (nCount = 0, data = (unsigned int)0xff & *pData++; nCount < 8; nCount++, data >>= 1)
        {
            if ((nCRC & 0x0001) ^ (data & 0x0001))
                nCRC = (nCRC >> 1) ^ POLY;
            else
                nCRC >>= 1;
        }
    } while (--nSize);

    nCRC = ~nCRC;
    data = nCRC;
    nCRC = (nCRC << 8) | (data >> 8 & 0xff);

    return (nCRC);
}

static int32_t DMLayer_GetVariableID (const char* pszTopic, size_t length)
{
    return ((int32_t) ((DMLayer_CRC16 ((const uint8_t*)pszTopic, length, 0) << 16) | DMLayer_CRC16 ((const uint8_t*)pszTopic, length, 0x8408)));
}

static bool DMLayer_CleanUpObserverCallback (ObsVariable* pVariable)
{
    bool nReturn = false;

    VERIFY (pVariable != NULL, "Obs Variable is invalid.", false);

    {
        Observable* pObservable = pVariable->pLastObserver;
        Observable* pTemp = NULL;

        while (pObservable != NULL)
        {
            pTemp = pObservable;
            pObservable = pObservable->pPrev;

            free (pTemp);
        }

        nReturn = true;
    }

    return nReturn;
}

static bool DMLayer_ResetVariable (ObsVariable* pVariable, bool nCleanBin, bool nCleanObservables)
{
    VERIFY (pVariable != NULL, "Invalid variable.", false);

    if (pVariable->pBinData != NULL)
    {
        if (nCleanBin == true)
        {
            free (pVariable->pBinData);
            pVariable->pBinData = NULL;
            pVariable->nBinAllocSize = 0;
        }
        else
        {
            if (memset ((void*)pVariable->pBinData, 0, pVariable->nBinAllocSize) != pVariable->pBinData)
            {
                free (pVariable->pBinData);
                pVariable->pBinData = NULL;
                pVariable->nBinAllocSize = 0;

                pVariable->nType = VAR_TYPE_NONE;
            }
        }
    }

    if (nCleanObservables == true && pVariable->pLastObserver != NULL)
    {
        VERIFY (DMLayer_CleanUpObserverCallback (pVariable) == true, "Fail to cleanup observables", false);
        pVariable->pLastObserver = NULL;
        pVariable->nLastEventPoint = 0;
    }

    pVariable->nValue = 0;
    pVariable->nUserType = 0;

    return true;
}


static ObsVariable* DMLayer_GetVariable (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", NULL);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", NULL);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", NULL);

    {
        uint32_t nTargetID = DMLayer_GetVariableID (pszVariableName, nVariableSize);
        ObsVariable* pVariable = pDMLayer->pObsVariables;
        while (pVariable != NULL)
        {
            if (pVariable->nVariableID == nTargetID)
            {
                return pVariable;
            }
            pVariable = pVariable->pPrev;
        }
    }

    return NULL;
}

static ObsVariable* DMLayer_CreateVariable (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize)
{
    ObsVariable* pVariable;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", NULL);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", NULL);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", NULL);
    VERIFY ((pVariable = malloc (sizeof (ObsVariable))) != NULL, "", NULL);

    VERIFY (DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize) == NULL, "Variable already exist, and will not be added", NULL);

    pVariable->nVariableID = DMLayer_GetVariableID (pszVariableName, nVariableSize);
    pVariable->pBinData = NULL;
    pVariable->pLastObserver = NULL;

    DMLayer_ResetVariable (pVariable, false, false);

    pVariable->pPrev = pDMLayer->pObsVariables;

    pDMLayer->pObsVariables = pVariable;

    return pVariable;
}

DMLayer* DMLayer_CreateInstance ()
{
    DMLayer* pDMLayer = NULL;

    VERIFY ((pDMLayer = malloc (sizeof (DMLayer))) != NULL, "", NULL);

    pDMLayer->enable = true;
    pDMLayer->pObsVariables = NULL;

    VERIFY (DMLayer_LockInit(pDMLayer), "Error initiating lock.", NULL);
    
    return pDMLayer;
}

bool DMLayer_ObserveVariable (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, size_t* pnUserType)
{
    bool nReturn = false;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);

    {
        ObsVariable variable, *pVariable;
        uint16_t nLastEvent;
        
        VERIFY (DMLayer_Lock(pDMLayer, false), "Fail to get exclusive variable lock", false);
        {
            UNLOCK_VERIFY ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL, false, "Variable does not exist.", false);
            variable = *pVariable;
        }
        VERIFY (DMLayer_Unlock(pDMLayer, false), "Fail to unlock exclusive variable lock", false);
        
        nLastEvent = variable.nLastEventPoint;

        do
        {            
            if (variable.nLastEventPoint != nLastEvent)
            {
                if (pnUserType != NULL)
                {
                    *pnUserType = variable.nUserType;
                }

                nReturn = true;
                break;
            }

            DMLayer_YieldContext ();

            VERIFY (DMLayer_Lock(pDMLayer, false), "Fail to get exclusive variable lock", false);
            {
                UNLOCK_VERIFY ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL, false, "Variable does not exist.", false);
                variable = *pVariable;
            }
            VERIFY (DMLayer_Unlock(pDMLayer, false), "Fail to unlock exclusive variable lock", false);
            
        } while (pVariable != NULL);
    }

    return nReturn;
}

static Observable* DMLayer_GetObserverCallback (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, obs_callback_func pFunc)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", NULL);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pFunc, "Function variable is null", false);

    {
        ObsVariable* pVariable = NULL;

        if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL)
        {
            Observable* pObservable = pVariable->pLastObserver;

            while (pObservable != NULL)
            {
                if (pObservable->callback == pFunc)
                {
                    return pObservable;
                }
            }
        }
    }

    return NULL;
}

bool DMLayer_AddObserverCallback (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, obs_callback_func pFunc)
{
    bool nReturn = false;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pFunc, "Function variable is null", false);

    {
        ObsVariable* pVariable;
        Observable* pObservable = NULL;

        if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) == NULL)
        {
            VERIFY (DMLayer_Lock (pDMLayer, false), "Fail to get variable lock.", false);
            {
                UNLOCK_VERIFY (pVariable = DMLayer_CreateVariable (pDMLayer, pszVariableName, nVariableSize), false, "Error creating Variable.", false);
            }
            VERIFY (DMLayer_Unlock (pDMLayer, false), "Fail to unlock variable lock.", false);
            
            pVariable->nType = VAR_TYPE_NONE;
        }
        
        VERIFY (DMLayer_Lock(pDMLayer, true), "Failed to get exclusive notify lock", false);
        {
            UNLOCK_VERIFY (DMLayer_GetObserverCallback(pDMLayer, pszVariableName, nVariableSize, pFunc) == NULL, true, "Error, Variable already exist", false);
            
            UNLOCK_VERIFY ((pObservable = malloc (sizeof (Observable))) != NULL, true, "", false);

            pObservable->callback = pFunc;
            pObservable->bEnable = true;
            pObservable->pPrev = pVariable->pLastObserver;

            pVariable->pLastObserver = pObservable;
        }
        VERIFY (DMLayer_Unlock(pDMLayer, true), "Error unlocking notify lock", false);
        
        nReturn = true;
    }

    return nReturn;
}

bool DMLayer_SetObservableCallback (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, obs_callback_func pFunc, bool bEnable)
{
    bool bReturn = false;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", NULL);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pFunc, "Function variable is null", false);
    
    VERIFY(DMLayer_SharedLock(pDMLayer, true), "Fail to get shared notification lock", false);
    {
        Observable* pObservable = NULL;
        
        SUNLOCK_VERIFY ((pObservable = DMLayer_GetObserverCallback(pDMLayer, pszVariableName, nVariableSize, pFunc)) != NULL, true, "Error, Variable does not exist", false);
        
        pObservable->bEnable = bEnable;
        
        bReturn = true;
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, true), "Fail to unlock shared notification lock", false);
    
    return bReturn;
}

bool DMLayer_IsObservableCallbackEnable (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, obs_callback_func pFunc, bool* pbSuccess)
{
    bool bReturn = false;
    
    if (pbSuccess != NULL) *pbSuccess = false;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", NULL);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pFunc, "Function variable is null", false);
    
    VERIFY(DMLayer_SharedLock(pDMLayer, true), "Fail to get shared notification lock", false);
    {
        Observable* pObservable = NULL;
        
        SUNLOCK_VERIFY ((pObservable = DMLayer_GetObserverCallback(pDMLayer, pszVariableName, nVariableSize, pFunc)) != NULL, true,  "Error, Variable does not exist", false);
        
        bReturn = pObservable->bEnable;
        
        if (pbSuccess != NULL) *pbSuccess = true;
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, true), "Fail to unlock shared notification lock", false);

    return bReturn;
}


bool DMLayer_RemoveObserverCallback (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, obs_callback_func pFunc)
{
    bool nReturn = false;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pFunc, "Function variable is null", false);
    
    VERIFY (DMLayer_Lock(pDMLayer, true), "Failed to get exclusive notify lock", false);
    {
        ObsVariable* pVariable = NULL;

        if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL)
        {
            Observable* pObservable = pVariable->pLastObserver;
            Observable* pObservablePrev = NULL;

            while (pObservable != NULL)
            {
                if (pObservable->callback == pFunc)
                {
                    if (pObservable == pVariable->pLastObserver)
                    {
                        UNLOCK_VERIFY (pObservablePrev == NULL, true, "Logic Error, please review.", false);

                        pVariable->pLastObserver = pObservable->pPrev;
                    }
                    else
                    {
                        UNLOCK_VERIFY (pObservablePrev != NULL, true, "Logic Error, please review.", false);

                        pObservablePrev->pPrev = pObservable->pPrev;
                    }

                    free (pObservable);

                    nReturn = true;
                }

                pObservablePrev = pObservable;
            }
        }
        VERIFY (DMLayer_Unlock(pDMLayer, true), "Failed to unlock exclusive notify lock", false);
    }

    return nReturn;
}

bool DMLayer_CleanUpVariables (DMLayer* pDMLayer)
{
    bool nReturn = false;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);

    VERIFY (DMLayer_Lock(pDMLayer, false), "Fail to get exclusive variable lock.", false);
    
    // Disable all interactions
    pDMLayer->enable = false;

    if (pDMLayer->pObsVariables != NULL)
    {
        ObsVariable* pVariable = pDMLayer->pObsVariables;
        ObsVariable* pTemp;

        while (pVariable != NULL)
        {
            pTemp = pVariable;

            if (pVariable->pLastObserver != NULL)
            {
                UNLOCK_VERIFY (DMLayer_Lock(pDMLayer, true), false, "Fail to get exclusive variable lock", false);
                UNLOCK_VERIFY (DMLayer_CleanUpObserverCallback (pVariable) == true, false, "Logic Error deleting callbacks", false);
                UNLOCK_VERIFY (DMLayer_Unlock(pDMLayer, true), false, "Fail to unlock exclusive variable lock", false);
            }

            pVariable = pVariable->pPrev;
            free (pTemp);
        }
        
        VERIFY (DMLayer_Unlock(pDMLayer, false), "Failt to unlock exclusive varialble lock.", false);
        nReturn = true;
    }
    else
    {
        nReturn = true;
    }

    return nReturn;
}

bool DMLayer_ReleaseInstance (DMLayer* pDMLayer)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);

    VERIFY (DMLayer_CleanUpVariables (pDMLayer) == true, "Error cleaning up Variables and observers.", false);

    return true;
}

static size_t DMLayer_Notify (DMLayer* pDMLayer, ObsVariable* pVariable, const char* pszVariableName, size_t nVariableSize, size_t nUserType, uint8_t nNotifyTpe)
{
    size_t nReturn = 0;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pVariable, "Variable is not valid.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);

    VERIFY (DMLayer_SharedLock (pDMLayer, true), "Error to get shared notify lock.", 0);
    {
        Observable* pObservable = pVariable->pLastObserver;

        pVariable->nUserType = nUserType;

        pVariable->nLastEventPoint++;

        while (pObservable != NULL)
        {
            if (pObservable->bEnable)
            {
                // Notifying all observables for a giving Variable
                pObservable->callback (pDMLayer, pszVariableName, nVariableSize, nUserType, nNotifyTpe);
            }

            nReturn++;

            pObservable = pObservable->pPrev;
        }
    }
    VERIFY (DMLayer_SharedUnlock (pDMLayer, true), "Error to unlock shared notify lock.", 0);


    return nReturn;
}

size_t DMLayer_NotifyOnly (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, size_t nUserType)
{
    size_t nReturn = 0;

    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);

    {
        ObsVariable* pVariable = NULL;

        if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL)
        {
            nReturn = DMLayer_Notify (pDMLayer, pVariable, pszVariableName, nVariableSize, nUserType, OBS_NOTIFY_NOTIFY);
        }
    }
    
    return nReturn;
}

bool DMLayer_SetNumber (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, size_t nUserType, dmlnumber nValue)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);

    {
        ObsVariable* pVariable = NULL;
        uint8_t nNotifyType = OBS_NOTIFY_CHANGED;

        VERIFY (DMLayer_Lock(pDMLayer, false), "Fail to get exclusive Lock", false);
        {
            if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) == NULL)
            {
                UNLOCK_VERIFY (pVariable = DMLayer_CreateVariable (pDMLayer, pszVariableName, nVariableSize), false, "Error creating Variable.", false);
            }
            else
            {
                UNLOCK_VERIFY (DMLayer_ResetVariable (pVariable, true, false), false, "Error reseting variable", false);
            }
            
            nNotifyType = pVariable->nType == VAR_TYPE_NONE ? OBS_NOTIFY_CREATED : nNotifyType;

            pVariable->nType = VAR_TYPE_NUMBER;
            pVariable->nValue = (dmlnumber) nValue;            
        }
        VERIFY (DMLayer_Unlock(pDMLayer, false), "Fail to unlock exclusive Lock", false);

        (void) DMLayer_Notify (pDMLayer, pVariable, pszVariableName, nVariableSize, nUserType, nNotifyType);
    }
    
    return true;
}

dmlnumber DMLayer_GetNumber (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, bool* pnSuccess)
{
    dmlnumber dmlValue = 0;
    ObsVariable variable;

    if (pnSuccess != NULL) *pnSuccess = false;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", 0);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", 0);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", 0);
    
    VERIFY(DMLayer_SharedLock(pDMLayer, false), "Failt to get shared variable lock.", 0);
    {
        ObsVariable *pVariable = NULL;
        
        UNLOCK_VERIFY((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL && pVariable->nType == VAR_TYPE_NUMBER, false, "Error, variable does not exist", 0);
        variable = *pVariable;
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, false), "Failt to unlock shared variable lock.", 0);
        
    VERIFY (variable.nType == VAR_TYPE_NUMBER, "Error, variable is not Number", 0);
    
    if (pnSuccess != NULL) *pnSuccess = true;

    dmlValue = variable.nValue;

    return dmlValue;
}

bool DMLayer_SetBinary (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, size_t nUserType, const void* pBinData, size_t nBinSize)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pBinData && nBinSize > 0, "Bin data inconsistent.", false);

    
    {
        ObsVariable* pVariable = NULL;
        uint8_t nNotifyType = OBS_NOTIFY_CHANGED;

        VERIFY (DMLayer_Lock(pDMLayer, false), "Fail to get exclusive variable lock.", false);
        {
            if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) == NULL)
            {
                VERIFY (pVariable = DMLayer_CreateVariable (pDMLayer, pszVariableName, nVariableSize), "Error creating Variable.", false);
            }
            else
            {
                VERIFY (DMLayer_ResetVariable (pVariable, true, false), "Error reseting variable", false);
            }
            
            pVariable->nType = VAR_TYPE_BINARY;

            if (pVariable->pBinData != NULL)
            {
                if (pVariable->nBinAllocSize < nBinSize)
                {
                    UNLOCK_VERIFY ((pVariable->pBinData = realloc (pVariable->pBinData, nBinSize)) == NULL, false, "", false);
                    pVariable->nBinAllocSize = nBinSize;
                }
            }
            else
            {
                if ((pVariable->pBinData = malloc (nBinSize)) == NULL)
                {
                    UNLOCK_VERIFY (DMLayer_ResetVariable (pVariable, true, false), false, "Error reseting variable", false);
                }

                nNotifyType = OBS_NOTIFY_CREATED;
            }

            pVariable->nBinDataSize = nBinSize;
            memcpy ((void*)pVariable->pBinData, (void*)pBinData, nBinSize);
        }
        VERIFY (DMLayer_Unlock(pDMLayer, false), "Fail to unlock exclusive variable lock.", false);
        
        (void) DMLayer_Notify (pDMLayer, pVariable, pszVariableName, nVariableSize, nUserType, nNotifyType);
    }

    return true;
}

uint8_t DMLayer_GetVariableType (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize)
{
    ObsVariable variable;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", VAR_TYPE_ERROR);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", VAR_TYPE_ERROR);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", VAR_TYPE_ERROR);

    VERIFY(DMLayer_SharedLock(pDMLayer, false), "Failt to get shared variable lock.", VAR_TYPE_ERROR);
    {
        ObsVariable* pVariable = NULL;
        UNLOCK_VERIFY ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL, false, "Fail to get exclusive variable lock.", VAR_TYPE_ERROR);
        variable = *pVariable;
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, false), "Failt to unlock shared variable lock.", VAR_TYPE_ERROR);

    return variable.nType;
}

size_t DMLayer_GetVariableBinarySize (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize)
{
    ObsVariable variable;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", 0);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", 0);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", 0);

    VERIFY(DMLayer_SharedLock(pDMLayer, false), "Failt to get shared variable lock.", 0);
    {
        ObsVariable* pVariable = NULL;
        UNLOCK_VERIFY ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL && pVariable->nType == VAR_TYPE_BINARY, false, "Fail to get exclusive variable lock.", 0);
        variable = *pVariable;
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, false), "Failt to unlock shared variable lock.", 0);

    return variable.nBinDataSize;
}

size_t DMLayer_GetUserType (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize)
{
    ObsVariable variable;
    
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", 0);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", 0);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", 0);

    VERIFY(DMLayer_SharedLock(pDMLayer, false), "Failt to get shared variable lock.", 0);
    {
        ObsVariable* pVariable = NULL;
        UNLOCK_VERIFY ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL && pVariable->nType == VAR_TYPE_BINARY, false, "Fail to get exclusive variable lock.", 0);
        variable = *pVariable;
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, false), "Failt to unlock shared variable lock.", 0);

    return variable.nUserType;
}

bool DMLayer_GetBinary (DMLayer* pDMLayer, const char* pszVariableName, size_t nVariableSize, void* pBinData, size_t nBinSize)
{
    bool nReturn = false;
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    VERIFY (false != pDMLayer->enable, "Error, DMLayer is disabled.", false);
    VERIFY (NULL != pszVariableName && 0 != nVariableSize, "Variable is null or empty.", false);
    VERIFY (NULL != pBinData && nBinSize > 0, "Bin data inconsistent.", false);

    VERIFY(DMLayer_SharedLock(pDMLayer, false), "Failt to get shared variable lock.", false);
    {
        ObsVariable* pVariable = NULL;

        if ((pVariable = DMLayer_GetVariable (pDMLayer, pszVariableName, nVariableSize)) != NULL && pVariable->nType == VAR_TYPE_BINARY)
        {
            VERIFY (memcpy (pBinData, pVariable->pBinData, nBinSize) == pBinData, "", false);
            nReturn = true;
        }
    }
    VERIFY(DMLayer_SharedUnlock(pDMLayer, false), "Failt to unlock shared variable lock.", 0);

    return nReturn;;
}

bool DMLayer_SetUserData (DMLayer* pDMLayer, void* pUserData, bool bIsNotify)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    
    if (bIsNotify)
        pDMLayer->pUserData.pNotifyLock = pUserData;
    else
        pDMLayer->pUserData.pVariableLock = pUserData;
    
    return true;
}

void* DMLayer_GetUserData (DMLayer* pDMLayer, bool bIsNotify)
{
    VERIFY (NULL != pDMLayer, "Error, DMLayer is invalid.", false);
    
    return bIsNotify ? pDMLayer->pUserData.pNotifyLock : pDMLayer->pUserData.pVariableLock;
}
