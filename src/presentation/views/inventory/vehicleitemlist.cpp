#include "presentation/views/inventory/vehicleitemlist.h"
#include "ui_vehicleitemlist.h"

#include "adapters/persistence/vehicleinventoryquery.h"

#include <QLocale>
#include <QPushButton>
#include <QStyle>

namespace {

// Marcador de dato ausente. Se usa uno solo para que la tarjeta se vea pareja
// cuando faltan varios campos.
const QString kMissing = QStringLiteral("—");

// Clave que consume el QSS embebido en ui/vehicleitemlist.ui para pintar la
// insignia: verde, ámbar o rojo.
QString statusStyleKey(const std::optional<domain::VehicleStatus> &status)
{
    if (!status)
        return QStringLiteral("unknown");
    switch (*status) {
    case domain::VehicleStatus::Disponible:
        return QStringLiteral("available");
    case domain::VehicleStatus::Apartado:
        return QStringLiteral("reserved");
    case domain::VehicleStatus::Vendido:
        return QStringLiteral("sold");
    }
    return QStringLiteral("unknown");
}

} // namespace

VehicleItemList::VehicleItemList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VehicleItemList)
{
    ui->setupUi(this);

    connect(ui->detailsButton, &QPushButton::clicked, this,
            [this] { emit detailsRequested(m_folio); });
}

VehicleItemList::~VehicleItemList()
{
    delete ui;
}

void VehicleItemList::setSummary(const VehicleSummary &summary)
{
    m_folio = summary.folio;

    const QLocale locale(QLocale::Spanish, QLocale::Mexico);

    ui->vehicleTitle->setText(summary.displayTitle());

    // Un estado que no mapea se muestra con su texto crudo y sin color de
    // insignia, en vez de disfrazarse de alguno de los válidos.
    setStatus(summary.status ? domain::displayLabel(*summary.status) : summary.statusRaw,
              statusStyleKey(summary.status));

    ui->priceLabel->setText(summary.salePrice
                                ? QStringLiteral("$%1").arg(locale.toString(*summary.salePrice, 'f', 2))
                                : kMissing);

    ui->yearLabel->setText(summary.yearModel > 0 ? QString::number(summary.yearModel) : kMissing);
    ui->colorLabel->setText(summary.color.isEmpty() ? kMissing : summary.color);
    ui->kmLabel->setText(QStringLiteral("%1 km").arg(locale.toString(summary.mileage)));

    ui->transmisionLabel->setText(summary.transmission
                                      ? domain::displayLabel(*summary.transmission)
                                      : kMissing);
    // "Motor" en esta tarjeta es el combustible, no el número de motor.
    ui->motorLabel->setText(summary.fuelTypeName.isEmpty() ? kMissing : summary.fuelTypeName);

    ui->fechaIngresoLabel->setText(summary.addedDate.isValid()
                                       ? summary.addedDate.toString(QStringLiteral("dd/MM/yyyy"))
                                       : kMissing);
}

void VehicleItemList::setStatus(const QString &label, const QString &styleKey)
{
    ui->statusBadge->setText(label);
    // El .ui trae reglas QSS por [status="available"|"reserved"|"sold"], pero
    // hasta ahora nadie fijaba la propiedad, así que nunca se activaban y la
    // insignia salía verde siempre. Hay que reaplicar el estilo a mano: Qt no
    // repinta solo cuando cambia una propiedad dinámica.
    ui->statusBadge->setProperty("status", styleKey);
    ui->statusBadge->style()->unpolish(ui->statusBadge);
    ui->statusBadge->style()->polish(ui->statusBadge);
}

void VehicleItemList::setImage(const QPixmap &pix)
{
    // Se le entrega el original y él se encarga de reescalar en cada resize.
    // Antes se escalaba aquí contra ui->vehicleImage->size(), que en este
    // punto todavía es el tamaño que puso el Designer y no el que la tarjeta
    // va a tener: la foto salía del tamaño equivocado en cuanto la lista
    // ajustaba el ancho.
    ui->vehicleImage->setSourcePixmap(pix);
}
