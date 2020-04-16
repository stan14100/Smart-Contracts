#include "eventaccount.hpp"

void eventaccount::emplaceevent(name founder, uint64_t event_id, std::string eventname, asset fundinggoal, time_point start, time_point end, vector<ticket> tickets, vector<professional> professionals, std::vector<product> products, std::vector<std::string> event_categories, std::string description, std::vector<std::string> eventtags, std::vector<std::string> eventpichash, std::vector<std::string> venues, std::vector<sponsorship> sponsorships )
{
  //Ensure that the action is authorized by the founder
  require_auth(founder);

  events_withoutcr_table prevents(_self, _self.value);
  eosio_assert(fundinggoal.amount > 0, "Funding goal should be greater than zero" );

  //Ensure that inputs is valid
  eosio_assert(start.elapsed._count > current_time(), "Starting date already passed" );
  eosio_assert(end > start, "Event end date should be later than start date" );

  auto events_iterator = prevents.find(event_id);
  if(events_iterator == prevents.end() ) {
    prevents.emplace(_self, [&](auto& new_event) {
      new_event.event_id = event_id;
      new_event.founder = founder;
      new_event.start = start;
      new_event.end = end;
      new_event.fundinggoal = fundinggoal;
      new_event.tickets = tickets;
      new_event.professionals = professionals;
      new_event.products = products;
     });
  }
  else {
    eosio_assert(false, "Event with this id already exists");
  }

}

void eventaccount::emplcrowdeve(name founder, uint64_t event_id, std::string eventname, asset fundinggoal, time_point start, time_point end, time_point fundingstart, time_point fundingend, vector<ticket> tickets, vector<professional> professionals, std::vector<product> products, std::vector<std::string> event_categories, std::string description, std::vector<std::string> eventtags, std::vector<std::string> eventpichash, std::vector<std::string> venues, std::vector<invest_cat> investor_categories, double founderBonus, asset min_contribution, asset max_contribution, std::vector<sponsorship> sponsorships )
{
  //Ensure that the action is authorized by the founder
  require_auth(founder);

  events_cr_table events(_self, _self.value);
  eosio_assert(fundinggoal.amount > 0, "Funding goal should be greater than zero" );

  //Ensure that inputs is valid
  // eosio_assert(fundingstart.elapsed._count > current_time(), "Funding starting date already passed" );
  // eosio_assert(fundingend > fundingstart, "Funding end date should be later than funding start date" );
  eosio_assert(start.elapsed._count/1000000 > fundingend.elapsed._count/1000000 /* + 172800 */, "Event should start after the end of funding" );
  eosio_assert(end > start, "Event end date should be later than start date" );


  //Create the event in the table if doesn't exist already
  auto events_iterator = events.find(event_id);
  if(events_iterator == events.end() ) {
    events.emplace(_self, [&](auto& new_event) {
      new_event.event_id = event_id;
      new_event.founder = founder;
      new_event.min_contribution = min_contribution;
      new_event.max_contribution = max_contribution;
      new_event.start = start;
      new_event.end = end;
      new_event.fundinggoal = fundinggoal;
      new_event.money_raised = fundinggoal - fundinggoal;
      new_event.fundingstart = fundingstart;
      new_event.fundingend = fundingend;
      new_event.founder_bonus = founderBonus;
      new_event.bonus_cat = investor_categories;
      new_event.tickets = tickets;
      new_event.professionals = professionals;
      new_event.products = products;
    });
  }
  else {
      eosio_assert(false, "Event with this id already exists");
  }

  action(
    permission_level{get_self(), "active"_n},
    get_self(),
    "fundgoalch"_n,
    std::make_tuple(event_id, (fundingend.elapsed._count - current_time())/1000000 , event_id)
  ).send();

}

