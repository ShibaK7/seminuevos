cmake_minimum_required(VERSION 3.22)

project(desktop-win-car-dealership VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Solo Qt 6: el proyecto usa APIs de Qt 6 y las bibliotecas por capa enlazan
# los targets Qt6:: por nombre. La plantilla Qt5/Android del asistente de Qt
# Creator se quitó porque nunca se usó.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools Sql Network PrintSupport Test)

# --- Capas (arquitectura hexagonal) -----------------------------------------
# Cada capa es una biblioteca estática que solo enlaza los módulos de Qt que
# tiene permitidos; ver cmake/SeminuevosLayers.cmake. Las carpetas que todavía
# no tienen capa propia se compilan dentro del ejecutable, más abajo, hasta que
# la mudanza del plan las ubique.
include(cmake/SeminuevosLayers.cmake)

# Dominio: entidades, value objects y reglas. Solo QtCore y sin moc.
seminuevos_layer(seminuevos_domain
    DIRS domain
    NO_MOC
)

# Presentación sin widgets: solo QtCore, para poder probarla sin GUI. Hoy solo
# tiene la navegación del wizard; los presenters y las tareas se suman cuando
# existan. Las vistas (presentation/views) son Widgets y siguen en el
# ejecutable hasta que tengan su propia biblioteca.
seminuevos_layer(seminuevos_presentation
    DIRS presentation/navigation
)

# --- Ejecutable ---------------------------------------------------------------
set(TS_FILES desktop-win-car-dealership_es_MX.ts)

set(PROJECT_SOURCES
        main.cpp
        src/loginwindow.cpp
        include/loginwindow.h
        ui/loginwindow.ui
        include/mainwindow.h
        src/mainwindow.cpp
        ui/mainwindow.ui
        include/vehicleitemlist.h
        src/vehicleitemlist.cpp
        ui/vehicleitemlist.ui
        include/utils/UIUtils.h
        include/db/connectionpool.h
        src/db/connectionpool.cpp
        include/db/devseeder.h
        src/db/devseeder.cpp
        include/db/conditioncatalog.h
        src/db/conditioncatalog.cpp
        include/db/vehicleinventoryquery.h
        src/db/vehicleinventoryquery.cpp
        include/db/counterpartyrepository.h
        src/db/counterpartyrepository.cpp
        include/db/vehiclerepository.h
        src/db/vehiclerepository.cpp
        include/db/vehicleregistrationworker.h
        src/db/vehicleregistrationworker.cpp
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
        include/auth/passwordhasher.h
        src/auth/passwordhasher.cpp
        include/auth/loginworker.h
        src/auth/loginworker.cpp
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
        include/vehiclewizard/uploadformatpolicy.h
        src/vehiclewizard/uploadformatpolicy.cpp
        include/presentation/views/support/formsupport.h
        src/presentation/views/support/formsupport.cpp
        include/storage/localfilestoragemanager.h
        src/storage/localfilestoragemanager.cpp
        include/contract/numbertowordses.h
        src/contract/numbertowordses.cpp
        include/contract/companyprofile.h
        include/contract/contractpdfgenerator.h
        src/contract/contractpdfgenerator.cpp
        resources/Resources.qrc
        ${TS_FILES}
)

qt_add_executable(desktop-win-car-dealership
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_include_directories(desktop-win-car-dealership PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

target_link_libraries(desktop-win-car-dealership PRIVATE
    seminuevos_domain
    seminuevos_presentation
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::PrintSupport
)

# Le da a main.cpp la ruta raíz del repo en tiempo de compilación, para poder
# encontrar el .env de desarrollo sin importar dónde termine el directorio
# de build (Qt Creator suele usar uno hermano, no un subdirectorio, del repo).
target_compile_definitions(desktop-win-car-dealership PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(desktop-win-car-dealership PROPERTIES
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

qt_finalize_executable(desktop-win-car-dealership)

# --- Pruebas --------------------------------------------------------------------
# Unitarias con QtTest (tests/CMakeLists.txt) y la de arquitectura, que revisa
# las dependencias entre capas sin compilar nada.
enable_testing()
add_subdirectory(tests)

# La carpeta include/ de Qt le permite a la prueba saber de qué módulo es un
# header sin prefijo (<QSqlQuery> es de QtSql).
if(DEFINED QT6_INSTALL_PREFIX AND DEFINED QT6_INSTALL_HEADERS)
    set(SEMINUEVOS_QT_HEADERS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_HEADERS}")
else()
    get_filename_component(SEMINUEVOS_QT_HEADERS "${Qt6Core_DIR}/../../../include" ABSOLUTE)
endif()

add_test(NAME architecture
    COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -DQT_HEADERS=${SEMINUEVOS_QT_HEADERS}
            -P ${PROJECT_SOURCE_DIR}/cmake/CheckArchitecture.cmake
)
set_tests_properties(architecture PROPERTIES LABELS "unit")
