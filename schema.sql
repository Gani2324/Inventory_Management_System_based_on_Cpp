
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS suppliers (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    phone       TEXT,
    email       TEXT
);

CREATE TABLE IF NOT EXISTS products (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL UNIQUE,
    supplier_id INTEGER,
    unit_price  REAL NOT NULL CHECK (unit_price >= 0),
    stock       INTEGER NOT NULL DEFAULT 0 CHECK (stock >= 0),
    FOREIGN KEY (supplier_id) REFERENCES suppliers(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS purchases (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    product_id   INTEGER NOT NULL,
    qty          INTEGER NOT NULL CHECK (qty > 0),
    cost_price   REAL NOT NULL CHECK (cost_price >= 0),
    purchased_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (product_id) REFERENCES products(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS sales (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    customer   TEXT,
    total      REAL NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS sale_items (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    sale_id    INTEGER NOT NULL,
    product_id INTEGER NOT NULL,
    qty        INTEGER NOT NULL CHECK (qty > 0),
    price      REAL NOT NULL CHECK (price >= 0),
    FOREIGN KEY (sale_id) REFERENCES sales(id) ON DELETE CASCADE,
    FOREIGN KEY (product_id) REFERENCES products(id) ON DELETE RESTRICT
);

-- Useful indexes
CREATE INDEX IF NOT EXISTS idx_products_supplier ON products(supplier_id);
CREATE INDEX IF NOT EXISTS idx_purchases_product ON purchases(product_id);
CREATE INDEX IF NOT EXISTS idx_sale_items_sale ON sale_items(sale_id);
CREATE INDEX IF NOT EXISTS idx_sale_items_product ON sale_items(product_id);

-- Trigger: stock increases after purchase
CREATE TRIGGER IF NOT EXISTS trg_purchase_after_insert
AFTER INSERT ON purchases
BEGIN
    UPDATE products SET stock = stock + NEW.qty WHERE id = NEW.product_id;
END;

-- Trigger: prevent negative stock BEFORE inserting sale_items
CREATE TRIGGER IF NOT EXISTS trg_sale_items_before_insert
BEFORE INSERT ON sale_items
BEGIN
    SELECT
        CASE
            WHEN (SELECT stock FROM products WHERE id = NEW.product_id) < NEW.qty
            THEN RAISE(ABORT, 'Insufficient stock for this product')
        END;
END;

-- Trigger: stock decreases after a sale item is inserted
CREATE TRIGGER IF NOT EXISTS trg_sale_items_after_insert
AFTER INSERT ON sale_items
BEGIN
    UPDATE products SET stock = stock - NEW.qty WHERE id = NEW.product_id;
END;

-- Trigger: restore stock if sale item is deleted
CREATE TRIGGER IF NOT EXISTS trg_sale_items_after_delete
AFTER DELETE ON sale_items
BEGIN
    UPDATE products SET stock = stock + OLD.qty WHERE id = OLD.product_id;
END;

-- Trigger: keep sales.total in sync (after insert & delete)
CREATE TRIGGER IF NOT EXISTS trg_sale_items_total_after_insert
AFTER INSERT ON sale_items
BEGIN
    UPDATE sales
    SET total = COALESCE((SELECT SUM(qty * price) FROM sale_items WHERE sale_id = NEW.sale_id), 0)
    WHERE id = NEW.sale_id;
END;

CREATE TRIGGER IF NOT EXISTS trg_sale_items_total_after_delete
AFTER DELETE ON sale_items
BEGIN
    UPDATE sales
    SET total = COALESCE((SELECT SUM(qty * price) FROM sale_items WHERE sale_id = OLD.sale_id), 0)
    WHERE id = OLD.sale_id;
END;
