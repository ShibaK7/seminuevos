cmake_minimum_required(VERSION 3.16)

project(desktop-win-car-dealership VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Sql Network PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql Network PrintSupport)

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
        include/db/catalogseeder.h
        src/db/catalogseeder.cpp
        include/db/conditioncatalog.h
        src/db/conditioncatalog.cpp
        include/db/vehicleinventoryquery.h
        src/db/vehicleinventoryquery.cpp
        include/db/vehicleinspectionquery.h
        src/db/vehicleinspectionquery.cpp
        include/app/appconfig.h
        src/app/appconfig.cpp
        include/components/statefultextbutton.h
        src/components/statefultextbutton.cpp
        include/components/sidebarmenuitem.h
        src/components/sidebarmenuitem.cpp
        include/components/navtabitem.h
        src/components/navtabitem.cpp
        include/components/outlinebutton.h
        src/components/outlinebutton.cpp
        include/db/conditioncatalogseeder.h
        src/db/conditioncatalogseeder.cpp
        include/auth/passwordhasher.h
        src/auth/passwordhasher.cpp
        include/auth/loginworker.h
        src/auth/loginworker.cpp
        include/domain/enums.h
        src/domain/enums.cpp
        include/domain/validationresult.h
        src/domain/validationresult.cpp
        include/domain/catalogref.h
        include/domain/counterparty.h
        src/domain/counterparty.cpp
        include/domain/vehicleconditions.h
        src/domain/vehicleconditions.cpp
        include/domain/inspectionitem.h
        src/domain/inspectionitem.cpp
        include/domain/inspection.h
        src/domain/inspection.cpp
        include/domain/vehicleimage.h
        include/domain/vehicledocument.h
        include/domain/vehiclevisitor.h
        include/domain/vehicle.h
        src/domain/vehicle.cpp
        include/domain/acquiredvehicle.h
        src/domain/acquiredvehicle.cpp
        include/domain/consignedvehicle.h
        src/domain/consignedvehicle.cpp
        include/domain/vehiclebuilder.h
        src/domain/vehiclebuilder.cpp
        include/db/counterpartyrepository.h
        src/db/counterpartyrepository.cpp
        include/db/vehiclerepository.h
        src/db/vehiclerepository.cpp
        include/vehiclewizard/vehiclewizardview.h
        src/vehiclewizard/vehiclewizardview.cpp
        include/vehiclewizard/components/conditionchecklistrow.h
        src/vehiclewizard/components/conditionchecklistrow.cpp
        include/vehiclewizard/components/wizardstepper.h
        src/vehiclewizard/components/wizardstepper.cpp
        include/vehiclewizard/components/aspectratioimagelabel.h
        src/vehiclewizard/components/aspectratioimagelabel.cpp
        include/vehiclewizard/steps/step1detailsview.h
        src/vehiclewizard/steps/step1detailsview.cpp
        include/vehiclewizard/steps/step2conditionview.h
        src/vehiclewizard/steps/step2conditionview.cpp
        include/vehiclewizard/steps/step3filesview.h
        src/vehiclewizard/steps/step3filesview.cpp
        include/storage/localfilestoragemanager.h
        src/storage/localfilestoragemanager.cpp
        include/db/vehicleregistrationworker.h
        src/db/vehicleregistrationworker.cpp
        include/contract/numbertowordses.h
        src/contract/numbertowordses.cpp
        include/contract/companyprofile.h
        include/contract/contractpdfgenerator.h
        src/contract/contractpdfgenerator.cpp
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(desktop-win-car-dealership
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
            resources/Resources.qrc
            include/mainwindow.h src/mainwindow.cpp ui/mainwindow.ui
            include/vehicleitemlist.h src/vehicleitemlist.cpp ui/vehicleitemlist.ui
            include/utils/UIUtils.h

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
    Qt${QT_VERSION_MAJOR}::PrintSupport
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