void eventaccount::crowdfunding(name from, uint64_t e_id, asset fund)
{
   //Ensure that the action is authorized by the founder
   require_auth(from);

   events_cr_table events(_self, _self.value);

   // Initialization string for eosio.token transfer memo
   std::string str("cometogetherfund:");

   //Send the eosio.token transfer differed transaction
   action pay = action(
     permission_level{from, "active"_n},
     "eosio.token"_n, //action
     "transfer"_n,  //contract
     std::make_tuple(from, get_self(), fund, std::string(str + std::to_string(e_id)))
   );
   pay.send();

   //Modify investors in events table
   auto events_iterator = events.find(e_id);
   std::string str1("Event with this id not found! Id: ");
   eosio_assert( events_iterator != events.end(), (str1 + std::to_string(e_id)).c_str());
   bool flag = false;
   for(unsigned int i = 0; i < events_iterator->investors.size(); i++){
     if(events_iterator->investors[i].investor_name == from )
     {
        flag= true;

        events.modify(events_iterator, _self, [&](auto& new_invest) {
        new_invest.investors[i].invested_amount += fund;
        new_invest.money_raised += fund;
        });
        break;
     }
   }
   if(!flag)
   {
      investor inv1;
      inv1.investor_name = from;
      inv1.invested_amount = fund;

      events.modify(events_iterator, _self, [&](auto& new_invest) {
         new_invest.investors.push_back(inv1);
         new_invest.money_raised += fund;
      });
   }
}

void eventaccount::preticket(name from, uint64_t e_id, vector< buy_ticket > buy_ticket_infos )
{
   //Ensure that the action is authorized by the founder
   require_auth(from);

   events_cr_table events(_self, _self.value);

   // Initialization string for eosio.token transfer memo
   std::string str("cometogetherpreticket:");
   bool flag = false;
   auto events_iterator = events.find(e_id);
   std::string str1("Event with this id not found! Id: ");
   eosio_assert( events_iterator != events.end(), (str1 + std::to_string(e_id)).c_str());
   unsigned int i;
   unsigned int j;

   for(j=0; j < buy_ticket_infos.size(); j++) {
     for(i = 0; i < events_iterator->tickets.size(); i++){
        if( buy_ticket_infos[j].category == events_iterator->tickets[i].name )
        {
          if( buy_ticket_infos[j].quantity <= events_iterator->tickets[i].max_pretickets )
          {
             if( buy_ticket_infos[j].cost/buy_ticket_infos[j].quantity == events_iterator->tickets[i].price )
             {
                //Send the eosio.token transfer differed transaction
                action pay = action(
                   permission_level{from, "active"_n},
                   "eosio.token"_n,
                   "transfer"_n,
                   std::make_tuple(from, get_self(), buy_ticket_infos[j].cost, std::string(str + std::to_string(e_id)))
                );
                pay.send();
                flag = true;
                break;
            }
            else {
               eosio_assert(false, "Wrong amount of payment for the ticket");
            }
         }
         else {
            std:: string no_more_tickets("No more funding tickets for category: ");
            eosio_assert(false, (no_more_tickets + buy_ticket_infos[j].category).c_str());
         }
      }
    }
    if(flag)
    {
      events.modify(events_iterator, _self, [&](auto& r) {
        for(unsigned int k = 0; k < buy_ticket_infos[j].quantity; k++) {
          r.tickets[i].owner.push_back(from);
        }
        r.tickets[i].max_pretickets -= buy_ticket_infos[j].quantity;
        r.tickets[i].sold += buy_ticket_infos[j].quantity;
        r.money_raised += buy_ticket_infos[j].cost;
      });
    }
  }
}

