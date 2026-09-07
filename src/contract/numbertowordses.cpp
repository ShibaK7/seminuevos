#include "../../include/contract/numbertowordses.h"

#include <QStringList>
#include <cmath>

namespace {

QString unitWord(int n)
{
    static const QStringList units = {
        "cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve"};
    return units.at(n);
}

QString tensWord(int n) // 10-99
{
    static const QStringList teens = {
        "diez", "once", "doce", "trece", "catorce", "quince",
        "dieciséis", "diecisiete", "dieciocho", "diecinueve"};
    if (n < 20)
        return teens.at(n - 10);

    static const QStringList veinti = {
        "veinte", "veintiuno", "veintidós", "veintitrés", "veinticuatro",
        "veinticinco", "veintiséis", "veintisiete", "veintiocho", "veintinueve"};
    if (n < 30)
        return veinti.at(n - 20);

    static const QStringList tensNames = {
        "", "", "", "treinta", "cuarenta", "cincuenta", "sesenta", "setenta", "ochenta", "noventa"};
    const int tens = n / 10;
    const int unit = n % 10;
    if (unit == 0)
        return tensNames.at(tens);
    return tensNames.at(tens) + QStringLiteral(" y ") + unitWord(unit);
}

QString hundredsGroupWord(int n) // 0-999
{
    if (n == 0)
        return QString();
    if (n == 100)
        return QStringLiteral("cien");

    static const QStringList hundreds = {
        "", "ciento", "doscientos", "trescientos", "cuatrocientos", "quinientos",
        "seiscientos", "setecientos", "ochocientos", "novecientos"};
    const int h = n / 100;
    const int rem = n % 100;

    QStringList parts;
    if (h > 0)
        parts << hundreds.at(h);
    if (rem > 0)
        parts << (rem < 10 ? unitWord(rem) : tensWord(rem));

    return parts.join(QStringLiteral(" "));
}

} // namespace

QString NumberToWordsEs::convert(double amount)
{
    const double flooredAmount = std::floor(amount);
    qint64 integerPart = static_cast<qint64>(flooredAmount);
    int cents = static_cast<int>(std::llround((amount - flooredAmount) * 100));
    if (cents >= 100) {
        integerPart += 1;
        cents = 0;
    }

    QString integerWords;
    if (integerPart == 0) {
        integerWords = QStringLiteral("cero");
    } else {
        const qint64 millions = integerPart / 1000000;
        const qint64 rest = integerPart % 1000000;
        const qint64 thousands = rest / 1000;
        const qint64 hundreds = rest % 1000;

        QStringList parts;
        if (millions > 0) {
            parts << (millions == 1 ? QStringLiteral("un millón")
                                     : hundredsGroupWord(static_cast<int>(millions)) + QStringLiteral(" millones"));
        }
        if (thousands > 0) {
            parts << (thousands == 1 ? QStringLiteral("mil")
                                      : hundredsGroupWord(static_cast<int>(thousands)) + QStringLiteral(" mil"));
        }
        if (hundreds > 0) {
            parts << hundredsGroupWord(static_cast<int>(hundreds));
        }
        integerWords = parts.join(QStringLiteral(" "));
    }

    return QStringLiteral("%1 PESOS %2/100 M.N.")
        .arg(integerWords.toUpper(), QStringLiteral("%1").arg(cents, 2, 10, QLatin1Char('0')));
}
