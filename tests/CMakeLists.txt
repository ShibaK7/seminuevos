# Pruebas unitarias con QtTest, registradas en ctest:
#   ctest --test-dir <carpeta de build> --output-on-failure
#
# Cada prueba es un ejecutable de consola aparte (sin WIN32: ctest necesita su
# salida estándar y su código de salida) y enlaza SOLO la biblioteca de la capa
# que prueba. Esa es la segunda cerca de la arquitectura: si el dominio usara
# algo de Qt Sql, sus pruebas no enlazarían.

# Carpeta del compilador. Ahí están libstdc++, libgcc y libwinpthread de
# MinGW, sin las que un ejecutable de prueba no arranca.
get_filename_component(SEMINUEVOS_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)

# seminuevos_add_test(<nombre> SOURCES <archivos>... [LIBS <bibliotecas>...])
function(seminuevos_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "seminuevos_add_test(${name}): falta SOURCES")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    # tests/support: dobles de prueba de los puertos y las vistas.
    target_include_directories(${name} PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
    )
    target_link_libraries(${name} PRIVATE ${ARG_LIBS} Qt6::Core Qt6::Test)

    add_test(NAME ${name} COMMAND ${name})
    # En Windows las DLL se buscan en el PATH. Desde Qt Creator la prueba las
    # encuentra porque el IDE arma el PATH, pero desde una consola con ctest
    # no arrancaría sin las de Qt y las de MinGW. offscreen evita que una
    # prueba que llegue a crear widgets intente abrir ventanas.
    #
    # QT_FORCE_STDERR_LOGGING: en Windows, un proceso sin consola propia (como
    # lo lanza ctest desde Git Bash o desde el IDE) manda el reporte de QtTest
    # a OutputDebugString en vez de a la salida estándar, y entonces
    # --output-on-failure no muestra qué comparación falló.
    set_tests_properties(${name} PROPERTIES
        LABELS "unit"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${SEMINUEVOS_TOOLCHAIN_BIN}"
    )
endfunction()

# --- Dominio ---------------------------------------------------------------
seminuevos_add_test(tst_cashpaymentlimit
    SOURCES domain/tst_cashpaymentlimit.cpp
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_acquiredvehicle
    SOURCES domain/tst_acquiredvehicle.cpp domain/vehiclefixtures.h
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_vehicle
    SOURCES domain/tst_vehicle.cpp domain/vehiclefixtures.h
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_vehiclepolymorphism
    SOURCES domain/tst_vehiclepolymorphism.cpp domain/vehiclefixtures.h
    LIBS seminuevos_domain
)

# --- Aplicación -----------------------------------------------------------------
seminuevos_add_test(tst_authenticationservice
    SOURCES application/tst_authenticationservice.cpp support/fakes.h
    LIBS seminuevos_application
)

# --- Presentación ---------------------------------------------------------------
seminuevos_add_test(tst_wizardnavigator
    SOURCES presentation/tst_wizardnavigator.cpp
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_loginpresenter
    SOURCES presentation/tst_loginpresenter.cpp support/fakes.h
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_pooledtaskrunner
    SOURCES presentation/tst_pooledtaskrunner.cpp
    LIBS seminuevos_presentation
)

# formsupport es apoyo de las vistas (Qt Widgets) y las vistas todavía no son
# biblioteca: la prueba compila sus fuentes directamente.
seminuevos_add_test(tst_formsupport
    SOURCES
        presentation/tst_formsupport.cpp
        "${PROJECT_SOURCE_DIR}/include/presentation/views/support/formsupport.h"
        "${PROJECT_SOURCE_DIR}/src/presentation/views/support/formsupport.cpp"
    LIBS seminuevos_domain Qt6::Widgets
)
