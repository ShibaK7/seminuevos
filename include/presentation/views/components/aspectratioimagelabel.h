#ifndef PRESENTATION_VIEWS_COMPONENTS_ASPECTRATIOIMAGELABEL_H
#define PRESENTATION_VIEWS_COMPONENTS_ASPECTRATIOIMAGELABEL_H

#include <QLabel>
#include <QPixmap>
#include <QString>

// QLabel que reescala su pixmap para llenar el ancho disponible (conservando
// aspecto) cada vez que cambia de tamaño, en vez de mostrar un pixmap de
// tamaño fijo centrado con espacio vacío alrededor. Así el margen entre la
// foto y el borde de su tarjeta queda controlado únicamente por el padding
// del layout contenedor, sin importar cuántas columnas tenga la galería.
//
// Se promueve en Qt Designer (clase base QLabel). Una imagen fija, como el
// logo, se declara con la propiedad dinámica `sourcePath`; las fotos que
// cambian en ejecución llegan por setSourcePixmap().
class AspectRatioImageLabel : public QLabel
{
    Q_OBJECT
    // Ruta de un recurso Qt (":/..."). Existe porque la propiedad `pixmap` de
    // QLabel no sirve aquí: uic la aplica con QLabel::setPixmap(), que no
    // alimenta el pixmap ORIGINAL del que se reescala, y la imagen quedaba en
    // blanco al primer cambio de tamaño.
    //
    // Solo acepta recursos ":/": las vistas no tocan el disco (la prueba
    // `architecture` lo revisa). Una ruta de archivo se ignora con un aviso en
    // consola y la etiqueta queda vacía; una imagen del disco la carga quien
    // sí puede leerlo y llega por setSourcePixmap().
    Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath)

public:
    explicit AspectRatioImageLabel(QWidget *parent = nullptr);

    // Reemplaza la imagen. También olvida sourcePath, que deja de describir lo
    // que se ve.
    void setSourcePixmap(const QPixmap &pixmap);

    QString sourcePath() const;
    void setSourcePath(const QString &path);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScaledPixmap();

    QPixmap m_sourcePixmap;
    QString m_sourcePath;
};

#endif // PRESENTATION_VIEWS_COMPONENTS_ASPECTRATIOIMAGELABEL_H