void eventaccount::tickets(name from, uint64_t e_id, vector< buy_ticket > buy_ticket_infos )
{
   //Ensure that the action is authorized by the founder
   require_auth(from);

   events_cr_table events(_self, _self.value);

   // Initialization string for eosio.token transfer memo
   std::string str("cometogetherticket:");
   bool flag = false;
   auto events_iterator = events.find(e_id);
   std::string str1("Event with this id not found! Id: ");
   eosio_assert( events_iterator != events.end(), (str1 + std::to_string(e_id)).c_str());
   unsigned int i;
   unsigned int j;

   eosio_assert( events_iterator->fundingend.elapsed._count < current_time(), "Tickets must be sold after crowdfunding end date" );

   for(j=0; j < buy_ticket_infos.size(); j++) {
     for(i = 0; i < events_iterator->tickets.size(); i++){
        if( buy_ticket_infos[j].category == events_iterator->tickets[i].name )
        {
          if( (buy_ticket_infos[j].quantity + events_iterator->tickets[i].sold) <= events_iterator->tickets[i].quantity )
          {
             if( buy_ticket_infos[j].cost/buy_ticket_infos[j].quantity == events_iterator->tickets[i].price )
             {
                //Send the eosio.token transfer differed transaction
                action pay = action(
                   permission_level{from, "active"_n},
                   "eosio.token"_n,
                   "transfer"_n,
                   std::make_tuple(from, get_self(), buy_ticket_infos[j].cost, std::string(str + std::to_string(e_id)))
                );
                pay.send();
                flag = true;
                break;
            }
            else {
               eosio_assert(false, "Wrong amount of payment for the ticket");
            }
         }
         else {
            uint32_t left = events_iterator->tickets[i].sold - events_iterator->tickets[i].quantity;
            std:: string no_more_tickets(" tickets left for category: ");
            eosio_assert(false, (std::to_string(left) + no_more_tickets + buy_ticket_infos[j].category).c_str());
         }
      }
    }
    if(flag)
    {
      events.modify(events_iterator, _self, [&](auto& r) {
        for(unsigned int k = 0; k < buy_ticket_infos[j].quantity; k++) {
          r.tickets[i].owner.push_back(from);
        }
        r.tickets[i].sold += buy_ticket_infos[j].quantity;
        r.money_raised += buy_ticket_infos[j].cost;
      });
    }
  }
}


void eventaccount::fundgoalch(uint64_t id, uint32_t delay, uint64_t sender_id)
{
  //Ensure that only contract account can call that contract
  require_auth(_self);
  print("Fund goal check function called \n");
  uint64_t max_delay = 3888000; //max delay supported by EOS

  //Check if funding ending date has already passedpayment
  if (delay <= max_delay && delay > 0)
  {
    // transaction to update the delay
    transaction last;
    last.actions.emplace_back(
      permission_level{get_self(), "active"_n},
      get_self(),
      "fundgoalch"_n,
      std::make_tuple(id, 0, ++sender_id)
    );
    last.delay_sec = delay; // last call to check action to reach the desired delay
    last.send(sender_id, get_self());
  }
  else if( delay == 0)
  {
    events_cr_table events(_self, _self.value);
    auto e_itr = events.find(id);
    std::string str("Event with this id not found! Id: ");
    eosio_assert( e_itr != events.end(), (str + std::to_string(id)).c_str());
    if( e_itr->fundinggoal.amount > e_itr->money_raised.amount )
    {
      action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "repayments"_n,
        std::make_tuple(id)
      ).send();
    }
    else
    {
      action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "acceptcheck"_n,
        std::make_tuple(id, ((e_itr->fundingend.elapsed._count - current_time())/1000000) + 172800 /*real time */ /*2 days after end of funding date*/, id)
      ).send();
    }
  }
  else
  {
       uint64_t remaining_delay = delay - max_delay;
       // transaction to update the delay
       transaction out;
       out.actions.emplace_back(
         permission_level{get_self(), "active"_n},
         get_self(),
         "fundgoalch"_n,
         std::make_tuple(id, remaining_delay, ++sender_id)
       );
       out.delay_sec = max_delay; // here we set the new delay which is maximum until remaining_delay is less the max_delay
       out.send(sender_id, get_self());
  }
}

void eventaccount::acceptcheck(uint64_t id, uint32_t delay, uint64_t sender_id)
{
  //Ensure that only contract account can call that contract
  require_auth(_self);
  print("professionals acceptance check \n");
  uint64_t max_delay = 3888000; //max delay supported by EOS

  //Check if funding ending date has already passedpayment
  if (delay <= max_delay && delay > 0)
  {
    // transaction to update the delay
    transaction last;
    last.actions.emplace_back(
      permission_level{get_self(), "active"_n},
      get_self(),
      "acceptcheck"_n,
      std::make_tuple(id, 0, ++sender_id)
    );
    last.delay_sec = delay; // last call to check action to reach the desired delay
    last.send(sender_id, get_self());
  }
  else if( delay == 0)
  {
    events_cr_table events(_self, _self.value);
    auto e_itr = events.find(id);
    std::string str("Event with this id not found! Id: ");
    eosio_assert( e_itr != events.end(), (str + std::to_string(id)).c_str());
    bool flag = false;
    for(auto& pr: e_itr->professionals )  {
      if( pr.accept == true )
      {
        flag = true;
      }
      else {
        flag = false;
        break;
      }
    }
    if(flag)
    {
      action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "paydelay"_n,
        std::make_tuple(id, ((e_itr->end.elapsed._count - current_time())/1000000) /*+ 172800 real mode*/ /*2 days after end of event */, id)
      ).send();
    }
    else
    {
      action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "repayments"_n,
        std::make_tuple(id)
      ).send();
    }
  }
  else
  {
       uint64_t remaining_delay = delay - max_delay;

       // transaction to update the delay
       transaction out;
       out.actions.emplace_back(
         permission_level{get_self(), "active"_n},
         get_self(),
         "acceptcheck"_n,
         std::make_tuple(id, remaining_delay, ++sender_id)
       );
       out.delay_sec = max_delay; // here we set the new delay which is maximum until remaining_delay is less the max_delay
       out.send(sender_id, get_self());
  }
}

