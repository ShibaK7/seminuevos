# DE LA HOZ Autos Seminuevos — Desktop UI Design System

This document outlines the official User Interface Design System for the **DE LA HOZ Autos Seminuevos** desktop application built with Qt (C++). All team members developing new screens, widgets, or components should adhere to these guidelines to ensure visual consistency across the platform.

---

## 📁 Style Sheet Files

| File | Resource path | Applied to |
| :--- | :--- | :--- |
| `global-style-clean.qss` | `:/styles/global-style-clean.qss` | The whole application, once the main window opens (`CompositionRoot`). |
| `login.qss` | `:/styles/login.qss` | The login window only. It is shown before the global sheet, which would override its background. |
| `vehicle-card.qss` | `:/styles/vehicle-card.qss` | Each inventory card (`VehicleItemList`), on the card itself so its rules win over the generic global ones. |

* **No `styleSheet` inside `.ui` files** and no `setStyleSheet()` for states: states are dynamic properties (`hasError`, `stepState`, `status`) matched by rules in these files. The `architecture` test rejects embedded style sheets.
* Style classes go in the `class` dynamic property (for example `primary`, `secondary`, `card`).

---

## 🎨 1. Color Palette & Design Tokens

The color system is derived from the official company branding. To prevent visual fatigue while keeping brand identity strong, **DE LA HOZ Royal Blue** is used for primary brand accents, and **DE LA HOZ Crimson Red** is reserved for high-priority alerts, errors, and status tags.

### Core Color Tokens

| Token Name | Hex Code / Value | Usage & Context |
| :--- | :--- | :--- |
| `color-primary` | `#0A25C9` | Primary brand color, main call-to-action buttons, price tags, selected menu items. |
| `color-primary-hover` | `#081CA2` | Hover state for primary buttons. |
| `color-primary-pressed` | `#06157B` | Active/Pressed state for primary buttons. |
| `color-primary-tint` | `rgba(10, 37, 201, 0.15)` | Soft transparent background for active sidebar menu items. |
| `color-accent-red` | `#D90429` | Error validation borders/text, danger buttons, alert status pills (`Apartado`). |
| `color-accent-red-hover` | `#B80322` | Hover state for danger buttons. |
| `color-neutral-dark` | `#212529` | Primary body text, page headers (`H1`), section headers (`H2`). |
| `color-neutral-medium` | `#495057` | Input field labels, secondary icons, subtext. |
| `color-neutral-muted` | `#6C757D` | Placeholder hints, secondary button borders, unselected tabs. |
| `color-neutral-border` | `#DEE2E6` | Card container borders, sidebar right divider, table grid lines. |
| `color-surface-canvas` | `#F4F6F9` | Main application background (behind content cards). |
| `color-surface-card` | `#FFFFFF` | Background for panels, vehicle cards, dialog modals, inputs, and sidebar. |
| `color-status-success-bg` | `#D1E7DD` | Background for `Disponible` status badge. |
| `color-status-success-fg` | `#0F5132` | Text color for `Disponible` status badge. |

---

## 🔤 2. Typography Rules

The typography system relies on system-native font stacks to ensure crisp vector rendering without requiring embedded binaries.

### Font Family Fallback Stack
* **Primary Sans-Serif:** `"Segoe UI", "Roboto", "Helvetica Neue", Arial, sans-serif`
* **Monospace / Financial:** `"Consolas", "Monaco", "Segoe UI", monospace` *(Used for VINs, prices, and vehicle specs)*

### Typography Hierarchy Table

| Level / Component | Font Size | Weight | Line / Spacing | Color Token | Usage |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Main Header (`H1`)** | `20pt` (26px) | `700` (Bold) | `-0.2px` | `#212529` | Top bar title (*"Inventario"*, *"Comercial"*). |
| **Section Header (`H2`)** | `15pt` (20px) | `700` (Bold) | `0px` | `#212529` | Modal titles, card section headers. |
| **Card / Subtitle (`H3`)** | `13pt` (16px) | `600` (Semi-Bold) | `0px` | `#495057` | Card titles, tab labels, group headers. |
| **Standard Input / Text** | `12pt` (13px) | `400` (Regular) | `0px` | `#212529` | `QLineEdit`, `QComboBox`, table text. |
| **Form Labels** | `9.5pt` (12.5px) | `600` (Semi-Bold) | `0px` | `#495057` | Labels placed directly above inputs. |
| **Placeholder Hints** | `9.5pt` (12.5px) | `400` (Regular) | `0px` | `#6C757D` | Search cues, input format hints. |
| **Error Messages** | `8.5pt` (11px) | `600` (Semi-Bold) | `0px` | `#D90429` | Inline text under invalid inputs (`class="field-error"`; `formsupport::addFieldErrorLabels()` creates them, never the `.ui`). |
| **Financial / Prices** | `16pt` (21px) | `800` (Extra-Bold)| `-0.5px` | `#0A25C9` | Vehicle prices, totals. |

---

## 🧩 3. Component Conventions & QSS Classes

To ensure styles apply consistently across team contributions, use **Dynamic Properties** (`setProperty("class", "...")`) or explicit `setObjectName` on C++ widgets.

### A. Buttons

#### 1. Primary Action Button (`class="primary"`)
Used for the single main action on a page (e.g., `+ Agregar Vehículo`, `Guardar`).
* **Background:** `#0A25C9`
* **Text:** `#FFFFFF` (Bold `10pt`)
* **Usage in C++:**
  ```cpp
  QPushButton *btn = new QPushButton("+ Agregar Vehículo", this);
  btn->setProperty("class", "primary");
  ```

## 📁 4. Project Setup & Stylesheet Loading

The global stylesheet is located in the resource bundle at:
`:/styles/global-style-clean.qss` (file `resources/styles/global-style-clean.qss`).

### Global Scope Setup (excluding Login Window)
`CompositionRoot::showMain()` (`src/app/compositionroot.cpp`) applies it to the whole application right after the login closes, so it never overrides the login design; the login uses its own `:/styles/login.qss`. See "Style Sheet Files" at the top of this document.
## 🛠️ 5. Rules for Developers Adding New Views
1. **Do NOT Hardcode Styles in C++:** Avoid calling `widget->setStyleSheet("color: red; ...")` directly in your view classes. Use `setProperty("class", "primary")` or `setObjectName("...")` and define rules in global-style-clean.qss or create a dedicated style sheet if needed.
2. **Container Wrappers:** Always place modular view contents inside a `QFrame` or `QWidget` with `setObjectName("cardPanel")` (or, in Designer, the `class` property set to `card`) to inherit the rounded white card container style.
3. **Dynamic Property Updates:** If you change a widget's property dynamically at runtime (e.g., `setProperty("hasError", true)`), force Qt to refresh rendering via:
    ```cpp
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    ```
