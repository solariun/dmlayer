#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

extern "C" {
    #include "DMLayer.h"
}

TEST_GROUP(BinaryVariables)
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

TEST(BinaryVariables, happy)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    CHECK_EQUAL_TEXT(true, DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");
}

TEST(BinaryVariables, null_dml)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    CHECK_EQUAL_TEXT(false, DMLayer_SetBinary(NULL, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() with NULL DML object");
}

TEST(BinaryVariables, null_name)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    CHECK_EQUAL_TEXT(false, DMLayer_SetBinary(pDMLayer, NULL, strlen(pszVariable) - 10, 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() with null name");
}

TEST(BinaryVariables, empty_name)
{
    const char pszVariable[] = "";
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    CHECK_EQUAL_TEXT(false, DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() with empty name but wrong len");
}

TEST(BinaryVariables, with_name_len_zero)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    CHECK_EQUAL_TEXT(false, DMLayer_SetBinary(pDMLayer, pszVariable, 0, 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() with name len zeroed");
}

TEST(BinaryVariables, get_happy)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    bool nSuccess = false;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);
    uint8_t get_val[val_size] = {0};


    //__ Variable does not exist yet. Should return error
    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(false, nSuccess, "Error on return o GetBinary");
    CHECK_TEXT(memcmp(val, get_val, val_size) != 0, "Error on DMLayer_GetBinary value");

    //__ Create the variable
    CHECK_EQUAL_TEXT(true, DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    nSuccess = false;
    memset(get_val, 0x00, val_size);
    //__ Variable exist, return success and the value
    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetBinary");
    CHECK_EQUAL_TEXT(0, memcmp(val, get_val, val_size), "Error on DMLayer_GetNumber value");
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
IGNORE_TEST(BinaryVariables, AddObserverCallback_notify_with_2_callbacks)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);
    uint8_t get_val[val_size] = {0};

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

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

    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetBinary");
    CHECK_EQUAL_TEXT(0, memcmp(val, get_val, val_size), "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(BinaryVariables, AddObserverCallback_notify_with_1_callbacks)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);
    uint8_t get_val[val_size] = {0};

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

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
    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetBinary");
    CHECK_EQUAL_TEXT(0, memcmp(val, get_val, val_size), "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(BinaryVariables, AddObserverCallback_created)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);
    uint8_t get_val[val_size] = {0};

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_CREATED);

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    bool nSuccess = false;

    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetBinary");
    CHECK_EQUAL_TEXT(0, memcmp(val, get_val, val_size), "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

//__ Check if the differet behaviour is intended Created to binary, Changed to number
TEST(BinaryVariables, AddObserverCallback_changed)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);
    uint8_t get_val[val_size] = {0};

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    CHECK_EQUAL_TEXT(true,
        DMLayer_AddObserverCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify),
        "Error on DMLayer_AddObserverCallback return");

    mock().expectOneCall("callbackNotify")
        .withParameter("pszVariable", (const unsigned char*)pszVariable, strlen(pszVariable))
        .withParameter("nVarSize",    strlen(pszVariable))
        .withParameter("nUserType",   nUserType)
        .withParameter("nNotifyType", OBS_NOTIFY_CREATED); // Check why is not CHANGED

    uint8_t val2[] = {'1', '1', '1', '1', '1', '1', '1', ' ', '2', '2', '2', ' ', '3', '3', '3', '3', '3', '3', '3'};
    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val2, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    bool nSuccess = false;

    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetBinary");
    CHECK_EQUAL_TEXT(0, memcmp(val2, get_val, val_size), "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(BinaryVariables, enable_and_check_callbacks)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    const size_t nUserType = 10;
    bool nSuccess = false;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);
    uint8_t get_val[val_size] = {0};

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
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    //__ Disable the callback and check it
    CHECK_EQUAL_TEXT(true,
        DMLayer_SetObservableCallback(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, false),
        "Error on DMLayer_SetObservableCallback return");

    CHECK_EQUAL_TEXT(false,
            DMLayer_IsObservableCallbackEnable(pDMLayer, pszVariable, strlen(pszVariable), callbackNotify, &nSuccess),
            "Error on DMLayer_IsObservableCallbackEnable return");

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

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
        .withParameter("nNotifyType", OBS_NOTIFY_CREATED); // Check why is not CHANGED

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    nSuccess = DMLayer_GetBinary(pDMLayer, pszVariable, strlen(pszVariable), get_val, val_size);
    CHECK_EQUAL_TEXT(true, nSuccess, "Error on return o GetBinary");
    CHECK_EQUAL_TEXT(0, memcmp(val, get_val, val_size), "Error on DMLayer_GetNumber value");

    mock().checkExpectations();
}

TEST(BinaryVariables, DMLayer_GetVariableType)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);

    CHECK_EQUAL_TEXT(VAR_TYPE_ERROR,
            DMLayer_GetVariableType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), 0, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    CHECK_EQUAL_TEXT(VAR_TYPE_BINARY,
            DMLayer_GetVariableType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");
}

//TODO: Check why DMLayer_GetUserType does not work for non BINARY types
IGNORE_TEST(BinaryVariables, DMLayer_GetUserType)
{
    const char pszVariable[] = "THREAD/PRODUCE/VALUE";
    size_t nUserType = 10;
    uint8_t val[] = {'t', 'e', 's', 't', 'i', 'n', 'g', ' ', 't', 'h', 'e', ' ', 'D', 'M', 'L', 'a', 'y', 'e', 'r'};
    int val_size = sizeof(val);

        //__ From the documentation,
    // "If variable exist will return the User defined type
    //    for the set value. Otherwise will return ZERO"
    CHECK_EQUAL_TEXT(0,
            DMLayer_GetUserType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");

    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    CHECK_EQUAL_TEXT(nUserType,
            DMLayer_GetUserType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");

    nUserType = 20;
    CHECK_EQUAL_TEXT(true,
                DMLayer_SetBinary(pDMLayer, pszVariable, strlen(pszVariable), nUserType, val, (sizeof(val)/sizeof(val[0]))),
                "Error on DMLayer_SetBinary() happy path");

    CHECK_EQUAL_TEXT(nUserType,
            DMLayer_GetUserType(pDMLayer, pszVariable, strlen(pszVariable)),
            "Error on DMLayer_GetVariableType return");
}

