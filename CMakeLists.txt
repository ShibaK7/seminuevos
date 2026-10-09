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
# tiene permitidos; ver cmake/SeminuevosLayers.cmake.
include(cmake/SeminuevosLayers.cmake)

# Dominio: entidades, value objects y reglas. Solo QtCore y sin moc.
seminuevos_layer(seminuevos_domain
    DIRS domain
    NO_MOC
)

# Aplicación: DTOs, puertos y casos de uso. Solo QtCore y sin moc: los
# servicios no son QObject.
seminuevos_layer(seminuevos_application
    DIRS application
    DEPENDS seminuevos_domain
    NO_MOC
)

# Front (src/presentation): cada pantalla tiene en su carpeta su .ui, su vista
# y su presenter, y aun así se reparte en dos bibliotecas por nombre de archivo
# (la regla está en cmake/SeminuevosFront.cmake):
#   - seminuevos_presentation: la lógica de pantalla (presenters, interfaces de
#     vista, navegación, tareas). Solo QtCore, para probarla sin GUI; un
#     QWidget ahí no compila.
#   - seminuevos_views: los widgets y sus formularios. Ven la lógica, los DTO y
#     los value objects, pero no enlazan ningún adaptador ni Qt Sql; la prueba
#     `architecture` revisa además que no toquen el disco.
# Cada .ui está junto al .cpp que lo incluye, así que uic lo encuentra solo.
include(cmake/SeminuevosFront.cmake)
seminuevos_front_files("${CMAKE_CURRENT_SOURCE_DIR}" SEMINUEVOS_FRONT_LOGIC SEMINUEVOS_FRONT_VIEWS)

seminuevos_layer(seminuevos_presentation
    FILES ${SEMINUEVOS_FRONT_LOGIC}
    DEPENDS seminuevos_application
    SOURCE_HEADERS
)

seminuevos_layer(seminuevos_views
    FILES ${SEMINUEVOS_FRONT_VIEWS}
    DEPENDS seminuevos_presentation
    QT Widgets
    SOURCE_HEADERS
)

# Adaptadores de salida: cada uno en su biblioteca, con solo el módulo de Qt
# de su tecnología, para que ninguno vea a otro.
seminuevos_layer(seminuevos_persistence
    DIRS adapters/persistence
    DEPENDS seminuevos_application
    QT Sql
)
seminuevos_layer(seminuevos_storage
    DIRS adapters/storage
    DEPENDS seminuevos_application
    QT Gui            # QImage, para recomprimir las fotos
)
seminuevos_layer(seminuevos_contract
    DIRS adapters/contract
    DEPENDS seminuevos_application
    QT Gui PrintSupport   # QTextDocument + QPrinter para el PDF
)
seminuevos_layer(seminuevos_security
    DIRS adapters/security
    DEPENDS seminuevos_application
    QT Network        # QPasswordDigestor (PBKDF2)
)

# --- Ejecutable ---------------------------------------------------------------
# Solo main.cpp, la raíz de composición (app/) y los recursos: todo lo demás
# son las bibliotecas de arriba. app/ es la única carpeta que ve todas las capas.
set(TS_FILES desktop-win-car-dealership_es_MX.ts)

file(GLOB_RECURSE SEMINUEVOS_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/app/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp"
)

qt_add_executable(desktop-win-car-dealership
    MANUAL_FINALIZATION
    main.cpp
    resources/Resources.qrc
    ${TS_FILES}
    ${SEMINUEVOS_APP_SOURCES}
)

target_include_directories(desktop-win-car-dealership PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

target_link_libraries(desktop-win-car-dealership PRIVATE
    seminuevos_views
    seminuevos_domain
    seminuevos_application
    seminuevos_presentation
    seminuevos_persistence
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