void eventaccount::paydelay(uint64_t id, uint32_t delay, uint64_t sender_id)
{
  //Ensure that only contract account can call that contract
  require_auth(_self);
  print("payments 2 days after end of event \n");
  uint64_t max_delay = 3888000; //max delay supported by EOS

  //Check if funding ending date has already passedpayment
  if (delay <= max_delay && delay > 0)
  {
    // transaction to update the delay
    transaction last;
    last.actions.emplace_back(
      permission_level{get_self(), "active"_n},
      get_self(),
      "paydelay"_n,
      std::make_tuple(id, 0, ++sender_id)
    );
    last.delay_sec = delay; // last call to check action to reach the desired delay
    last.send(sender_id, get_self());
  }
  else if( delay == 0)
  {
    //we should implement an arbitrator here
      action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "payments"_n,
        std::make_tuple(id)
      ).send();

      action(
        permission_level{get_self(), "active"_n},
        get_self(),
        "invpayment"_n,
        std::make_tuple(id)
      ).send();

  }
  else
  {
       uint64_t remaining_delay = delay - max_delay;

       // transaction to update the delay
       transaction out;
       out.actions.emplace_back(
         permission_level{get_self(), "active"_n},
         get_self(),
         "paydelay"_n,
         std::make_tuple(id, remaining_delay, ++sender_id)
       );
       out.delay_sec = max_delay; // here we set the new delay which is maximum until remaining_delay is less the max_delay
       out.send(sender_id, get_self());
  }
}

