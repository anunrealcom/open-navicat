cmake_minimum_required(VERSION 3.21)

project(DataMG VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg)

qt_standard_project_setup()

# ---------------- 第三方运行库 ----------------
set(MARIADB_DIR "${CMAKE_SOURCE_DIR}/third_party/mariadb")
set(PGSQL_DIR   "${CMAKE_SOURCE_DIR}/third_party/pgsql")

# Windows 版本信息资源（含应用图标）
if(WIN32)
    enable_language(RC)
    set(DATAMG_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/images/logo.ico")
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_BINARY_DIR}/app.rc"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/app.manifest"
        "${CMAKE_BINARY_DIR}/app.manifest"
        COPYONLY
    )
endif()

set(DATAMG_SOURCES
    src/main.cpp
    src/core/ConnectionProfile.cpp
    src/core/SimpleCrypt.cpp
    src/core/ConnectionStore.cpp
    src/core/DbClient.cpp
    src/core/MySqlClient.cpp
    src/core/PgClient.cpp
    src/core/SqliteClient.cpp
    src/core/SessionManager.cpp
    src/core/Metadata.cpp
    src/core/EditableTableModel.cpp
    src/core/SqlHighlighter.cpp
    src/ui/IconManager.cpp
    src/ui/FramelessDialog.cpp
    src/ui/ConnectionDialog.cpp
    src/ui/DatabaseDialog.cpp
    src/ui/TableDesignerDialog.cpp
    src/ui/ObjectTree.cpp
    src/ui/DataView.cpp
    src/ui/QueryView.cpp
    src/ui/ErDialog.cpp
    src/ui/TitleBar.cpp
    src/ui/MainWindow.cpp
)

qt_add_executable(DataMG WIN32
    ${DATAMG_SOURCES}
    resources/resources.qrc
    $<$<BOOL:${WIN32}>:${CMAKE_BINARY_DIR}/app.rc>
)

target_include_directories(DataMG PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${MARIADB_DIR}/include"
)

target_link_libraries(DataMG PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Svg
)

if(MINGW)
    target_compile_options(DataMG PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        $<$<CONFIG:Release>:-O2>
    )
endif()

# ---------------- 运行库部署 ----------------
if(WIN32)
    set(PGSQL_RUNTIME_DLLS
        "${PGSQL_DIR}/bin/libpq.dll"
        "${PGSQL_DIR}/bin/libssl-3-x64.dll"
        "${PGSQL_DIR}/bin/libcrypto-3-x64.dll"
        "${PGSQL_DIR}/bin/gssapi64.dll"
        "${PGSQL_DIR}/bin/comerr64.dll"
        "${PGSQL_DIR}/bin/krb5_64.dll"
        "${PGSQL_DIR}/bin/krbcc64.dll"
        "${PGSQL_DIR}/bin/k5sprt64.dll"
    )

    foreach(_dll ${PGSQL_RUNTIME_DLLS})
        list(APPEND COPY_DLL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dll}" "$<TARGET_FILE_DIR:DataMG>")
    endforeach()

    add_custom_command(TARGET DataMG POST_BUILD
        # MariaDB 原生客户端（QLibrary 运行期加载）
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MARIADB_DIR}/bin/libmariadb.dll" "$<TARGET_FILE_DIR:DataMG>"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:DataMG>/plugin"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${MARIADB_DIR}/plugin" "$<TARGET_FILE_DIR:DataMG>/plugin"
        # PostgreSQL 客户端运行库
        ${COPY_DLL_COMMANDS}
        COMMENT "Deploying third-party runtime DLLs"
        VERBATIM
    )

    # windeployqt 部署 Qt 运行库与插件（含 sqldrivers/qsqlpsql.dll）
    get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
    set(WINDEPLOYQT "${_qt_bin_dir}/windeployqt.exe")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_deploy_mode "--debug")
    else()
        set(_deploy_mode "--release")
    endif()

    add_custom_command(TARGET DataMG POST_BUILD
        COMMAND "${WINDEPLOYQT}" ${_deploy_mode} --no-translations
                --no-system-d3d-compiler --no-opengl-sw
                "$<TARGET_FILE:DataMG>"
        COMMENT "Running windeployqt"
        VERBATIM
    )
endif()
