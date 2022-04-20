#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/crypto.hpp>
#include <eosio/system.hpp>

#include <string>
#include <vector>
#include <memory>

using namespace std;
using namespace eosio;

#define EOSIO_CONTRACT ("eosio"_n)
#define EOSIO_REX_CONTRACT ("eosio.rex"_n)
#define EOSIO_TOKEN_CONTRACT ("eosio.token"_n)
#define EOSIO_CORE_SYMBOL (eosio::symbol("TLOS", 4))

CONTRACT liquidrextok : public contract
{
public:
    using eosio::contract::contract;

    /**
     * Allows `issuer` account to create a token in supply of `maximum_supply`. If validation is successful a new entry in statstable for token symbol scope gets created.
     *
     * @param issuer - the account that creates the token,
     * @param maximum_supply - the maximum supply set for the token created.
     *
     * @pre Token symbol has to be valid,
     * @pre Token symbol must not be already created,
     * @pre maximum_supply has to be smaller than the maximum supply allowed by the system: 1^62 - 1.
     * @pre Maximum supply must be positive;
     */
    ACTION create(const name &issuer,
                  const asset &maximum_supply);
    /**
     *  This action issues to `to` account a `quantity` of tokens.
     *
     * @param to - the account to issue tokens to, it must be the same as the issuer,
     * @param quntity - the amount of tokens to be issued,
     * @memo - the memo string that accompanies the token issue transaction.
     */
    ACTION issue(const name &to, const asset &quantity, const string &memo);

    /**
     * The opposite for create action, if all validations succeed,
     * it debits the statstable.supply amount.
     *
     * @param quantity - the quantity of tokens to retire,
     * @param memo - the memo string to accompany the transaction.
     */
    ACTION retire(const asset &quantity, const string &memo);

    /**
     * Allows `from` account to transfer to `to` account the `quantity` tokens.
     * One account is debited and the other is credited with quantity tokens.
     *
     * @param from - the account to transfer from,
     * @param to - the account to be transferred to,
     * @param quantity - the quantity of tokens to be transferred,
     * @param memo - the memo string to accompany the transaction.
     */
    ACTION transfer(const name from,
                    const name to,
                    const asset quantity,
                    const string memo);
    /**
     * Allows `ram_payer` to create an account `owner` with zero balance for
     * token `symbol` at the expense of `ram_payer`.
     *
     * @param owner - the account to be created,
     * @param symbol - the token to be payed with by `ram_payer`,
     * @param ram_payer - the account that supports the cost of this action.
     *
     * More information can be read [here](https://github.com/EOSIO/eosio.contracts/issues/62)
     * and [here](https://github.com/EOSIO/eosio.contracts/issues/61).
     */
    ACTION open(const name &owner, const symbol &symbol, const name &ram_payer);

    /**
     * This action is the opposite for open, it closes the account `owner`
     * for token `symbol`.
     *
     * @param owner - the owner account to execute the close action for,
     * @param symbol - the symbol of the token to execute the close action for.
     *
     * @pre The pair of owner plus symbol has to exist otherwise no action is executed,
     * @pre If the pair of owner plus symbol exists, the balance has to be zero.
     */
    ACTION close(const name &owner, const symbol &symbol);

    ACTION test(const name &from);

    // Public but not a directly callable action
    // Called indirectly by sending TLOS to this contract
    ACTION issuerex(name from, name to, asset quantity, string memo);
    ACTION issuerex2(name recipient, int64_t rex_balance);
    ACTION redeemrex(name recipient);

    // Dummy functions for action wrapper templates
    ACTION log(string message) {}
    ACTION dummydeposit(const name &owner, const asset &amount) {}
    ACTION dummywithdrw(const name &owner, const asset &amount) {}
    ACTION dummybuyrex(const name &from, const asset &amount) {}
    ACTION dummysellrex(const name &from, const asset &amount) {}

    static asset get_supply(const name &token_contract_account, const symbol_code &sym_code)
    {
        stats statstable(token_contract_account, sym_code.raw());
        const auto &st = statstable.get(sym_code.raw());
        return st.supply;
    }

    static asset get_balance(const name &token_contract_account, const name &owner, const symbol_code &sym_code)
    {
        accounts accountstable(token_contract_account, owner.value);
        const auto &ac = accountstable.get(sym_code.raw());
        return ac.balance;
    }

    using create_action = eosio::action_wrapper<"create"_n, &liquidrextok::create>;
    using issue_action = eosio::action_wrapper<"issue"_n, &liquidrextok::issue>;
    using retire_action = eosio::action_wrapper<"retire"_n, &liquidrextok::retire>;
    using transfer_action = eosio::action_wrapper<"transfer"_n, &liquidrextok::transfer>;
    using open_action = eosio::action_wrapper<"open"_n, &liquidrextok::open>;
    using close_action = eosio::action_wrapper<"close"_n, &liquidrextok::close>;
    using log_action = eosio::action_wrapper<"log"_n, &liquidrextok::log>;

    // Interal Use
    using issuerex2_action = eosio::action_wrapper<"issuerex2"_n, &liquidrextok::issuerex2>;
    using redeemrex_action = eosio::action_wrapper<"redeemrex"_n, &liquidrextok::redeemrex>;

    // REX
    using rex_deposit_action = eosio::action_wrapper<"deposit"_n, &liquidrextok::dummydeposit>;
    using rex_withdraw_action = eosio::action_wrapper<"withdraw"_n, &liquidrextok::dummywithdrw>;
    using rex_buyrex_action = eosio::action_wrapper<"buyrex"_n, &liquidrextok::dummybuyrex>;
    using rex_sellrex_action = eosio::action_wrapper<"sellrex"_n, &liquidrextok::dummysellrex>;

private:
    struct [[eosio::table]] account
    {
        asset balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };

    struct [[eosio::table]] currency_stats
    {
        asset supply;
        asset max_supply;
        name issuer;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };

    // https://github.com/telosnetwork/telos.contracts/blob/master/contracts/eosio.system/include/eosio.system/eosio.system.hpp#L473
    /**
     * `rex_balance` structure underlying the rex balance table.
     *
     * @details A rex balance table entry is defined by:
     * - `version` defaulted to zero,
     * - `owner` the owner of the rex fund,
     * - `vote_stake` the amount of CORE_SYMBOL currently included in owner's vote,
     * - `rex_balance` the amount of REX owned by owner,
     * - `matured_rex` matured REX available for selling.
     */
    struct [[eosio::table, eosio::contract("eosio.system")]] rex_balance
    {
        uint8_t version = 0;
        name owner;
        asset vote_stake;
        asset rex_balance;
        int64_t matured_rex = 0;
        std::deque<std::pair<time_point_sec, int64_t>> rex_maturities; /// REX daily maturity buckets

        uint64_t primary_key() const { return owner.value; }
    };

    // `rex_fund` structure underlying the rex fund table. A rex fund table entry is defined by:
    // - `version` defaulted to zero,
    // - `owner` the owner of the rex fund,
    // - `balance` the balance of the fund.
    struct [[eosio::table, eosio::contract("eosio.system")]] rex_fund
    {
        uint8_t version = 0;
        name owner;
        asset balance;

        uint64_t primary_key() const { return owner.value; }
    };

    /**
     * Rex balance table
     *
     * @details The rex balance table is storing all the `rex_balance`s instances.
     */
    typedef eosio::multi_index<"rexbal"_n, rex_balance> rex_balance_table;
    typedef eosio::multi_index<"rexfund"_n, rex_fund> rex_fund_table;

    typedef eosio::multi_index<"accounts"_n, account> accounts;
    typedef eosio::multi_index<"stat"_n, currency_stats> stats;

    void sub_balance(const name &owner, const asset &value);
    void add_balance(const name &owner, const asset &value, const name &ram_payer);

    int64_t get_rex_balance();
    int64_t get_core_balance();
};