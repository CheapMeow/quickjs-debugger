#pragma once

#include <string>
#include <vector>

struct JSRuntime;
struct JSContext;

class Debugger;

void InstallQuickJSDebugger(JSRuntime* rt, JSContext* ctx, Debugger* debugger);

struct Variable
{
    std::string name;
    std::string value;
};

/* Must be called while the VM is paused. frame_index 0 = innermost. */
std::vector<Variable> GetFrameVariables(JSContext* ctx, int frameIndex);