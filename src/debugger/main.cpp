#include "CLIDebugger.h"

#include <iostream>

int main(int argc, char* argv[])
{
    CLIDebugger cli;

    if (argc > 1)
    {
        if (!cli.LoadScript(argv[1]))
            return 1;
    }
    else
    {
        std::cerr << "Usage: Debugger <script.js>" << std::endl;

        return 1;
    }

    cli.Run();

    return 0;
}
