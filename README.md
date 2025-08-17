# 🛒 Inventory Management System (C++ + SQLite)

## 📌 Overview
The **Inventory Management System** is a console-based application written in **C++** with an integrated **SQLite database**.  
It is designed to help manage day-to-day operations of inventory such as maintaining product stock, handling suppliers, recording purchases, and generating customer bills.

---

## ✨ Key Features
- ➕ **Add / Update / Delete Products**  
- 🏢 **Manage Suppliers**  
- 📦 **Record Purchases** with automatic stock updates  
- 🧾 **Generate Bills** with itemized details  
- 📊 **Sales Reports** using SQL queries (`SUM`, `COUNT`)  
- ⚡ **SQL Triggers** ensure data consistency and prevent negative stock  

---

## 🗂️ Database Design
The project uses the following tables:
- **products** – Product details, stock levels, pricing  
- **suppliers** – Supplier information  
- **purchases** – Purchase records (restocking)  
- **sales** – Customer order details  
- **sale_items** – Line items for each bill  

---

## 🛠️ Technologies Used
- **C++17** for application logic  
- **SQLite3** for database management  
- **CMake / g++** for compilation  

---

## 🚀 Installation & Setup

### 1. Clone Repository
```bash
git clone https://github.com/Gani2324/Inventory_Management_System_based_on_Cpp.git
cd Inventory_Management_System_based_on_Cpp
