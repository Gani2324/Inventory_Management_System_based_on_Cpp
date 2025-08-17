#include <sqlite3.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <fstream>

static int exec_noresult(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (errMsg ? errMsg : "unknown") << "\n";
        if (errMsg) sqlite3_free(errMsg);
    }
    return rc;
}

static bool prepare_and_bind(sqlite3* db, const std::string& sql, sqlite3_stmt** stmt) {
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sql << "\n";
        std::cerr << "SQLite error: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    return true;
}

static void init_schema(sqlite3* db) {
    // Load schema.sql if present, else use embedded minimal schema (same content trimmed).
    std::ifstream in("sql/schema.sql");
    std::stringstream buffer;
    if (in) {
        buffer << in.rdbuf();
    } else {
        buffer << R"SQL(
            PRAGMA foreign_keys = ON;
            CREATE TABLE IF NOT EXISTS suppliers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                phone TEXT, email TEXT
            );
            CREATE TABLE IF NOT EXISTS products (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL UNIQUE,
                supplier_id INTEGER,
                unit_price REAL NOT NULL CHECK (unit_price >= 0),
                stock INTEGER NOT NULL DEFAULT 0 CHECK (stock >= 0),
                FOREIGN KEY (supplier_id) REFERENCES suppliers(id) ON DELETE SET NULL
            );
            CREATE TABLE IF NOT EXISTS purchases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                product_id INTEGER NOT NULL,
                qty INTEGER NOT NULL CHECK (qty > 0),
                cost_price REAL NOT NULL CHECK (cost_price >= 0),
                purchased_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (product_id) REFERENCES products(id) ON DELETE CASCADE
            );
            CREATE TABLE IF NOT EXISTS sales (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                customer TEXT,
                total REAL NOT NULL DEFAULT 0,
                created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
            );
            CREATE TABLE IF NOT EXISTS sale_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sale_id INTEGER NOT NULL,
                product_id INTEGER NOT NULL,
                qty INTEGER NOT NULL CHECK (qty > 0),
                price REAL NOT NULL CHECK (price >= 0),
                FOREIGN KEY (sale_id) REFERENCES sales(id) ON DELETE CASCADE,
                FOREIGN KEY (product_id) REFERENCES products(id) ON DELETE RESTRICT
            );
            CREATE TRIGGER IF NOT EXISTS trg_purchase_after_insert
            AFTER INSERT ON purchases
            BEGIN
                UPDATE products SET stock = stock + NEW.qty WHERE id = NEW.product_id;
            END;
            CREATE TRIGGER IF NOT EXISTS trg_sale_items_before_insert
            BEFORE INSERT ON sale_items
            BEGIN
                SELECT CASE
                    WHEN (SELECT stock FROM products WHERE id = NEW.product_id) < NEW.qty
                    THEN RAISE(ABORT, 'Insufficient stock for this product')
                END;
            END;
            CREATE TRIGGER IF NOT EXISTS trg_sale_items_after_insert
            AFTER INSERT ON sale_items
            BEGIN
                UPDATE products SET stock = stock - NEW.qty WHERE id = NEW.product_id;
            END;
            CREATE TRIGGER IF NOT EXISTS trg_sale_items_after_delete
            AFTER DELETE ON sale_items
            BEGIN
                UPDATE products SET stock = stock + OLD.qty WHERE id = OLD.product_id;
            END;
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
        )SQL";
    }
    if (exec_noresult(db, buffer.str()) != SQLITE_OK) {
        std::cerr << "Failed to initialize schema.\n";
    }
}

static void add_supplier(sqlite3* db) {
    std::string name, phone, email;
    std::cout << "Supplier name: ";
    std::getline(std::cin, name);
    std::cout << "Phone (optional): ";
    std::getline(std::cin, phone);
    std::cout << "Email (optional): ";
    std::getline(std::cin, email);

    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, "INSERT INTO suppliers(name, phone, email) VALUES(?,?,?)", &stmt)) return;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, phone.empty()? nullptr: phone.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, email.empty()? nullptr: email.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) std::cout << "Supplier added.\n";
    else std::cerr << "Failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(stmt);
}

static std::optional<int> get_supplier_id(sqlite3* db, const std::string& name) {
    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, "SELECT id FROM suppliers WHERE name = ?", &stmt)) return std::nullopt;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return id;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

static void add_product(sqlite3* db) {
    std::string name, supplierName;
    double price;
    std::cout << "Product name: ";
    std::getline(std::cin, name);
    std::cout << "Supplier name (existing or leave blank): ";
    std::getline(std::cin, supplierName);
    std::cout << "Unit selling price: ";
    std::cin >> price;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::optional<int> supId = std::nullopt;
    if (!supplierName.empty()) supId = get_supplier_id(db, supplierName);

    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, "INSERT INTO products(name, supplier_id, unit_price) VALUES(?,?,?)", &stmt)) return;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    if (supId) sqlite3_bind_int(stmt, 2, *supId);
    else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_double(stmt, 3, price);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) std::cout << "Product added.\n";
    else std::cerr << "Failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(stmt);
}

