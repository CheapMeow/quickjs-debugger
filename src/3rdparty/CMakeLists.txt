include(FetchContent)

option(QJS_BUILD_LIBC "" ON)

FetchContent_Declare(quickjs
                     GIT_REPOSITORY https://github.com/quickjs-ng/quickjs)

FetchContent_MakeAvailable(quickjs)