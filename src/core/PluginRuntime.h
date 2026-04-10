#pragma once

namespace rsf
{
    class PluginRuntime
    {
    public:
        bool start(HMODULE moduleHandle);
        void stop();

    private:
        static DWORD WINAPI ThreadProc(LPVOID parameter);
        void run();

        HMODULE moduleHandle_ = nullptr;
        HANDLE workerThread_ = nullptr;
        HANDLE stopEvent_ = nullptr;
    };
}
