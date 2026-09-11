#pragma once
#include <memory>
#include <algorithm>
#include "../events.h"
#include "../features.h"

namespace app {
using namespace shop;

struct Funds: ft::Funds {
    int money = 0;  // 所持金

    bool has_funds(int money) const override {
        return this->money >= money;
    }
    void spend(int money) override {
        this->money -= money;
    }
};

struct Item: ft::Item {
    int price = 0;  // 商品価格
    int stock = 0;  // 在庫数
	std::shared_ptr<ft::Item_UI> p_ui;
    
    bool has_funds(const ft::Funds& f0, int num = 1) const override {
        return f0.has_funds(price * num);    
    }
    
    bool has_stock(int num = 1) const override {
        return stock >= num;
    }

    void purchase(ft::Funds& f0, int num = 1) override {
        f0.spend(price * num);
        stock -= num;
    }

    void update(ft::Funds& f0, int num = 1, bool isFocused = false, bool isSelected = false) override {
        
        if(isFocused) {
            if(isSelected) {
                p_ui->quantityselect();
            }
            else {
                p_ui->focus();
            }
        } 
        else {
            if (has_stock(num)) {
                if (f0.has_funds(price * num)){
                    p_ui->normal();
                }
                else {
                    p_ui->locked();
                }
            } 
            else {
                p_ui->soldout();
            }
        }
    }

    void change_quantity(const ev::ChangeQuantity& e, ft::Funds& f0, ft::Selection& f1) override {
        // 購入できる数を計算するための処理
        int buyable_num = stock;
        if (price > 0){
            for(int i = 1; i <= stock; ++i) {
                if (!f0.has_funds(price * i)) {
                    buyable_num = i - 1;
                    break;
                }
            }
        }

        f1.change_quantity(e.quantity, buyable_num);
    }
};

struct Selection: ft::Selection {
    int num = 1;    // 購入個数
    
    bool has_funds(const ft::Item& f0,const ft::Funds& f1) const override {
        return f0.has_funds(f1, num);
    }
    
    bool has_stock(const ft::Item& f0) const override {
        return f0.has_stock(num);
    }

    void change_quantity(int quantity = 1, int buyable_num = 1) override {
        int new_quantity = num + quantity;
        if(new_quantity > buyable_num) {
            new_quantity = 1;
        }
        else if(new_quantity < 1) {
            new_quantity = buyable_num;
        }
        num = new_quantity;
    }

    void purchase(ft::Item& f0, ft::Funds& f1) override {
        f0.purchase(f1, num);
        reset();
    }

    void update(ft::Item& f0, ft::Funds& f1, bool isFocused = false, bool isSelected = false) override {
        f0.update(f1, num, isFocused, isSelected);
    }

    void reset() override {
        num = 1;
    }
};

struct ItemList: ft::ItemList {
    vector_type<Item> items;
    int selected_index = 0;
    bool isSelecting = false;
    
    bool can_buy(const ft::Selection& f0, const ft::Funds& f1) const override {
        return f0.has_funds(items[selected_index],f1) && has_stock(f0);
    }

    bool has_stock(const ft::Selection& f0) const override {
        return f0.has_stock(items[selected_index]);
    }
    
    void change_quantity(const ev::ChangeQuantity& e, ft::Selection& f0, ft::Funds& f1) override {
        items[selected_index].change_quantity(e, f1, f0);
    }

    void purchase(ft::Selection& f0, ft::Funds& f1) override {
        f0.purchase(items[selected_index], f1);
    }

    void update(ft::Selection& f0, ft::Funds& f1) override {
        for(size_t i = 0; i < items.size(); ++i) {
            bool isFocused = (i == static_cast<size_t>(selected_index));
            f0.update(items[i], f1, isFocused, isFocused && isSelecting);
        }
    }

    void focus(const ev::SelectItem& e) override {
        selected_index = e.item_num;
    }

    void set_selecting(bool isSelecting = false) override {
        this->isSelecting = isSelecting;
    }
};
} // namespace app