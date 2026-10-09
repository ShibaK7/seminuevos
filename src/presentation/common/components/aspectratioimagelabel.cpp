#include "presentation/common/components/aspectratioimagelabel.h"

AspectRatioImageLabel::AspectRatioImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setMinimumWidth(1);
}

void AspectRatioImageLabel::setSourcePixmap(const QPixmap &pixmap)
{
    m_sourcePath.clear();
    m_sourcePixmap = pixmap;
    updateScaledPixmap();
}

QString AspectRatioImageLabel::sourcePath() const
{
    return m_sourcePath;
}

void AspectRatioImageLabel::setSourcePath(const QString &path)
{
    QPixmap pixmap;
    if (path.startsWith(QStringLiteral(":/")))
        pixmap.load(path);
    else if (!path.isEmpty())
        qWarning("AspectRatioImageLabel: sourcePath solo acepta recursos \":/\"; se ignora \"%s\"",
                 qPrintable(path));

    setSourcePixmap(pixmap);
    // Después de setSourcePixmap(), que la borra.
    m_sourcePath = path;
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
