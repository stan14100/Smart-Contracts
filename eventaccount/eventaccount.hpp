#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/transaction.hpp>

using std::vector;
using namespace eosio;


class [[eosio::contract]] eventaccount : public eosio::contract {

  private:
    struct ticket {
      std::string name;
      std::vector<eosio::name> owner;
      uint32_t max_pretickets; //check max_pretickets > quantity && quantity > 0
      uint32_t sold = 0;
      asset price;
      uint32_t quantity;
    };

    struct invest_cat {
      uint8_t invest_cat_id;
      eosio::asset min_contr;
      double bonus;
    };

    struct investor {
      name investor_name;
      asset invested_amount;
    };

    struct product {
      uint64_t product_id;
      name seller;
      eosio::asset seller_price;
      eosio::asset event_price;
      uint32_t quantity;
      //product(): quantity(0) {}
    };

    struct professional {
      name serv_provider;
      eosio::asset reward;
      bool accept = false;
      //professional(): accept(false) {}
    };

    struct buy_ticket {
      std::string category;
      uint8_t quantity;
      eosio::asset cost;
    };

    struct sponsorship
    {
        std::string name;
        eosio::asset price;
        uint32_t spots;
    };

    struct [[eosio::table]] event_cr {
      uint64_t event_id;
      name founder;
      asset fundinggoal;
      asset money_raised;
      asset min_contribution;
      asset max_contribution;
      time_point start;
      time_point end;
      time_point fundingstart;
      time_point fundingend;
      double founder_bonus;
      std::vector<invest_cat> bonus_cat;
      std::vector<professional> professionals;
      std::vector<ticket> tickets;
      std::vector<product> products;
      std::vector<investor> investors;

      uint64_t primary_key() const { return event_id; }
    };

    typedef eosio::multi_index<"events"_n, event_cr> events_cr_table;

    struct [[eosio::table]] event_wcr {
      uint64_t event_id;
      name founder;
      asset fundinggoal;
      time_point start;
      time_point end;
      std::vector<professional> professionals;
      std::vector<ticket> tickets;
      std::vector<product> products;

      uint64_t primary_key() const { return event_id; }
    };

    typedef eosio::multi_index<"prevents"_n, event_wcr> events_withoutcr_table;


    /**
     * Calculate the profits achieved in total on the products sold
     *
     * \f$profits_on_products = sum(product.event_price - product.seller_price)\f$
     */
    template <typename T>
    invest_cat invest_cat_finder(T const &events_itr, const eosio::asset& contributed);

    /**
     * Calculate the profits achieved in total on the products sold
     *
     * \f$profits_on_products = sum(product.event_price - product.seller_price)\f$
     */
    template <typename T>
    eosio::asset product_profit_calculator(T const &events_itr);

    /**
     * Calculate the profits achieved in total on the tickets sold
     *
     * \f$profits_on_products = sum(ticket.price*ticket.quantity_sold\f$
     */
    template <typename T>
    eosio::asset ticket_profit_calculator(T const & events_itr);

    /**
     * Calculate the general percentage
     *
     * \f$gen_percentage = event.founder_bonus + sum(invest_cat.bonus)\f$
     */
    template <typename T>
    double general_percentage_calc(T const &events_itr);

    /**
     * Calculate the money raised per invest category
     */
    template <typename T>
    std::map<uint64_t, eosio::asset> sum_per_category(T const & events_itr);

    /**
     * Calculate for every user who has contributed to the event
     * what is the compensation.
     *
     * @param event_itr the event to calculate the compensations from
     * @param profits the profits made on the event
     * @param g_percentage the general percentage to distrbute
     * @param total_per_c the contributions categorized
     *
     * @return a map of account_name as key asset as value
     */
    template <typename T>
    std::map<eosio::name, eosio::asset> get_compensations( T const &events_itr,
                                                                          const eosio::asset& profits,
                                                                          const double& total_bonus,
                                                                          std::map<uint64_t, eosio::asset>& total_per_c );