void eventaccount::transfer(name from, name to, asset quantity, std::string memo )
{
  //Ensure that the action is authorized by the sender
  require_auth(from);

  events_cr_table events(_self, _self.value);
  events_withoutcr_table prevents(_self,_self.value);

  eosio_assert(from == _self || to == _self, "Must be incoming or outgoing transfer");

  //Ensure this is not an outgoing transfer
  if(from == _self || to != _self) {
    //do nothing
    return;
  }

  //Transfer variables validation
  //eosio_assert( quantity.symbol == S(4, EOS) , "We accept only EOS for the early stages");
  eosio_assert( quantity.is_valid(), "invalid quantity" );
  eosio_assert( quantity.amount > 0, "must transfer positive quantity" );

  //Check if the memo starts with cometogether
  std::string memocheck = "cometogether";
  auto memostart = memo.rfind(memocheck, 0);

  if(memostart == 0)
  {

    auto pos=memo.find("myevent:");
    if(pos != -1)
    {
      printf("myevent /n");
      auto des_id = memo.substr(pos+8);
      std::string::size_type sz = 0;
      auto id = std::stoull (des_id,&sz,0);
      auto events_iterator = prevents.find(id);
      std::string str("Event with this id not found! Id: ");
      eosio_assert( events_iterator != prevents.end(), (str + std::to_string(id)).c_str());
      eosio_assert( events_iterator->founder == from, ("Only " + std::to_string(id) + " should pay for this event").c_str() );
      eosio_assert( quantity.amount == events_iterator->fundinggoal.amount, "You should cover all funding goal cost");

    }

    pos = memo.find("fund:");
    if(pos != -1)
    {
      printf("fund /n");
      auto des_id = memo.substr(pos+5);
      std::string::size_type sz = 0;
      auto id = std::stoull (des_id,&sz,0);
      auto events_iterator = events.find(id);
      std::string str("Event with this id not found! Id: ");
      eosio_assert( events_iterator != events.end(), (str + std::to_string(id)).c_str());
//      eosio_assert(events_iterator->fundingstart.elapsed._count <= current_time(), "Funding date has not started yet");
      eosio_assert( events_iterator->fundingend.elapsed._count >= current_time(), "Funding ending date already passed" );
      eosio_assert( events_iterator->fundinggoal.amount >= events_iterator->money_raised.amount, "We will overtake the funding goal" );
      eosio_assert( events_iterator->min_contribution <= quantity, ("Your contribution should be over " + std::to_string(events_iterator->min_contribution.amount)).c_str());
      eosio_assert( events_iterator->max_contribution >= quantity, ("Your contribution should be below " + std::to_string(events_iterator->max_contribution.amount)).c_str());
    }

    pos = memo.find("pretick:");
    if(pos != -1)
    {
      printf("pretick /n");
      auto desir_id = memo.substr(pos+10);
      std::string::size_type s = 0;
      auto e_id = std::stoull (desir_id,&s,0);
      auto events_iterator = events.find(e_id);
      std::string str("Event with this id not found! Id: ");
      eosio_assert( events_iterator != events.end(), (str + std::to_string(e_id)).c_str());
//      eosio_assert(events_iterator->fundingstart.elapsed._count <= current_time(), "Funding date has not started yet");
      eosio_assert(events_iterator->fundingend.elapsed._count >= current_time(), "Funding ending date already passed" );
      eosio_assert( events_iterator->fundinggoal.amount >= events_iterator->money_raised.amount, "We will overtake the funding goal");
    }

    bool crowdfunding_flag = true;

    auto posit = memo.find("withoutcr");
    if(posit != -1)
    {
      crowdfunding_flag= false;
    }

    pos = memo.find("ticket:");
    if(pos != -1)
    {
      printf("ticket /n");
      auto desir_id = memo.substr(pos+7);
      std::string::size_type s = 0;
      auto e_id = std::stoull (desir_id,&s,0);
      if(crowdfunding_flag)
      {
        auto events_iterator = events.find(e_id);
        std::string str("Event with this id not found! Id: ");
        eosio_assert( events_iterator != events.end(), (str + std::to_string(e_id)).c_str());
        eosio_assert( events_iterator->fundingend.elapsed._count <= current_time(), "Funding ending date already passed" );
      }
      else {
        auto events_iterator = prevents.find(e_id);
        std::string str("Event with this id not found! Id: ");
        eosio_assert( events_iterator != prevents.end(), (str + std::to_string(e_id)).c_str());
      }

    }

    pos = memo.find("sponsorship:");
    if(pos != -1)
    {
      printf("sponsorship /n");
      auto desir_id = memo.substr(pos+12);
      std::string::size_type s = 0;
      auto e_id = std::stoull (desir_id,&s,0);
      if(crowdfunding_flag)
      {
        auto events_iterator = events.find(e_id);
        std::string str("Event with this id not found! Id: ");
        eosio_assert( events_iterator != events.end(), (str + std::to_string(e_id)).c_str());
        eosio_assert(events_iterator->fundingend.elapsed._count <= current_time(), "Funding ending date already passed" );
      }
      else {
        auto events_iterator = prevents.find(e_id);
        std::string str("Event with this id not found! Id: ");
        eosio_assert( events_iterator != prevents.end(), (str + std::to_string(e_id)).c_str());
      }

    }
  }
  else {
    eosio_assert(false, "Invalid memo for eosio.token transfer please try again if you want to contribute to an event or want to buy a ticket/sponsorship" );
  }
}

void eventaccount::accept(name from, uint64_t event_id)
{
  require_auth(from);

  events_cr_table events(_self, _self.value);

  //Find the event in the events time_table
  auto e_iterator = events.find(event_id);
  std::string str("Event with this id not found! Id: ");
  eosio_assert( current_time()/1000000 < e_iterator->fundingend.elapsed._count/1000000 + 172800, "Period of acceptance has come to an end");
  for(unsigned int i = 0; i < e_iterator->professionals.size(); i++) {
    if(from == e_iterator->professionals[i].serv_provider)
    {
      events.modify(e_iterator, _self, [&](auto& accept) {
        accept.professionals[i].accept = true;
      });
      break;
    }
  }
}

