# CLox Compiler

This is a follow up/re-implementation of `clox` programming language from [Crafting Interpreters Book](https://craftinginterpreters.com/a-bytecode-virtual-machine.html).

I've made some changes, in namings, files, etc. I've also added array support as well.

## Structure

- `cmake` any utility and re-usable files for your cmakes goes here.
- `extern` folder is where you put your external dependencies, as an example I have used `clove-unit` library for unit testing.
- `include` is where you put your exported header files.
- `src` is where your `.c` files must be placed.
- `tests` are written using `clove-unit` framework which is a lightweight and single header library.

**👉 NOTE:** You can refer to [here](https://github.com/fdefelici/clove-unit) for more information about `clove-unit`.

**👉 NOTE:** There is also `.clang-format` available for you to use.

**👉 NOTE:** All the build artifacts will be placed in `out` folder, all the build artifacts for tests will be placed in `out_tests` folder.

## CMake Configuration Options

- `clox_ENABLE_NAN_BOXING` -> `ON` by default
- `clox_ENABLE_DEBUG_PRINT_CODE` -> `OFF` by default
- `clox_ENABLE_DEBUG_TRACE_EXECUTION` -> `OFF` by default
- `clox_ENABLE_DEBUG_STRESS_GC` -> `ON` by default
- `clox_ENABLE_DEBUG_LOG_GC` -> `OFF` by default

## License

Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0) License.

Please refer to [LICENSE](/LICENSE) file.
