{
  "targets": [
    {
      "target_name": "SRAL_bridge",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [ "src/SRAL_bridge.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../../Include"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "conditions": [
        ["OS=='win'", {
          "libraries": [ "-l../../out/build/SRAL.lib" ],
          "msvs_settings": {
            "VCCLCompilerTool": { "ExceptionHandling": 1 }
          }
        }],
        ["OS=='linux'", {
          "libraries": [ "-L../../out/build", "-lsral" ],
          "cflags_cc": [ "-std=c++20" ]
        }],
        ["OS=='mac'", {
          "libraries": [ "-L../../out/build", "-lsral" ],
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          }
        }]
      ]
    }
  ]
}
