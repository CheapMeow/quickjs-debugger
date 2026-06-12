#include "CLIDebugger.h"
#include "DAPServer.h"

#include <cstring>
#include <iostream>

int main(int argc, char* argv[])
{
    bool dapMode = false;
    const char* scriptFile = nullptr;

    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--dap") == 0)
            dapMode = true;
        else if (!scriptFile)
            scriptFile = argv[i];
    }

    if (dapMode)
    {
        DAPServer server;

        if (scriptFile)
        {
            // Let DAP launch handle loading; store path for later use.
            // DAP launch request provides the program path.
        }

        server.Run();

        return 0;
    }

    // CLI mode
    if (!scriptFile)
    {
        std::cerr << "Usage: Debugger <script.js> [--dap]" << std::endl;

        return 1;
    }

    CLIDebugger cli;

    if (!cli.LoadScript(scriptFile))
        return 1;

    cli.Run();

    return 0;
}
