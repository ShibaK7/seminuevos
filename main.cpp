#include "app/compositionroot.h"

#include <QApplication>
#include <QStyleFactory>

// Punto de entrada. Todo el armado (configuración, adaptadores, casos de uso,
// presenters y ventanas) vive en CompositionRoot.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Seminuevos"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // La aplicación termina SOLO cuando se destruye la ventana principal o se
    // cierra el login (lo conecta CompositionRoot). Sin esto habría un segundo
    // camino de salida: Qt cierra la aplicación cuando cree que se ocultó la
    // última ventana, y eso pasa en momentos que no son un cierre de verdad,
    // como entre ocultar el login y mostrar la ventana principal.
    app.setQuitOnLastWindowClosed(false);

    CompositionRoot root;
    if (!root.start())
        return 1;
    return app.exec();
}
