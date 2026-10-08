#include "presentation/views/components/aspectratioimagelabel.h"

AspectRatioImageLabel::AspectRatioImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setMinimumWidth(1);
}

void AspectRatioImageLabel::setSourcePixmap(const QPixmap &pixmap)
{
    m_sourcePixmap = pixmap;
    updateScaledPixmap();
}

void AspectRatioImageLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateScaledPixmap();
}

void AspectRatioImageLabel::updateScaledPixmap()
{
    if (m_sourcePixmap.isNull()) {
        QLabel::setPixmap(QPixmap());
        return;
    }
    QLabel::setPixmap(m_sourcePixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
