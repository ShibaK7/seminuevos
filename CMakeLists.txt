cmake_minimum_required(VERSION 3.16)

project(desktop-win-car-dealership VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql Network)

set(TS_FILES desktop-win-car-dealership_es_MX.ts)

set(PROJECT_SOURCES
        main.cpp
        src/loginwindow.cpp
        include/loginwindow.h
        ui/loginwindow.ui
        include/db/connectionpool.h
        src/db/connectionpool.cpp
        include/db/schemainitializer.h
        src/db/schemainitializer.cpp
        include/db/devseeder.h
        src/db/devseeder.cpp
        include/auth/passwordhasher.h
        src/auth/passwordhasher.cpp
        include/auth/loginworker.h
        src/auth/loginworker.cpp
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(desktop-win-car-dealership
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
            resources/Resources.qrc
            include/mainwindow.h src/mainwindow.cpp ui/mainwindow.ui
            include/vehicleitemlist.h src/vehicleitemlist.cpp ui/vehicleitemlist.ui

    )

target_include_directories(desktop-win-car-dealership PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
)
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET desktop-win-car-dealership APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(desktop-win-car-dealership SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(desktop-win-car-dealership
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(desktop-win-car-dealership PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
)

# Le da a main.cpp la ruta raíz del repo en tiempo de compilación, para poder
# encontrar el .env de desarrollo sin importar dónde termine el directorio
# de build (Qt Creator suele usar uno hermano, no un subdirectorio, del repo).
target_compile_definitions(desktop-win-car-dealership PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.desktop-win-car-dealership)
endif()
set_target_properties(desktop-win-car-dealership PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS desktop-win-car-dealership
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(desktop-win-car-dealership)
endif()
