#include "liquidrextok.hpp"

void liquidrextok::create(const name &issuer, const asset &maximum_supply)
{
	require_auth(_self);

	auto sym = maximum_supply.symbol;
	check(sym.is_valid(), "invalid symbol name");
	check(maximum_supply.is_valid(), "invalid supply");
	// check(maximum_supply.amount > 0, "max-supply must be positive");

	stats statstable(_self, sym.code().raw());
	auto existing = statstable.find(sym.code().raw());
	check(existing == statstable.end(), "token with symbol already exists");

	statstable.emplace(_self, [&](auto &s)
					   {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer; });
}

void liquidrextok::issue(const name &to, const asset &quantity, const string &memo)
{
	auto sym = quantity.symbol;
	check(sym.is_valid(), "invalid symbol name");
	check(memo.size() <= 256, "memo has more than 256 bytes");

	stats statstable(_self, sym.code().raw());
	auto existing = statstable.find(sym.code().raw());
	check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
	const auto &st = *existing;
	check(to == st.issuer, "tokens can only be issued to issuer account");

	require_auth(st.issuer);
	check(quantity.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must issue positive quantity");

	check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
	// check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

	statstable.modify(st, same_payer, [&](auto &s)
					  { s.supply += quantity; });

	add_balance(st.issuer, quantity, st.issuer);
}

void liquidrextok::retire(const asset &quantity, const string &memo)
{
	auto sym = quantity.symbol;
	check(sym.is_valid(), "invalid symbol name");
	check(memo.size() <= 256, "memo has more than 256 bytes");

	stats statstable(_self, sym.code().raw());
	auto existing = statstable.find(sym.code().raw());
	check(existing != statstable.end(), "token with symbol does not exist");
	const auto &st = *existing;

	require_auth(st.issuer);
	check(quantity.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must retire positive quantity");

	check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

	statstable.modify(st, same_payer, [&](auto &s)
					  { s.supply -= quantity; });

	sub_balance(st.issuer, quantity);
}

void liquidrextok::transfer(const name from,
							const name to,
							const asset quantity,
							const string memo)
{
	check(from != to, "cannot transfer to self");
	require_auth(from);
	check(is_account(to), "to account does not exist");
	auto sym = quantity.symbol.code();
	stats statstable(_self, sym.raw());
	const auto &st = statstable.get(sym.raw());

	require_recipient(from);
	require_recipient(to);

	check(quantity.is_valid(), "invalid quantity");
	check(quantity.amount > 0, "must transfer positive quantity");
	check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
	// check(memo.size() <= 256, "memo has more than 256 bytes");

	auto payer = has_auth(to) ? to : from;

	sub_balance(from, quantity);
	add_balance(to, quantity, payer);

	if (to == _self)
	{
		liquidrextok::retire_action retire(_self, {_self, "active"_n});
		liquidrextok::rex_sellrex_action sellrex(EOSIO_CONTRACT, {_self, "active"_n});
		liquidrextok::redeemrex_action redeemrex(_self, {_self, "active"_n});

		retire.send(quantity, "redeem REX tokens");
		sellrex.send(_self, quantity);
		redeemrex.send(from);
	}
}

void liquidrextok::sub_balance(const name &owner, const asset &value)
{
	accounts from_acnts(_self, owner.value);

	const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
	check(from.balance.amount >= value.amount, "overdrawn balance (liquidrextok)");

	from_acnts.modify(from, owner, [&](auto &a)
					  { a.balance -= value; });
}

void liquidrextok::add_balance(const name &owner, const asset &value, const name &ram_payer)
{
	accounts to_acnts(_self, owner.value);
	auto to = to_acnts.find(value.symbol.code().raw());
	if (to == to_acnts.end())
	{
		to_acnts.emplace(ram_payer, [&](auto &a)
						 { a.balance = value; });
	}
	else
	{
		to_acnts.modify(to, same_payer, [&](auto &a)
						{ a.balance += value; });
	}
}

void liquidrextok::open(const name &owner, const symbol &symbol, const name &ram_payer)
{
	require_auth(ram_payer);

	check(is_account(owner), "owner account does not exist");

	auto sym_code_raw = symbol.code().raw();
	stats statstable(_self, sym_code_raw);
	const auto &st = statstable.get(sym_code_raw, "symbol does not exist");
	check(st.supply.symbol == symbol, "symbol precision mismatch");

	accounts acnts(_self, owner.value);
	auto it = acnts.find(sym_code_raw);
	if (it == acnts.end())
	{
		acnts.emplace(ram_payer, [&](auto &a)
					  { a.balance = asset{0, symbol}; });
	}
}

void liquidrextok::close(const name &owner, const symbol &symbol)
{
	require_auth(owner);
	accounts acnts(_self, owner.value);
	auto it = acnts.find(symbol.code().raw());
	check(it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect.");
	check(it->balance.amount == 0, "Cannot close because the balance is not zero.");
	acnts.erase(it);
}

void liquidrextok::test(const name &from)
{

}

int64_t liquidrextok::get_core_balance()
{
	accounts accs(EOSIO_TOKEN_CONTRACT, _self.value);

	int64_t balance = 0;
	auto it = accs.find(EOSIO_CORE_SYMBOL.code().raw());
	if (it != accs.end())
	{
		balance = it->balance.amount;
	}

	return balance;
}

int64_t liquidrextok::get_rex_balance()
{
	liquidrextok::rex_balance_table rexbalance(EOSIO_CONTRACT, EOSIO_CONTRACT.value);
	
	int64_t balance = 0;
	auto it = rexbalance.find(_self.value);
	if (it != rexbalance.end())
	{
		balance = it->rex_balance.amount;
	}
	
	return balance;
}

void liquidrextok::issuerex(name from, name to, asset quantity, string memo)
{
	if (from == _self)
		return; // sending tokens, ignore

	if (from == EOSIO_REX_CONTRACT)
		return; // ignore redeeming from rex

	check(to == _self, "stop trying to hack the contract");
	check(quantity.amount > 0, "quantity amount must be greater than zero");

	int64_t balance = this->get_rex_balance();

	liquidrextok::rex_deposit_action deposit(EOSIO_CONTRACT, {_self, "active"_n});
	liquidrextok::rex_buyrex_action buyrex(EOSIO_CONTRACT, {_self, "active"_n});
	liquidrextok::issuerex2_action issuerex2(_self, {_self, "active"_n});

	deposit.send(_self, quantity);
	buyrex.send(_self, quantity);
	issuerex2.send(from, balance);
}

void liquidrextok::issuerex2(name recipient, int64_t rex_balance)
{
	require_auth(_self);

	int64_t balance = this->get_rex_balance();

	check(balance > rex_balance, "rex balance did not increase");

	eosio::asset quantity(balance - rex_balance, eosio::symbol("REX", 4));

	liquidrextok::issue_action issue(_self, {_self, "active"_n});
	liquidrextok::transfer_action transfer(_self, {_self, "active"_n});

	issue.send(_self, quantity, "mint new tokens");
	transfer.send(_self, recipient, quantity, "transfer new tokens to recipient");
}

void liquidrextok::redeemrex(name recipient) 
{
	require_auth(_self);

	int64_t balance = this->get_core_balance();

	liquidrextok::rex_fund_table rexfund(EOSIO_CONTRACT, EOSIO_CONTRACT.value);
	auto it = rexfund.find(_self.value);
	check(it != rexfund.end(), "no rexfund found");

	liquidrextok::rex_withdraw_action withdraw(EOSIO_CONTRACT, {_self, "active"_n});
	liquidrextok::transfer_action transfer(EOSIO_TOKEN_CONTRACT, {_self, "active"_n});

	withdraw.send(_self, it->balance);
	transfer.send(_self, recipient, it->balance, "redeem REX tokens");
}

extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
	if (code != receiver)
	{
		if (code == EOSIO_TOKEN_CONTRACT.value && action == ("transfer"_n).value)
		{
			eosio::execute_action(eosio::name(receiver), eosio::name(code), &liquidrextok::issuerex);
		}
	}
	else if (code == receiver)
	{
		//
		// Self dispatched actions (callable contract methods)
		//
		switch (action)
		{
			EOSIO_DISPATCH_HELPER(liquidrextok, (create)(issue)(retire)(transfer)(open)(close)(test)(issuerex2)(redeemrex));
		}
	}
}
