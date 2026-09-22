# Backend Execution & Database Access Guidelines

To achieve maximum development velocity while maintaining a clean code base, our team follows a Pragmatic 3-Tier Execution Rule Set.

We do not over-engineer with heavy enterprise patterns (like clean/onion architecture or custom C++ entities for every single table). Instead, we choose the right database access tool based on the complexity of the operation.

```plaintext
┌────────────────────────────────────────────────────────────────────────┐
│                         DB ACCESS TIER MATRIX                          │
├─────────────────────────┬────────────────────────┬─────────────────────┤
│    Operation Type       │     Access Method      │    Primary Tool     │
├─────────────────────────┼────────────────────────┼─────────────────────┤
│ 1. UI Dropdowns/Lists   │ Direct Qt Helpers      │ QSqlQuery / Models  │
│ 2. Single-Table CRUD    │ Repositories (DAOs)    │ Static Methods      │
│ 3. Multi-Table Workflows│ Business Services      │ SQL Transactions    │
└─────────────────────────┴────────────────────────┴─────────────────────┤
```

## Rule 1: UI Data Binding (Direct `QSqlQuery` & Qt SQL Models)
### When to Use
* Whenever data is being fetched strictly for display in UI controls (`QComboBox`, `QTableView`, `QListView`), avoid creating custom C++ model classes, DAOs, or DTO structs. Qt provides native data-binding mechanisms designed specifically to eliminate manual loop parsing.

* Populating simple dropdowns (`QComboBox`) from catalog tables (`brands_cat`, `fuel_type_cat`, `vehicle_categories_cat`, etc.).
* Rendering read-only grids/tables (`QTableView`, `QListView`) directly from `SELECT` statements, joins, or custom view queries (e.g., Inventory List, Sales History, Expense Logs).

### Scope & Limits
* **NO custom C++ model classes or repositories allowed for simple dropdowns.** Creating a repository or C++ struct just to populate a 2-column combo box is not allowed.
* Uses generic helper functions to eliminate repetitive code.
* Must store the database primary key `id` inside the `QComboBox` item data (`Qt::UserRole`).

### Standard Implementation Pattern
#### A. Generic Catalog Helper (`UIUtils::populateComboBox`)
Create a shared helper function in your UI utilities:
```c++
// uiutils.h
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

class UIUtils {
public:
    static void populateComboBox(QComboBox* comboBox, const QString& tableName, const QString& placeholder = "Seleccione una opción...") {
        comboBox->clear();
        
        if (!placeholder.isEmpty()) {
            comboBox->addItem(placeholder, -1); // Default unselected state
        }

        QSqlQuery query(QString("SELECT id, name FROM %1 ORDER BY name ASC").arg(tableName));
        while (query.next()) {
            int id = query.value("id").toInt();
            QString name = query.value("name").toString();
            comboBox->addItem(name, id); // Text = Display Name, UserRole = Primary Key
        }
    }
};

// Usage in UI Form:
UIUtils::populateComboBox(ui->cmbBrands, "brands_cat");
int selectedBrandId = ui->cmbBrands->currentData().toInt();
```

#### B. Grid Data Binding (`QSqlQueryModel` for `QTableView`)
```c++
// In your Inventory List View Widget:
void InventoryWidget::loadVehicleGrid() {
    QSqlQueryModel *model = new QSqlQueryModel(this);

    // Raw SQL with alias labels used directly as table header titles
    model->setQuery(R"(
        SELECT 
            v.folio AS "Folio",
            b.name AS "Marca",
            v.model AS "Modelo",
            v.year_model AS "Año",
            v.mileage AS "Kilometraje",
            v.status AS "Estado",
            v.acquisition_type AS "Origen"
        FROM vehicles v
        LEFT JOIN brands_cat b ON v.brand_id = b.id
        ORDER BY v.folio DESC
    )");

    if (model->lastError().isValid()) {
        qCritical() << "Error loading vehicle grid:" << model->lastError().text();
        delete model;
        return;
    }

    // Direct binding with zero manual row parsing
    ui->tableViewVehicles->setModel(model);
    ui->tableViewVehicles->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}
```

