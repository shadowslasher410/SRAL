local status, err = pcall(main)
if not status then
    print("Demo failed with error: " .. tostring(err))
end
local SRAL = require("sral")
local sral = SRAL.new()

local function sleep(n)
    if ffi.os == "Windows" then os.execute("timeout " .. n .. " > nul")
    else os.execute("sleep " .. n) end
end

local function error_handling_demo()
    print("\n=== Error Handling Demo ===")
    local standalone = SRAL.new()
    print("Attempting operation without initialization...")
    local result = standalone:speak("This should fail", true)
    print("Result: " .. tostring(result) .. " (expected: false)")
end

local function main()
    print("=== SRAL Library Demo ===")
    
    if not sral:is_initialized() then
        print("Initializing SRAL...")
        if not sral:initialize() then
            print("Failed to initialize SRAL!")
            return
        end
    end

    print("SRAL initialized successfully!")
    
    local current = sral:get_current_engine()
    print("Active Engine: " .. sral:get_engine_name(current))

    sral:speak_ex(current, "Lua bindings initialized. Testing keyboard hooks.", true)
    
    if sral:register_keyboard_hooks() then
        print("Hooks active. Use Ctrl to stop or Shift to pause.")
        sral:delay(2000)
        sral:unregister_keyboard_hooks()
    end

    print("Speaking basic text...")
    sral:speak("Hello from LuaJIT! Testing SRAL on " .. ffi.os, true)
    
    print("Is speaking: " .. tostring(sral:is_speaking()))
    if ffi.os == "Windows" then os.execute("timeout /t 2 >nul") else os.execute("sleep 2") end

    print("\nCleaning up...")
    sral:uninitialize()
    print("SRAL uninitialized successfully!")
end

local status, err = pcall(main)
if not status then
    print("Demo failed with error: " .. tostring(err))
end

error_handling_demo()