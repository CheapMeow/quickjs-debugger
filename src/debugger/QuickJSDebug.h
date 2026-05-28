#pragma once

struct JSRuntime;
struct JSContext;

class Debugger;

void InstallQuickJSDebugger(JSRuntime* rt, JSContext* ctx, Debugger* debugger);