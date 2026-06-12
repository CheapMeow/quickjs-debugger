#!/bin/bash
send() {
    local json="$1"
    printf "Content-Length: %d\r\n\r\n%s" ${#json} "$json"
}

{
    send '{"type":"request","seq":1,"command":"initialize","arguments":{}}'
    sleep 0.1
    send '{"type":"request","seq":2,"command":"launch","arguments":{"program":"test_cli.js"}}'
    sleep 0.1
    send '{"type":"request","seq":3,"command":"setBreakpoints","arguments":{"source":{"path":"test_cli.js"},"breakpoints":[{"line":6}]}}'
    sleep 0.1
    send '{"type":"request","seq":4,"command":"configurationDone"}'
    sleep 0.3
    send '{"type":"request","seq":5,"command":"threads"}'
    sleep 0.1
    send '{"type":"request","seq":6,"command":"stackTrace","arguments":{"threadId":1}}'
    sleep 0.1
    send '{"type":"request","seq":7,"command":"scopes","arguments":{"frameId":0}}'
    sleep 0.1
    send '{"type":"request","seq":8,"command":"variables","arguments":{"variablesReference":1}}'
    sleep 0.1
    send '{"type":"request","seq":9,"command":"continue"}'
    sleep 0.3
    send '{"type":"request","seq":10,"command":"disconnect"}'
} | ./build-clangd/src/debugger/Debugger.exe --dap 2>/dev/null