## Rule 2: Repositories / DAOs (Single-Table Atomic CRUD)
### When to Use
* When a single table requires **reusable insert, update, delete, or fetch queries** across different parts of the application.
* When managing simple administrative screens (e.g., adding a new brand, updating operational expense categories, or managing users).

### Scope & Limits
* **Strictly 1 Repository = 1 Table or Domain.**
* **Zero business logic allowed.** A repository must not calculate prices, validate complex rules, or check permissions.
* **No multi-table transactions.** Repositories only run individual, atomic queries.
* All methods must be `static` to avoid unnecessary object instantiation overhead.

### Standard Implementation Pattern
```c++
// catalogrepository.h
#include <QSqlQuery>
#include <QVariant>
#include <QPair>
#include <QList>

class CatalogRepository {
public:
    // Insert new catalog entry
    static bool addCatalogItem(const QString& tableName, const QString& name, int& outId) {
        QSqlQuery q;
        q.prepare(QString("INSERT INTO %1 (name) VALUES (:name) RETURNING id;").arg(tableName));
        q.bindValue(":name", name);
        
        if (q.exec() && q.next()) {
            outId = q.value(0).toInt();
            return true;
        }
        return false;
    }

    // Get hierarchical categories (e.g., vehicle_categories_cat)
    static QList<QPair<int, QString>> getSubcategories(int parentId) {
        QList<QPair<int, QString>> list;
        QSqlQuery q;
        q.prepare("SELECT id, name FROM vehicle_categories_cat WHERE parent_id = :parentId ORDER BY name ASC;");
        q.bindValue(":parentId", parentId);
        
        if (q.exec()) {
            while (q.next()) {
                list.append({q.value(0).toInt(), q.value(1).toString()});
            }
        }
        return list;
    }
};
```

## Rule 3: Business Services (Workflow Orchestration & Transactions)
### When to Use
* Any business process that spans **2 or more database tables** (e.g., registering an inventory acquisition, executing a sale, or generating financing payments).
* Operations requiring **atomic safety** (if step 3 fails, steps 1 and 2 must rollback).
* Operations involving **business logic, calculations, or validations** before touching the database.

### Scope & Limits
* Services control `QSqlDatabase::transaction()` and `QSqlDatabase::commit()` / `rollback()`. Repositories must never handle transactions directly.
* Receives inputs from UI as Plain Data Objects (C++ `struct` / DTO).
* Returns clear status (`bool`) along with user-facing error messages via reference parameters.

