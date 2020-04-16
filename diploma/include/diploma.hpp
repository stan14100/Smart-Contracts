#pragma once

#include <eosio/asset.hpp>
#include <eosio/symbol.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>
#include <string>
#include <vector>
#include "functions.hpp"


using namespace std;
using namespace eosio;
using namespace utility;

class [[eosio::contract("diploma")]] diploma :public eosio::contract {
   public:
     const int WEEK_SEC = 3600*24*7;

    [[eosio::action]]
    void create(name issuer,
                name event,
                name ticket_name,
                bool burnable,
                bool sellable,
                bool transferable,
                asset price,
                double max_deviation,
                double sale_split,
                string base_uri,
                asset max_supply,
                vector<name> validators);


    [[eosio::action]]
    void issue(name to,
               name event,
               name ticket_name,
               asset quantity,
               string relative_uri,
               string memo);

    [[eosio::action]]
    void setconfig(string version);

    [[eosio::action]]
    void burn(name owner, vector<uint64_t> ticket_ids);

    [[eosio::action]]
    void transfer(name from, name to, vector<uint64_t> ticket_ids, string memo);

    [[eosio::action]]
    void listsale(name seller, name event, name ticket_name, vector<uint64_t> ticket_ids, asset net_sale_price);

    [[eosio::action]]
    void closesale(name seller, uint64_t ticket_id);

    [[eosio::on_notify("eosio.token::transfer")]]
    void buy(name from, name to, asset quantity, string memo);

    [[eosio::action]]
    void invalidate(name validator, name event, name owner, uint64_t ticket_id,string validation_stamp_uri);

    diploma(name receiver, name code, datastream<const char*> ds): contract(receiver, code, ds) {}

     struct [[eosio::table]] tokenconfigs {
        name standard;
        string version;
        uint64_t event_ticket_id;
     };

     struct [[eosio::table]] event {
        name event;
        name creator;

        uint64_t primary_key() const { return event.value; }
     };

     // scope is event
     struct [[eosio::table]] ticket_stat {
        uint64_t event_ticket_id;
        bool burnable;
        bool sellable;
        bool transferable;
        name issuer;
        name ticket_name;
        asset price;
        double max_deviation;
        asset max_supply;
        asset current_supply;
        asset issued_supply;
        double sale_split;
        string base_uri;
        vector<name> validators;

        uint64_t primary_key() const { return ticket_name.value; }
     };

     //scope is self
     struct [[eosio::table]] ticket {
        uint64_t id;
        uint64_t serial_number;
        name event;
        name owner;
        name ticket_name;
        bool valid;
        std::optional<string> relative_uri; //for specific metadata to the ticket


        uint64_t primary_key() const { return id; }
        uint64_t get_owner() const { return owner.value; }
     };

     EOSLIB_SERIALIZE( ticket, (id)(serial_number)(event)(owner)(ticket_name)(valid)(relative_uri) )


     // scope is owner
     struct [[eosio::table]] account {
        uint64_t event_ticket_id;
        name event;
        name ticket_name;
        asset amount;

        uint64_t primary_key() const { return event_ticket_id; }
     };

     struct [[eosio::table]] ask {
       uint64_t batch_id;
       vector<uint64_t> ticket_ids;
       name event;
       name seller;
       asset ask_price;
       time_point_sec expiration;

       uint64_t primary_key() const { return batch_id; }
       uint64_t get_byevent() const { return event.value; }
       uint64_t get_byprice() const { return ask_price.amount; }

     };

     // if you add transferability
     struct [[eosio::table]] lockedticket {
        uint64_t ticket_id;

        uint64_t primary_key() const { return ticket_id; }
     };

     using config_index = eosio::singleton<"tokenconfigs"_n, tokenconfigs>;
     using event_index = eosio::multi_index<"events"_n, event>;
     using account_index = eosio::multi_index<"accounts"_n, account>;
     using stat_index = eosio::multi_index<"ticketstats"_n, ticket_stat>;
     using ticket_index = eosio::multi_index<"tickets"_n, ticket, indexed_by<"byowner"_n, const_mem_fun<ticket, uint64_t, &ticket::get_owner>>>;
     using ask_index = eosio::multi_index<"asks"_n, ask, indexed_by<"byevent"_n, const_mem_fun<ask, uint64_t, &ask::get_byevent>>, indexed_by<"byprice"_n, const_mem_fun< ask, uint64_t, &ask::get_byprice>>>;
     using lock_index = eosio::multi_index<"lockedtick"_n, lockedticket>;

    private:

     map<name, asset> calcfees(vector<uint64_t> ticket_ids, const asset& ask_amount, const name& seller );
     void changeowner(const name& from, const name& to, vector<uint64_t> ticket_ids, const string& memo, bool istransfer);
     void checkasset(const asset& amount);
     void mint(const name& to, const name& issuer, const name& event, const name& ticket_name, const asset& issued_supply, const string& relative_uri);
     void add_balance(const name& owner, const name& ram_payer, const name& event, const name& ticket_name, const uint64_t& event_ticket_id, const asset& quantity );
     void sub_balance(const name& owner, const uint64_t& event_ticket_id, const asset& quantity);


};