    /**
     * Compensate every investor by sending them the tokens through
     * the contract "eosio.token" action "transfer"
     *
     * @param compensations the map returned by get_compensations
     */
    void compensate(std::map<eosio::name, eosio::asset>& compensations);

  public:
    using contract::contract;
    eventaccount(name receiver, name code,  datastream<const char*> ds): contract(receiver, code, ds) {}

    /**
     * Emplace the events that are going to implemented via the crowdfunding process and fires the fundgoalch action
     *
     */
    [[eosio::action]]
    void emplcrowdeve(name founder, uint64_t event_id, std::string eventname, asset fundinggoal, time_point start, time_point end, time_point fundingstart, time_point fundingend, vector<ticket> tickets, vector<professional> professionals, std::vector<product> products, std::vector<std::string> event_categories, std::string description, std::vector<std::string> eventtags, std::vector<std::string> eventpichash, std::vector<std::string> venues, std::vector<invest_cat> investor_categories, double founderBonus, asset min_contribution, asset max_contribution, std::vector<sponsorship> sponsorships );

    /**
     * Emplace the events that are not going to have crowdfunding process
     *
     */
    [[eosio::action]]
    void emplaceevent(name founder, uint64_t event_id, std::string eventname, asset fundinggoal, time_point start, time_point end, vector<ticket> tickets, vector<professional> professionals, std::vector<product> products, std::vector<std::string> event_categories, std::string description, std::vector<std::string> eventtags, std::vector<std::string> eventpichash, std::vector<std::string> venues, std::vector<sponsorship> sponsorships );

    /**
     * Listens every incoming transaction and filters them with respect to the memo
     *
     */
    [[eosio::action]]
    void transfer(name from, name to, asset balance, std::string memo);

    /**
     * In the end of funding date checks if the funding goal reached and if so it fires the acceptcheck action, otherwise it fires the repayments action
     *
     */
    [[eosio::action]]
    void fundgoalch(uint64_t id, uint32_t delay, uint64_t sender_id);

    /**
     * 2 days after the funding end date checks if service providers have their invitation accepted if so fires the paydelay action
     *
     */
    [[eosio::action]]
    void acceptcheck(uint64_t id, uint32_t delay, uint64_t sender_id);

    /**
     * 2 days after the end of the event if no disputes have been raised fires the payments action and the profits action
     *
     */
    [[eosio::action]]
    void paydelay(uint64_t id, uint32_t delay, uint64_t sender_id);

    /**
     * Sends the invested money back to investors when an event have been canceled
     *
     */
    [[eosio::action]]
    void repayments(uint64_t e_id);

    /**
     * Sends the rewards to service providers (All the money are from funding goal)
     *
     */
    [[eosio::action]]
    void payments(uint64_t event_id);

    /**
     * Transfers the money to the event through crowdfunding and updates the table of the event
     *
     */
    [[eosio::action]]
    void crowdfunding(name from, uint64_t e_id, asset fund);

    /**
     * Transfers the money to the event in exchange of a ticket(preticket). The money being added to the crowdfunding raised money
     * (You can buy preticket on;y in the period of time that crowdfunding takes place )
     *
     */
    [[eosio::action]]
    void preticket(name from, uint64_t e_id, vector< buy_ticket > buy_ticket_infos );

    /**
     * Transfers the money to the event in exchange of a ticket
     * (You can buy ticket only after funding ending date if there is one, otherwise whenever you want)
     */
    [[eosio::action]]
    void tickets(name from, uint64_t e_id, vector< buy_ticket > buy_ticket_infos );

    /**
     * Every professional call this function in order to accept the invitation that send to him, so as to assure his participation to the certain event
     *
     */
    [[eosio::action]]
    void accept(name from, uint64_t event_id);

    /**
     * Sends all the raised money to investors with respect to the fiscal plan of the event
     *
     *
     *
     */
    [[eosio::action]]
    void invpayment(const uint64_t& event_id);

    /**
     * Resend a transaction if it fails
     *
     *
     *
     */
    [[eosio::action]]
    void onerror(uint64_t receiver, const eosio::onerror& error);

};
