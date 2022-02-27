#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

extern "C" {
    #include "DMLayer.h"
}

TEST_GROUP(NumberVariables)
{
    DMLayer* pDMLayer = NULL;

    void setup()
    {
        pDMLayer = DMLayer_CreateInstance();
    }

    void teardown()
    {
        DMLayer_ReleaseInstance(pDMLayer);
        mock().clear();
    }
};

TEST(NumberVariables, happy)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(true, DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), 0, val),
                "Error on DMLayer_SetNumber() happy path");
}

TEST(NumberVariables, null_dml)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(false, DMLayer_SetNumber(NULL, pszVariable, strlen(pszVariable), 0, val),
                "Error on DMLayer_SetNumber() with NULL DML object");
}

TEST(NumberVariables, null_name)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(false, DMLayer_SetNumber(pDMLayer, NULL, strlen(pszVariable) - 10, 0, val),
                "Error on DMLayer_SetNumber() with null name");
}

TEST(NumberVariables, empty_name)
{
    const char pszVariable[] = "";
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(false, DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), 0, val),
                "Error on DMLayer_SetNumber()  with empty name but wrong len");
}

//TODO: Check expected behavior
//TEST(NumberVariables, empty_name_but_name_len_not_zeroed)
//{
//    const char pszVariable[] = "";
//    dmlnumber val = 0;
//    CHECK_EQUAL_TEXT(false, DMLayer_SetNumber(pDMLayer, pszVariable, 10, 0, val),
//                "Error on DMLayer_SetNumber()  with empty name but len not zeroed");
//}

TEST(NumberVariables, with_name_len_zero)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(false, DMLayer_SetNumber(pDMLayer, pszVariable, 0, 0, val),
                "Error on DMLayer_SetNumber() with name len zeroed");
}

TEST(NumberVariables, GetNumber_happy)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    bool nSuccess = false;
    dmlnumber val = 20, get_val = 10;

    //__ Variable does not exist yet. Should return error
    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(false, nSuccess, "Error on return o GetNumber");
    CHECK_TEXT(val != get_val, "Error on DMLayer_GetNumber value");

    //__ Create the variable
    CHECK_EQUAL_TEXT(true, DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), 0, val),
                "Error on DMLayer_SetNumber() happy path");

    nSuccess = false;
    get_val = 10;
    //__ Variable exist, return success and the value
    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetNumber");
    CHECK_EQUAL_TEXT(val, get_val, "Error on DMLayer_GetNumber value");
}

static void callbackNotify(DMLayer* pDMLayer, const char* pszVariable, size_t nVarSize,
                    size_t nUserType, uint8_t nNotifyType)
{
    mock().actualCall(__func__)
        .withParameter("pszVariable", (const unsigned char*)pszVariable, nVarSize)
        .withParameter("nVarSize",    nVarSize)
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", nNotifyType);
}

static void callbackNotify2(DMLayer* pDMLayer, const char* pszVariable, size_t nVarSize,
                    size_t nUserType, uint8_t nNotifyType)
{
    mock().actualCall(__func__)
        .withParameter("pszVariable", (const unsigned char*)pszVariable, nVarSize)
        .withParameter("nVarSize",    nVarSize)
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", nNotifyType);
}

