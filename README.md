# 🛒 Inventory Management System (C++ + SQL)

## 📌 Overview
The **Inventory Management System** is a console-based C++ application integrated with SQL (SQLite) that helps businesses efficiently manage their products, suppliers, and sales.  
It supports adding and updating stock, generating bills, and viewing reports with real-time database consistency.

---

## ✨ Features
- ➕ Add, update, and delete products  
- 🏢 Manage supplier details  
- 📦 Record purchases and update stock automatically  
- 🧾 Generate customer bills with sales tracking  
- 📊 Sales reports with SQL aggregates (`SUM`, `COUNT`)  
- ⚡ SQL Triggers to prevent negative stock and ensure data consistency  

---

## 🗂️ Database Schema
The system uses the following main tables:  
- **products** – Stores product details and stock  
- **suppliers** – Supplier information  
- **purchases** – Records purchase entries  
- **sales** – Customer sales records  
- **sale_items** – Itemized bill details  

---

## 🛠️ Tech Stack
- **C++17** (Application Logic)  
- **SQLite3** (Embedded Database)  
- **CMake/Make** (Build System)  

---

## 🚀 Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/your-username/inventory-management-system.git
cd inventory-management-system
