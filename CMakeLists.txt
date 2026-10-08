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
# tiene permitidos; ver cmake/SeminuevosLayers.cmake. Lo que todavía no cumple
# la regla de su capa se compila dentro del ejecutable, más abajo, hasta que el
# commit del plan que lo corrige lo convierta en biblioteca.
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

# Adaptadores de salida: cada uno en su biblioteca, con solo el módulo de Qt
# de su tecnología, para que ninguno vea a otro.
seminuevos_layer(seminuevos_storage
    DIRS adapters/storage
    QT Gui            # QImage, para recomprimir las fotos
)
seminuevos_layer(seminuevos_contract
    DIRS adapters/contract
    DEPENDS seminuevos_domain
    QT Gui PrintSupport   # QTextDocument + QPrinter para el PDF
)
seminuevos_layer(seminuevos_security
    DIRS adapters/security
    QT Network        # QPasswordDigestor (PBKDF2)
)

# --- Ejecutable ---------------------------------------------------------------
# Además de main.cpp, compila lo que todavía no es biblioteca: la persistencia
# (hasta que deje de depender de otro adaptador), las vistas y las carpetas
# viejas que el plan va borrando (app, auth, db, utils, vehiclewizard). Se
# listan por carpeta para que una vista o un .ui nuevo no obligue a tocar este
# archivo.
set(TS_FILES desktop-win-car-dealership_es_MX.ts)

set(SEMINUEVOS_EXE_DIRS
    adapters/persistence
    presentation/views
    app
    auth
    db
    utils
    vehiclewizard
)
set(PROJECT_SOURCES main.cpp resources/Resources.qrc ${TS_FILES})
foreach(dir IN LISTS SEMINUEVOS_EXE_DIRS)
    file(GLOB_RECURSE dir_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/${dir}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${dir}/*.cpp"
    )
    list(APPEND PROJECT_SOURCES ${dir_files})
endforeach()
file(GLOB SEMINUEVOS_FORMS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.ui")
list(APPEND PROJECT_SOURCES ${SEMINUEVOS_FORMS})

qt_add_executable(desktop-win-car-dealership
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Los .ui viven en src/ui; con esta ruta, "#include "ui_x.h"" encuentra el
# formulario sin rutas relativas.
set_target_properties(desktop-win-car-dealership PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

target_include_directories(desktop-win-car-dealership PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

target_link_libraries(desktop-win-car-dealership PRIVATE
    seminuevos_domain
    seminuevos_presentation
    seminuevos_storage
    seminuevos_contract
    seminuevos_security
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
