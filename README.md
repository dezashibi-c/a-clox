# CLox Compiler

This is a beginner friendly `cmake` for `C` projects. while it's not by all means the **"best"** approach but it's clearly crafted together and commented for you to be able to adjust it to your needs.

## Structure

- `cmake` any utility and re-usable files for your cmakes goes here.
- `extern` folder is where you put your external dependencies, as an example I have used `clove-unit` library for unit testing.
- `include` is where you put your exported header files.
- `src` is where your `.c` files must be placed.
- `tests` are written using `clove-unit` framework which is a lightweight and single header library.

**👉 NOTE:** You can refer to [here](https://github.com/fdefelici/clove-unit) for more information about `clove-unit`.

**👉 NOTE:** There is also `.clang-format` available for you to use.

**👉 NOTE:** All the build artifacts will be placed in `out` folder, all the build artifacts for tests will be placed in `out_tests` folder.

## License

Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0) License.

Please refer to [LICENSE](/LICENSE) file.