//TODO: Check why get stuck on the second DMLayer_AddObserverCallback, in a forever while on DMLayer_GetObserverCallback
IGNORE_TEST(NumberVariables, AddObserverCallback_notify_with_2_callbacks)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_NOTIFY);

    CHECK_EQUAL_TEXT(1, DMLayer_NotifyOnly(pDMLayer, pszVariable, strlen(pszVariable), nUserType),
            "Error on DMLayer_NotifyOnly return");

    //__ Add a second callback
    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify2),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_NOTIFY);

    mock().expectOneCall("callbackNotify2")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_NOTIFY);

    CHECK_EQUAL_TEXT(2, DMLayer_NotifyOnly(pDMLayer, pszVariable, strlen(pszVariable), nUserType),
            "Error on DMLayer_NotifyOnly return");

    bool nSuccess = false;
    dmlnumber get_val = ~(uint32_t)val;

    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetNumber");
    CHECK_EQUAL_TEXT(val, get_val, "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(NumberVariables, AddObserverCallback_notify_with_1_callbacks)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_NOTIFY);

    CHECK_EQUAL_TEXT(1, DMLayer_NotifyOnly(pDMLayer, pszVariable, strlen(pszVariable), nUserType),
            "Error on DMLayer_NotifyOnly return");

    bool nSuccess = false;
    dmlnumber get_val = ~(uint32_t)val;

    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetNumber");
    CHECK_EQUAL_TEXT(val, get_val, "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(NumberVariables, AddObserverCallback_created)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    dmlnumber val = 0;

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_CREATED);

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    bool nSuccess = false;
    dmlnumber get_val = ~(uint32_t)val;

    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetNumber");
    CHECK_EQUAL_TEXT(val, get_val, "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(NumberVariables, AddObserverCallback_changed)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    dmlnumber val = 0;
    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_CHANGED);

    val = 5;
    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    bool nSuccess = false;
    dmlnumber get_val = ~(uint32_t)val;

    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetNumber");
    CHECK_EQUAL_TEXT(val, get_val, "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(NumberVariables, enable_and_check_callbacks)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    bool nSuccess = false;
    dmlnumber val = 0;

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    //__ Check if a callback is enabled
    CHECK_EQUAL_TEXT(true,
            DMLayer_IsObservableCallbackEnable(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, &nSuccess),
            "Error on DMLayer_IsObservableCallbackEnable return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_CREATED);

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    //__ Disable the callback and check it
    CHECK_EQUAL_TEXT(true,
        DMLayer_SetObservableCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, false),
        "Error on DMLayer_SetObservableCallback return");

    CHECK_EQUAL_TEXT(false,
            DMLayer_IsObservableCallbackEnable(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, &nSuccess),
            "Error on DMLayer_IsObservableCallbackEnable return");

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    //__ Enable it again
    CHECK_EQUAL_TEXT(true,
        DMLayer_SetObservableCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, true),
        "Error on DMLayer_SetObservableCallback return");

    CHECK_EQUAL_TEXT(true,
            DMLayer_IsObservableCallbackEnable(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, &nSuccess),
            "Error on DMLayer_IsObservableCallbackEnable return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_CHANGED);

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    dmlnumber get_val = ~(uint32_t)val;
    get_val = DMLayer_GetNumber(pDMLayer, pszVariable, strlen(pszVariable), &nSuccess);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetNumber");
    CHECK_EQUAL_TEXT(val, get_val, "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(NumberVariables, DMLayer_GetVariableType)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    dmlnumber val = 0;

    CHECK_EQUAL_TEXT(VAR_TYPE_ERROR,
            DMLayer_GetVariableType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");

    CHECK_EQUAL_TEXT(true, DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), 0, val),
                "Error on DMLayer_SetNumber() happy path");

    CHECK_EQUAL_TEXT(VAR_TYPE_NUMBER,
            DMLayer_GetVariableType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");
}

//TODO: Check why DMLayer_GetUserType does not work for non BINARY types
IGNORE_TEST(NumberVariables, DMLayer_GetUserType)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    size_t nUserType = 10;
    dmlnumber val = 0;

    //__ From the documentation,
    // "If variable exist will return the User defined type
    //    for the set value. Otherwise will return ZERO"
    CHECK_EQUAL_TEXT(0,
            DMLayer_GetUserType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");

    CHECK_EQUAL_TEXT(true, DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    CHECK_EQUAL_TEXT(nUserType,
            DMLayer_GetUserType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");

    nUserType = 20;
    CHECK_EQUAL_TEXT(true, DMLayer_SetNumber(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val),
                "Error on DMLayer_SetNumber() happy path");

    CHECK_EQUAL_TEXT(nUserType,
            DMLayer_GetUserType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");
}

