{
  "targets": [
    {
      "target_name": "autolib",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [
        "src/addon.c",
        "src/keysender.c",
        "src/keymonitor.c",
        "src/process.c",
        "src/mouse.c",
        "src/selection.c",
        "src/window.c"
      ],
      "include_dirs": [],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 0
            }
          },
          "libraries": [
            "psapi.lib",
            "version.lib"
          ]
        }],
        ["OS=='mac'", {
          "libraries": [
            "-framework ApplicationServices",
            "-framework Foundation",
            "-framework AppKit"
          ]
        }],
        ["OS=='linux'", {
          "libraries": [
            "-lm"
          ]
        }]
      ]
    }
  ]
}