static void receive_stock(sqlite3* db) {
    int productId, qty;
    double costPrice;
    std::cout << "Product ID: ";
    std::cin >> productId;
    std::cout << "Quantity received: ";
    std::cin >> qty;
    std::cout << "Cost price per unit: ";
    std::cin >> costPrice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, "INSERT INTO purchases(product_id, qty, cost_price) VALUES(?,?,?)", &stmt)) return;
    sqlite3_bind_int(stmt, 1, productId);
    sqlite3_bind_int(stmt, 2, qty);
    sqlite3_bind_double(stmt, 3, costPrice);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) std::cout << "Stock received. (Triggers updated inventory.)\n";
    else std::cerr << "Failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_finalize(stmt);
}

static void list_inventory(sqlite3* db) {
    const char* sql =
        "SELECT p.id, p.name, COALESCE(s.name,'-') as supplier, p.unit_price, p.stock "
        "FROM products p LEFT JOIN suppliers s ON s.id = p.supplier_id "
        "ORDER BY p.id";
    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, sql, &stmt)) return;
    std::cout << std::left << std::setw(5) << "ID" << std::setw(25) << "Product"
              << std::setw(20) << "Supplier" << std::setw(12) << "Price"
              << std::setw(8) << "Stock" << "\n";
    std::cout << std::string(70,'-') << "\n";
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
        std::string supplier = reinterpret_cast<const char*>(sqlite3_column_text(stmt,2));
        double price = sqlite3_column_double(stmt,3);
        int stock = sqlite3_column_int(stmt,4);
        std::cout << std::left << std::setw(5) << id << std::setw(25) << name
                  << std::setw(20) << supplier << std::setw(12) << std::fixed << std::setprecision(2) << price
                  << std::setw(8) << stock << "\n";
    }
    sqlite3_finalize(stmt);
}

static void low_stock(sqlite3* db) {
    int threshold;
    std::cout << "Low-stock threshold: ";
    std::cin >> threshold;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    const char* sql =
        "SELECT id, name, stock FROM products WHERE stock < ? ORDER BY stock ASC";
    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, sql, &stmt)) return;
    sqlite3_bind_int(stmt, 1, threshold);

    std::cout << std::left << std::setw(5) << "ID" << std::setw(25) << "Product"
              << std::setw(8) << "Stock" << "\n";
    std::cout << std::string(40,'-') << "\n";
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::cout << std::left << std::setw(5) << sqlite3_column_int(stmt,0)
                  << std::setw(25) << reinterpret_cast<const char*>(sqlite3_column_text(stmt,1))
                  << std::setw(8) << sqlite3_column_int(stmt,2) << "\n";
    }
    sqlite3_finalize(stmt);
}

static void update_price(sqlite3* db) {
    int productId;
    double newPrice;
    std::cout << "Product ID: ";
    std::cin >> productId;
    std::cout << "New unit price: ";
    std::cin >> newPrice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, "UPDATE products SET unit_price = ? WHERE id = ?", &stmt)) return;
    sqlite3_bind_double(stmt, 1, newPrice);
    sqlite3_bind_int(stmt, 2, productId);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE && sqlite3_changes(db) > 0) std::cout << "Price updated.\n";
    else std::cerr << "Failed or product not found.\n";
    sqlite3_finalize(stmt);
}

static void sales_summary(sqlite3* db) {
    std::string from, to;
    std::cout << "From date (YYYY-MM-DD) or blank: ";
    std::getline(std::cin, from);
    std::cout << "To date (YYYY-MM-DD) or blank: ";
    std::getline(std::cin, to);

    std::string sql =
        "SELECT COUNT(*) AS num_sales, "
        "       COALESCE(SUM(total),0) AS gross_revenue "
        "FROM sales WHERE 1=1 ";
    if (!from.empty()) sql += " AND date(created_at) >= date(?)";
    if (!to.empty())   sql += " AND date(created_at) <= date(?)";

    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, sql, &stmt)) return;
    int idx = 1;
    if (!from.empty()) sqlite3_bind_text(stmt, idx++, from.c_str(), -1, SQLITE_TRANSIENT);
    if (!to.empty())   sqlite3_bind_text(stmt, idx++, to.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        double revenue = sqlite3_column_double(stmt, 1);
        std::cout << "Sales count: " << count << "\nTotal revenue: " << std::fixed << std::setprecision(2) << revenue << "\n";
    } else {
        std::cerr << "Failed: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
}

