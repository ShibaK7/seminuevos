#ifndef PRESENTATION_COMMERCIAL_SALEDETAILS_ADDPAYMENTDIALOG_H
#define PRESENTATION_COMMERCIAL_SALEDETAILS_ADDPAYMENTDIALOG_H

#include "application/commercial/dto/commercialdtos.h"

#include <QDialog>
#include <QPointer>

class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class AddPaymentDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AddPaymentDialog(const application::CommercialPaymentDto &payment,
                              QWidget *parent = nullptr);

    void setBusy(bool busy);
    void showSaveError(const QString &message);
    void finishSave();

public slots:
    void reject() override;

signals:
    void submitted(const application::CommercialPaymentDto &payment);

private:
    QWidget *makeField(QWidget *editor, QLabel **errorLabel);
    bool validate();
    void submit();
    bool confirmDiscard();

    application::CommercialPaymentDto m_payment;
    QLabel *m_saleFolioLabel = nullptr;
    QLabel *m_installmentLabel = nullptr;
    QLabel *m_scheduledDateLabel = nullptr;
    QLabel *m_expectedAmountLabel = nullptr;
    QDateTimeEdit *m_paidDate = nullptr;
    QLineEdit *m_paidAmount = nullptr;
    QLineEdit *m_lateFee = nullptr;
    QLineEdit *m_paymentMethod = nullptr;
    QComboBox *m_status = nullptr;
    QLineEdit *m_referenceNumber = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QLabel *m_paidDateError = nullptr;
    QLabel *m_paidAmountError = nullptr;
    QLabel *m_lateFeeError = nullptr;
    QLabel *m_paymentMethodError = nullptr;
    QLabel *m_statusError = nullptr;
    QLabel *m_saveError = nullptr;
    QPushButton *m_acceptButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    bool m_dirty = false;
    bool m_discardConfirmed = false;
};

#endif // PRESENTATION_COMMERCIAL_SALEDETAILS_ADDPAYMENTDIALOG_H
