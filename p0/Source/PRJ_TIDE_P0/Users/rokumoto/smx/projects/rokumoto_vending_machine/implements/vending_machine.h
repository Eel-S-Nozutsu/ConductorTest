#pragma once
#include <memory>
#include "../events.h"
#include "../features.h"

namespace app {
namespace rokumoto {
using namespace vending_machine;

//--------------------------------------
// Deposit implement
//--------------------------------------
struct Deposit: ft::Deposit {
	int deposit_amount = 0;	// 現在の投入残高
	int refund_amount = 0;	// 返金額

    bool is_ready(const ev::Deposit& e) const override
	{
		return (deposit_amount + e.money) > 0;
    }

    bool has_deposit(int money = 1) const override
	{
		return deposit_amount >= money;
    }

    void buy(int price = 0) override
	{
		if (price > 0 && deposit_amount >= price)
			deposit_amount -= price;
    }

    void refund() override
	{
		refund_amount += deposit_amount;
		deposit_amount = 0;
    }

    void deposit(const ev::Deposit& e) override
	{
		deposit_amount += e.money;
    }
};

//--------------------------------------
// DispensingSlot implement
//--------------------------------------
struct DispensingSlot: ft::DispensingSlot {
	vector_type<string_type> purchases_;

	const vector_type<string_type>& purchases() const
	{
		return purchases_;
	}

    void dispense(string_type name) override
	{
		purchases_.push_back(name);
    }
};

//--------------------------------------
// Slot implement
//--------------------------------------
struct Slot: ft::Slot {
	string_type name;
	int price = 0;
	int stock = 0;
	std::shared_ptr<ft::SlotUI> p_ui;

    Slot(string_type name, int price, int stock)
		: name(name)
		, price(price)
		, stock(stock)
	{}

    bool has_deposit(const ft::Deposit& f0) const override
	{
        return f0.has_deposit(price);
    }

    bool has_stock() const override
	{
        return stock > 0;
    }

    void buy(ft::Deposit& f0, ft::DispensingSlot& f1) override
	{
		if (stock > 0) {
			f0.buy(price);
			f1.dispense(name);
			--stock;
		}
    }

    void update(ft::Deposit& f0) override
	{
		if (p_ui) {
			p_ui->set_info(ev::ProductInfo{name, price, stock});
			if (stock <= 0)
				p_ui->no_stock();
			else if (f0.has_deposit(price))
				p_ui->activate();
			else
				p_ui->standby();
		}
	}
};

//--------------------------------------
// Slots implement
//--------------------------------------
struct Slots: ft::Slots {
	vector_type<Slot> slots_;

	size_t size() const
	{
		return slots_.size();
	}

    bool can_buy(const ev::Buy& e, const ft::Deposit& f0) const override
	{
		return static_cast<size_t>(e.slot) < size()
			&& slots_[e.slot].has_stock()
			&& slots_[e.slot].has_deposit(f0);
    }

    bool has_slot(const ev::Buy& e) const override
	{
		return static_cast<size_t>(e.slot) < size() && slots_[e.slot].has_stock();
    }

    void buy(const ev::Buy& e, ft::Deposit& f0, ft::DispensingSlot& f1) override
	{
		if (static_cast<size_t>(e.slot) < size()) {
			auto& slot = slots_[e.slot];
			slot.buy(f0, f1);
		}
    }

    void update(ft::Deposit& f0) override
	{
		for (auto& slot: slots_)
			slot.update(f0);
	}
};

} // namespace rokumoto
} // namespace app
