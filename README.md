# ECOSTL

[ECOBuild](https://github.com/superFUKA/ECOBuild)で管理しているモジュールです（型：cpp）。

## ECOBuildで使う

```sh
ecobuild module clone <所有者>/ECOSTL   # cloneして依存先も用意する
ecobuild build
ecobuild test
```

作業はIssueごとのタスクブランチで行います（`ecobuild task new "<題名>" --start` → 編集 → `ecobuild task commit --message "<内容>"` → `ecobuild task submit` → `ecobuild task finish`）。

## ECOBuildなしで使う

CMake 3.24以上とC++20のコンパイラがあれば、cloneしたままビルドできます（依存先は構成時に取得されます）。

```sh
cmake -S .ecobuild/.cppbuild -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
