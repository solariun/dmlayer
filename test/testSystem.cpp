#include "CppUTest/TestHarness.h"

extern "C" {
    #include "DMLayer.h"
}

bool DMLayer_LockInit(DMLayer* pDMLayer)
{
    return true;
}
void DMLayer_YieldContext(void) { }
bool DMLayer_Lock(DMLayer* pDMLayer, bool bIsNotify)
{
    return true;
}
bool DMLayer_SharedLock(DMLayer* pDMLayer, bool bIsNotify)
{
    return true;
}
bool DMLayer_Unlock(DMLayer* pDMLayer, bool bIsNotify)
{
    return true;
}
bool DMLayer_SharedUnlock(DMLayer* pDMLayer, bool bIsNotify)
{
    return true;
}
bool DMLayer_LockEnd(DMLayer* pDMLayer)
{
    return true;
}

TEST_GROUP(SystemChecks)
{
};

TEST(SystemChecks, CreateInstance)
{
    DMLayer* pDMLayer = NULL;
    pDMLayer = DMLayer_CreateInstance();
    CHECK_TEXT(pDMLayer != NULL, "Error creating DMLayer instance");
}

TEST(SystemChecks, CreatingAndReleasing)
{
    DMLayer* pDMLayer = NULL;
    pDMLayer = DMLayer_CreateInstance();
    CHECK_TEXT(pDMLayer != NULL, "Error creating DMLayer instance");
    CHECK_TEXT(true == DMLayer_ReleaseInstance(pDMLayer), "Error on release checks");
}

TEST(SystemChecks, ReleaseWithoutCreating)
{
    CHECK_TEXT(false == DMLayer_ReleaseInstance(NULL), "Error on release checks");
}

TEST(SystemChecks, InstanceNull)
{

}