void eventaccount::repayments(uint64_t e_id)
{
  //Ensure that only contract account can call that contract
  require_auth(_self);

  print("Repayments method \n");

  events_cr_table events(_self, _self.value);

  //Find the event in the events time_table
  auto e_iterator = events.find(e_id);
  std::string str("Event with this id not found! Id: ");
  eosio_assert( e_iterator != events.end(), (str + std::to_string(e_id)).c_str());
  std::string memo("Repayment from cometogether for event with id:");
  for(auto& investor : e_iterator->investors ) {
    action(
      permission_level{get_self(), "active"_n},
      "eosio.token"_n,
      "transfer"_n,
      std::make_tuple(get_self(), investor.investor_name, investor.invested_amount, std::string("cometogether " + memo + std::to_string(e_id)))
    ).send();
  }
  for(auto& category : e_iterator->tickets ) {
    for(auto& ticket_owner : category.owner ) {
      action(
        permission_level{get_self(), "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(get_self(), ticket_owner, category.price, std::string("cometogether " + memo + std::to_string(e_id)))
      ).send();
    }
  }
}

void eventaccount::payments(uint64_t event_id) {

  //Ensure that only contract account can call that contract   require_auth(get_self());
  require_auth(_self);

  print("Payments method \n");

  events_cr_table events(_self, _self.value);

  auto events_itr = events.find(event_id);
  if( events_itr != events.end() ) {
    std::string memo("Payment from cometogether for event with id:");
    for(auto& e : events_itr->professionals ) {
          action(
            permission_level{get_self(), "active"_n},
            "eosio.token"_n,
            "transfer"_n,
            std::make_tuple( get_self(), e.serv_provider, e.reward, std::string("cometogether " + memo + std::to_string(event_id)))
          ).send();
    }
  }
}

void eventaccount::onerror(uint64_t receiver, const eosio::onerror& error){
  print("starting onerror\n");
  const auto self = receiver;

  auto id = error.sender_id;
  print("Resending transaction ", error.sender_id, "as ", ++id, "\n");
  transaction failed_tx = error.unpack_sent_trx();
  failed_tx.send(id, _self);
}

template <typename T>
eventaccount::invest_cat eventaccount::invest_cat_finder(T const &events_itr, const eosio::asset& contributed)
{
  eosio::asset max;
  eventaccount::invest_cat invest_c;
  for (auto& investc: events_itr.bonus_cat) {
      if (investc.min_contr < contributed &&
          max < invest_c.min_contr) {

          invest_c = investc;
          max = investc.min_contr;
      }
  }
  return invest_c;
}

template <typename T>
eosio::asset eventaccount::product_profit_calculator(T const &events_itr)
{
    eosio::asset profits;

    for (auto ev_prod : events_itr.products )
    {
        profits += (ev_prod.event_price - ev_prod.seller_price) * ev_prod.quantity;
    }

    return profits;
}

template <typename T>
eosio::asset eventaccount::ticket_profit_calculator(T const & events_itr)
{
    eosio::asset profits;

    for (auto ev_ticket : events_itr.tickets )
    {
        profits += ev_ticket.price * ev_ticket.sold;
    }


    return profits;
}

template <typename T>
double eventaccount::general_percentage_calc(T const & events_itr)
{
    double percentage = events_itr.founder_bonus;

    for (auto contrib_c : events_itr.bonus_cat )
    {
            percentage += contrib_c.bonus;
    }

    return percentage;
}

template <typename T>
std::map<uint64_t, eosio::asset> eventaccount::sum_per_category(T const & events_itr)
{
    std::map<uint64_t, eosio::asset> res;

    for (auto& inv : events_itr.investors )
    {
          auto invest_c = invest_cat_finder(events_itr, inv.invested_amount);
          res[invest_c.invest_cat_id] += inv.invested_amount;
    }
    return res;
}

template <typename T>
std::map<eosio::name, eosio::asset> eventaccount::get_compensations(
            T const  &events_itr,
            const eosio::asset& profits,
            const double& total_bonus,
            std::map<uint64_t, eosio::asset>& total_per_c)
{
    std::map<eosio::name, eosio::asset> compensations;
    for (auto inv : events_itr.investors) {
        auto invest_c = invest_cat_finder(events_itr, inv.invested_amount);
        auto money_raised_in_cat =
            total_per_c[invest_c.invest_cat_id].amount;

        print("amount :",money_raised_in_cat );

        // payback without bonus = (invested_amount / money_raised) * profits * (1 - total_bonus)
        eosio::asset comp(inv.invested_amount / events_itr.money_raised, events_itr.money_raised.symbol);
        comp *=(profits.amount * (1 - total_bonus));

        //bonus money = invest_cat(invested_amount.amount).bonus * (invested_amount / sum(invest_cat(invested_amount).amount)) * profits
        eosio::asset tmp(invest_c.bonus * profits.amount * inv.invested_amount / money_raised_in_cat);

        //founder bonus
        if ( events_itr.founder == inv.investor_name ) {
            //bonus money for founder = founder_bonus * profits
            tmp += (events_itr.founder_bonus * profits);
        }

        //final payback
        comp += tmp;
        compensations[inv.investor_name] = comp;
    }
    return compensations;
}

void eventaccount::compensate(std::map<eosio::name, eosio::asset>&
                              compensations)
{
    for (auto& compensation : compensations) {
        name user = compensation.first;
        eosio::asset amount = compensation.second;
        std::string memo("compensation");
        auto data = std::make_tuple(_self, user, amount, memo);
        eosio::action(
            permission_level{get_self(), "active"_n}, //std::vector<eosio::permission_level>(2, {_self, N(active)}),
            "eosio.token"_n,   // contract
            "transfer"_n,      // action
            data
        ).send();
    }
}

void eventaccount::invpayment(const uint64_t& event_id)
{
    require_auth(_self);

    events_cr_table events(_self, _self.value);

    auto events_itr = events.get(event_id);
    eosio_assert(event_id == events_itr.event_id, "Couldn't find it");

    eosio::asset profits = product_profit_calculator(events_itr);
    profits += ticket_profit_calculator(events_itr);
    double g_percentage = general_percentage_calc(events_itr);
    auto total_per_c = sum_per_category(events_itr);
    auto compensations = get_compensations(events_itr,
                                           profits,
                                           g_percentage,
                                           total_per_c);

    compensate(compensations);

}

extern "C" {
  void apply(uint64_t receiver, uint64_t code, uint64_t action) {
      //eventaccount _eventaccount(name(receiver));

    if(code==receiver && action== name("emplaceevent").value) {
      execute_action(name(receiver), name(code), &eventaccount::emplaceevent );
    }
    else if(code==receiver && action== name("emplcrowdeve").value) {
      execute_action(name(receiver), name(code), &eventaccount::emplcrowdeve );
    }
    else if(code==receiver && action== name("crowdfunding").value) {
      execute_action(name(receiver), name(code), &eventaccount::crowdfunding );
    }
    else if(code==receiver && action== name("preticket").value) {
      execute_action(name(receiver), name(code), &eventaccount::preticket );
    }
    else if(code==receiver && action== name("tickets").value) {
      execute_action(name(receiver), name(code), &eventaccount::tickets );
    }
    else if(code==receiver && action== name("fundgoalch").value) {
      execute_action(name(receiver), name(code), &eventaccount::fundgoalch );
    }
    else if(code==receiver && action== name("acceptcheck").value) {
      execute_action(name(receiver), name(code), &eventaccount::acceptcheck );
    }
    else if(code==receiver && action== name("paydelay").value) {
      execute_action(name(receiver), name(code), &eventaccount::paydelay );
    }
    else if(code==receiver && action== name("accept").value) {
      execute_action(name(receiver), name(code), &eventaccount::accept );
    }
    else if(code==receiver && action== name("repayments").value) {
      execute_action(name(receiver), name(code), &eventaccount::repayments );
    }
    else if(code==receiver && action== name("payments").value) {
      execute_action(name(receiver), name(code), &eventaccount::payments );
    }
    else if(code==receiver && action== name("invpayment").value) {
      execute_action(name(receiver), name(code), &eventaccount::invpayment );
    }
    else if(code==name("eosio").value && action == name("onerror").value ) {
      //apply_onerror(receiver, onerror::from_current_action());
      execute_action(name(receiver), name(code), &eventaccount::onerror );
    }
    else if(code==name("eosio.token").value && action== name("transfer").value) {
      execute_action(name(receiver), name(code), &eventaccount::transfer );
    }
  }
};
