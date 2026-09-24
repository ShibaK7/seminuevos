#ifndef UIUTILS_H
#define UIUTILS_H

#include <QVariant>
#include <QDebug>

class UIUtils {
public:
    // Previene la instanciación de la clase (todos los métodos son estáticos)
    UIUtils() = delete;

    /**
     * @brief Llena un QComboBox directamente desde una tabla de catálogo simple.
     * @param comboBox Puntero al control QComboBox en la interfaz.
     * @param tableName Nombre de la tabla catálogo (ej. "brands_cat", "fuel_type_cat").
     * @param placeholder Texto inicial opcional (ej. "Seleccione una opción..."). Si es vacío, no agrega placeholder.
     * @param displayColumn Columna a mostrar como texto en la UI (por defecto 'name').
     * @param idColumn Columna a guardar como UserRole / valor interno (por defecto 'id').
     */
    static bool populateComboBox(QComboBox* comboBox, 
                                const QString& tableName, 
                                const QString& placeholder = "Seleccione una opción...",
                                const QString& displayColumn = "name",
                                const QString& idColumn = "id") 
    {
        if (!comboBox) {
            qWarning() << "UIUtils::populateComboBox - QComboBox nulo.";
            return false;
        }

        comboBox->clear();

        // Si se define un placeholder, lo asignamos con ID invalido (-1)
        if (!placeholder.isEmpty()) {
            comboBox->addItem(placeholder, -1);
        }

        // Construcción y ejecución directa del query SQL
        QString queryString = QString("SELECT %1, %2 FROM %3 ORDER BY %2 ASC;")
                                .arg(idColumn, displayColumn, tableName);
        QSqlQuery query(queryString);

        if (!query.exec()) {
            qCritical() << "UIUtils::populateComboBox Error al consultar la tabla" 
                       << tableName << ":" << query.lastError().text();
            return false;
        }

        while (query.next()) {
            int id = query.value(0).toInt();
            QString name = query.value(1).toString();
            
            // Asigna el texto a mostrar y guarda el ID en Qt::UserRole
            comboBox->addItem(name, id);
        }

        return true;
    }

    /**
     * @brief Llena un QComboBox con categorías (padres o subcategorías) de vehicle_categories_cat.
     * @param comboBox Puntero al QComboBox.
     * @param parentId ID de la categoría padre. Si es <= 0, recupera las categorías padre (parent_id IS NULL).
     * @param placeholder Texto inicial opcional.
     */
    static bool populateCategoriesComboBox(QComboBox* comboBox, 
                                          int parentId = -1, 
                                          const QString& placeholder = "Seleccione una opción...")  
    {
        if (!comboBox) return false;

        comboBox->clear();

        if (!placeholder.isEmpty()) {
            comboBox->addItem(placeholder, -1);
        }

        QSqlQuery query;

        // Evaluamos si consultamos categorías padre o subcategorías de un padre específico
        if (parentId <= 0) {
            query.prepare("SELECT id, name FROM vehicle_categories_cat WHERE parent_id IS NULL ORDER BY name ASC;");
        } else {
            query.prepare("SELECT id, name FROM vehicle_categories_cat WHERE parent_id = :parentId ORDER BY name ASC;");
            query.bindValue(":parentId", parentId);
        }

        if (!query.exec()) {
            qCritical() << "UIUtils::populateCategoriesComboBox Error:" << query.lastError().text();
            return false;
        }

        while (query.next()) {
            comboBox->addItem(query.value("name").toString(), query.value("id").toInt());
        }

        return true;
    }
};

#endif // UIUTILS_H
