#ifndef ASPECTRATIOIMAGELABEL_H
#define ASPECTRATIOIMAGELABEL_H

#include <QLabel>
#include <QPixmap>

// QLabel que reescala su pixmap para llenar el ancho disponible (conservando
// aspecto) cada vez que cambia de tamaño, en vez de mostrar un pixmap de
// tamaño fijo centrado con espacio vacío alrededor. Así el margen entre la
// foto y el borde de su tarjeta queda controlado únicamente por el padding
// del layout contenedor, sin importar cuántas columnas tenga la galería.
class AspectRatioImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit AspectRatioImageLabel(QWidget *parent = nullptr);

    void setSourcePixmap(const QPixmap &pixmap);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScaledPixmap();

    QPixmap m_sourcePixmap;
};

#endif // ASPECTRATIOIMAGELABEL_H
