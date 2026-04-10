#pragma once

class IPlugin
{
public:
    virtual ~IPlugin() = default;

    virtual const char* GetName(void) = 0;
    virtual void DrawFrontEndPage(void) = 0;
    virtual void DrawResultsUI(void) = 0;
    virtual void HandleFrontEndEvents(
        char txtKeyboard,
        bool bUp,
        bool bDown,
        bool bLeft,
        bool bRight,
        bool bSelect) = 0;
    virtual void TickFrontEndPage(float fTimeDelta) = 0;
    virtual void StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart) = 0;
    virtual void HandleResults(float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName) = 0;
    virtual void CheckPoint(float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName) = 0;
};