static void make_sale(sqlite3* db) {
    std::string customer;
    std::cout << "Customer name (optional): ";
    std::getline(std::cin, customer);

    // Create sale
    sqlite3_stmt* createSale = nullptr;
    if (!prepare_and_bind(db, "INSERT INTO sales(customer) VALUES(?)", &createSale)) return;
    if (customer.empty()) sqlite3_bind_null(createSale, 1);
    else sqlite3_bind_text(createSale, 1, customer.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(createSale);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to create sale: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(createSale);
        return;
    }
    sqlite3_finalize(createSale);
    int saleId = (int)sqlite3_last_insert_rowid(db);
    std::cout << "Created sale ID: " << saleId << "\n";

    // Add items loop
    while (true) {
        std::string ans;
        std::cout << "Add item? (y/n): ";
        std::getline(std::cin, ans);
        if (ans != "y" && ans != "Y") break;

        int productId, qty;
        double price;
        std::cout << "Product ID: ";
        std::cin >> productId;
        std::cout << "Quantity: ";
        std::cin >> qty;
        std::cout << "Price per unit (leave 0 to use product's unit_price): ";
        std::cin >> price;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (price <= 0) {
            sqlite3_stmt* stmt = nullptr;
            if (prepare_and_bind(db, "SELECT unit_price FROM products WHERE id = ?", &stmt)) {
                sqlite3_bind_int(stmt, 1, productId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    price = sqlite3_column_double(stmt, 0);
                } else {
                    std::cerr << "Product not found.\n";
                    sqlite3_finalize(stmt);
                    continue;
                }
                sqlite3_finalize(stmt);
            }
        }

        sqlite3_stmt* addItem = nullptr;
        if (!prepare_and_bind(db, "INSERT INTO sale_items(sale_id, product_id, qty, price) VALUES(?,?,?,?)", &addItem)) return;
        sqlite3_bind_int(addItem, 1, saleId);
        sqlite3_bind_int(addItem, 2, productId);
        sqlite3_bind_int(addItem, 3, qty);
        sqlite3_bind_double(addItem, 4, price);

        rc = sqlite3_step(addItem);
        if (rc == SQLITE_DONE) {
            std::cout << "Item added.\n";
        } else {
            std::cerr << "Failed to add item: " << sqlite3_errmsg(db) << "\n";
        }
        sqlite3_finalize(addItem);
    }

    // Print bill
    std::cout << "\n===== BILL (Sale ID: " << saleId << ") =====\n";
    std::cout << std::left << std::setw(5) << "ID" << std::setw(25) << "Product"
              << std::setw(8) << "Qty" << std::setw(12) << "Price"
              << std::setw(12) << "Line Total" << "\n";
    std::cout << std::string(70,'-') << "\n";

    const char* billSql =
        "SELECT si.id, p.name, si.qty, si.price, (si.qty*si.price) AS line_total "
        "FROM sale_items si JOIN products p ON p.id = si.product_id "
        "WHERE si.sale_id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (!prepare_and_bind(db, billSql, &stmt)) return;
    sqlite3_bind_int(stmt, 1, saleId);
    double total = 0.0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
        int qty = sqlite3_column_int(stmt,2);
        double price = sqlite3_column_double(stmt,3);
        double line = sqlite3_column_double(stmt,4);
        total += line;
        std::cout << std::left << std::setw(5) << id << std::setw(25) << name
                  << std::setw(8) << qty << std::setw(12) << std::fixed << std::setprecision(2) << price
                  << std::setw(12) << line << "\n";
    }
    sqlite3_finalize(stmt);

    // Get authoritative total from sales table (maintained by trigger)
    sqlite3_stmt* tstmt = nullptr;
    if (prepare_and_bind(db, "SELECT total FROM sales WHERE id = ?", &tstmt)) {
        sqlite3_bind_int(tstmt, 1, saleId);
        if (sqlite3_step(tstmt) == SQLITE_ROW) {
            total = sqlite3_column_double(tstmt, 0);
        }
        sqlite3_finalize(tstmt);
    }
    std::cout << std::string(70,'-') << "\n";
    std::cout << "TOTAL: " << std::fixed << std::setprecision(2) << total << "\n";
    std::cout << "=====================================\n\n";
}

static void menu() {
    std::cout << "\n=== Inventory Management (C++ + SQLite) ===\n"
              << "1) Add Supplier\n"
              << "2) Add Product\n"
              << "3) Receive Stock (Purchase)\n"
              << "4) Make Sale (Generate Bill)\n"
              << "5) Update Product Price\n"
              << "6) List Inventory\n"
              << "7) Low-Stock Report\n"
              << "8) Sales Summary\n"
              << "9) Exit\n"
              << "Choice: ";
}

int main() {
    sqlite3* db = nullptr;
    int rc = sqlite3_open("inventory.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << "\n";
        return 1;
    }
    exec_noresult(db, "PRAGMA foreign_keys = ON;");
    init_schema(db);

    while (true) {
        menu();
        int choice = 0;
        if (!(std::cin >> choice)) break;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        switch (choice) {
            case 1: add_supplier(db); break;
            case 2: add_product(db); break;
            case 3: receive_stock(db); break;
            case 4: make_sale(db); break;
            case 5: update_price(db); break;
            case 6: list_inventory(db); break;
            case 7: low_stock(db); break;
            case 8: sales_summary(db); break;
            case 9: std::cout << "Bye!\n"; sqlite3_close(db); return 0;
            default: std::cout << "Invalid choice.\n"; break;
        }
    }
    sqlite3_close(db);
    return 0;
}