### Standard Implementation Pattern
```c++
// vehicleservice.h
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMap>

struct AcquisitionDTO { // --> May be a struct or a class
    // Core Vehicle Data
    QString acquisitionType;
    int vehicleTypeId;
    int subtypeId;
    int brandId;
    QString model;
    int yearModel;
    QString color;
    int mileage;
    QString serialNumber;

    // Acquisition Financials
    int sellerId;
    QString invoiceType;
    double purchasePrice;
    double salePrice;

    // Conditions & Specs
    int fuelTypeId;
    int cylinders;
    QString transmission;
    QString windowRegulators;
    QString airConditioning;

    // Checklist Inspection (Element ID -> Pass/Fail State)
    QMap<int, bool> checklist;
};

class VehicleService {
public:
    static bool registerAcquisition(const AcquisitionDTO& dto, int& outVehicleFolio, QString& outErrorMessage) {
        // 1. Business Logic / Pre-validation
        if (dto.purchasePrice <= 0 || dto.salePrice <= 0) {
            outErrorMessage = "Los precios de compra y venta deben ser mayores a $0.";
            return false;
        }

        QSqlDatabase db = QSqlDatabase::database();
        if (!db.transaction()) {
            outErrorMessage = "No se pudo iniciar la transacción en la base de datos.";
            return false;
        }

        QSqlQuery q;

        // Step 1: Insert Core Vehicle
        q.prepare(R"(
            INSERT INTO vehicles (acquisition_type, vehicle_type_id, subtype_id, brand_id, model, year_model, color, mileage, serial_number)
            VALUES (:acqType, :vType, :subType, :brand, :model, :year, :color, :mileage, :serial)
            RETURNING folio;
        )");
        q.bindValue(":acqType", dto.acquisitionType);
        q.bindValue(":vType", dto.vehicleTypeId);
        q.bindValue(":subType", dto.subtypeId);
        q.bindValue(":brand", dto.brandId);
        q.bindValue(":model", dto.model);
        q.bindValue(":year", dto.yearModel);
        q.bindValue(":color", dto.color);
        q.bindValue(":mileage", dto.mileage);
        q.bindValue(":serial", dto.serialNumber);

        if (!q.exec() || !q.next()) {
            db.rollback();
            outErrorMessage = "Error al registrar la información base del vehículo.";
            return false;
        }
        outVehicleFolio = q.value(0).toInt();

        // Step 2: Insert Acquisition Details
        q.prepare(R"(
            INSERT INTO vehicle_acquisitions (vehicle_folio, seller_id, invoice_type, purchase_price, sale_price)
            VALUES (:folio, :seller, :invType, :purchase, :sale);
        )");
        q.bindValue(":folio", outVehicleFolio);
        q.bindValue(":seller", dto.sellerId);
        q.bindValue(":invType", dto.invoiceType);
        q.bindValue(":purchase", dto.purchasePrice);
        q.bindValue(":sale", dto.salePrice);

        if (!q.exec()) {
            db.rollback();
            outErrorMessage = "Error al registrar los datos de la adquisición.";
            return false;
        }

        // Step 3: Insert Vehicle Conditions Specs
        q.prepare(R"(
            INSERT INTO vehicle_conditions (vehicle_folio, fuel_type_id, cylinders, transmission, window_regulators, air_conditioning)
            VALUES (:folio, :fuel, :cyl, :trans, :win, :ac);
        )");
        q.bindValue(":folio", outVehicleFolio);
        q.bindValue(":fuel", dto.fuelTypeId);
        q.bindValue(":cyl", dto.cylinders);
        q.bindValue(":trans", dto.transmission);
        q.bindValue(":win", dto.windowRegulators);
        q.bindValue(":ac", dto.airConditioning);

        if (!q.exec()) {
            db.rollback();
            outErrorMessage = "Error al registrar las condiciones mecánicas.";
            return false;
        }

        // Step 4: Batch Insert Inspection Checklist
        for (auto it = dto.checklist.constBegin(); it != dto.checklist.constEnd(); ++it) {
            q.prepare(R"(
                INSERT INTO vehicle_inspection (vehicle_folio, element_id, state)
                VALUES (:folio, :element, :state);
            )");
            q.bindValue(":folio", outVehicleFolio);
            q.bindValue(":element", it.key());
            q.bindValue(":state", it.value());

            if (!q.exec()) {
                db.rollback();
                outErrorMessage = "Error al guardar el checklist de inspección.";
                return false;
            }
        }

        // Everything succeeded! Commit transaction
        db.commit();
        return true;
    }
};
```

## Summary Cheat Sheet for Developers

| Requirement | Use Direct QSqlQuery / Helper | Use Repository (DAO) | Use Business Service |
| :--- | :---: | :---: | :---: |
| Populate `QComboBox` from a catalog | **YES** | NO | NO |
| Populate a read-only `QTableView` grid | **YES** | NO | NO |
| Add a single row to a catalog table | NO | **YES** | NO |
| Simple lookup/fetch by ID on 1 table | NO | **YES** | NO |
| Save complex multi-step Wizard (e.g. Inventory) | NO | NO | **YES** |
| Requires SQL `TRANSACTION` (Rollback on error) | NO | NO | **YES** |
| Performs price or financial calculation | NO | NO | **YES** |